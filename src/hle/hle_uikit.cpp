#include "../core/runtime.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace hl3 {

namespace {

constexpr u32 kUiOrientationLandscapeRight = 3u;
constexpr float kLogicalW = 480.0f;
constexpr float kLogicalH = 320.0f;

struct UIViewState {
    float x = 0.0f;
    float y = 0.0f;
    float w = kLogicalW;
    float h = kLogicalH;
    u32 layer = 0;
    std::vector<u32> subviews;
};

UIViewState& ensure_view(Runtime& rt, u32 obj) {
    auto* v = rt.objc().get_payload<UIViewState>(obj);
    if (!v) {
        rt.objc().set_payload(obj, UIViewState{});
        v = rt.objc().get_payload<UIViewState>(obj);
    }
    return *v;
}

Bytes pack_rect(float x, float y, float w, float h) {
    Bytes b(16);
    write_u32_le(b.data() + 0, f32_to_bits(x));
    write_u32_le(b.data() + 4, f32_to_bits(y));
    write_u32_le(b.data() + 8, f32_to_bits(w));
    write_u32_le(b.data() + 12, f32_to_bits(h));
    return b;
}

Bytes pack_point(float x, float y) {
    Bytes b(8);
    write_u32_le(b.data() + 0, f32_to_bits(x));
    write_u32_le(b.data() + 4, f32_to_bits(y));
    return b;
}

double now_monotonic() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

void register_hle_uikit(Runtime& rt, HleRegistry& reg) {
    reg.data_strings["_kEAGLColorFormatRGBA8"] = "EAGLColorFormatRGBA8";
    reg.data_strings["_kEAGLDrawablePropertyColorFormat"] = "EAGLDrawablePropertyColorFormat";
    reg.data_strings["_kEAGLDrawablePropertyRetainedBacking"] = "EAGLDrawablePropertyRetainedBacking";

    reg.add_func("_UIApplicationMain", [&rt](Machine& m) {
        rt.set_pending_app_main({
            rt.objc().get_str(m.reg(2)),
            rt.objc().get_str(m.reg(3)),
        });
        m.stop_reason.clear();
        m.request_stop();
        return TrapResult::val(0u);
    });

    // ---- UIScreen -------------------------------------------------------------------
    reg.add_cm("UIScreen", "mainScreen", [](Runtime& rt, u32, const ObjCArgs&) {
        if (rt.screen_obj == 0) rt.screen_obj = rt.objc().new_obj("UIScreen");
        return ObjCRet::val(rt.screen_obj);
    });
    reg.add_im("UIScreen", "bounds|applicationFrame", [](Runtime&, u32, const ObjCArgs&) {
        return ObjCRet::stret_bytes(pack_rect(0.0f, 0.0f, kLogicalH, kLogicalW));
    });
    reg.add_im("UIScreen", "scale", [](Runtime&, u32, const ObjCArgs&) {
        return ObjCRet::val(f32_to_bits(1.0f));
    });

    // ---- UIApplication --------------------------------------------------------------
    reg.add_cm("UIApplication", "sharedApplication", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.app_obj());
    });
    reg.add_im("UIApplication", "statusBarOrientation", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.orientation);
    });
    reg.add_im("UIApplication", "setStatusBarOrientation:|setStatusBarOrientation:animated:",
               [](Runtime& rt, u32, const ObjCArgs& a) {
                   rt.orientation = a[0];
                   return ObjCRet::val(0);
               });
    reg.add_im("UIApplication", "delegate", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.delegate_obj());
    });
    reg.add_im("UIApplication", "keyWindow", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.main_window);
    });
    reg.add_im("UIApplication",
               "setIdleTimerDisabled:|setStatusBarHidden:|setStatusBarHidden:animated:|"
               "setApplicationIconBadgeNumber:|registerForRemoteNotificationTypes:|"
               "setNetworkActivityIndicatorVisible:|setStatusBarStyle:|applicationState",
               [](Runtime&, u32, const ObjCArgs&) { return ObjCRet::val(0); });

    // ---- UIDevice -------------------------------------------------------------------
    reg.add_cm("UIDevice", "currentDevice", [](Runtime& rt, u32, const ObjCArgs&) {
        if (rt.device_obj == 0) rt.device_obj = rt.objc().new_obj("UIDevice");
        return ObjCRet::val(rt.device_obj);
    });
    reg.add_im("UIDevice", "systemVersion", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().nsstring("4.1"));
    });
    reg.add_im("UIDevice", "model|localizedModel|name", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().nsstring("iPhone"));
    });
    reg.add_im("UIDevice", "uniqueIdentifier", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().nsstring("0000000000000000000000000000000000000003"));
    });

    // ---- UIView / UIWindow / CAEAGLLayer -------------------------------------------
    reg.add_im("UIView", "initWithFrame:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& v = ensure_view(rt, recv);
        v.x = a.f32(0);
        v.y = a.f32(1);
        v.w = a.f32(2);
        v.h = a.f32(3);
        return ObjCRet::val(recv);
    });
    reg.add_im("UIView", "frame|bounds", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto& v = ensure_view(rt, recv);
        return ObjCRet::stret_bytes(pack_rect(0.0f, 0.0f, v.w, v.h));
    });
    reg.add_im("UIView", "setFrame:|setBounds:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& v = ensure_view(rt, recv);
        v.x = a.f32(0);
        v.y = a.f32(1);
        v.w = a.f32(2);
        v.h = a.f32(3);
        return ObjCRet::val(0);
    });
    reg.add_im("UIView", "addSubview:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        ensure_view(rt, recv).subviews.push_back(a[0]);
        return ObjCRet::val(0);
    });
    reg.add_im("UIView", "layer", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto& v = ensure_view(rt, recv);
        if (v.layer == 0) v.layer = rt.objc().new_obj("CAEAGLLayer");
        return ObjCRet::val(v.layer);
    });
    reg.add_im("UIView", "convertPoint:fromView:|convertPoint:toView:", [](Runtime&, u32, const ObjCArgs& a) {
        return ObjCRet::stret_bytes(pack_point(a.f32(0), a.f32(1)));
    });
    reg.add_im("UIWindow", "makeKeyAndVisible|makeKeyWindow", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        rt.main_window = recv;
        return ObjCRet::val(0);
    });
    reg.add_cm("CAEAGLLayer", "class", [](Runtime&, u32 recv, const ObjCArgs&) { return ObjCRet::val(recv); });
    reg.add_im("CAEAGLLayer", "setDrawableProperties:|setOpaque:",
               [](Runtime&, u32, const ObjCArgs&) { return ObjCRet::val(0); });

    // ---- EAGLContext ----------------------------------------------------------------
    reg.add_im("EAGLContext", "initWithAPI:|initWithAPI:sharegroup:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        rt.objc().set_payload(recv, u32(a[0]));
        return ObjCRet::val(recv);
    });
    reg.add_cm("EAGLContext", "setCurrentContext:", [](Runtime& rt, u32, const ObjCArgs& a) {
        rt.gl_context = a[0];
        return ObjCRet::val(1);
    });
    reg.add_cm("EAGLContext", "currentContext", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.gl_context);
    });
    reg.add_im("EAGLContext", "renderbufferStorage:fromDrawable:", [](Runtime&, u32, const ObjCArgs&) {
        return ObjCRet::val(1);
    });
    reg.add_im("EAGLContext", "presentRenderbuffer:", [](Runtime& rt, u32, const ObjCArgs&) {
        rt.frames_presented += 1u;
        if (rt.present_hook) rt.present_hook();
        return ObjCRet::val(1);
    });

    // ---- Touch System ---------------------------------------------------------------
    reg.add_im("NSSet", "countByEnumeratingWithState:objects:count:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        Machine& m = rt.machine();
        u32 state_ptr = a[0], stackbuf = a[1], count = a[2];
        auto* items = rt.objc().get_payload<std::vector<u32>>(recv);
        if (!items || items->empty()) return ObjCRet::val(0);
        if (m.rd32(state_ptr) == 0) {
            u32 n = std::min<u32>(static_cast<u32>(items->size()), count);
            for (u32 i = 0; i < n; ++i) m.wr32(stackbuf + 4u * i, (*items)[i]);
            m.wr32(state_ptr, 1u);
            m.wr32(state_ptr + 4u, stackbuf);
            if (rt.mutations_dummy == 0) {
                rt.mutations_dummy = m.malloc_bytes(8);
                m.wr32(rt.mutations_dummy, 1u);
            }
            m.wr32(state_ptr + 8u, rt.mutations_dummy);
            return ObjCRet::val(n);
        }
        return ObjCRet::val(0);
    });
    reg.add_im("UIEvent", "allTouches", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto* touches = rt.objc().get_payload<std::vector<u32>>(recv);
        return ObjCRet::val(rt.objc().new_obj_with("NSSet", touches ? *touches : std::vector<u32>{}));
    });
    reg.add_im("UITouch", "phase", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto* tp = rt.objc().get_payload<TouchPoint>(recv);
        return ObjCRet::val(tp ? tp->phase : 0u);
    });
    reg.add_im("UITouch", "locationInView:", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto* tp = rt.objc().get_payload<TouchPoint>(recv);
        return ObjCRet::stret_bytes(tp ? pack_point(tp->x, tp->y) : pack_point(0.0f, 0.0f));
    });
    reg.add_im("UITouch", "previousLocationInView:", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto* tp = rt.objc().get_payload<TouchPoint>(recv);
        return ObjCRet::stret_bytes(tp ? pack_point(tp->prev_x, tp->prev_y) : pack_point(0.0f, 0.0f));
    });
    reg.add_im("UITouch", "timestamp", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto* tp = rt.objc().get_payload<TouchPoint>(recv);
        double ts = (tp && tp->timestamp > 0.0) ? tp->timestamp : now_monotonic();
        u64 bits = f64_to_bits(ts);
        return ObjCRet::pair(u32(bits & 0xFFFFFFFFu), u32(bits >> 32));
    });

    // ---- mach / sysctl / CoreGraphics helpers ---------------------------------------
    reg.add_func("_mach_host_self", [](Machine&) { return TrapResult::val(0x103u); });
    reg.add_func("_host_page_size", [](Machine& m) {
        m.wr32(m.reg(1), 4096u);
        return TrapResult::val(0u);
    });
    reg.add_func("_host_statistics", [](Machine& m) {
        u32 info = m.reg(2);
        const u32 counts[4] = {20000u, 10000u, 5000u, 5000u};
        for (u32 i = 0; i < 4; ++i) m.wr32(info + 4u * i, counts[i]);
        return TrapResult::val(0u);
    });
    reg.add_func("_sysctlbyname", [](Machine& m) {
        std::string name = m.cstr(m.reg(0));
        u32 oldp = m.reg(1), oldlenp = m.reg(2);
        std::string val;
        if (name == "hw.machine") val = "iPhone1,2";
        else if (name == "hw.model") val = "N82AP";
        else return TrapResult::val(0xFFFFFFFFu);
        Bytes b(val.begin(), val.end());
        b.push_back(0);
        if (oldp) m.wrbytes(oldp, b);
        if (oldlenp) m.wr32(oldlenp, static_cast<u32>(b.size()));
        return TrapResult::val(0u);
    });
    reg.add_func("_CGAffineTransformMakeRotation", [](Machine& m) {
        u32 buf = m.reg(0);
        float angle = bits_to_f32(m.reg(1));
        float c = std::cos(angle);
        float s = std::sin(angle);
        const float vals[6] = {c, s, -s, c, 0.0f, 0.0f};
        Bytes b(24);
        for (int i = 0; i < 6; ++i) write_u32_le(b.data() + i * 4, f32_to_bits(vals[i]));
        m.wrbytes(buf, b);
        return TrapResult::val(buf);
    });
}

}  // namespace hl3
