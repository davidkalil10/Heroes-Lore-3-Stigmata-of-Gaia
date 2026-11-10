"""Objective-C runtime model: class registry, objc_msgSend dispatch, host-backed Foundation objects."""

from __future__ import annotations

import struct

from .machine import Machine, GuestFault, RET_STUB

# Superclass defaults for imported (HLE) classes.
HLE_SUPERCLASS = {
    "NSObject": None,
    "NSString": "NSObject",
    "NSConstantString": "NSString",
    "NSMutableString": "NSString",
    "NSArray": "NSObject",
    "NSMutableArray": "NSArray",
    "NSDictionary": "NSObject",
    "NSMutableDictionary": "NSDictionary",
    "NSData": "NSObject",
    "NSMutableData": "NSData",
    "NSSet": "NSObject",
    "NSMutableSet": "NSSet",
    "NSNumber": "NSObject",
    "NSDecimalNumber": "NSNumber",
    "UIView": "UIResponder",
    "UIResponder": "NSObject",
    "UIWindow": "UIView",
    "UIViewController": "UIResponder",
    "UIControl": "UIView",
    "UIButton": "UIControl",
    "UITextField": "UIControl",
    "UILabel": "UIView",
    "UIImageView": "UIView",
    "UIWebView": "UIView",
    "UIActivityIndicatorView": "UIView",
    "UIApplication": "UIResponder",
    "CAEAGLLayer": "NSObject",
    "NSMutableURLRequest": "NSURLRequest",
    "NSMutableCharacterSet": "NSCharacterSet",
}


IGNORED_SELECTORS = {
    "bringSubviewToFront:",
    "sendSubviewToBack:",
    "removeFromSuperview",
    "becomeFirstResponder",
    "resignFirstResponder",
    "layoutIfNeeded",
    "setNeedsLayout",
    "setNeedsDisplay",
    "startAnimating",
    "stopAnimating",
    "cancel",
    "invalidate",
    "makeKeyWindow",
}


class HClass:
    def __init__(self, name: str, addr: int, meta: int, sup: "HClass | None", instance_size: int = 4, image: bool = False):
        self.name = name
        self.addr = addr
        self.meta = meta
        self.sup = sup
        self.instance_size = instance_size
        self.image = image
        self.imethods: dict[str, object] = {}  # sel -> int (guest IMP) | callable (HLE)
        self.cmethods: dict[str, object] = {}
        self.ivars: dict[str, int] = {}

    def chain(self):
        c = self
        while c is not None:
            yield c
            c = c.sup


class Args:
    """Accessor for message arguments (after receiver and selector)."""

    def __init__(self, m: Machine, first_word: int):
        self.m = m
        self.first = first_word

    def __getitem__(self, i: int) -> int:
        return self.m.arg(self.first + i)

    def f32(self, i: int) -> float:
        return struct.unpack("<f", struct.pack("<I", self[i]))[0]

    def f64(self, i: int) -> float:
        return struct.unpack("<d", struct.pack("<II", self[i], self[i + 1]))[0]


# Registries filled by decorators in hle modules.
IMETHODS: dict[tuple[str, str], object] = {}
CMETHODS: dict[tuple[str, str], object] = {}
FUNCS: dict[str, object] = {}
DATA_STRINGS: dict[str, str] = {}
DATA_ZERO: dict[str, int] = {}


def im(cls: str, sel: str):
    def deco(fn):
        for s in sel.split("|"):
            IMETHODS[(cls, s)] = fn
        return fn

    return deco


def cm(cls: str, sel: str):
    def deco(fn):
        for s in sel.split("|"):
            CMETHODS[(cls, s)] = fn
        return fn

    return deco


def func(name: str):
    def deco(fn):
        for n in name.split("|"):
            FUNCS[n] = fn
        return fn

    return deco


