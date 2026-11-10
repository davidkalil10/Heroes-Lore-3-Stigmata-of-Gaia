"""Runtime assembly: Bundle + Machine + ObjC + HLE modules, boot sequence and trace."""

from __future__ import annotations

import json
import struct
from pathlib import Path

from . import objc as objc_mod
from .machine import (
    Bundle,
    GuestFault,
    HOST_STOP,
    Machine,
    ROOT,
    STACK_TOP,
)
from .objc import CMETHODS, DATA_STRINGS, FUNCS, IMETHODS, ObjC

APP_PATH = "/var/mobile/Applications/HL3/Heroes Lore.app"
DOCS_PATH = "/var/mobile/Applications/HL3/Documents"
HOST_DOCS = ROOT / "data" / "internal" / "Documents"

# Imported symbols that are data (not functions).
DATA_ZERO_SIZES = {
    "__objc_empty_cache": 16,
    "__objc_empty_vtable": 16,
    "___stderrp": 4,
    "_CGRectZero": 16,
    "__DefaultRuneLocale": 0x2000,
}


class HostArgs:
    def __init__(self, args):
        self.args = list(args)

    def __getitem__(self, i: int) -> int:
        return self.args[i] if i < len(self.args) else 0

    def f32(self, i: int) -> float:
        val = self[i]
        if isinstance(val, float):
            return val
        return struct.unpack("<f", struct.pack("<I", val))[0]

    def f64(self, i: int) -> float:
        val = self[i]
        if isinstance(val, float):
            return val
        return struct.unpack("<d", struct.pack("<II", self[i], self[i + 1]))[0]


