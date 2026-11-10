"""libc / libgcc / libstdc++ HLE."""

from __future__ import annotations

import math
import struct
import sys

from .objc import func
from .machine import GuestFault


def _u64(lo, hi):
    return (hi << 32) | lo


def _s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def _s64(v):
    return v - (1 << 64) if v & (1 << 63) else v


def _dbl(lo, hi):
    return struct.unpack("<d", struct.pack("<II", lo, hi))[0]


def _flt(w):
    return struct.unpack("<f", struct.pack("<I", w))[0]


def _ret_dbl(v):
    lo, hi = struct.unpack("<II", struct.pack("<d", v))
    return (lo, hi)


def _ret_flt(v):
    try:
        return struct.unpack("<I", struct.pack("<f", v))[0]
    except OverflowError:
        return struct.unpack("<I", struct.pack("<f", math.copysign(math.inf, v)))[0]


def _safe(op, a, b):
    try:
        return op(a, b)
    except ZeroDivisionError:
        if a == 0 or a != a:
            return math.nan
        return math.copysign(math.inf, a) * math.copysign(1.0, b)


# ---- VFP soft-float helpers (iOS ARMv6 libgcc) -----------------------------------
for _n, _op in (("add", lambda a, b: a + b), ("sub", lambda a, b: a - b), ("mul", lambda a, b: a * b), ("div", None)):
    pass


def _def_double_op(name, op):
    @func(f"___{name}df3vfp")
    def _f(rt, op=op):
        m = rt.m
        a = _dbl(m.reg(0), m.reg(1))
        b = _dbl(m.reg(2), m.reg(3))
        return _ret_dbl(_safe(op, a, b))


def _def_float_op(name, op):
    @func(f"___{name}sf3vfp")
    def _f(rt, op=op):
        m = rt.m
        return _ret_flt(_safe(op, _flt(m.reg(0)), _flt(m.reg(1))))


for _name, _op in (("add", lambda a, b: a + b), ("sub", lambda a, b: a - b), ("mul", lambda a, b: a * b), ("div", lambda a, b: a / b)):
    _def_double_op(_name, _op)
    _def_float_op(_name, _op)


def _def_dcmp(name, fn):
    @func(f"___{name}df2vfp")
    def _f(rt, fn=fn):
        m = rt.m
        return fn(_dbl(m.reg(0), m.reg(1)), _dbl(m.reg(2), m.reg(3))) & 0xFFFFFFFF


def _def_fcmp(name, fn):
    @func(f"___{name}sf2vfp")
    def _f(rt, fn=fn):
        m = rt.m
        return fn(_flt(m.reg(0)), _flt(m.reg(1))) & 0xFFFFFFFF


def _cmp_result(nan_val, test_true, a, b):
    if a != a or b != b:
        return nan_val
    return test_true(a, b)


for _n, _nanv, _t in (
    ("eq", 1, lambda a, b: 0 if a == b else 1),
    ("ne", 1, lambda a, b: 0 if a != b else 1),
    ("ge", -1, lambda a, b: 1 if a >= b else -1),
    ("gt", -1, lambda a, b: 1 if a > b else -1),
    ("le", 1, lambda a, b: -1 if a <= b else 1),
    ("lt", 1, lambda a, b: -1 if a < b else 1),
):
    _def_dcmp(_n, lambda a, b, _nanv=_nanv, _t=_t: _cmp_result(_nanv, _t, a, b))
    _def_fcmp(_n, lambda a, b, _nanv=_nanv, _t=_t: _cmp_result(_nanv, _t, a, b))


@func("___unorddf2vfp")
def _unord_d(rt):
    m = rt.m
    a, b = _dbl(m.reg(0), m.reg(1)), _dbl(m.reg(2), m.reg(3))
    return 1 if (a != a or b != b) else 0


@func("___unordsf2vfp")
def _unord_f(rt):
    m = rt.m
    a, b = _flt(m.reg(0)), _flt(m.reg(1))
    return 1 if (a != a or b != b) else 0


def _clamp_int(v):
    if v != v:
        return 0
    iv = int(v) if abs(v) != math.inf else (1 << 31) * (1 if v > 0 else -1)
    return max(-(1 << 31), min((1 << 31) - 1, iv)) & 0xFFFFFFFF


@func("___fixdfsivfp")
def _fixdfsi(rt):
    return _clamp_int(_dbl(rt.m.reg(0), rt.m.reg(1)))


@func("___fixsfsivfp")
def _fixsfsi(rt):
    return _clamp_int(_flt(rt.m.reg(0)))


@func("___floatsidfvfp")
def _floatsidf(rt):
    return _ret_dbl(float(_s32(rt.m.reg(0))))


