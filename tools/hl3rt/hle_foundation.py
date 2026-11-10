"""Foundation / CoreFoundation HLE."""

from __future__ import annotations

import struct
import time
from pathlib import Path

from .objc import CMETHODS, DATA_STRINGS, IMETHODS, cm, func, im
from .hle_libc import ArgReader, format_c

for _k, _v in {
    "_NSLocalizedDescriptionKey": "NSLocalizedDescription",
    "_NSUnderlyingErrorKey": "NSUnderlyingError",
    "_NSFileSystemFreeSize": "NSFileSystemFreeSize",
    "_NSLocaleCountryCode": "NSLocaleCountryCode",
}.items():
    DATA_STRINGS[_k] = _v


def _describe(rt, obj):
    if obj == 0:
        return "(null)"
    s = rt.objc.pystr(obj)
    if s is not None:
        return s
    cls, is_cls = rt.objc.class_of(obj)
    return f"<{cls.name if cls else '?'}: 0x{obj:08x}>"


import types as _types


def _install_describe():
    from .objc import ObjC

    ObjC.describe = lambda self, obj: _describe(self.m.rt, obj)


_install_describe()


# ------------------------------------------------------------------ NSObject
@cm("NSObject", "alloc|allocWithZone:")
def _alloc(rt, recv, a):
    cls = rt.objc.by_addr[recv]
    return rt.objc.alloc_instance(cls)


@cm("NSObject", "new")
def _new(rt, recv, a):
    cls = rt.objc.by_addr[recv]
    obj = rt.objc.alloc_instance(cls)
    return obj


@cm("NSObject", "class")
def _cm_class(rt, recv, a):
    return recv


@im("NSObject", "init")
def _init(rt, recv, a):
    return recv


@im("NSObject", "retain|autorelease|self|copy|mutableCopy")
def _retain(rt, recv, a):
    return recv


@im("NSObject", "release|dealloc")
def _release(rt, recv, a):
    return 0


@im("NSObject", "class")
def _class(rt, recv, a):
    cls, _ = rt.objc.class_of(recv)
    return cls.addr


@im("NSObject", "isKindOfClass:|isMemberOfClass:")
def _is_kind(rt, recv, a):
    cls, _ = rt.objc.class_of(recv)
    target = rt.objc.by_addr.get(a[0])
    return 1 if target is not None and cls is not None and target in list(cls.chain()) else 0


@im("NSObject", "respondsToSelector:")
def _responds(rt, recv, a):
    cls, is_class = rt.objc.class_of(recv)
    sel = rt.objc.sel_name(a[0])
    return 1 if cls is not None and rt.objc.lookup(cls, sel, is_class) is not None else 0


@im("NSObject", "isEqual:")
def _is_equal(rt, recv, a):
    if recv == a[0]:
        return 1
    s1, s2 = rt.objc.pystr(recv), rt.objc.pystr(a[0])
    return 1 if s1 is not None and s1 == s2 else 0


@im("NSObject", "hash")
def _hash(rt, recv, a):
    return recv


@im("NSObject", "description")
def _description(rt, recv, a):
    return rt.objc.nsstring(_describe(rt, recv))


@func("_NSLog")
def _nslog(rt):
    m = rt.m
    fmt = rt.objc.pystr(m.reg(0)) or ""
    m.log("NSLog: " + format_c(m, fmt, ArgReader(m, 1, False), rt.objc))
    return 0


@func("_NSSetUncaughtExceptionHandler|_objc_enumerationMutation|_objc_sync_enter|_objc_sync_exit")
def _noop(rt):
    return 0


@func("_objc_getProperty")
def _getprop(rt):
    m = rt.m
    return m.rd32(m.reg(0) + m.reg(2))


@func("_objc_setProperty")
def _setprop(rt):
    m = rt.m
    m.wr32(m.reg(0) + m.reg(2), m.reg(3))
    return 0


# --------------------------------------------------------------- autorelease pool
@im("NSAutoreleasePool", "init|drain|release")
def _pool(rt, recv, a):
    return recv


@cm("NSAutoreleasePool", "alloc")
def _pool_alloc(rt, recv, a):
    return rt.objc.new_obj("NSAutoreleasePool")


# ------------------------------------------------------------------- NSString
@cm("NSString", "string")
def _s_string(rt, recv, a):
    return rt.objc.nsstring("")


def _fmt(rt, fmt_obj, a, first):
    m = rt.m
    return format_c(m, rt.objc.pystr(fmt_obj) or "", ArgReader(m, a.first + first, False), rt.objc)