class Runtime:
    def __init__(self, trace: bool = False):
        self.bundle = Bundle()
        self.m = Machine(self.bundle)
        self.m.trace_enabled = trace
        self.m.load_image(self.bundle.macho)
        self.objc = ObjC(self.m)
        self.m.rt = self
        self.exit_code = None
        self.pending_app_main = None
        self.state: dict = {}
        # import all HLE modules so decorators register
        from . import hle_libc, hle_foundation, hle_uikit, hle_gl, hle_audio  # noqa: F401

        self.audio_mixer = hle_audio.AudioMixer(self.bundle)
        self.objc.register_image_classes()
        self.m.install_traps(self._resolve)

    # ---------------------------------------------------------------- imports
    def _resolve(self, name: str):
        m = self.m
        cls_addr = self.objc.class_for_symbol(name)
        if cls_addr is not None:
            return "data", cls_addr
        if name == "___CFConstantStringClassReference":
            return "data", self.objc.hle_class("NSConstantString").addr
        if name in DATA_STRINGS:
            cell = m.malloc(8)
            m.wr32(cell, self.objc.nsstring(DATA_STRINGS[name]))
            return "data", cell
        if name in DATA_ZERO_SIZES:
            return "data", m.malloc(DATA_ZERO_SIZES[name])
        if name.startswith("__ZTV") or name.startswith("__ZTI"):
            return "data", m.malloc(64)
        if name == "_objc_msgSend":
            return "func", lambda mm: self.objc.msg_send()
        if name == "_objc_msgSend_stret":
            return "func", lambda mm: self.objc.msg_send(stret=True)
        if name == "_objc_msgSendSuper2":
            return "func", lambda mm: self.objc.msg_send(super_=True)
        fn = FUNCS.get(name)
        if fn is not None:
            return "func", lambda mm, fn=fn: fn(self)
        if name.startswith("_gl") or name.startswith("_al") or name.startswith("_AudioSession"):
            # state-setting GL / AL / audio-session calls: counted no-ops until the real backends exist
            return "func", lambda mm: 0

        def unimpl(mm, name=name):
            mm.unimplemented[name] += 1
            if mm.unimplemented[name] == 1:
                mm.log(f"UNIMPL import {name} from lr=0x{mm.lr:08x}")
            return 0

        return "func", unimpl

    # ------------------------------------------------------------------ paths
    def resolve_path(self, path: str):
        """Returns ('bundle', rel) | ('docs', Path) | None."""
        if path.startswith(APP_PATH):
            rel = path[len(APP_PATH) :].lstrip("/")
            return "bundle", rel
        if path.startswith(DOCS_PATH):
            rel = path[len(DOCS_PATH) :].lstrip("/")
            return "docs", HOST_DOCS / rel
        if path.startswith("/tmp") or path.startswith("/var/mobile/Applications/HL3/tmp"):
            return "docs", HOST_DOCS / "tmp" / path.split("/tmp", 1)[1].lstrip("/")
        return None

    # ------------------------------------------------------------------- boot
    def setup_main_stack(self):
        m = self.m
        sp = STACK_TOP - 0x400
        arg0 = m.new_cstr(APP_PATH + "/Heroes Lore")
        apple = m.new_cstr(APP_PATH + "/Heroes Lore")
        words = [1, arg0, 0, 0, apple, 0]  # argc, argv[0], NULL, envp NULL, apple[0], NULL
        sp -= len(words) * 4
        sp &= ~7
        for i, w in enumerate(words):
            m.wr32(sp + 4 * i, w)
        return sp

    def run_mod_inits(self):
        m = self.m
        sp = STACK_TOP - 0x2000
        for i, fn in enumerate(m.mod_inits):
            m.call(fn, (), sp=sp)

    def boot_to_app_main(self):
        """Runs mod_init + start until UIApplicationMain is trapped."""
        m = self.m
        self.run_mod_inits()
        sp = self.setup_main_stack()
        m.phase = "boot"
        m.uc.reg_write(13, sp)  # UC_ARM_REG_SP == 12? use named const below
        from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR

        m.uc.reg_write(UC_ARM_REG_SP, sp)
        m.uc.reg_write(UC_ARM_REG_LR, HOST_STOP)
        m.stop_reason = None
        try:
            m.uc.emu_start(m.entry, HOST_STOP)
        except Exception as exc:  # unicorn raises on faults
            if not self.pending_app_main:
                raise GuestFault(f"boot failed: {exc}; {m.stop_reason}")
        if not self.pending_app_main:
            raise GuestFault(f"boot ended without UIApplicationMain: {m.stop_reason}")

    def report(self) -> dict:
        m = self.m
        return {
            "phase_counters": {ph: dict(c.most_common()) for ph, c in m.counters.items()},
            "unimplemented": dict(m.unimplemented.most_common()),
        }

    # -------------------------------------------------------------- lifecycle
    def sel(self, name: str) -> int:
        cache = self.state.setdefault("sels", {})
        if name not in cache:
            ptr = None
            for ref in self.m.objc_info["selector_refs"]:
                if ref["selector"] == name:
                    ptr = self.m.rd32(int(ref["slot"], 16))
                    break
            cache[name] = ptr if ptr is not None else self.m.new_cstr(name)
        return cache[name]

    def send(self, recv: int, sel: str, *args, phase: str | None = None) -> int:
        """Host-initiated message send (top level only; never from inside an HLE handler)."""
        cls, is_class = self.objc.class_of(recv)
        target = self.objc.lookup(cls, sel, is_class)
        if target is None:
            return 0
        if isinstance(target, int):
            return self.m.call(target, (recv, self.sel(sel)) + tuple(args), sp=self.host_sp)
        if callable(target):
            return target(self, recv, HostArgs(args))
        raise GuestFault(f"host send of unsupported target for {sel}")

    def call_import(self, name: str, *args):
        """Invoke an imported trap by name from the host test runner."""
        fn = FUNCS.get(name)
        if fn is not None:
            return fn(self)
        addr = self.m.import_addr.get(name)
        if addr:
            sp = self.host_sp if hasattr(self, "host_sp") else STACK_TOP - 0x1000
            return self.m.call(addr, args, sp=sp)
        return 0

    def run_launch(self):
        info = self.pending_app_main
        m = self.m
        self.host_sp = (m.sp - 0x200) & ~7
        app = self.objc.new_obj("UIApplication")
        self.state["app"] = app
        delegate_cls = self.objc.by_name[info["delegate"]]
        delegate = self.objc.alloc_instance(delegate_cls)
        self.state["delegate"] = delegate
        self.objc.hobj[app] = {"delegate": delegate}
        m.phase = "launch"
        opts = self.objc.new_obj("NSDictionary", {})
        cls, _ = self.objc.class_of(delegate)
        if self.objc.lookup(cls, "application:didFinishLaunchingWithOptions:", False) is not None:
            self.send(delegate, "application:didFinishLaunchingWithOptions:", app, opts)
        else:
            self.send(delegate, "applicationDidFinishLaunching:", app)
        self.send(delegate, "applicationDidBecomeActive:", app)

    def read_file(self, path: str):
        r = self.resolve_path(path)
        if r is None:
            return None
        kind, p = r
        if kind == "bundle":
            return self.bundle.read(p) if self.bundle.exists(p) else None
        return p.read_bytes() if p.exists() else None