@func("___floatsisfvfp")
def _floatsisf(rt):
    return _ret_flt(float(_s32(rt.m.reg(0))))


@func("___floatunssidfvfp")
def _floatunsidf(rt):
    return _ret_dbl(float(rt.m.reg(0)))


@func("___extendsfdf2vfp")
def _extsfdf(rt):
    return _ret_dbl(_flt(rt.m.reg(0)))


@func("___truncdfsf2vfp")
def _truncdfsf(rt):
    return _ret_flt(_dbl(rt.m.reg(0), rt.m.reg(1)))


def _trunc_div(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


@func("___divsi3")
def _divsi3(rt):
    a, b = _s32(rt.m.reg(0)), _s32(rt.m.reg(1))
    return 0 if b == 0 else _trunc_div(a, b) & 0xFFFFFFFF


@func("___modsi3")
def _modsi3(rt):
    a, b = _s32(rt.m.reg(0)), _s32(rt.m.reg(1))
    return 0 if b == 0 else (a - _trunc_div(a, b) * b) & 0xFFFFFFFF


@func("___umodsi3")
def _umodsi3(rt):
    a, b = rt.m.reg(0), rt.m.reg(1)
    return 0 if b == 0 else a % b


@func("___divdi3")
def _divdi3(rt):
    m = rt.m
    a, b = _s64(_u64(m.reg(0), m.reg(1))), _s64(_u64(m.reg(2), m.reg(3)))
    r = 0 if b == 0 else _trunc_div(a, b) & ((1 << 64) - 1)
    return (r & 0xFFFFFFFF, r >> 32)


@func("___moddi3")
def _moddi3(rt):
    m = rt.m
    a, b = _s64(_u64(m.reg(0), m.reg(1))), _s64(_u64(m.reg(2), m.reg(3)))
    r = 0 if b == 0 else (a - _trunc_div(a, b) * b) & ((1 << 64) - 1)
    return (r & 0xFFFFFFFF, r >> 32)


@func("_ceilf")
def _ceilf(rt):
    return _ret_flt(float(math.ceil(_flt(rt.m.reg(0)))))


# ---- memory / string ---------------------------------------------------------------
@func("_malloc")
def _malloc(rt):
    return rt.m.malloc(rt.m.reg(0))


@func("_free")
def _free(rt):
    return 0


@func("_memcpy|_memmove")
def _memcpy(rt):
    m = rt.m
    dst, src, n = m.reg(0), m.reg(1), m.reg(2)
    if n:
        m.wrbytes(dst, m.rdbytes(src, n))
    return dst


@func("_memset")
def _memset(rt):
    m = rt.m
    dst, c, n = m.reg(0), m.reg(1) & 0xFF, m.reg(2)
    if n:
        m.wrbytes(dst, bytes([c]) * n)
    return dst


@func("_memcmp")
def _memcmp(rt):
    m = rt.m
    a, b = m.rdbytes(m.reg(0), m.reg(2)), m.rdbytes(m.reg(1), m.reg(2))
    return (a > b) - (a < b) & 0xFFFFFFFF


def _cs(rt, i):
    return rt.m.cstr(rt.m.reg(i))


@func("_strlen")
def _strlen(rt):
    return len(_cs(rt, 0).encode("utf-8"))


def _cbytes(m, addr, limit=1 << 20):
    out = bytearray()
    while len(out) < limit:
        chunk = bytes(m.uc.mem_read(addr + len(out), 64))
        i = chunk.find(b"\x00")
        if i >= 0:
            out += chunk[:i]
            return bytes(out)
        out += chunk
    return bytes(out)


@func("_strcmp")
def _strcmp(rt):
    a, b = _cbytes(rt.m, rt.m.reg(0)), _cbytes(rt.m, rt.m.reg(1))
    return ((a > b) - (a < b)) & 0xFFFFFFFF


@func("_strncmp")
def _strncmp(rt):
    n = rt.m.reg(2)
    a, b = _cbytes(rt.m, rt.m.reg(0))[:n], _cbytes(rt.m, rt.m.reg(1))[:n]
    return ((a > b) - (a < b)) & 0xFFFFFFFF


@func("_strcpy")
def _strcpy(rt):
    m = rt.m
    m.wrbytes(m.reg(0), _cbytes(m, m.reg(1)) + b"\x00")
    return m.reg(0)


@func("_strncpy")
def _strncpy(rt):
    m = rt.m
    n = m.reg(2)
    s = _cbytes(m, m.reg(1))[:n]
    m.wrbytes(m.reg(0), s + b"\x00" * (n - len(s)))
    return m.reg(0)


@func("_strcat")
def _strcat(rt):
    m = rt.m
    d = _cbytes(m, m.reg(0))
    m.wrbytes(m.reg(0) + len(d), _cbytes(m, m.reg(1)) + b"\x00")
    return m.reg(0)


@func("_strchr")
def _strchr(rt):
    m = rt.m
    s = _cbytes(m, m.reg(0))
    c = m.reg(1) & 0xFF
    if c == 0:
        return m.reg(0) + len(s)
    i = s.find(bytes([c]))
    return 0 if i < 0 else m.reg(0) + i


@func("_strrchr")
def _strrchr(rt):
    m = rt.m
    s = _cbytes(m, m.reg(0))
    c = m.reg(1) & 0xFF
    if c == 0:
        return m.reg(0) + len(s)
    i = s.rfind(bytes([c]))
    return 0 if i < 0 else m.reg(0) + i


@func("_strstr")
def _strstr(rt):
    m = rt.m
    s, t = _cbytes(m, m.reg(0)), _cbytes(m, m.reg(1))
    i = s.find(t)
    return 0 if i < 0 else m.reg(0) + i


@func("_strcspn")
def _strcspn(rt):
    m = rt.m
    s, rej = _cbytes(m, m.reg(0)), set(_cbytes(m, m.reg(1)))
    for i, ch in enumerate(s):
        if ch in rej:
            return i
    return len(s)


@func("_strdup")
def _strdup(rt):
    m = rt.m
    b = _cbytes(m, m.reg(0)) + b"\x00"
    a = m.malloc(len(b))
    m.wrbytes(a, b)
    return a


@func("_atoi")
def _atoi(rt):
    s = _cs(rt, 0).strip()
    num = ""
    for i, ch in enumerate(s):
        if ch.isdigit() or (i == 0 and ch in "+-"):
            num += ch
        else:
            break
    try:
        return int(num) & 0xFFFFFFFF
    except ValueError:
        return 0


_rand_state = [12345]


@func("_rand|_arc4random")
def _rand(rt):
    _rand_state[0] = (_rand_state[0] * 1103515245 + 12345) & 0x7FFFFFFF
    return _rand_state[0]


@func("___error")
def _error(rt):
    if not hasattr(rt, "errno_addr"):
        rt.errno_addr = rt.m.malloc(16)
    return rt.errno_addr


@func("___assert_rtn")
def _assert_rtn(rt):
    m = rt.m
    raise GuestFault(f"assert failed: {m.cstr(m.reg(0))} {m.cstr(m.reg(1))}:{m.reg(2)} {m.cstr(m.reg(3))}")


@func("_exit")
def _exit(rt):
    rt.exit_code = rt.m.reg(0)
    raise GuestFault(f"exit({rt.m.reg(0)})")


@func("___cxa_atexit|___cxa_pure_virtual|___cxa_rethrow")
def _cxa(rt):
    return 0


@func("___maskrune")
def _maskrune(rt):
    c, mask = rt.m.reg(0), rt.m.reg(1)
    ch = chr(c) if c < 256 else "\0"
    # _CTYPE_ flags (BSD): A=0x100 C=0x200 D=0x400 G=0x800 L=0x1000 P=0x2000 S=0x4000 U=0x8000 X=0x10000 B=0x20000 R=0x40000 I=0x80000 T=0x100000
    flags = 0
    if ch.isalpha():
        flags |= 0x100
    if ch.isdigit():
        flags |= 0x400
    if ch.isprintable() and ch != " ":
        flags |= 0x800
    if ch.islower():
        flags |= 0x1000
    if ch.isprintable() and not ch.isalnum() and ch != " ":
        flags |= 0x2000
    if ch.isspace():
        flags |= 0x4000
    if ch.isupper():
        flags |= 0x8000
    if ch in "0123456789abcdefABCDEF":
        flags |= 0x10000
    if ch == " ":
        flags |= 0x20000
    if ch.isprintable():
        flags |= 0x40000
    return 1 if flags & mask else 0


# ---- printf family -----------------------------------------------------------------
class ArgReader:
    def __init__(self, m, ptr_or_first, from_va_list: bool, objc=None):
        self.m = m
        self.idx = 0
        self.ptr = ptr_or_first if from_va_list else None
        self.first = None if from_va_list else ptr_or_first
        self.objc = objc

    def word(self) -> int:
        if self.ptr is not None:
            v = self.m.rd32(self.ptr)
            self.ptr += 4
            return v
        v = self.m.arg(self.first + self.idx)
        self.idx += 1
        return v


def format_c(m, fmt: str, rd: ArgReader, objc=None) -> str:
    out = []
    i = 0
    while i < len(fmt):
        ch = fmt[i]
        if ch != "%":
            out.append(ch)
            i += 1
            continue
        j = i + 1
        flags = ""
        while j < len(fmt) and fmt[j] in "-+ #0":
            flags += fmt[j]
            j += 1
        width = ""
        if j < len(fmt) and fmt[j] == "*":
            width = str(_s32(rd.word()))
            j += 1
        while j < len(fmt) and fmt[j].isdigit():
            width += fmt[j]
            j += 1
        prec = None
        if j < len(fmt) and fmt[j] == ".":
            j += 1
            p = ""
            if j < len(fmt) and fmt[j] == "*":
                p = str(_s32(rd.word()))
                j += 1
            while j < len(fmt) and fmt[j].isdigit():
                p += fmt[j]
                j += 1
            prec = int(p or 0)
        length = ""
        while j < len(fmt) and fmt[j] in "hlqLzjt":
            length += fmt[j]
            j += 1
        if j >= len(fmt):
            break
        conv = fmt[j]
        spec = "%" + flags + width + ("." + str(prec) if prec is not None else "")
        if conv == "%":
            out.append("%")
        elif conv in "di":
            if length in ("ll", "q"):
                lo, hi = rd.word(), rd.word()
                v = _s64(_u64(lo, hi))
            else:
                v = _s32(rd.word())
                if length == "h":
                    v = struct.unpack("<h", struct.pack("<H", v & 0xFFFF))[0]
            out.append((spec + "d") % v)
        elif conv in "uxXo":
            if length in ("ll", "q"):
                lo, hi = rd.word(), rd.word()
                v = _u64(lo, hi)
            else:
                v = rd.word()
                if length == "h":
                    v &= 0xFFFF
            out.append((spec + conv.replace("u", "d")) % v)
        elif conv in "fFeEgG":
            lo, hi = rd.word(), rd.word()
            out.append((spec + conv) % _dbl(lo, hi))
        elif conv == "c":
            out.append((spec + "c") % chr(rd.word() & 0xFF))
        elif conv == "s":
            ptr = rd.word()
            s = m.cstr(ptr) if ptr else "(null)"
            out.append((spec + "s") % s)
        elif conv == "p":
            out.append("0x%x" % rd.word())
        elif conv == "@":
            obj = rd.word()
            s = objc.describe(obj) if objc else "<obj>"
            out.append((spec + "s") % s)
        else:
            out.append(fmt[i : j + 1])
        i = j + 1
    return "".join(out)


@func("_sprintf")
def _sprintf(rt):
    m = rt.m
    s = format_c(m, m.cstr(m.reg(1)), ArgReader(m, 2, False))
    m.wrbytes(m.reg(0), s.encode("utf-8") + b"\x00")
    return len(s)


@func("_vsprintf")
def _vsprintf(rt):
    m = rt.m
    s = format_c(m, m.cstr(m.reg(1)), ArgReader(m, m.reg(2), True))
    m.wrbytes(m.reg(0), s.encode("utf-8") + b"\x00")
    return len(s)


@func("_vsnprintf")
def _vsnprintf(rt):
    m = rt.m
    s = format_c(m, m.cstr(m.reg(2)), ArgReader(m, m.reg(3), True)).encode("utf-8")
    n = m.reg(1)
    if n:
        m.wrbytes(m.reg(0), s[: n - 1] + b"\x00")
    return len(s)


@func("_printf")
def _printf(rt):
    m = rt.m
    s = format_c(m, m.cstr(m.reg(0)), ArgReader(m, 1, False))
    m.log("printf: " + s.rstrip("\n"))
    return len(s)


@func("_puts")
def _puts(rt):
    rt.m.log("puts: " + rt.m.cstr(rt.m.reg(0)))
    return 1


@func("_perror")
def _perror(rt):
    rt.m.log("perror: " + rt.m.cstr(rt.m.reg(0)))
    return 0


# ---- libstdc++ (COW std::string, old ABI) and SJLJ unwinding ---------------------
STRING_REP = 12  # _Rep { size_t length; size_t capacity; int refcount; }


def new_std_string(m, data: bytes) -> int:
    rep = m.malloc(STRING_REP + len(data) + 1)
    m.wr32(rep, len(data))
    m.wr32(rep + 4, len(data))
    m.wr32(rep + 8, 0)
    m.wrbytes(rep + STRING_REP, data + b"\x00")
    return rep + STRING_REP


@func("__ZNSsC1EPKcRKSaIcE")
def _string_ctor(rt):
    m = rt.m
    m.wr32(m.reg(0), new_std_string(m, _cbytes(m, m.reg(1))))
    return m.reg(0)


@func("__ZNSsD2Ev")
def _string_dtor(rt):
    return 0


@func("__Unwind_SjLj_Register")
def _sjlj_register(rt):
    m = rt.m
    fc = m.reg(0)
    m.wr32(fc, getattr(rt, "sjlj_top", 0))
    rt.sjlj_top = fc
    return 0


@func("__Unwind_SjLj_Unregister")
def _sjlj_unregister(rt):
    m = rt.m
    rt.sjlj_top = m.rd32(m.reg(0))
    return 0


@func("__Unwind_SjLj_Resume|___gxx_personality_sj0|___objc_personality_v0")
def _sjlj_resume(rt):
    raise GuestFault("C++/ObjC exception unwinding is not implemented")


# ---- stdio ---------------------------------------------------------------------------
class HFile:
    def __init__(self, path, mode, data, host_path):
        self.path, self.mode, self.data, self.host_path = path, mode, bytearray(data), host_path
        self.pos = 0
        self.dirty = False


@func("_fopen")
def _fopen(rt):
    m = rt.m
    path, mode = m.cstr(m.reg(0)), m.cstr(m.reg(1))
    r = rt.resolve_path(path)
    m.log(f"fopen({path!r}, {mode!r})")
    if r is None:
        return 0
    kind, p = r
    if kind == "bundle":
        if "w" in mode or "a" in mode or not rt.bundle.exists(p):
            return 0
        data, host = rt.bundle.read(p), None
    else:
        host = p
        if "w" in mode:
            data = b""
        elif host.exists():
            data = host.read_bytes()
        elif "a" in mode:
            data = b""
        else:
            return 0
    files = rt.state.setdefault("files", {})
    handle = m.malloc(16)
    f = HFile(path, mode, data, host)
    if "a" in mode:
        f.pos = len(f.data)
    if "w" in mode:
        f.dirty = True
    files[handle] = f
    return handle


def _get_file(rt, h):
    return rt.state.setdefault("files", {}).get(h)


def _flush(f):
    if f.host_path is not None and f.dirty:
        f.host_path.parent.mkdir(parents=True, exist_ok=True)
        f.host_path.write_bytes(bytes(f.data))
        f.dirty = False


@func("_fclose")
def _fclose(rt):
    f = rt.state.setdefault("files", {}).pop(rt.m.reg(0), None)
    if f:
        _flush(f)
        return 0
    return 0xFFFFFFFF


@func("_fflush")
def _fflush(rt):
    f = _get_file(rt, rt.m.reg(0))
    if f:
        _flush(f)
    return 0


@func("_fread")
def _fread(rt):
    m = rt.m
    buf, size, count, h = m.reg(0), m.reg(1), m.reg(2), m.reg(3)
    f = _get_file(rt, h)
    if not f or size == 0:
        return 0
    want = size * count
    chunk = bytes(f.data[f.pos : f.pos + want])
    f.pos += len(chunk)
    m.wrbytes(buf, chunk)
    return len(chunk) // size


@func("_fwrite")
def _fwrite(rt):
    m = rt.m
    buf, size, count, h = m.reg(0), m.reg(1), m.reg(2), m.reg(3)
    f = _get_file(rt, h)
    if not f:
        return 0
    data = m.rdbytes(buf, size * count)
    f.data[f.pos : f.pos + len(data)] = data
    f.pos += len(data)
    f.dirty = True
    return count


@func("_fputs")
def _fputs(rt):
    m = rt.m
    f = _get_file(rt, m.reg(1))
    s = m.cstr(m.reg(0)).encode("utf-8")
    if f:
        f.data[f.pos : f.pos + len(s)] = s
        f.pos += len(s)
        f.dirty = True
    else:
        m.log("stderr: " + s.decode("utf-8", "replace").rstrip())
    return 1


@func("_fseek")
def _fseek(rt):
    m = rt.m
    f = _get_file(rt, m.reg(0))
    if not f:
        return 0xFFFFFFFF
    off, whence = _s32(m.reg(1)), m.reg(2)
    f.pos = max(0, off if whence == 0 else (f.pos + off if whence == 1 else len(f.data) + off))
    return 0


@func("_ftell")
def _ftell(rt):
    f = _get_file(rt, rt.m.reg(0))
    return f.pos if f else 0xFFFFFFFF


@func("_remove")
def _remove(rt):
    r = rt.resolve_path(rt.m.cstr(rt.m.reg(0)))
    if r and r[0] == "docs" and r[1].exists():
        r[1].unlink()
        return 0
    return 0xFFFFFFFF