@cm("NSString", "stringWithFormat:")
def _s_with_format(rt, recv, a):
    return rt.objc.nsstring(_fmt(rt, a[0], a, 1))


@cm("NSString", "stringWithUTF8String:")
def _s_utf8(rt, recv, a):
    return rt.objc.nsstring(rt.m.cstr(a[0]))


@cm("NSString", "stringWithString:")
def _s_with_string(rt, recv, a):
    return rt.objc.nsstring(rt.objc.pystr(a[0]) or "")


@im("NSString", "UTF8String|cString")
def _s_to_utf8(rt, recv, a):
    s = rt.objc.pystr(recv) or ""
    cache = rt.state.setdefault("utf8cache", {})
    if recv not in cache or cache[recv][0] != s:
        cache[recv] = (s, rt.m.new_cstr(s))
    return cache[recv][1]


@im("NSString", "length")
def _s_length(rt, recv, a):
    return len(rt.objc.pystr(recv) or "")


@im("NSString", "isEqualToString:")
def _s_eq(rt, recv, a):
    return 1 if rt.objc.pystr(recv) == rt.objc.pystr(a[0]) else 0


@im("NSString", "stringByAppendingString:")
def _s_append(rt, recv, a):
    return rt.objc.nsstring((rt.objc.pystr(recv) or "") + (rt.objc.pystr(a[0]) or ""))


@im("NSString", "stringByAppendingPathComponent:")
def _s_append_path(rt, recv, a):
    base = (rt.objc.pystr(recv) or "").rstrip("/")
    return rt.objc.nsstring(base + "/" + (rt.objc.pystr(a[0]) or ""))


@im("NSString", "initWithString:|initWithUTF8String:|initWithCString:")
def _s_init(rt, recv, a):
    v = rt.objc.pystr(a[0]) if rt.objc.pystr(a[0]) is not None else rt.m.cstr(a[0])
    rt.objc.hobj[recv] = v
    return recv


@im("NSString", "initWithFormat:")
def _s_init_fmt(rt, recv, a):
    rt.objc.hobj[recv] = _fmt(rt, a[0], a, 1)
    return recv


@im("NSString", "intValue|integerValue")
def _s_int(rt, recv, a):
    try:
        return int((rt.objc.pystr(recv) or "0").strip().split()[0]) & 0xFFFFFFFF
    except (ValueError, IndexError):
        return 0


@im("NSString", "hasPrefix:")
def _s_hasprefix(rt, recv, a):
    return 1 if (rt.objc.pystr(recv) or "").startswith(rt.objc.pystr(a[0]) or "\0") else 0


@im("NSString", "hasSuffix:")
def _s_hassuffix(rt, recv, a):
    return 1 if (rt.objc.pystr(recv) or "").endswith(rt.objc.pystr(a[0]) or "\0") else 0


@im("NSString", "lowercaseString")
def _s_lower(rt, recv, a):
    return rt.objc.nsstring((rt.objc.pystr(recv) or "").lower())


@im("NSString", "uppercaseString")
def _s_upper(rt, recv, a):
    return rt.objc.nsstring((rt.objc.pystr(recv) or "").upper())


@im("NSString", "substringToIndex:")
def _s_sub_to(rt, recv, a):
    return rt.objc.nsstring((rt.objc.pystr(recv) or "")[: a[0]])


@im("NSString", "substringFromIndex:")
def _s_sub_from(rt, recv, a):
    return rt.objc.nsstring((rt.objc.pystr(recv) or "")[a[0] :])


@im("NSString", "substringWithRange:")
def _s_sub_range(rt, recv, a):
    return rt.objc.nsstring((rt.objc.pystr(recv) or "")[a[0] : a[0] + a[1]])


@im("NSString", "characterAtIndex:")
def _s_char(rt, recv, a):
    s = rt.objc.pystr(recv) or ""
    return ord(s[a[0]]) if a[0] < len(s) else 0


@im("NSString", "pathExtension")
def _s_pathext(rt, recv, a):
    s = rt.objc.pystr(recv) or ""
    return rt.objc.nsstring(s.rsplit(".", 1)[1] if "." in s.rsplit("/", 1)[-1] else "")


@im("NSString", "stringByDeletingPathExtension")
def _s_delext(rt, recv, a):
    s = rt.objc.pystr(recv) or ""
    last = s.rsplit("/", 1)[-1]
    return rt.objc.nsstring(s[: len(s) - len(last.rsplit(".", 1)[1]) - 1] if "." in last else s)


