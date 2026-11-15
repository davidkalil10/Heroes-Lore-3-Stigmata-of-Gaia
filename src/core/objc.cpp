#include "objc.h"

#include <cstring>
#include <sstream>

namespace hl3 {

namespace {

std::vector<std::string> split_pipe(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find('|', start);
        if (pos == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

const std::unordered_map<std::string, std::string>& hle_superclass_map() {
    static const std::unordered_map<std::string, std::string> kMap = {
        {"NSString", "NSObject"},
        {"NSConstantString", "NSString"},
        {"NSMutableString", "NSString"},
        {"NSArray", "NSObject"},
        {"NSMutableArray", "NSArray"},
        {"NSDictionary", "NSObject"},
        {"NSMutableDictionary", "NSDictionary"},
        {"NSData", "NSObject"},
        {"NSMutableData", "NSData"},
        {"NSSet", "NSObject"},
        {"NSMutableSet", "NSSet"},
        {"NSNumber", "NSObject"},
        {"NSDecimalNumber", "NSNumber"},
        {"UIView", "UIResponder"},
        {"UIResponder", "NSObject"},
        {"UIWindow", "UIView"},
        {"UIViewController", "UIResponder"},
        {"UIControl", "UIView"},
        {"UIButton", "UIControl"},
        {"UITextField", "UIControl"},
        {"UILabel", "UIView"},
        {"UIImageView", "UIView"},
        {"UIWebView", "UIView"},
        {"UIActivityIndicatorView", "UIView"},
        {"UIApplication", "UIResponder"},
        {"CAEAGLLayer", "NSObject"},
        {"NSMutableURLRequest", "NSURLRequest"},
        {"NSMutableCharacterSet", "NSCharacterSet"},
    };
    return kMap;
}

bool is_ignored_selector(const std::string& sel) {
    static const std::unordered_set<std::string> kIgnored = {
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
    };
    return sel.rfind("set", 0) == 0 || kIgnored.find(sel) != kIgnored.end();
}

}  // namespace

void HleRegistry::add_im(const std::string& cls, const std::string& sels, MethodFn fn) {
    for (const auto& sel : split_pipe(sels)) {
        imethods[cls + "|" + sel] = fn;
    }
}

void HleRegistry::add_cm(const std::string& cls, const std::string& sels, MethodFn fn) {
    for (const auto& sel : split_pipe(sels)) {
        cmethods[cls + "|" + sel] = fn;
    }
}

void HleRegistry::add_func(const std::string& names, TrapFn fn) {
    for (const auto& n : split_pipe(names)) {
        funcs[n] = fn;
    }
}

ObjCRuntime::ObjCRuntime(Machine& m, const HleRegistry& reg)
    : m_(m), reg_(reg), dispatch_cache_(4096) {}

void ObjCRuntime::register_image_classes() {
    for (const auto& c : m_.image().objc_classes) {
        auto hc = std::make_unique<HClass>();
        hc->name = c.name;
        hc->addr = c.class_addr;
        hc->meta = c.metaclass_addr;
        hc->instance_size = c.instance_size;
        hc->from_image = true;
        for (const auto& mt : c.instance_methods) {
            hc->imethods[mt.selector] = {true, mt.imp, {}};
        }
        for (const auto& mt : c.class_methods) {
            hc->cmethods[mt.selector] = {true, mt.imp, {}};
        }
        HClass* raw = hc.get();
        owned_[c.name] = std::move(hc);
        by_name_[raw->name] = raw;
        by_addr_[raw->addr] = raw;
        by_meta_[raw->meta] = raw;
    }
    for (const auto& c : m_.image().objc_classes) {
        HClass* hc = by_name_[c.name];
        if (!c.superclass_name.empty()) {
            hc->sup = hle_class(c.superclass_name);
        }
    }
}

HClass* ObjCRuntime::hle_class(const std::string& name) {
    if (auto it = by_name_.find(name); it != by_name_.end()) {
        return it->second;
    }
    HClass* sup = nullptr;
    if (name != "NSObject") {
        const auto& smap = hle_superclass_map();
        auto sit = smap.find(name);
        std::string sup_name = (sit != smap.end()) ? sit->second : "NSObject";
        if (!sup_name.empty()) {
            sup = hle_class(sup_name);
        }
    }
    u32 addr = m_.class_alloc(32);
    u32 meta = m_.class_alloc(32);

    auto hc = std::make_unique<HClass>();
    hc->name = name;
    hc->addr = addr;
    hc->meta = meta;
    hc->sup = sup;
    hc->instance_size = 8;
    hc->from_image = false;

    m_.wr32(addr, meta);
    m_.wr32(addr + 4u, sup ? sup->addr : 0u);
    HClass* nsobj = (name != "NSObject") ? hle_class("NSObject") : nullptr;
    m_.wr32(meta, nsobj ? nsobj->meta : meta);
    m_.wr32(meta + 4u, sup ? sup->meta : addr);

    std::string prefix = name + "|";
    for (const auto& [k, fn] : reg_.imethods) {
        if (k.rfind(prefix, 0) == 0) {
            hc->imethods[k.substr(prefix.size())] = {false, 0, fn};
        }
    }
    for (const auto& [k, fn] : reg_.cmethods) {
        if (k.rfind(prefix, 0) == 0) {
            hc->cmethods[k.substr(prefix.size())] = {false, 0, fn};
        }
    }

    HClass* raw = hc.get();
    owned_[name] = std::move(hc);
    by_name_[name] = raw;
    by_addr_[addr] = raw;
    by_meta_[meta] = raw;
    return raw;
}

HClass* ObjCRuntime::find_class_by_name(const std::string& name) const {
    auto it = by_name_.find(name);
    return (it == by_name_.end()) ? nullptr : it->second;
}

HClass* ObjCRuntime::find_class_by_addr(u32 addr) const {
    auto it = by_addr_.find(addr);
    return (it == by_addr_.end()) ? nullptr : it->second;
}

bool ObjCRuntime::try_class_for_symbol(const std::string& sym, u32& out_addr) {
    static constexpr const char* kClsPrefix = "_OBJC_CLASS_$_";
    static constexpr const char* kMetaPrefix = "_OBJC_METACLASS_$_";
    if (sym.rfind(kClsPrefix, 0) == 0) {
        out_addr = hle_class(sym.substr(std::strlen(kClsPrefix)))->addr;
        return true;
    }
    if (sym.rfind(kMetaPrefix, 0) == 0) {
        out_addr = hle_class(sym.substr(std::strlen(kMetaPrefix)))->meta;
        return true;
    }
    return false;
}

u32 ObjCRuntime::alloc_instance(HClass* cls) {
    u32 sz = std::max<u32>(cls->instance_size, 8u);
    u32 addr = m_.malloc_bytes(sz);
    m_.wr32(addr, cls->addr);
    return addr;
}

u32 ObjCRuntime::new_obj(const std::string& clsname) {
    return alloc_instance(hle_class(clsname));
}

std::pair<HClass*, bool> ObjCRuntime::class_of(u32 obj) const {
    if (obj == 0) return {nullptr, false};
    u32 isa = 0;
    try {
        isa = m_.rd32(obj);
    } catch (...) {
        return {nullptr, false};
    }
    if (auto it = by_meta_.find(isa); it != by_meta_.end()) {
        return {it->second, true};
    }
    if (auto it = by_addr_.find(isa); it != by_addr_.end()) {
        return {it->second, false};
    }
    return {nullptr, false};
}

const HMethodEntry* ObjCRuntime::lookup(HClass* cls, const std::string& sel, bool is_class) const {
    for (HClass* c = cls; c != nullptr; c = c->sup) {
        const auto& table = is_class ? c->cmethods : c->imethods;
        auto it = table.find(sel);
        if (it != table.end()) return &it->second;
    }
    if (is_class) {
        for (HClass* c = cls; c != nullptr; c = c->sup) {
            if (c->name == "NSObject") {
                auto it = c->imethods.find(sel);
                if (it != c->imethods.end()) return &it->second;
            }
        }
    }
    return nullptr;
}

const std::string& ObjCRuntime::sel_name_ref(u32 ptr) {
    static const std::string kEmpty;
    if (ptr == 0) return kEmpty;
    auto it = sel_cache_.find(ptr);
    if (it != sel_cache_.end()) return it->second;
    auto [inserted, _] = sel_cache_.emplace(ptr, m_.cstr(ptr));
    return inserted->second;
}

std::string ObjCRuntime::sel_name(u32 ptr) {
    return sel_name_ref(ptr);
}

u32 ObjCRuntime::resolve_send_slot(bool stret, bool super_) {
    if (stret) {
        if (slot_msg_send_stret_ == 0xFFFFFFFFu) {
            slot_msg_send_stret_ = m_.import_addr("_objc_msgSend_stret");
        }
        return slot_msg_send_stret_;
    }
    if (super_) {
        if (slot_msg_send_super2_ == 0xFFFFFFFFu) {
            slot_msg_send_super2_ = m_.import_addr("_objc_msgSendSuper2");
        }
        return slot_msg_send_super2_;
    }
    if (slot_msg_send_ == 0xFFFFFFFFu) {
        slot_msg_send_ = m_.import_addr("_objc_msgSend");
    }
    return slot_msg_send_;
}

bool ObjCRuntime::try_get_str(u32 obj, std::string& out) const {
    if (obj == 0) return false;
    if (auto it = hobj_.find(obj); it != hobj_.end()) {
        if (const auto* s = std::any_cast<std::string>(&it->second)) {
            out = *s;
            return true;
        }
        if (const auto* b = std::any_cast<Bytes>(&it->second)) {
            out.assign(reinterpret_cast<const char*>(b->data()), b->size());
            return true;
        }
    }
    auto [cls, is_cls] = class_of(obj);
    if (!is_cls && cls && cls->name == "NSConstantString") {
        u32 flags = m_.rd32(obj + 4u);
        u32 ptr = m_.rd32(obj + 8u);
        u32 length = m_.rd32(obj + 12u);
        Bytes raw = m_.rdbytes(ptr, (flags & 0x10000u || flags == 0x7C8u || flags == (0x7C8u | 0x10u))
                                        ? size_t(length)
                                        : size_t(length) * 2u);
        if (flags & 0x10000u || flags == 0x7C8u || flags == (0x7C8u | 0x10u)) {
            // Convert ISO-8859-1 to UTF-8 safely
            out.clear();
            for (u8 c : raw) {
                if (c < 0x80u) {
                    out.push_back(static_cast<char>(c));
                } else {
                    out.push_back(static_cast<char>(0xC0u | (c >> 6)));
                    out.push_back(static_cast<char>(0x80u | (c & 0x3Fu)));
                }
            }
            return true;
        } else {
            // UTF-16LE -> UTF-8
            out.clear();
            for (size_t i = 0; i + 1 < raw.size(); i += 2) {
                u16 ch = read_u16_le(raw.data() + i);
                if (ch < 0x80u) {
                    out.push_back(static_cast<char>(ch));
                } else if (ch < 0x800u) {
                    out.push_back(static_cast<char>(0xC0u | (ch >> 6)));
                    out.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
                } else {
                    out.push_back(static_cast<char>(0xE0u | (ch >> 12)));
                    out.push_back(static_cast<char>(0x80u | ((ch >> 6) & 0x3Fu)));
                    out.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
                }
            }
            return true;
        }
    }
    return false;
}

std::string ObjCRuntime::get_str(u32 obj) const {
    std::string s;
    try_get_str(obj, s);
    return s;
}

u32 ObjCRuntime::nsstring(const std::string& text) {
    return new_obj_with("NSString", text);
}

void ObjCRuntime::msg_send(Runtime& rt, bool stret, bool super_) {
    int base = stret ? 1 : 0;
    u32 recv = m_.reg(base);
    u32 sel_ptr = m_.reg(base + 1);
    u32 slot = resolve_send_slot(stret, super_);
    m_.reset_forward(slot);

    if (recv == 0) {
        m_.set_reg(0, 0);
        m_.set_reg(1, 0);
        return;
    }

    HClass* cls = nullptr;
    bool is_class = false;
    const HMethodEntry* target = nullptr;

    if (!super_ && !msg_trace_) {
        const u8* isa_p = m_.guest_rptr(recv, 4);
        if (isa_p) {
            u32 isa = read_u32_le(isa_p);
            u32 cidx = ((isa >> 2) ^ (sel_ptr >> 2) ^ (isa >> 11)) & 4095u;
            const DispatchCacheEntry& ce = dispatch_cache_[cidx];
            if (ce.isa == isa && ce.sel_ptr == sel_ptr && ce.method != nullptr) {
                cls = ce.cls;
                is_class = ce.is_class;
                target = ce.method;
            } else {
                auto [c, is_c] = class_of(recv);
                cls = c;
                is_class = is_c;
                if (cls) {
                    const std::string& sel = sel_name_ref(sel_ptr);
                    target = lookup(cls, sel, is_class);
                    if (target) {
                        dispatch_cache_[cidx] = {isa, sel_ptr, cls, is_class, target};
                    }
                }
            }
        }
    }

    if (!cls) {
        const std::string& sel = sel_name_ref(sel_ptr);
        HClass* start_cls = nullptr;
        if (super_) {
            u32 sup_ptr = recv;
            recv = m_.rd32(sup_ptr);
            u32 cls_ptr = m_.rd32(sup_ptr + 4u);
            HClass* cur = nullptr;
            if (auto it = by_addr_.find(cls_ptr); it != by_addr_.end()) cur = it->second;
            else if (auto it2 = by_meta_.find(cls_ptr); it2 != by_meta_.end()) cur = it2->second;
            auto [c, is_c] = class_of(recv);
            cls = c;
            is_class = is_c;
            if (cls && cur) {
                start_cls = cur->sup;
            }
            if (recv == 0) {
                m_.set_reg(0, 0);
                m_.set_reg(1, 0);
                return;
            }
        } else {
            auto [c, is_c] = class_of(recv);
            cls = c;
            is_class = is_c;
        }
        if (!cls) {
            m_.record_unimplemented("msg-to-unknown-object:" + sel);
            m_.set_reg(0, 0);
            return;
        }
        HClass* lookup_cls = start_cls ? start_cls : cls;
        if (msg_trace_) {
            m_.log(std::string("MSG ") + (is_class ? "+" : "-") + "[" + cls->name + " " + sel + "] r0=" +
                   hex32(m_.reg(0)) + " lr=" + hex32(m_.lr()));
        }
        target = lookup(lookup_cls, sel, is_class);
        if (!target) {
            std::string key = std::string(is_class ? "+" : "-") + "[" + cls->name + " " + sel + "]";
            m_.record_unimplemented(key);
            m_.set_reg(0, 0);
            m_.set_reg(1, 0);
            if (!is_ignored_selector(sel) && logged_unimpl_.insert(key).second) {
                m_.log("UNIMPL msg " + key + " from lr=" + hex32(m_.lr()));
            }
            return;
        }
    } else if (!target) {
        const std::string& sel = sel_name_ref(sel_ptr);
        std::string key = std::string(is_class ? "+" : "-") + "[" + cls->name + " " + sel + "]";
        m_.record_unimplemented(key);
        m_.set_reg(0, 0);
        m_.set_reg(1, 0);
        if (!is_ignored_selector(sel) && logged_unimpl_.insert(key).second) {
            m_.log("UNIMPL msg " + key + " from lr=" + hex32(m_.lr()));
        }
        return;
    }

    if (target->is_guest) {
        if (super_) {
            m_.set_reg(0, recv);
        }
        m_.forward_to(slot, target->guest_imp);
        return;
    }

    ObjCArgs args(m_, base + 2);
    ObjCRet result = target->host_fn(rt, recv, args);
    if (stret) {
        u32 buf = m_.reg(0);
        if (result.is_bytes && !result.bytes.empty()) {
            m_.wrbytes(buf, result.bytes);
        }
        return;
    }
    if (result.is_pair) {
        m_.set_reg(0, result.r0);
        m_.set_reg(1, result.r1);
    } else {
        m_.set_reg(0, result.r0);
    }
}

}  // namespace hl3
