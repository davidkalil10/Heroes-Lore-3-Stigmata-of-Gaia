"""Guest machine: Unicorn ARM CPU, memory map, Mach-O image loading, relocations and import traps."""

from __future__ import annotations

import ctypes
import hashlib
import io
import json
import struct
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

from unicorn import (
    UC_ARCH_ARM,
    UC_HOOK_CODE,
    UC_HOOK_MEM_READ_UNMAPPED,
    UC_HOOK_MEM_WRITE_UNMAPPED,
    UC_HOOK_MEM_FETCH_UNMAPPED,
    UC_MODE_ARM,
    Uc,
    UcError,
)
try:
    from unicorn.unicorn_py3.unicorn import uclib as _uclib
except Exception:  # pragma: no cover
    _uclib = None
from unicorn.arm_const import (
    UC_ARM_REG_C1_C0_2,
    UC_ARM_REG_CPSR,
    UC_ARM_REG_FPEXC,
    UC_ARM_REG_LR,
    UC_ARM_REG_PC,
    UC_ARM_REG_R0,
    UC_ARM_REG_R1,
    UC_ARM_REG_SP,
)

if getattr(sys, "frozen", False):
    _exe_dir = Path(sys.executable).resolve().parent
    if (_exe_dir / "project.json").exists():
        ROOT = _exe_dir
    elif hasattr(sys, "_MEIPASS") and (Path(sys._MEIPASS) / "project.json").exists():
        ROOT = Path(sys._MEIPASS)
    else:
        ROOT = _exe_dir
else:
    ROOT = Path(__file__).resolve().parents[2]

sys.path.insert(0, str(ROOT / "tools"))
import probe_macho_contract as probe  # noqa: E402
from inspect_apk import parse_macho_armv6  # noqa: E402
from .fast_stubs import install_native_fast_stubs

APP_PREFIX = probe.APP_PREFIX

IMAGE_BASE = 0x1000
IMAGE_END = 0xEA000
HEAP_BASE = 0x10000000
HEAP_SIZE = 0x08000000
CLASS_BASE = 0x38000000
CLASS_SIZE = 0x00400000
TRAP_BASE = 0x7F000000
TRAP_SIZE = 0x00100000
RET_STUB = 0x7F100000  # contains `bx lr`
HOST_STOP = 0x7F100010  # `until` address for host->guest calls
STACK_TOP = 0xBFFFF000
STACK_BASE = 0xBFF00000

ARM_LDR_PC_MINUS4 = 0xE51FF004
ARM_BX_LR = 0xE12FFF1E


class GuestFault(Exception):
    pass