@im("NSString", "lastPathComponent")
def _s_lastcomp(rt, recv, a):
    return rt.objc.nsstring((rt.objc.pystr(recv) or "").rstrip("/").rsplit("/", 1)[-1])


@im("NSMutableString", "appendString:")
def _ms_append(rt, recv, a):
    rt.objc.hobj[recv] = (rt.objc.pystr(recv) or "") + (rt.objc.pystr(a[0]) or "")
    return 0


@im("NSMutableString", "appendFormat:")
def _ms_append_fmt(rt, recv, a):
    rt.objc.hobj[recv] = (rt.objc.pystr(recv) or "") + _fmt(rt, a[0], a, 1)
    return 0


@cm("NSMutableString", "string|stringWithCapacity:")
def _ms_string(rt, recv, a):
    return rt.objc.new_obj("NSMutableString", "")


@im("NSMutableString", "setString:")
def _ms_set(rt, recv, a):
    rt.objc.hobj[recv] = rt.objc.pystr(a[0]) or ""
    return 0


@im("NSString", "dataUsingEncoding:")
def _s_data(rt, recv, a):
    return rt.objc.new_obj("NSData", (rt.objc.pystr(recv) or "").encode("utf-8"))


# --------------------------------------------------------------------- CoreFoundation strings
@func("_CFStringCreateWithCString")
def _cfs_create(rt):
    return rt.objc.nsstring(rt.m.cstr(rt.m.reg(1)))


@func("_CFStringGetCString")
def _cfs_get(rt):
    m = rt.m
    s = (rt.objc.pystr(m.reg(0)) or "").encode("utf-8")
    cap = m.reg(2)
    if len(s) + 1 > cap:
        return 0
    m.wrbytes(m.reg(1), s + b"\x00")
    return 1


@func("_CFStringAppendCharacters")
def _cfs_append(rt):
    m = rt.m
    chars = m.rdbytes(m.reg(1), m.reg(2) * 2).decode("utf-16le", errors="replace")
    rt.objc.hobj[m.reg(0)] = (rt.objc.pystr(m.reg(0)) or "") + chars
    return 0


@func("_CFRelease")
def _cf_release(rt):
    return 0


# ---------------------------------------------------------------------- NSArray
def _list(rt, obj):
    h = rt.objc.hobj.get(obj)
    if not isinstance(h, list):
        h = []
        rt.objc.hobj[obj] = h
    return h


@cm("NSArray", "array")
def _a_array(rt, recv, a):
    return rt.objc.new_obj("NSMutableArray" if rt.objc.by_addr[recv].name == "NSMutableArray" else "NSArray", [])


@cm("NSMutableArray", "array|arrayWithCapacity:")
def _ma_array(rt, recv, a):
    return rt.objc.new_obj("NSMutableArray", [])


@im("NSArray", "init|initWithCapacity:")
def _a_init(rt, recv, a):
    rt.objc.hobj[recv] = []
    return recv


@im("NSArray", "count")
def _a_count(rt, recv, a):
    return len(_list(rt, recv))


@im("NSArray", "objectAtIndex:")
def _a_at(rt, recv, a):
    lst = _list(rt, recv)
    return lst[a[0]] if a[0] < len(lst) else 0


@im("NSArray", "lastObject")
def _a_last(rt, recv, a):
    lst = _list(rt, recv)
    return lst[-1] if lst else 0


@im("NSMutableArray", "addObject:")
def _a_add(rt, recv, a):
    _list(rt, recv).append(a[0])
    return 0


@im("NSMutableArray", "insertObject:atIndex:")
def _a_insert(rt, recv, a):
    _list(rt, recv).insert(a[1], a[0])
    return 0


@im("NSMutableArray", "removeObjectAtIndex:")
def _a_remove_at(rt, recv, a):
    lst = _list(rt, recv)
    if a[0] < len(lst):
        del lst[a[0]]
    return 0


@im("NSMutableArray", "removeObject:")
def _a_remove(rt, recv, a):
    lst = _list(rt, recv)
    rt.objc.hobj[recv] = [x for x in lst if x != a[0]]
    return 0


@im("NSMutableArray", "removeAllObjects")
def _a_clear(rt, recv, a):
    rt.objc.hobj[recv] = []
    return 0


@im("NSArray", "containsObject:")
def _a_contains(rt, recv, a):
    return 1 if a[0] in _list(rt, recv) else 0


