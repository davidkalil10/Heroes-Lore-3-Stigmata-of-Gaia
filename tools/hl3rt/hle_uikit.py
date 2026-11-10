"""UIKit / EAGL / QuartzCore HLE (functional core: application, window, view, screen, timers)."""

from __future__ import annotations

import struct

from .objc import DATA_STRINGS, cm, func, im
from .hle_foundation import _key
from .machine import GuestFault

DATA_STRINGS["_kEAGLColorFormatRGBA8"] = "EAGLColorFormatRGBA8"
DATA_STRINGS["_kEAGLDrawablePropertyColorFormat"] = "EAGLDrawablePropertyColorFormat"
DATA_STRINGS["_kEAGLDrawablePropertyRetainedBacking"] = "EAGLDrawablePropertyRetainedBacking"

UI_ORIENTATION_LANDSCAPE_RIGHT = 3
LOGICAL_W, LOGICAL_H = 480, 320


@func("_UIApplicationMain")
def _ui_app_main(rt):
    m = rt.m
    rt.pending_app_main = {
        "argc": m.reg(0),
        "argv": m.reg(1),
        "principal": rt.objc.pystr(m.reg(2)),
        "delegate": rt.objc.pystr(m.reg(3)),
    }
    m.stop_reason = None
    return 0


def _rect(x, y, w, h):
    return struct.pack("<ffff", x, y, w, h)


# ---- UIScreen ----------------------------------------------------------------------
@cm("UIScreen", "mainScreen")
def _screen_main(rt, recv, a):
    if "screen" not in rt.state:
        rt.state["screen"] = rt.objc.new_obj("UIScreen")
    return rt.state["screen"]


@im("UIScreen", "bounds|applicationFrame")
def _screen_bounds(rt, recv, a):
    return _rect(0, 0, LOGICAL_H, LOGICAL_W)  # portrait bounds; the app rotates to landscape


@im("UIScreen", "scale")
def _screen_scale(rt, recv, a):
    return struct.unpack("<I", struct.pack("<f", 1.0))[0]


# ---- UIApplication -----------------------------------------------------------------
@cm("UIApplication", "sharedApplication")
def _app_shared(rt, recv, a):
    return rt.state["app"]


@im("UIApplication", "statusBarOrientation")
def _app_orientation(rt, recv, a):
    return rt.state.get("orientation", UI_ORIENTATION_LANDSCAPE_RIGHT)


@im("UIApplication", "setStatusBarOrientation:|setStatusBarOrientation:animated:")
def _app_set_orientation(rt, recv, a):
    rt.state["orientation"] = a[0]
    return 0


@im("UIApplication", "delegate")
def _app_delegate(rt, recv, a):
    return rt.state.get("delegate", 0)


@im("UIApplication", "keyWindow")
def _app_keywindow(rt, recv, a):
    return rt.state.get("window", 0)


@im("UIApplication", "setIdleTimerDisabled:|setStatusBarHidden:|setStatusBarHidden:animated:|setApplicationIconBadgeNumber:|registerForRemoteNotificationTypes:|setNetworkActivityIndicatorVisible:|setStatusBarStyle:")
def _app_noop(rt, recv, a):
    return 0


@im("UIApplication", "applicationState")
def _app_state(rt, recv, a):
    return 0


# ---- UIDevice ----------------------------------------------------------------------
@cm("UIDevice", "currentDevice")
def _device(rt, recv, a):
    if "device" not in rt.state:
        rt.state["device"] = rt.objc.new_obj("UIDevice")
    return rt.state["device"]


@im("UIDevice", "systemVersion")
def _dev_ver(rt, recv, a):
    return rt.objc.nsstring("4.1")


@im("UIDevice", "model|localizedModel")
def _dev_model(rt, recv, a):
    return rt.objc.nsstring("iPhone")


@im("UIDevice", "name")
def _dev_name(rt, recv, a):
    return rt.objc.nsstring("iPhone")


@im("UIDevice", "uniqueIdentifier")
def _dev_uid(rt, recv, a):
    return rt.objc.nsstring("0000000000000000000000000000000000000003")