class Machine:
    def __init__(self, resource_loader: "Bundle", native_fast_traps: bool = True):
        self.bundle = resource_loader
        self.native_fast_traps = native_fast_traps
        self.native_stubs: dict[str, int] = {}
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        try:
            from unicorn.arm_const import UC_CPU_ARM_1176

            self.uc.ctl_set_cpu_model(UC_CPU_ARM_1176)
        except Exception:  # pragma: no cover - older unicorn
            pass
        self.heap_ptr = HEAP_BASE + 0x1000
        self.alloc_sizes: dict[int, int] = {}
        self.class_ptr = CLASS_BASE
        self.trap_names: list[str] = []
        self.trap_handlers: list = []
        self.counters: dict[str, Counter] = defaultdict(Counter)
        self.phase = "boot"
        self.unimplemented: Counter = Counter()
        self.log_lines: list[str] = []
        self.trace_enabled = False
        self.stop_reason: str | None = None
        self.import_addr: dict[str, int] = {}
        self.data_symbols: dict[str, int] = {}
        self._uch = getattr(self.uc, "_uch", None) if _uclib is not None else None
        self._reg_u32 = ctypes.c_uint32()
        self._reg_u32_ptr = ctypes.byref(self._reg_u32)
        self._mem_u32_ba = bytearray(4)
        self._mem_u32_buf = (ctypes.c_char * 4).from_buffer(self._mem_u32_ba)
        self._mem_u32_view = memoryview(self._mem_u32_ba).cast("I")
        self._mem_buf_cap = 65536
        self._mem_buf = (ctypes.c_char * self._mem_buf_cap)()
        self._map_memory()
        self._enable_vfp()

    # ------------------------------------------------------------------ memory
    def _map_memory(self):
        uc = self.uc
        uc.mem_map(IMAGE_BASE, IMAGE_END - IMAGE_BASE)
        uc.mem_map(HEAP_BASE, HEAP_SIZE)
        uc.mem_map(CLASS_BASE, CLASS_SIZE)
        uc.mem_map(TRAP_BASE, TRAP_SIZE)
        uc.mem_map(RET_STUB, 0x1000)
        uc.mem_map(STACK_BASE, STACK_TOP + 0x1000 - STACK_BASE)
        uc.mem_write(RET_STUB, struct.pack("<I", ARM_BX_LR))
        uc.mem_write(HOST_STOP, struct.pack("<I", ARM_BX_LR))
        if self.native_fast_traps:
            self.native_stubs = install_native_fast_stubs(uc)

    def _enable_vfp(self):
        uc = self.uc
        try:
            uc.reg_write(UC_ARM_REG_C1_C0_2, uc.reg_read(UC_ARM_REG_C1_C0_2) | (0xF << 20))
            uc.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
        except UcError as exc:  # pragma: no cover
            self.log(f"WARN: VFP enable failed: {exc}")

    def log(self, msg: str):
        self.log_lines.append(msg)
        if self.trace_enabled:
            print(msg, file=sys.stderr)

    def rd32(self, addr: int) -> int:
        if self._uch is not None:
            status = _uclib.uc_mem_read(self._uch, addr, self._mem_u32_buf, 4)
            if status != 0:
                raise UcError(status, addr, 4)
            return self._mem_u32_view[0]
        return struct.unpack("<I", self.uc.mem_read(addr, 4))[0]

    def rd16(self, addr: int) -> int:
        return struct.unpack("<H", self.uc.mem_read(addr, 2))[0]

    def rds16(self, addr: int) -> int:
        return struct.unpack("<h", self.uc.mem_read(addr, 2))[0]

    def rd8(self, addr: int) -> int:
        return self.uc.mem_read(addr, 1)[0]

    def rds8(self, addr: int) -> int:
        return struct.unpack("<b", self.uc.mem_read(addr, 1))[0]

    def rdf32(self, addr: int) -> float:
        return struct.unpack("<f", self.uc.mem_read(addr, 4))[0]

    def wr32(self, addr: int, value: int):
        if self._uch is not None:
            self._mem_u32_view[0] = value & 0xFFFFFFFF
            status = _uclib.uc_mem_write(self._uch, addr, self._mem_u32_buf, 4)
            if status != 0:
                raise UcError(status, addr, 4)
            return
        self.uc.mem_write(addr, struct.pack("<I", value & 0xFFFFFFFF))

    def wr8(self, addr: int, value: int):
        self.uc.mem_write(addr, bytes([value & 0xFF]))

    def rdbytes(self, addr: int, n: int) -> bytes:
        if n <= 0:
            return b""
        if self._uch is not None:
            if n > self._mem_buf_cap:
                self._mem_buf_cap = max(n, self._mem_buf_cap * 2)
                self._mem_buf = (ctypes.c_char * self._mem_buf_cap)()
            status = _uclib.uc_mem_read(self._uch, addr, self._mem_buf, n)
            if status != 0:
                raise UcError(status, addr, n)
            return ctypes.string_at(self._mem_buf, n)
        return bytes(self.uc.mem_read(addr, n))

    def rdinto(self, addr: int, buf, n: int):
        """Read `n` bytes from guest `addr` directly into a ctypes char array `buf`."""
        if n <= 0:
            return
        if self._uch is not None:
            status = _uclib.uc_mem_read(self._uch, addr, buf, n)
            if status != 0:
                raise UcError(status, addr, n)
            return
        raw = bytes(self.uc.mem_read(addr, n))
        ctypes.memmove(buf, raw, n)

    def wrbytes(self, addr: int, data: bytes):
        if data:
            self.uc.mem_write(addr, data)

    def cstr(self, addr: int, limit: int = 1 << 16) -> str:
        if addr == 0:
            return ""
        out = bytearray()
        while len(out) < limit:
            chunk = bytes(self.uc.mem_read(addr + len(out), 64))
            idx = chunk.find(b"\x00")
            if idx >= 0:
                out += chunk[:idx]
                break
            out += chunk
        return out.decode("utf-8", errors="replace")

    def malloc(self, size: int, zero: bool = True) -> int:
        size = max(8, (size + 15) & ~15)
        addr = self.heap_ptr
        if addr + size > HEAP_BASE + HEAP_SIZE:
            raise GuestFault("guest heap exhausted")
        self.heap_ptr += size
        self.alloc_sizes[addr] = size
        return addr  # fresh pages are zero; the bump allocator never reuses memory

    def new_cstr(self, text: str) -> int:
        raw = text.encode("utf-8") + b"\x00"
        addr = self.malloc(len(raw))
        self.wrbytes(addr, raw)
        return addr

    def class_alloc(self, size: int) -> int:
        addr = self.class_ptr
        self.class_ptr += (size + 15) & ~15
        if self.class_ptr > CLASS_BASE + CLASS_SIZE:
            raise GuestFault("class region exhausted")
        return addr

    # --------------------------------------------------------------- registers
    def reg(self, n: int) -> int:
        if self._uch is not None:
            _uclib.uc_reg_read(self._uch, UC_ARM_REG_R0 + n, self._reg_u32_ptr)
            return self._reg_u32.value
        return self.uc.reg_read(UC_ARM_REG_R0 + n)

    def set_reg(self, n: int, value: int):
        if self._uch is not None:
            self._reg_u32.value = value & 0xFFFFFFFF
            _uclib.uc_reg_write(self._uch, UC_ARM_REG_R0 + n, self._reg_u32_ptr)
            return
        self.uc.reg_write(UC_ARM_REG_R0 + n, value & 0xFFFFFFFF)

    @property
    def sp(self) -> int:
        if self._uch is not None:
            _uclib.uc_reg_read(self._uch, UC_ARM_REG_SP, self._reg_u32_ptr)
            return self._reg_u32.value
        return self.uc.reg_read(UC_ARM_REG_SP)

    @property
    def lr(self) -> int:
        if self._uch is not None:
            _uclib.uc_reg_read(self._uch, UC_ARM_REG_LR, self._reg_u32_ptr)
            return self._reg_u32.value
        return self.uc.reg_read(UC_ARM_REG_LR)

    @property
    def cpsr(self) -> int:
        return self.uc.reg_read(UC_ARM_REG_CPSR)

    @cpsr.setter
    def cpsr(self, value: int):
        self.uc.reg_write(UC_ARM_REG_CPSR, value & 0xFFFFFFFF)

    def arg(self, n: int) -> int:
        """n-th 32-bit argument word (r0-r3, then the stack) at a trap entry."""
        if n < 4:
            return self.reg(n)
        return self.rd32(self.sp + 4 * (n - 4))

    # --------------------------------------------------------------- Mach-O load
    def load_image(self, macho: bytes):
        inventory, symbols, _ = parse_macho_armv6(macho)
        self.macho = macho
        self.inventory = inventory
        self.symbols = symbols
        self.img = probe.Image(macho, inventory, symbols)
        for name, vmaddr, vmsize, fileoff, filesize in self.img.segments:
            if name == "__PAGEZERO" or filesize == 0:
                continue
            self.uc.mem_write(vmaddr, macho[fileoff : fileoff + filesize])
        self.bindings = probe.build_bindings(self.img)
        self.objc_info = probe.build_objc(self.img)
        self.entry = int(inventory["entrypoint_unixthread"]["pc"], 16)
        self.mod_inits = [int(p, 16) for p in inventory["mod_init_func_pointers"]]

    def install_traps(self, resolver):
        """Assign a trap slot to every imported function and patch pointers/relocations.

        `resolver(name, kind)` returns ('func', handler) | ('data', address) | ('class', addr) ...
        """
        slot = TRAP_BASE
        for imp in self.bindings["imports"]:
            name = imp["name"]
            kind, payload = resolver(name)
            if kind == "func":
                if self.native_fast_traps and name in self.native_stubs:
                    target = self.native_stubs[name]
                    self.import_addr[name] = target
                else:
                    idx = len(self.trap_names)
                    self.trap_names.append(name)
                    self.trap_handlers.append(payload)
                    addr = TRAP_BASE + idx * 8
                    self.uc.mem_write(addr, struct.pack("<II", ARM_LDR_PC_MINUS4, RET_STUB))
                    self.import_addr[name] = addr
                    target = addr
            else:
                target = payload
                self.data_symbols[name] = payload
            for key in ("nl_symbol_ptr_addrs", "la_symbol_ptr_addrs"):
                for a in imp[key]:
                    self.wr32(int(a, 16), target)
            for a in imp["data_relocation_addrs"]:
                addr = int(a, 16)
                self.wr32(addr, (self.rd32(addr) + target) & 0xFFFFFFFF)
            # stubs: ldr-based stubs read the lazy pointer, which now holds the trap address.
        for p in self.bindings.get("defined_symbol_pointers", []):
            self.wr32(int(p["slot"], 16), int(p["value"], 16))
        n = len(self.trap_names)
        assert n * 8 < TRAP_SIZE
        self.uc.hook_add(UC_HOOK_CODE, self._trap_hook, begin=TRAP_BASE, end=TRAP_BASE + n * 8)
        self.uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED, self._fault_hook)

    def _fault_hook(self, uc, access, address, size, value, user):
        pc = uc.reg_read(UC_ARM_REG_PC)
        self.stop_reason = f"unmapped access type={access} addr=0x{address:08x} size={size} pc=0x{pc:08x} lr=0x{self.lr:08x}"
        return False

    def _trap_hook(self, uc, address, size, user):
        if address % 8:
            return
        idx = (address - TRAP_BASE) // 8
        name = self.trap_names[idx]
        self.counters[self.phase][name] += 1
        handler = self.trap_handlers[idx]
        try:
            result = handler(self)
        except GuestFault as exc:
            self.stop_reason = f"{name}: {exc}"
            uc.emu_stop()
            return
        if result is not None:
            if isinstance(result, tuple):
                self.set_reg(0, result[0])
                self.set_reg(1, result[1])
            else:
                self.set_reg(0, result)

    def forward_to(self, trap_addr_for_slot: int, target: int):
        """Make the current trap slot jump to `target` instead of returning (used by objc_msgSend)."""
        self.wr32(trap_addr_for_slot + 4, target)

    def reset_forward(self, trap_addr_for_slot: int):
        self.wr32(trap_addr_for_slot + 4, RET_STUB)

    # ------------------------------------------------------------- host -> guest
    def call(self, addr: int, args=(), sp: int | None = None, max_insn: int = 0) -> int:
        uc = self.uc
        for i, a in enumerate(args[:4]):
            self.set_reg(i, a)
        if sp is not None:
            uc.reg_write(UC_ARM_REG_SP, sp)
        sp0 = self.sp
        extra = args[4:]
        if extra:
            sp0 -= 4 * len(extra)
            sp0 &= ~7
            for i, a in enumerate(extra):
                self.wr32(sp0 + 4 * i, a)
            uc.reg_write(UC_ARM_REG_SP, sp0)
        uc.reg_write(UC_ARM_REG_LR, HOST_STOP)
        thumb = addr & 1
        cpsr = uc.reg_read(UC_ARM_REG_CPSR)
        uc.reg_write(UC_ARM_REG_CPSR, (cpsr | 0x20) if thumb else (cpsr & ~0x20))
        self.stop_reason = None
        try:
            uc.emu_start(addr & ~1, HOST_STOP, count=max_insn)
        except UcError as exc:
            regs = " ".join(f"r{i}=0x{self.reg(i):08x}" for i in range(13))
            raise GuestFault(
                f"{exc}: pc=0x{uc.reg_read(UC_ARM_REG_PC):08x} lr=0x{self.lr:08x} sp=0x{self.sp:08x} {regs} ({self.stop_reason})"
            )
        if self.stop_reason:
            raise GuestFault(self.stop_reason)
        return self.reg(0)