@im("NSArray", "indexOfObject:")
def _a_index(rt, recv, a):
    lst = _list(rt, recv)
    return lst.index(a[0]) if a[0] in lst else 0x7FFFFFFF


# ------------------------------------------------------------------ NSDictionary
def _dict(rt, obj):
    h = rt.objc.hobj.get(obj)
    if not isinstance(h, dict):
        h = {}
        rt.objc.hobj[obj] = h
    return h


def _key(rt, k):
    s = rt.objc.pystr(k)
    return ("s", s) if s is not None else ("p", k)


@cm("NSDictionary", "dictionary")
def _d_dictionary(rt, recv, a):
    return rt.objc.new_obj(rt.objc.by_addr[recv].name, {})


@im("NSDictionary", "init|initWithCapacity:")
def _d_init(rt, recv, a):
    rt.objc.hobj[recv] = {}
    return recv


@cm("NSMutableDictionary", "dictionary|dictionaryWithCapacity:")
def _md_dictionary(rt, recv, a):
    return rt.objc.new_obj("NSMutableDictionary", {})


@im("NSDictionary", "count")
def _d_count(rt, recv, a):
    return len(_dict(rt, recv))


@im("NSDictionary", "objectForKey:")
def _d_get(rt, recv, a):
    v = _dict(rt, recv).get(_key(rt, a[0]))
    return v[1] if v else 0


@im("NSMutableDictionary", "setObject:forKey:")
def _d_set(rt, recv, a):
    _dict(rt, recv)[_key(rt, a[1])] = (a[1], a[0])
    return 0


@im("NSMutableDictionary", "removeObjectForKey:")
def _d_remove(rt, recv, a):
    _dict(rt, recv).pop(_key(rt, a[0]), None)
    return 0


@im("NSMutableDictionary", "removeAllObjects")
def _d_clear(rt, recv, a):
    rt.objc.hobj[recv] = {}
    return 0


# ------------------------------------------------------------------ NSNumber
@cm("NSNumber", "numberWithInt:|numberWithInteger:|numberWithUnsignedInt:|numberWithBool:|numberWithLong:")
def _n_int(rt, recv, a):
    return rt.objc.new_obj("NSNumber", int(a[0]))


@cm("NSNumber", "numberWithFloat:")
def _n_float(rt, recv, a):
    return rt.objc.new_obj("NSNumber", a.f32(0))


@cm("NSNumber", "numberWithDouble:")
def _n_double(rt, recv, a):
    return rt.objc.new_obj("NSNumber", a.f64(0))


@im("NSNumber", "intValue|integerValue|unsignedIntValue|boolValue|longValue")
def _n_intvalue(rt, recv, a):
    v = rt.objc.hobj.get(recv, 0)
    return int(v) & 0xFFFFFFFF


@im("NSNumber", "floatValue")
def _n_floatvalue(rt, recv, a):
    return struct.unpack("<I", struct.pack("<f", float(rt.objc.hobj.get(recv, 0))))[0]


@im("NSNumber", "doubleValue")
def _n_doublevalue(rt, recv, a):
    return struct.unpack("<II", struct.pack("<d", float(rt.objc.hobj.get(recv, 0))))


# -------------------------------------------------------------------- NSData
@cm("NSData", "dataWithBytes:length:")
def _data_bytes(rt, recv, a):
    return rt.objc.new_obj("NSData", rt.m.rdbytes(a[0], a[1]))


@cm("NSData", "dataWithContentsOfFile:")
def _data_file(rt, recv, a):
    data = rt.read_file(rt.objc.pystr(a[0]) or "")
    return rt.objc.new_obj("NSData", data) if data is not None else 0


@im("NSData", "length")
def _data_len(rt, recv, a):
    return len(rt.objc.hobj.get(recv, b""))


@im("NSData", "bytes")
def _data_bytes_ptr(rt, recv, a):
    cache = rt.state.setdefault("databytes", {})
    data = rt.objc.hobj.get(recv, b"")
    if recv not in cache or cache[recv][0] != len(data):
        p = rt.m.malloc(len(data) + 1)
        rt.m.wrbytes(p, data)
        cache[recv] = (len(data), p)
    return cache[recv][1]


# ------------------------------------------------------------------ NSBundle
@cm("NSBundle", "mainBundle")
def _bundle_main(rt, recv, a):
    if "mainBundle" not in rt.state:
        rt.state["mainBundle"] = rt.objc.new_obj("NSBundle")
    return rt.state["mainBundle"]