# ---- UIView / UIWindow ---------------------------------------------------------------
def _view(rt, obj):
    h = rt.objc.hobj.get(obj)
    if not isinstance(h, dict):
        h = {"frame": (0.0, 0.0, float(LOGICAL_W), float(LOGICAL_H)), "subviews": []}
        rt.objc.hobj[obj] = h
    return h


def _read_rect_args(a, i=0):
    return tuple(a.f32(i + k) for k in range(4))


@im("UIView", "initWithFrame:")
def _view_init(rt, recv, a):
    _view(rt, recv)["frame"] = _read_rect_args(a)
    return recv


@im("UIView", "frame|bounds")
def _view_frame(rt, recv, a):
    x, y, w, h = _view(rt, recv)["frame"]
    return _rect(0, 0, w, h)


@im("UIView", "setFrame:|setBounds:")
def _view_set_frame(rt, recv, a):
    _view(rt, recv)["frame"] = _read_rect_args(a)
    return 0


@im("UIView", "addSubview:")
def _view_add(rt, recv, a):
    _view(rt, recv)["subviews"].append(a[0])
    return 0


@im("UIView", "layer")
def _view_layer(rt, recv, a):
    v = _view(rt, recv)
    if "layer" not in v:
        v["layer"] = rt.objc.new_obj("CAEAGLLayer", {})
    return v["layer"]


@im("UIWindow", "makeKeyAndVisible|makeKeyWindow")
def _win_key(rt, recv, a):
    rt.state["window"] = recv
    return 0


@cm("CAEAGLLayer", "class")
def _layer_class(rt, recv, a):
    return recv


# ---- EAGL ----------------------------------------------------------------------------
@im("EAGLContext", "initWithAPI:|initWithAPI:sharegroup:")
def _eagl_init(rt, recv, a):
    rt.objc.hobj[recv] = {"api": a[0]}
    return recv


@cm("EAGLContext", "setCurrentContext:")
def _eagl_set_current(rt, recv, a):
    rt.state["gl_context"] = a[0]
    return 1


@cm("EAGLContext", "currentContext")
def _eagl_current(rt, recv, a):
    return rt.state.get("gl_context", 0)


@im("EAGLContext", "renderbufferStorage:fromDrawable:")
def _eagl_rb_storage(rt, recv, a):
    return 1


@im("EAGLContext", "presentRenderbuffer:")
def _eagl_present(rt, recv, a):
    rt.state["frames_presented"] = rt.state.get("frames_presented", 0) + 1
    if getattr(rt, "present_hook", None):
        rt.present_hook()
    return 1


@im("CAEAGLLayer", "setDrawableProperties:|setOpaque:")
def _layer_props(rt, recv, a):
    return 0


# ---- mach host / sysctl -------------------------------------------------------------
@func("_mach_host_self")
def _mach_host_self(rt):
    return 0x103


@func("_host_page_size")
def _host_page_size(rt):
    rt.m.wr32(rt.m.reg(1), 4096)
    return 0


@func("_host_statistics")
def _host_statistics(rt):
    m = rt.m
    info = m.reg(2)
    # vm_statistics: free, active, inactive, wire (page counts)
    for i, v in enumerate((20000, 10000, 5000, 5000)):
        m.wr32(info + 4 * i, v)
    return 0


@func("_sysctlbyname")
def _sysctlbyname(rt):
    m = rt.m
    name = m.cstr(m.reg(0))
    oldp, oldlenp = m.reg(1), m.reg(2)
    values = {"hw.machine": b"iPhone1,2\x00", "hw.model": b"N82AP\x00"}
    data = values.get(name)
    m.log(f"sysctlbyname({name})")
    if data is None:
        return 0xFFFFFFFF
    if oldp:
        m.wrbytes(oldp, data)
    if oldlenp:
        m.wr32(oldlenp, len(data))
    return 0


# ---- NSValue / NSNumber extras -----------------------------------------------------
@cm("NSValue", "valueWithPointer:")
def _value_ptr(rt, recv, a):
    return rt.objc.new_obj("NSValue", a[0])


@im("NSValue", "pointerValue")
def _value_pointer(rt, recv, a):
    return rt.objc.hobj.get(recv, 0)