class Bundle:
    """Read-only view of Payload/Heroes Lore.app with localized asset overrides (S10)."""

    def __init__(self):
        project = json.loads((ROOT / "project.json").read_text(encoding="utf-8"))
        ipa_file = ROOT / "assets" / "game.ipa"
        if ipa_file.exists():
            ipa_bytes = ipa_file.read_bytes()
        else:
            apk = zipfile.ZipFile(ROOT / project["original_apk"]["path"])
            ipa_bytes = apk.read("assets/game.ipa")
        if hashlib.sha256(ipa_bytes).hexdigest() != project["nested_ipa"]["sha256"]:
            raise ValueError("IPA hash mismatch")
        self.zf = zipfile.ZipFile(io.BytesIO(ipa_bytes))
        self.names = {i.filename for i in self.zf.infolist()}
        self.macho = self.zf.read(APP_PREFIX + "Heroes Lore")
        if hashlib.sha256(self.macho).hexdigest() != project["macho_executable"]["sha256"]:
            raise ValueError("Mach-O hash mismatch")
        self.cache: dict[object, bytes] = {}
        assets_dir = ROOT / "assets"
        # Extended PT-BR fonts remain 100% compatible with EN & preserve original glyphs
        self._font_overrides: dict[str, Path] = {}
        for fn in ("font_mainmenu", "font_menu_medium", "font_menu_small"):
            for ext in ("rff", "pvr"):
                self._font_overrides[f"newres/fonts/{fn}.{ext}"] = assets_dir / "fonts" / f"{fn}.{ext}"
        self._ptbr_text_overrides: dict[str, Path] = {
            "newres/texts/t_en": assets_dir / "texts" / "t_pt-br",
            "newres/texts/hdr": assets_dir / "texts" / "hdr_pt-br",
            "dat/InGame_txt_2": assets_dir / "texts" / "InGame_txt_2_pt-br",
        }

    def _active_override_path(self, rel: str) -> Path | None:
        fpath = self._font_overrides.get(rel)
        if fpath is not None and fpath.exists():
            return fpath
        tpath = self._ptbr_text_overrides.get(rel)
        if tpath is not None and tpath.exists():
            from .i18n import get_active_language

            if get_active_language() == "pt-BR":
                return tpath
        return None

    def exists(self, rel: str) -> bool:
        if (APP_PREFIX + rel) in self.names:
            return True
        return self._active_override_path(rel) is not None

    def read(self, rel: str) -> bytes:
        ovr = self._active_override_path(rel)
        if rel in self._ptbr_text_overrides:
            from .i18n import get_active_language

            key: object = (rel, get_active_language())
        else:
            key = rel
        if key not in self.cache:
            if ovr is not None:
                self.cache[key] = ovr.read_bytes()
            else:
                self.cache[key] = self.zf.read(APP_PREFIX + rel)
        return self.cache[key]