@im("NSBundle", "resourcePath|bundlePath")
def _bundle_path(rt, recv, a):
    from .runtime import APP_PATH

    return rt.objc.nsstring(APP_PATH)


@im("NSBundle", "pathForResource:ofType:")
def _bundle_pfr(rt, recv, a):
    from .runtime import APP_PATH

    name, ext = rt.objc.pystr(a[0]) or "", rt.objc.pystr(a[1])
    rel = name + (("." + ext) if ext else "")
    for cand in (rel, "en.lproj/" + rel):
        if rt.bundle.exists(cand):
            return rt.objc.nsstring(APP_PATH + "/" + cand)
    return 0


@im("NSBundle", "pathForResource:ofType:inDirectory:")
def _bundle_pfrd(rt, recv, a):
    from .runtime import APP_PATH

    name, ext, d = rt.objc.pystr(a[0]) or "", rt.objc.pystr(a[1]), rt.objc.pystr(a[2])
    rel = (d + "/" if d else "") + name + (("." + ext) if ext else "")
    if rt.bundle.exists(rel):
        return rt.objc.nsstring(APP_PATH + "/" + rel)
    return 0


def _plist_to_objc(rt, v):
    o = rt.objc
    if isinstance(v, bool):
        return o.new_obj("NSNumber", int(v))
    if isinstance(v, int):
        return o.new_obj("NSNumber", v)
    if isinstance(v, float):
        return o.new_obj("NSNumber", v)
    if isinstance(v, str):
        return o.nsstring(v)
    if isinstance(v, bytes):
        return o.new_obj("NSData", v)
    if isinstance(v, list):
        return o.new_obj("NSArray", [_plist_to_objc(rt, x) for x in v])
    if isinstance(v, dict):
        d = {}
        for k, x in v.items():
            ko = o.nsstring(k)
            d[("s", k)] = (ko, _plist_to_objc(rt, x))
        return o.new_obj("NSDictionary", d)
    return 0


@im("NSBundle", "infoDictionary|localizedInfoDictionary")
def _bundle_info(rt, recv, a):
    import plistlib

    if "info_dict" not in rt.state:
        info = plistlib.loads(rt.bundle.read("Info.plist"))
        rt.state["info_dict"] = _plist_to_objc(rt, info)
    return rt.state["info_dict"]


@im("NSBundle", "objectForInfoDictionaryKey:")
def _bundle_info_key(rt, recv, a):
    d = _bundle_info(rt, recv, a)
    v = _dict(rt, d).get(_key(rt, a[0]))
    return v[1] if v else 0


@func("_CFBundleGetMainBundle")
def _cfb_main(rt):
    if "cfbundle" not in rt.state:
        rt.state["cfbundle"] = rt.objc.new_obj("NSBundle")
    return rt.state["cfbundle"]


@func("_CFBundleCopyResourceURL")
def _cfb_copy_url(rt):
    from .runtime import APP_PATH

    m = rt.m
    name, ext = rt.objc.pystr(m.reg(1)) or "", rt.objc.pystr(m.reg(2))
    d = (rt.objc.pystr(m.reg(3)) or "").strip("/")
    name = name.lstrip("/")
    rel = (d + "/" if d else "") + name + (("." + ext) if ext else "")
    if rt.bundle.exists(rel):
        return rt.objc.new_obj("NSURL", APP_PATH + "/" + rel)
    # also try without extension if name already has extension
    for cand in (rel, name, "en.lproj/" + rel):
        if rt.bundle.exists(cand):
            return rt.objc.new_obj("NSURL", APP_PATH + "/" + cand)
    return 0


@func("_CFURLCopyFileSystemPath")
def _cfurl_path(rt):
    return rt.objc.nsstring(rt.objc.hobj.get(rt.m.reg(0), ""))


@cm("NSURL", "fileURLWithPath:")
def _url_file(rt, recv, a):
    return rt.objc.new_obj("NSURL", rt.objc.pystr(a[0]) or "")


@im("NSURL", "path")
def _url_path(rt, recv, a):
    return rt.objc.nsstring(rt.objc.hobj.get(recv, ""))


@func("_NSHomeDirectory")
def _home(rt):
    from .runtime import DOCS_PATH

    return rt.objc.nsstring(DOCS_PATH.rsplit("/", 1)[0])


@func("_NSTemporaryDirectory")
def _tmp(rt):
    from .runtime import DOCS_PATH

    return rt.objc.nsstring(DOCS_PATH.rsplit("/", 1)[0] + "/tmp")