@im("NSNumber", "longLongValue|unsignedLongLongValue")
def _n_longlong(rt, recv, a):
    v = int(rt.objc.hobj.get(recv, 0)) & 0xFFFFFFFFFFFFFFFF
    return (v & 0xFFFFFFFF, v >> 32)


@im("NSObject", "performSelector:|performSelector:withObject:")
def _perform(rt, recv, a):
    return 0


@func("_CGAffineTransformMakeRotation")
def _cg_rotation(rt):
    import math

    m = rt.m
    buf, angle = m.reg(0), struct.unpack("<f", struct.pack("<I", m.reg(1)))[0]
    c, s = math.cos(angle), math.sin(angle)
    m.wrbytes(buf, struct.pack("<ffffff", c, s, -s, c, 0.0, 0.0))
    return buf


@cm("NSDictionary", "dictionaryWithObjectsAndKeys:")
def _d_with_objects_keys(rt, recv, a):
    d = {}
    i = 0
    while True:
        obj = a[i]
        if obj == 0:
            break
        key = a[i + 1]
        if key == 0:
            break
        d[_key(rt, key)] = (key, obj)
        i += 2
    return rt.objc.new_obj("NSMutableDictionary" if rt.objc.by_addr[recv].name == "NSMutableDictionary" else "NSDictionary", d)



# ---- Touch System (UITouch, UIEvent, NSFastEnumeration for NSSet) -------------------
class Touch:
    def __init__(self, x: float, y: float, phase: int = 0):
        self.x, self.y = float(x), float(y)
        self.prev_x, self.prev_y = float(x), float(y)
        self.phase = phase
        self.timestamp = 0.0


@im("NSSet", "countByEnumeratingWithState:objects:count:")
def _set_fast_enum(rt, recv, a):
    m = rt.m
    state_ptr, stackbuf, count = a[0], a[1], a[2]
    items = rt.objc.hobj.get(recv, [])
    # NSFastEnumerationState: unsigned long state; id *itemsPtr; unsigned long *mutationsPtr; unsigned long extra[5];
    state_val = m.rd32(state_ptr)
    if state_val == 0 and len(items) > 0:
        n = min(len(items), count)
        for i in range(n):
            m.wr32(stackbuf + 4 * i, items[i])
        m.wr32(state_ptr, 1)  # advance state
        m.wr32(state_ptr + 4, stackbuf)  # itemsPtr points to stackbuf
        if not hasattr(rt, "_mutations_dummy"):
            rt._mutations_dummy = m.malloc(8)
            m.wr32(rt._mutations_dummy, 1)
        m.wr32(state_ptr + 8, rt._mutations_dummy)
        return n
    return 0


@im("UIEvent", "allTouches")
def _evt_all_touches(rt, recv, a):
    touches = rt.objc.hobj.get(recv, {}).get("touches", [])
    return rt.objc.new_obj("NSSet", touches)


@im("UITouch", "phase")
def _touch_phase(rt, recv, a):
    t = rt.objc.hobj.get(recv)
    return t.phase if isinstance(t, Touch) else 0


@im("UITouch", "locationInView:")
def _touch_loc(rt, recv, a):
    t = rt.objc.hobj.get(recv)
    if isinstance(t, Touch):
        return struct.pack("<ff", t.x, t.y)
    return struct.pack("<ff", 0.0, 0.0)


@im("UITouch", "previousLocationInView:")
def _touch_prev_loc(rt, recv, a):
    t = rt.objc.hobj.get(recv)
    if isinstance(t, Touch):
        return struct.pack("<ff", t.prev_x, t.prev_y)
    return struct.pack("<ff", 0.0, 0.0)


@im("UITouch", "timestamp")
def _touch_timestamp(rt, recv, a):
    import time
    t = rt.objc.hobj.get(recv)
    ts = t.timestamp if isinstance(t, Touch) else time.time()
    return struct.unpack("<II", struct.pack("<d", ts))


@im("UIView", "convertPoint:fromView:|convertPoint:toView:")
def _view_convert_point(rt, recv, a):
    # Returns point directly in 480x320 landscape coordinate space
    x, y = a.f32(0), a.f32(1)
    return struct.pack("<ff", x, y)