class ObjC:
    def __init__(self, m: Machine):
        self.m = m
        self.by_name: dict[str, HClass] = {}
        self.by_addr: dict[int, HClass] = {}
        self.by_meta: dict[int, HClass] = {}
        self.hobj: dict[int, object] = {}
        self.sel_cache: dict[int, str] = {}
        self.msg_log_unimpl: set[tuple[str, str]] = set()
        self.msg_counters = m.counters
        self.nsconst_class: HClass | None = None

    # -------------------------------------------------------------- classes
    def register_image_classes(self):
        m = self.m
        info = m.objc_info
        for c in info["classes"]:
            addr = int(c["class_addr"], 16)
            meta = int(c["metaclass_addr"], 16)
            hc = HClass(c["name"], addr, meta, None, c["instance_size"], image=True)
            for mt in c["instance_methods"]:
                hc.imethods[mt["selector"]] = int(mt["imp"], 16) | (1 if mt["imp_isa"] == "thumb" else 0)
            for mt in c["class_methods"]:
                hc.cmethods[mt["selector"]] = int(mt["imp"], 16) | (1 if mt["imp_isa"] == "thumb" else 0)
            for iv in c["ivars"]:
                hc.ivars[iv["name"]] = iv["offset"] if iv["offset"] is not None else 0
            self.by_name[hc.name] = hc
            self.by_addr[addr] = hc
            self.by_meta[meta] = hc
        for c in info["classes"]:
            hc = self.by_name[c["name"]]
            sup_name = c["superclass"]
            if sup_name:
                hc.sup = self.hle_class(sup_name)
            # The image's own class_t superclass word was patched by relocation for imported
            # supers; for image supers it already points at the right class_t.

    def hle_class(self, name: str) -> HClass:
        if name in self.by_name:
            return self.by_name[name]
        sup_name = HLE_SUPERCLASS.get(name, "NSObject" if name != "NSObject" else None)
        sup = self.hle_class(sup_name) if sup_name else None
        m = self.m
        addr = m.class_alloc(32)
        meta = m.class_alloc(32)
        hc = HClass(name, addr, meta, sup, instance_size=8)
        # class_t: isa, superclass, cache, vtable, data
        m.wr32(addr, meta)
        m.wr32(addr + 4, sup.addr if sup else 0)
        m.wr32(meta, self.hle_class("NSObject").meta if name != "NSObject" else meta)
        m.wr32(meta + 4, sup.meta if sup else addr)
        self.by_name[name] = hc
        self.by_addr[addr] = hc
        self.by_meta[meta] = hc
        for (cn, sel), fn in IMETHODS.items():
            if cn == name:
                hc.imethods[sel] = fn
        for (cn, sel), fn in CMETHODS.items():
            if cn == name:
                hc.cmethods[sel] = fn
        return hc

    def class_for_symbol(self, sym: str):
        if sym.startswith("_OBJC_CLASS_$_"):
            return self.hle_class(sym[len("_OBJC_CLASS_$_") :]).addr
        if sym.startswith("_OBJC_METACLASS_$_"):
            return self.hle_class(sym[len("_OBJC_METACLASS_$_") :]).meta
        return None

    # -------------------------------------------------------------- objects
    def alloc_instance(self, cls: HClass) -> int:
        addr = self.m.malloc(max(cls.instance_size, 8))
        self.m.wr32(addr, cls.addr)
        return addr

    def new_obj(self, clsname: str, payload=None) -> int:
        cls = self.hle_class(clsname)
        addr = self.alloc_instance(cls)
        if payload is not None:
            self.hobj[addr] = payload
        return addr

    def class_of(self, obj: int) -> tuple[HClass | None, bool]:
        """Returns (class, is_class_object)."""
        if obj == 0:
            return None, False
        try:
            isa = self.m.rd32(obj)
        except Exception:
            return None, False
        if isa in self.by_meta:
            return self.by_meta[isa], True
        if isa in self.by_addr:
            return self.by_addr[isa], False
        # metaclass-of-metaclass etc.
        return None, False

    def lookup(self, cls: HClass, sel: str, is_class: bool):
        for c in cls.chain():
            table = c.cmethods if is_class else c.imethods
            if sel in table:
                return table[sel]
        if is_class:  # class objects respond to root-class instance methods
            for c in cls.chain():
                if sel in c.imethods and c.name == "NSObject":
                    return c.imethods[sel]
        return None

    def sel_name(self, ptr: int) -> str:
        s = self.sel_cache.get(ptr)
        if s is None:
            s = self.m.cstr(ptr)
            self.sel_cache[ptr] = s
        return s

    # ------------------------------------------------------------ strings
    def pystr(self, obj: int):
        if obj == 0:
            return None
        h = self.hobj.get(obj)
        if isinstance(h, str):
            return h
        cls, is_class = self.class_of(obj)
        if cls is not None and cls.name == "NSConstantString":
            flags = self.m.rd32(obj + 4)
            ptr = self.m.rd32(obj + 8)
            length = self.m.rd32(obj + 12)
            if flags & 0x10000 or flags in (0x7C8, 0x7C8 | 0x10):
                return self.m.rdbytes(ptr, length).decode("latin-1")
            return self.m.rdbytes(ptr, length * 2).decode("utf-16le", errors="replace")
        if isinstance(h, (bytes, bytearray)):
            return bytes(h).decode("utf-8", errors="replace")
        return None

    def nsstring(self, text: str) -> int:
        return self.new_obj("NSString", text)

    # ------------------------------------------------------------ dispatch
    def msg_send(self, stret: bool = False, super_: bool = False):
        m = self.m
        base = 1 if stret else 0
        recv = m.reg(base)
        sel_ptr = m.reg(base + 1)
        sel = self.sel_name(sel_ptr)
        slot = m.import_addr["_objc_msgSend_stret" if stret else ("_objc_msgSendSuper2" if super_ else "_objc_msgSend")]
        start_cls = None
        if super_:
            sup_ptr = recv
            recv = m.rd32(sup_ptr)
            cur = self.by_addr.get(m.rd32(sup_ptr + 4)) or self.by_meta.get(m.rd32(sup_ptr + 4))
            cls, is_class = self.class_of(recv)
            if cls is not None and cur is not None:
                start_cls = cur.sup
        else:
            cls, is_class = self.class_of(recv)
        m.reset_forward(slot)
        if recv == 0:
            m.set_reg(0, 0)
            m.set_reg(1, 0)
            return
        if cls is None:
            self.m.unimplemented[f"msg-to-unknown-object:{sel}"] += 1
            m.set_reg(0, 0)
            return
        lookup_cls = start_cls if start_cls is not None else cls
        if getattr(m, "msg_trace", False):
            m.log(f"MSG {'+' if is_class else '-'}[{cls.name} {sel}] r0=0x{m.reg(0):08x} lr=0x{m.lr:08x}")
        target = self.lookup(lookup_cls, sel, is_class)
        self.msg_counters[m.phase][f"-[{cls.name} {sel}]" if not is_class else f"+[{cls.name} {sel}]"] += 1
        if target is None:
            m.unimplemented[f"{'+' if is_class else '-'}[{cls.name} {sel}]"] += 1
            m.set_reg(0, 0)
            m.set_reg(1, 0)
            if sel.startswith("set") or sel in IGNORED_SELECTORS:
                return  # property setters / lifecycle no-ops on HLE objects
            key = (cls.name, sel, is_class)
            if key not in self.msg_log_unimpl:
                self.msg_log_unimpl.add(key)
                m.log(f"UNIMPL msg {'+' if is_class else '-'}[{cls.name} {sel}] from lr=0x{m.lr:08x}")
            return
        if isinstance(target, int):
            m.forward_to(slot, target)
            return
        args = Args(m, base + 2)
        result = target(m.rt, recv, args)
        if stret:
            buf = m.reg(0)
            if isinstance(result, (bytes, bytearray)):
                m.wrbytes(buf, bytes(result))
            return
        if isinstance(result, tuple):
            m.set_reg(0, result[0])
            m.set_reg(1, result[1])
        else:
            m.set_reg(0, 0 if result is None else result)