@func("_NSSearchPathForDirectoriesInDomains")
def _search_path(rt):
    from .runtime import DOCS_PATH

    return rt.objc.new_obj("NSArray", [rt.objc.nsstring(DOCS_PATH)])


# ------------------------------------------------------------- NSUserDefaults
@cm("NSUserDefaults", "standardUserDefaults")
def _ud_std(rt, recv, a):
    if "ud" not in rt.state:
        rt.state["ud"] = rt.objc.new_obj("NSUserDefaults", {})
    return rt.state["ud"]


@im("NSUserDefaults", "objectForKey:|stringForKey:|dataForKey:")
def _ud_get(rt, recv, a):
    return _d_get(rt, recv, a)


@im("NSUserDefaults", "integerForKey:|boolForKey:")
def _ud_int(rt, recv, a):
    v = _d_get(rt, recv, a)
    return int(rt.objc.hobj.get(v, 0)) & 0xFFFFFFFF if v else 0


@im("NSUserDefaults", "setObject:forKey:")
def _ud_set(rt, recv, a):
    return _d_set(rt, recv, a)


@im("NSUserDefaults", "setInteger:forKey:|setBool:forKey:")
def _ud_set_int(rt, recv, a):
    _dict(rt, recv)[_key(rt, a[1])] = (a[1], rt.objc.new_obj("NSNumber", int(a[0])))
    return 0


@im("NSUserDefaults", "synchronize")
def _ud_sync(rt, recv, a):
    return 1


# ------------------------------------------------------------- NSFileManager
@cm("NSFileManager", "defaultManager")
def _fm_default(rt, recv, a):
    if "fm" not in rt.state:
        rt.state["fm"] = rt.objc.new_obj("NSFileManager")
    return rt.state["fm"]


def _exists(rt, path):
    r = rt.resolve_path(path)
    if r is None:
        return False
    kind, p = r
    if kind == "bundle":
        return rt.bundle.exists(p) or any(n.startswith("Payload/Heroes Lore.app/" + p.rstrip("/") + "/") for n in rt.bundle.names)
    return Path(p).exists()


@im("NSFileManager", "fileExistsAtPath:")
def _fm_exists(rt, recv, a):
    return 1 if _exists(rt, rt.objc.pystr(a[0]) or "") else 0


@im("NSFileManager", "createDirectoryAtPath:attributes:|createDirectoryAtPath:withIntermediateDirectories:attributes:error:")
def _fm_mkdir(rt, recv, a):
    r = rt.resolve_path(rt.objc.pystr(a[0]) or "")
    if r and r[0] == "docs":
        Path(r[1]).mkdir(parents=True, exist_ok=True)
        return 1
    return 0


@im("NSFileManager", "removeItemAtPath:error:|removeItemAtPath:")
def _fm_remove(rt, recv, a):
    r = rt.resolve_path(rt.objc.pystr(a[0]) or "")
    if r and r[0] == "docs" and Path(r[1]).exists():
        Path(r[1]).unlink()
        return 1
    return 0


@im("NSFileManager", "attributesOfFileSystemForPath:error:")
def _fm_attrs(rt, recv, a):
    d = {_key(rt, rt.objc.nsstring("NSFileSystemFreeSize")): (0, rt.objc.new_obj("NSNumber", 1 << 30))}
    return rt.objc.new_obj("NSDictionary", d)


# --------------------------------------------------------------------- misc
@cm("NSDate", "date")
def _date(rt, recv, a):
    return rt.objc.new_obj("NSDate", time.time())


@im("NSDate", "timeIntervalSince1970|timeIntervalSinceReferenceDate")
def _date_ti(rt, recv, a):
    return struct.unpack("<II", struct.pack("<d", rt.objc.hobj.get(recv, time.time())))


@cm("NSLocale", "currentLocale|autoupdatingCurrentLocale")
def _locale(rt, recv, a):
    return rt.objc.new_obj("NSLocale")


@im("NSLocale", "objectForKey:")
def _locale_get(rt, recv, a):
    return rt.objc.nsstring("US")


@cm("NSLocale", "preferredLanguages")
def _locale_langs(rt, recv, a):
    return rt.objc.new_obj("NSArray", [rt.objc.nsstring("en")])


@cm("NSNull", "null")
def _null(rt, recv, a):
    return rt.objc.new_obj("NSNull")


@cm("NSThread", "isMainThread")
def _is_main(rt, recv, a):
    return 1


@cm("NSThread", "sleepForTimeInterval:")
def _sleep(rt, recv, a):
    return 0
