#include "hle_libc.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace hl3 {

namespace {

using ObjCArray = std::vector<u32>;
using ObjCDict = std::unordered_map<std::string, u32>;

struct CachedPtr {
    size_t len = 0;
    u32 ptr = 0;
};

struct CachedUtf8 {
    std::string text;
    u32 ptr = 0;
};

std::string dict_key(Runtime& rt, u32 key_obj) {
    std::string s;
    if (rt.objc().try_get_str(key_obj, s)) {
        return "s:" + s;
    }
    return "p:" + hex32(key_obj);
}

ObjCArray& ensure_array(Runtime& rt, u32 obj) {
    auto* arr = rt.objc().get_payload<ObjCArray>(obj);
    if (!arr) {
        rt.objc().set_payload(obj, ObjCArray{});
        arr = rt.objc().get_payload<ObjCArray>(obj);
    }
    return *arr;
}

ObjCDict& ensure_dict(Runtime& rt, u32 obj) {
    auto* d = rt.objc().get_payload<ObjCDict>(obj);
    if (!d) {
        rt.objc().set_payload(obj, ObjCDict{});
        d = rt.objc().get_payload<ObjCDict>(obj);
    }
    return *d;
}

double now_epoch_seconds() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

double num_as_double(Runtime& rt, u32 obj) {
    if (auto* d = rt.objc().get_payload<double>(obj)) return *d;
    if (auto* f = rt.objc().get_payload<float>(obj)) return static_cast<double>(*f);
    if (auto* i = rt.objc().get_payload<i64>(obj)) return static_cast<double>(*i);
    if (auto* u = rt.objc().get_payload<u64>(obj)) return static_cast<double>(*u);
    return 0.0;
}

i64 num_as_i64(Runtime& rt, u32 obj) {
    if (auto* i = rt.objc().get_payload<i64>(obj)) return *i;
    if (auto* u = rt.objc().get_payload<u64>(obj)) return static_cast<i64>(*u);
    if (auto* d = rt.objc().get_payload<double>(obj)) return static_cast<i64>(*d);
    if (auto* f = rt.objc().get_payload<float>(obj)) return static_cast<i64>(*f);
    return 0;
}

std::string rstrip_slashes(std::string s) {
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string trim_slashes(std::string s) {
    while (!s.empty() && s.front() == '/') s.erase(s.begin());
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

// Minimal Apple binary plist (`bplist00`) reader for Info.plist.
class BplistReader {
public:
    BplistReader(Runtime& rt, const Bytes& data) : rt_(rt), d_(data) {}

    u32 parse_root() {
        if (d_.size() < 40 || std::memcmp(d_.data(), "bplist00", 8) != 0) return 0;
        const u8* trailer = d_.data() + d_.size() - 32;
        off_int_size_ = trailer[6];
        obj_ref_size_ = trailer[7];
        num_objects_ = read_be_u64(trailer + 8, 8);
        top_object_ = read_be_u64(trailer + 16, 8);
        offset_table_offset_ = read_be_u64(trailer + 24, 8);
        if (off_int_size_ == 0 || obj_ref_size_ == 0 ||
            offset_table_offset_ + num_objects_ * off_int_size_ > d_.size() - 32) {
            return 0;
        }
        return parse_object(top_object_);
    }

private:
    u64 read_be_u64(const u8* p, size_t len) const {
        u64 v = 0;
        for (size_t i = 0; i < len; ++i) v = (v << 8) | p[i];
        return v;
    }

    u64 object_offset(u64 obj_idx) const {
        size_t pos = static_cast<size_t>(offset_table_offset_ + obj_idx * off_int_size_);
        return read_be_u64(d_.data() + pos, off_int_size_);
    }

    u64 read_len(size_t& pos, u8 info) const {
        if (info != 0x0Fu) return info;
        if (pos >= d_.size()) return 0;
        u8 marker = d_[pos++];
        u8 sz = u8(1u) << (marker & 0x0Fu);
        if (pos + sz > d_.size()) return 0;
        u64 v = read_be_u64(d_.data() + pos, sz);
        pos += sz;
        return v;
    }

    u32 parse_object(u64 idx) {
        if (idx >= num_objects_) return 0;
        size_t pos = static_cast<size_t>(object_offset(idx));
        if (pos >= d_.size()) return 0;
        u8 marker = d_[pos++];
        u8 type = (marker >> 4) & 0x0Fu;
        u8 info = marker & 0x0Fu;

        switch (type) {
            case 0x0:
                if (info == 8) return rt_.objc().new_obj_with("NSNumber", i64(0));
                if (info == 9) return rt_.objc().new_obj_with("NSNumber", i64(1));
                return 0;
            case 0x1: {
                size_t n = size_t(1) << info;
                if (pos + n > d_.size()) return 0;
                u64 val = read_be_u64(d_.data() + pos, n);
                return rt_.objc().new_obj_with("NSNumber", static_cast<i64>(val));
            }
            case 0x2: {
                size_t n = size_t(1) << info;
                if (pos + n > d_.size()) return 0;
                u64 raw = read_be_u64(d_.data() + pos, n);
                double dv = 0.0;
                if (n == 4) {
                    u32 u = static_cast<u32>(raw);
                    float f = 0.0f;
                    std::memcpy(&f, &u, 4);
                    dv = f;
                } else if (n == 8) {
                    std::memcpy(&dv, &raw, 8);
                }
                return rt_.objc().new_obj_with("NSNumber", dv);
            }
            case 0x4: {
                u64 len = read_len(pos, info);
                if (pos + len > d_.size()) return 0;
                Bytes b(d_.begin() + pos, d_.begin() + pos + len);
                return rt_.objc().new_obj_with("NSData", std::move(b));
            }
            case 0x5: {
                u64 len = read_len(pos, info);
                if (pos + len > d_.size()) return 0;
                std::string s(reinterpret_cast<const char*>(d_.data() + pos), static_cast<size_t>(len));
                return rt_.objc().nsstring(s);
            }
            case 0x6: {
                u64 count = read_len(pos, info);
                if (pos + count * 2 > d_.size()) return 0;
                std::string s;
                for (u64 i = 0; i < count; ++i) {
                    u16 ch = u16(d_[pos + i * 2] << 8) | d_[pos + i * 2 + 1];
                    if (ch < 0x80u) s.push_back(static_cast<char>(ch));
                    else if (ch < 0x800u) {
                        s.push_back(static_cast<char>(0xC0u | (ch >> 6)));
                        s.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
                    } else {
                        s.push_back(static_cast<char>(0xE0u | (ch >> 12)));
                        s.push_back(static_cast<char>(0x80u | ((ch >> 6) & 0x3Fu)));
                        s.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
                    }
                }
                return rt_.objc().nsstring(s);
            }
            case 0xA: {
                u64 count = read_len(pos, info);
                ObjCArray arr;
                for (u64 i = 0; i < count && pos + obj_ref_size_ <= d_.size(); ++i) {
                    u64 ref = read_be_u64(d_.data() + pos, obj_ref_size_);
                    pos += obj_ref_size_;
                    arr.push_back(parse_object(ref));
                }
                return rt_.objc().new_obj_with("NSArray", std::move(arr));
            }
            case 0xD: {
                u64 count = read_len(pos, info);
                ObjCDict dict;
                size_t keys_pos = pos;
                size_t vals_pos = pos + static_cast<size_t>(count * obj_ref_size_);
                if (vals_pos + count * obj_ref_size_ > d_.size()) return 0;
                for (u64 i = 0; i < count; ++i) {
                    u64 kref = read_be_u64(d_.data() + keys_pos + i * obj_ref_size_, obj_ref_size_);
                    u64 vref = read_be_u64(d_.data() + vals_pos + i * obj_ref_size_, obj_ref_size_);
                    u32 kobj = parse_object(kref);
                    u32 vobj = parse_object(vref);
                    dict[dict_key(rt_, kobj)] = vobj;
                }
                return rt_.objc().new_obj_with("NSDictionary", std::move(dict));
            }
            default:
                return 0;
        }
    }

    Runtime& rt_;
    const Bytes& d_;
    u8 off_int_size_ = 1;
    u8 obj_ref_size_ = 1;
    u64 num_objects_ = 0;
    u64 top_object_ = 0;
    u64 offset_table_offset_ = 0;
};

}  // namespace

void register_hle_foundation_runtime_funcs(Runtime& rt, HleRegistry& reg);

void register_hle_foundation(Runtime& rt, HleRegistry& reg) {
    register_hle_foundation_runtime_funcs(rt, reg);
    reg.data_strings["_NSLocalizedDescriptionKey"] = "NSLocalizedDescription";
    reg.data_strings["_NSUnderlyingErrorKey"] = "NSUnderlyingError";
    reg.data_strings["_NSFileSystemFreeSize"] = "NSFileSystemFreeSize";
    reg.data_strings["_NSLocaleCountryCode"] = "NSLocaleCountryCode";

    // ---- NSObject -------------------------------------------------------------------
    reg.add_cm("NSObject", "alloc|allocWithZone:|new", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        HClass* cls = rt.objc().find_class_by_addr(recv);
        if (!cls) return ObjCRet::val(0);
        u32 inst = rt.objc().alloc_instance(cls);
        if (cls->name == "TouchResponder") rt.touch_responder = inst;
        return ObjCRet::val(inst);
    });
    reg.add_cm("NSObject", "class", [](Runtime&, u32 recv, const ObjCArgs&) {
        return ObjCRet::val(recv);
    });
    reg.add_im("NSObject", "init|retain|autorelease|self|copy|mutableCopy",
               [](Runtime&, u32 recv, const ObjCArgs&) { return ObjCRet::val(recv); });
    reg.add_im("NSObject", "release|dealloc|performSelector:|performSelector:withObject:",
               [](Runtime&, u32, const ObjCArgs&) { return ObjCRet::val(0); });
    reg.add_im("NSObject", "class", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto [cls, _] = rt.objc().class_of(recv);
        return ObjCRet::val(cls ? cls->addr : 0u);
    });
    reg.add_im("NSObject", "isKindOfClass:|isMemberOfClass:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto [cls, _] = rt.objc().class_of(recv);
        HClass* target = rt.objc().find_class_by_addr(a[0]);
        for (HClass* c = cls; c != nullptr && target != nullptr; c = c->sup) {
            if (c == target) return ObjCRet::val(1);
        }
        return ObjCRet::val(0);
    });
    reg.add_im("NSObject", "respondsToSelector:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto [cls, is_cls] = rt.objc().class_of(recv);
        std::string sel = rt.objc().sel_name(a[0]);
        return ObjCRet::val((cls && rt.objc().lookup(cls, sel, is_cls) != nullptr) ? 1u : 0u);
    });
    reg.add_im("NSObject", "isEqual:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        if (recv == a[0]) return ObjCRet::val(1);
        std::string s1, s2;
        if (rt.objc().try_get_str(recv, s1) && rt.objc().try_get_str(a[0], s2) && s1 == s2) {
            return ObjCRet::val(1);
        }
        return ObjCRet::val(0);
    });
    reg.add_im("NSObject", "hash", [](Runtime&, u32 recv, const ObjCArgs&) { return ObjCRet::val(recv); });
    reg.add_im("NSObject", "description", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        std::string s;
        if (rt.objc().try_get_str(recv, s)) return ObjCRet::val(rt.objc().nsstring(s));
        auto [cls, _] = rt.objc().class_of(recv);
        return ObjCRet::val(rt.objc().nsstring("<" + (cls ? cls->name : "?") + ": " + hex32(recv) + ">"));
    });

    reg.add_func("_NSLog", [](Machine&) {
        // Formatted log via Runtime capture is unnecessary at trap entry if wired below, keep placeholder?
        return TrapResult::val(0u);
    });
    reg.add_func("_NSSetUncaughtExceptionHandler|_objc_enumerationMutation|_objc_sync_enter|_objc_sync_exit|_CFRelease",
                 [](Machine&) { return TrapResult::val(0u); });
    reg.add_func("_objc_getProperty", [](Machine& m) {
        return TrapResult::val(m.rd32(m.reg(0) + m.reg(2)));
    });
    reg.add_func("_objc_setProperty", [](Machine& m) {
        m.wr32(m.reg(0) + m.reg(2), m.reg(3));
        return TrapResult::val(0u);
    });

    // ---- NSAutoreleasePool -----------------------------------------------------------
    reg.add_cm("NSAutoreleasePool", "alloc", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().new_obj("NSAutoreleasePool"));
    });
    reg.add_im("NSAutoreleasePool", "init|drain|release",
               [](Runtime&, u32 recv, const ObjCArgs&) { return ObjCRet::val(recv); });

    // ---- NSString / NSMutableString -------------------------------------------------
    reg.add_cm("NSString", "string", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().nsstring(""));
    });
    reg.add_cm("NSString", "stringWithFormat:", [](Runtime& rt, u32, const ObjCArgs& a) {
        GuestArgReader rd(rt.machine(), 3, false);  // after recv(0), sel(1), fmt(2)
        return ObjCRet::val(rt.objc().nsstring(format_guest_c(rt, rt.objc().get_str(a[0]), rd, true)));
    });
    reg.add_cm("NSString", "stringWithUTF8String:|stringWithCString:encoding:", [](Runtime& rt, u32, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().nsstring(a[0] ? rt.machine().cstr(a[0]) : ""));
    });
    reg.add_cm("NSString", "stringWithCString:length:", [](Runtime& rt, u32, const ObjCArgs& a) {
        u32 ptr = a[0];
        u32 len = a[1];
        if (!ptr || len == 0) return ObjCRet::val(rt.objc().nsstring(""));
        Bytes raw = rt.machine().rdbytes(ptr, std::min<u32>(len, 1u << 20));
        auto nul = std::find(raw.begin(), raw.end(), static_cast<u8>(0));
        return ObjCRet::val(rt.objc().nsstring(std::string(raw.begin(), nul)));
    });
    reg.add_cm("NSString", "stringWithString:", [](Runtime& rt, u32, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().nsstring(rt.objc().get_str(a[0])));
    });
    reg.add_im("NSString", "UTF8String|cString", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        std::string s = rt.objc().get_str(recv);
        auto& cache = [&]() -> std::unordered_map<u32, CachedUtf8>& {
            auto* m = std::any_cast<std::unordered_map<u32, CachedUtf8>>(&rt.kv_state["utf8cache"]);
            if (!m) {
                rt.kv_state["utf8cache"] = std::unordered_map<u32, CachedUtf8>{};
                m = std::any_cast<std::unordered_map<u32, CachedUtf8>>(&rt.kv_state["utf8cache"]);
            }
            return *m;
        }();
        auto it = cache.find(recv);
        if (it == cache.end() || it->second.text != s) {
            u32 ptr = rt.machine().new_cstr(s);
            cache[recv] = {s, ptr};
            return ObjCRet::val(ptr);
        }
        return ObjCRet::val(it->second.ptr);
    });
    reg.add_im("NSString", "length", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        return ObjCRet::val(static_cast<u32>(rt.objc().get_str(recv).size()));
    });
    reg.add_im("NSString", "isEqualToString:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().get_str(recv) == rt.objc().get_str(a[0]) ? 1u : 0u);
    });
    reg.add_im("NSString", "stringByAppendingString:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().nsstring(rt.objc().get_str(recv) + rt.objc().get_str(a[0])));
    });
    reg.add_im("NSString", "stringByAppendingFormat:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        GuestArgReader rd(rt.machine(), 3, false);  // after recv(0), sel(1), fmt(2)
        std::string tail = format_guest_c(rt, rt.objc().get_str(a[0]), rd, true);
        return ObjCRet::val(rt.objc().nsstring(rt.objc().get_str(recv) + tail));
    });
    reg.add_im("NSString", "stringByAppendingPathComponent:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().nsstring(rstrip_slashes(rt.objc().get_str(recv)) + "/" + rt.objc().get_str(a[0])));
    });
    reg.add_im("NSString", "initWithString:|initWithUTF8String:|initWithCString:",
               [](Runtime& rt, u32 recv, const ObjCArgs& a) {
                   std::string v;
                   if (!rt.objc().try_get_str(a[0], v)) v = rt.machine().cstr(a[0]);
                   rt.objc().set_payload(recv, v);
                   return ObjCRet::val(recv);
               });
    reg.add_im("NSString", "initWithFormat:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        GuestArgReader rd(rt.machine(), 3, false);
        rt.objc().set_payload(recv, format_guest_c(rt, rt.objc().get_str(a[0]), rd, true));
        return ObjCRet::val(recv);
    });
    reg.add_im("NSString", "intValue|integerValue", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        return ObjCRet::val(static_cast<u32>(std::atoi(rt.objc().get_str(recv).c_str())));
    });
    reg.add_im("NSString", "hasPrefix:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        std::string p;
        if (!rt.objc().try_get_str(a[0], p) || p.empty()) return ObjCRet::val(0);
        return ObjCRet::val(rt.objc().get_str(recv).rfind(p, 0) == 0 ? 1u : 0u);
    });
    reg.add_im("NSString", "hasSuffix:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        std::string s = rt.objc().get_str(recv);
        std::string suf;
        if (!rt.objc().try_get_str(a[0], suf) || suf.size() > s.size()) return ObjCRet::val(0);
        return ObjCRet::val(s.compare(s.size() - suf.size(), suf.size(), suf) == 0 ? 1u : 0u);
    });
    reg.add_im("NSString", "lowercaseString", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        std::string s = rt.objc().get_str(recv);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ObjCRet::val(rt.objc().nsstring(s));
    });
    reg.add_im("NSString", "uppercaseString", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        std::string s = rt.objc().get_str(recv);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return ObjCRet::val(rt.objc().nsstring(s));
    });
    reg.add_im("NSString", "substringToIndex:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        std::string s = rt.objc().get_str(recv);
        return ObjCRet::val(rt.objc().nsstring(s.substr(0, std::min<size_t>(a[0], s.size()))));
    });
    reg.add_im("NSString", "substringFromIndex:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        std::string s = rt.objc().get_str(recv);
        return ObjCRet::val(rt.objc().nsstring(a[0] < s.size() ? s.substr(a[0]) : ""));
    });
    reg.add_im("NSString", "substringWithRange:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        std::string s = rt.objc().get_str(recv);
        size_t start = std::min<size_t>(a[0], s.size());
        size_t len = std::min<size_t>(a[1], s.size() - start);
        return ObjCRet::val(rt.objc().nsstring(s.substr(start, len)));
    });
    reg.add_im("NSString", "characterAtIndex:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        std::string s = rt.objc().get_str(recv);
        return ObjCRet::val(a[0] < s.size() ? static_cast<u8>(s[a[0]]) : 0u);
    });
    reg.add_im("NSString", "pathExtension", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        std::string s = rt.objc().get_str(recv);
        size_t slash = s.rfind('/');
        std::string last = (slash == std::string::npos) ? s : s.substr(slash + 1);
        size_t dot = last.rfind('.');
        return ObjCRet::val(rt.objc().nsstring(dot == std::string::npos ? "" : last.substr(dot + 1)));
    });
    reg.add_im("NSString", "stringByDeletingPathExtension", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        std::string s = rt.objc().get_str(recv);
        size_t slash = s.rfind('/');
        std::string last = (slash == std::string::npos) ? s : s.substr(slash + 1);
        size_t dot = last.rfind('.');
        if (dot == std::string::npos) return ObjCRet::val(rt.objc().nsstring(s));
        size_t cut = s.size() - (last.size() - dot);
        return ObjCRet::val(rt.objc().nsstring(s.substr(0, cut)));
    });
    reg.add_im("NSString", "lastPathComponent", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        std::string s = rstrip_slashes(rt.objc().get_str(recv));
        size_t slash = s.rfind('/');
        return ObjCRet::val(rt.objc().nsstring(slash == std::string::npos ? s : s.substr(slash + 1)));
    });
    reg.add_im("NSString", "dataUsingEncoding:", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        std::string s = rt.objc().get_str(recv);
        Bytes b(s.begin(), s.end());
        return ObjCRet::val(rt.objc().new_obj_with("NSData", std::move(b)));
    });
    reg.add_cm("NSMutableString", "string|stringWithCapacity:", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().new_obj_with("NSMutableString", std::string{}));
    });
    reg.add_im("NSMutableString", "setString:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        rt.objc().set_payload(recv, rt.objc().get_str(a[0]));
        return ObjCRet::val(0);
    });
    reg.add_im("NSMutableString", "appendString:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        rt.objc().set_payload(recv, rt.objc().get_str(recv) + rt.objc().get_str(a[0]));
        return ObjCRet::val(0);
    });
    reg.add_im("NSMutableString", "appendFormat:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        GuestArgReader rd(rt.machine(), 3, false);
        rt.objc().set_payload(recv, rt.objc().get_str(recv) + format_guest_c(rt, rt.objc().get_str(a[0]), rd, true));
        return ObjCRet::val(0);
    });

    // ---- NSArray / NSMutableArray ---------------------------------------------------
    reg.add_cm("NSArray", "array", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        HClass* cls = rt.objc().find_class_by_addr(recv);
        std::string cname = (cls && cls->name == "NSMutableArray") ? "NSMutableArray" : "NSArray";
        return ObjCRet::val(rt.objc().new_obj_with(cname, ObjCArray{}));
    });
    reg.add_cm("NSMutableArray", "array|arrayWithCapacity:", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().new_obj_with("NSMutableArray", ObjCArray{}));
    });
    reg.add_im("NSArray", "init|initWithCapacity:", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        rt.objc().set_payload(recv, ObjCArray{});
        return ObjCRet::val(recv);
    });
    reg.add_im("NSArray", "count", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        return ObjCRet::val(static_cast<u32>(ensure_array(rt, recv).size()));
    });
    reg.add_im("NSArray", "objectAtIndex:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& arr = ensure_array(rt, recv);
        return ObjCRet::val(a[0] < arr.size() ? arr[a[0]] : 0u);
    });
    reg.add_im("NSArray", "lastObject", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto& arr = ensure_array(rt, recv);
        return ObjCRet::val(arr.empty() ? 0u : arr.back());
    });
    reg.add_im("NSArray", "containsObject:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& arr = ensure_array(rt, recv);
        return ObjCRet::val(std::find(arr.begin(), arr.end(), a[0]) != arr.end() ? 1u : 0u);
    });
    reg.add_im("NSArray", "indexOfObject:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& arr = ensure_array(rt, recv);
        auto it = std::find(arr.begin(), arr.end(), a[0]);
        return ObjCRet::val(it == arr.end() ? 0x7FFFFFFFu : static_cast<u32>(it - arr.begin()));
    });
    reg.add_im("NSMutableArray", "addObject:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        ensure_array(rt, recv).push_back(a[0]);
        return ObjCRet::val(0);
    });
    reg.add_im("NSMutableArray", "insertObject:atIndex:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& arr = ensure_array(rt, recv);
        size_t idx = std::min<size_t>(a[1], arr.size());
        arr.insert(arr.begin() + idx, a[0]);
        return ObjCRet::val(0);
    });
    reg.add_im("NSMutableArray", "removeObjectAtIndex:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& arr = ensure_array(rt, recv);
        if (a[0] < arr.size()) arr.erase(arr.begin() + a[0]);
        return ObjCRet::val(0);
    });
    reg.add_im("NSMutableArray", "removeObject:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& arr = ensure_array(rt, recv);
        arr.erase(std::remove(arr.begin(), arr.end(), a[0]), arr.end());
        return ObjCRet::val(0);
    });
    reg.add_im("NSMutableArray", "removeAllObjects", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        ensure_array(rt, recv).clear();
        return ObjCRet::val(0);
    });

    // ---- NSDictionary / NSMutableDictionary -----------------------------------------
    reg.add_cm("NSDictionary", "dictionary", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        HClass* cls = rt.objc().find_class_by_addr(recv);
        return ObjCRet::val(rt.objc().new_obj_with(cls ? cls->name : "NSDictionary", ObjCDict{}));
    });
    reg.add_cm("NSMutableDictionary", "dictionary|dictionaryWithCapacity:", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().new_obj_with("NSMutableDictionary", ObjCDict{}));
    });
    reg.add_im("NSDictionary", "init|initWithCapacity:", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        rt.objc().set_payload(recv, ObjCDict{});
        return ObjCRet::val(recv);
    });
    reg.add_im("NSDictionary", "count", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        return ObjCRet::val(static_cast<u32>(ensure_dict(rt, recv).size()));
    });
    reg.add_im("NSDictionary", "objectForKey:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& d = ensure_dict(rt, recv);
        auto it = d.find(dict_key(rt, a[0]));
        return ObjCRet::val(it == d.end() ? 0u : it->second);
    });
    reg.add_im("NSMutableDictionary", "setObject:forKey:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        ensure_dict(rt, recv)[dict_key(rt, a[1])] = a[0];
        return ObjCRet::val(0);
    });
    reg.add_im("NSMutableDictionary", "removeObjectForKey:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        ensure_dict(rt, recv).erase(dict_key(rt, a[0]));
        return ObjCRet::val(0);
    });
    reg.add_im("NSMutableDictionary", "removeAllObjects", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        ensure_dict(rt, recv).clear();
        return ObjCRet::val(0);
    });
    reg.add_cm("NSDictionary", "dictionaryWithObjectsAndKeys:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        ObjCDict d;
        for (int i = 0;; i += 2) {
            u32 obj = a[i];
            if (obj == 0) break;
            u32 key = a[i + 1];
            if (key == 0) break;
            d[dict_key(rt, key)] = obj;
        }
        HClass* cls = rt.objc().find_class_by_addr(recv);
        std::string cname = (cls && cls->name == "NSMutableDictionary") ? "NSMutableDictionary" : "NSDictionary";
        return ObjCRet::val(rt.objc().new_obj_with(cname, std::move(d)));
    });

    // ---- NSNumber -------------------------------------------------------------------
    reg.add_cm("NSNumber", "numberWithInt:|numberWithInteger:|numberWithUnsignedInt:|numberWithBool:|numberWithLong:",
               [](Runtime& rt, u32, const ObjCArgs& a) {
                   return ObjCRet::val(rt.objc().new_obj_with("NSNumber", i64(static_cast<i32>(a[0]))));
               });
    reg.add_cm("NSNumber", "numberWithFloat:", [](Runtime& rt, u32, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().new_obj_with("NSNumber", double(a.f32(0))));
    });
    reg.add_cm("NSNumber", "numberWithDouble:", [](Runtime& rt, u32, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().new_obj_with("NSNumber", a.f64(0)));
    });
    reg.add_im("NSNumber", "intValue|integerValue|unsignedIntValue|boolValue|longValue",
               [](Runtime& rt, u32 recv, const ObjCArgs&) {
                   return ObjCRet::val(static_cast<u32>(num_as_i64(rt, recv)));
               });
    reg.add_im("NSNumber", "floatValue", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        return ObjCRet::val(f32_to_bits(static_cast<float>(num_as_double(rt, recv))));
    });
    reg.add_im("NSNumber", "doubleValue", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        u64 bits = f64_to_bits(num_as_double(rt, recv));
        return ObjCRet::pair(u32(bits & 0xFFFFFFFFu), u32(bits >> 32));
    });
    reg.add_im("NSNumber", "longLongValue|unsignedLongLongValue", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        u64 v = static_cast<u64>(num_as_i64(rt, recv));
        return ObjCRet::pair(u32(v & 0xFFFFFFFFu), u32(v >> 32));
    });

    // ---- NSValue --------------------------------------------------------------------
    reg.add_cm("NSValue", "valueWithPointer:", [](Runtime& rt, u32, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().new_obj_with("NSValue", u32(a[0])));
    });
    reg.add_im("NSValue", "pointerValue", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto* p = rt.objc().get_payload<u32>(recv);
        return ObjCRet::val(p ? *p : 0u);
    });

    // ---- NSData ---------------------------------------------------------------------
    reg.add_cm("NSData", "dataWithBytes:length:", [](Runtime& rt, u32, const ObjCArgs& a) {
        Bytes b = rt.machine().rdbytes(a[0], a[1]);
        return ObjCRet::val(rt.objc().new_obj_with("NSData", std::move(b)));
    });
    reg.add_cm("NSData", "dataWithContentsOfFile:", [](Runtime& rt, u32, const ObjCArgs& a) {
        Bytes b;
        if (!rt.read_guest_file(rt.objc().get_str(a[0]), b)) return ObjCRet::val(0);
        return ObjCRet::val(rt.objc().new_obj_with("NSData", std::move(b)));
    });
    reg.add_im("NSData", "length", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto* b = rt.objc().get_payload<Bytes>(recv);
        return ObjCRet::val(b ? static_cast<u32>(b->size()) : 0u);
    });
    reg.add_im("NSData", "bytes", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        auto* b = rt.objc().get_payload<Bytes>(recv);
        if (!b) return ObjCRet::val(0);
        auto* m = std::any_cast<std::unordered_map<u32, CachedPtr>>(&rt.kv_state["databytes"]);
        if (!m) {
            rt.kv_state["databytes"] = std::unordered_map<u32, CachedPtr>{};
            m = std::any_cast<std::unordered_map<u32, CachedPtr>>(&rt.kv_state["databytes"]);
        }
        auto it = m->find(recv);
        if (it == m->end() || it->second.len != b->size()) {
            u32 p = rt.machine().malloc_bytes(static_cast<u32>(b->size()) + 1u);
            if (!b->empty()) rt.machine().wrbytes(p, *b);
            (*m)[recv] = {b->size(), p};
            return ObjCRet::val(p);
        }
        return ObjCRet::val(it->second.ptr);
    });

    // ---- NSBundle / NSURL / paths ---------------------------------------------------
    reg.add_cm("NSBundle", "mainBundle", [](Runtime& rt, u32, const ObjCArgs&) {
        if (rt.main_bundle_obj == 0) rt.main_bundle_obj = rt.objc().new_obj("NSBundle");
        return ObjCRet::val(rt.main_bundle_obj);
    });
    reg.add_im("NSBundle", "resourcePath|bundlePath", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().nsstring(kGuestAppPath));
    });
    reg.add_im("NSBundle", "pathForResource:ofType:", [](Runtime& rt, u32, const ObjCArgs& a) {
        std::string name = rt.objc().get_str(a[0]);
        std::string ext = rt.objc().get_str(a[1]);
        std::string rel = ext.empty() ? name : (name + "." + ext);
        for (const std::string& cand : {rel, "en.lproj/" + rel}) {
            if (rt.bundle().has_asset(cand)) {
                return ObjCRet::val(rt.objc().nsstring(std::string(kGuestAppPath) + "/" + cand));
            }
        }
        return ObjCRet::val(0);
    });
    reg.add_im("NSBundle", "pathForResource:ofType:inDirectory:", [](Runtime& rt, u32, const ObjCArgs& a) {
        std::string name = rt.objc().get_str(a[0]);
        std::string ext = rt.objc().get_str(a[1]);
        std::string dir = trim_slashes(rt.objc().get_str(a[2]));
        std::string rel = (dir.empty() ? "" : (dir + "/")) + name + (ext.empty() ? "" : ("." + ext));
        if (rt.bundle().has_asset(rel)) {
            return ObjCRet::val(rt.objc().nsstring(std::string(kGuestAppPath) + "/" + rel));
        }
        return ObjCRet::val(0);
    });
    reg.add_im("NSBundle", "infoDictionary|localizedInfoDictionary", [](Runtime& rt, u32, const ObjCArgs&) {
        if (rt.info_dict_obj == 0) {
            Bytes plist_bytes;
            if (rt.bundle().read_asset("Info.plist", plist_bytes)) {
                BplistReader reader(rt, plist_bytes);
                rt.info_dict_obj = reader.parse_root();
            }
            if (rt.info_dict_obj == 0) {
                rt.info_dict_obj = rt.objc().new_obj_with("NSDictionary", ObjCDict{});
            }
        }
        return ObjCRet::val(rt.info_dict_obj);
    });
    reg.add_im("NSBundle", "objectForInfoDictionaryKey:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto* impl = rt.objc().lookup(rt.objc().hle_class("NSBundle"), "infoDictionary", false);
        u32 dict_obj = impl ? impl->host_fn(rt, recv, a).r0 : 0u;
        auto& d = ensure_dict(rt, dict_obj);
        auto it = d.find(dict_key(rt, a[0]));
        return ObjCRet::val(it == d.end() ? 0u : it->second);
    });

    reg.add_cm("NSURL", "fileURLWithPath:", [](Runtime& rt, u32, const ObjCArgs& a) {
        return ObjCRet::val(rt.objc().new_obj_with("NSURL", rt.objc().get_str(a[0])));
    });
    reg.add_im("NSURL", "path", [](Runtime& rt, u32 recv, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().nsstring(rt.objc().get_str(recv)));
    });

    // ---- NSUserDefaults -------------------------------------------------------------
    reg.add_cm("NSUserDefaults", "standardUserDefaults", [](Runtime& rt, u32, const ObjCArgs&) {
        if (rt.user_defaults_obj == 0) {
            rt.user_defaults_obj = rt.objc().new_obj_with("NSUserDefaults", ObjCDict{});
        }
        return ObjCRet::val(rt.user_defaults_obj);
    });
    reg.add_im("NSUserDefaults", "objectForKey:|stringForKey:|dataForKey:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& d = ensure_dict(rt, recv);
        auto it = d.find(dict_key(rt, a[0]));
        return ObjCRet::val(it == d.end() ? 0u : it->second);
    });
    reg.add_im("NSUserDefaults", "integerForKey:|boolForKey:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        auto& d = ensure_dict(rt, recv);
        auto it = d.find(dict_key(rt, a[0]));
        if (it == d.end() || it->second == 0) return ObjCRet::val(0);
        return ObjCRet::val(static_cast<u32>(num_as_i64(rt, it->second)));
    });
    reg.add_im("NSUserDefaults", "setObject:forKey:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        ensure_dict(rt, recv)[dict_key(rt, a[1])] = a[0];
        return ObjCRet::val(0);
    });
    reg.add_im("NSUserDefaults", "setInteger:forKey:|setBool:forKey:", [](Runtime& rt, u32 recv, const ObjCArgs& a) {
        ensure_dict(rt, recv)[dict_key(rt, a[1])] = rt.objc().new_obj_with("NSNumber", i64(static_cast<i32>(a[0])));
        return ObjCRet::val(0);
    });
    reg.add_im("NSUserDefaults", "synchronize", [](Runtime&, u32, const ObjCArgs&) { return ObjCRet::val(1); });

    // ---- NSFileManager --------------------------------------------------------------
    reg.add_cm("NSFileManager", "defaultManager", [](Runtime& rt, u32, const ObjCArgs&) {
        if (rt.file_manager_obj == 0) rt.file_manager_obj = rt.objc().new_obj("NSFileManager");
        return ObjCRet::val(rt.file_manager_obj);
    });
    reg.add_im("NSFileManager", "fileExistsAtPath:", [](Runtime& rt, u32, const ObjCArgs& a) {
        auto r = rt.resolve_path(rt.objc().get_str(a[0]));
        bool exists = false;
        if (r.kind == Runtime::ResolvedPath::Kind::Bundle) {
            exists = rt.bundle().has_asset_or_dir(r.bundle_rel);
        } else if (r.kind == Runtime::ResolvedPath::Kind::Docs) {
            std::error_code ec;
            exists = std::filesystem::exists(r.host_path, ec);
        }
        return ObjCRet::val(exists ? 1u : 0u);
    });
    reg.add_im("NSFileManager",
               "createDirectoryAtPath:attributes:|createDirectoryAtPath:withIntermediateDirectories:attributes:error:",
               [](Runtime& rt, u32, const ObjCArgs& a) {
                   auto r = rt.resolve_path(rt.objc().get_str(a[0]));
                   if (r.kind == Runtime::ResolvedPath::Kind::Docs) {
                       std::error_code ec;
                       std::filesystem::create_directories(r.host_path, ec);
                       return ObjCRet::val(ec ? 0u : 1u);
                   }
                   return ObjCRet::val(0u);
               });
    reg.add_im("NSFileManager", "removeItemAtPath:error:|removeItemAtPath:", [](Runtime& rt, u32, const ObjCArgs& a) {
        auto r = rt.resolve_path(rt.objc().get_str(a[0]));
        std::error_code ec;
        if (r.kind == Runtime::ResolvedPath::Kind::Docs && std::filesystem::exists(r.host_path, ec)) {
            std::filesystem::remove(r.host_path, ec);
            return ObjCRet::val(ec ? 0u : 1u);
        }
        return ObjCRet::val(0u);
    });
    reg.add_im("NSFileManager", "attributesOfFileSystemForPath:error:", [](Runtime& rt, u32, const ObjCArgs&) {
        ObjCDict d;
        d["s:NSFileSystemFreeSize"] = rt.objc().new_obj_with("NSNumber", i64(1LL << 30));
        return ObjCRet::val(rt.objc().new_obj_with("NSDictionary", std::move(d)));
    });

    // ---- NSDate / NSLocale / NSNull / NSThread --------------------------------------
    reg.add_cm("NSDate", "date", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().new_obj_with("NSDate", now_epoch_seconds()));
    });
    reg.add_im("NSDate", "timeIntervalSince1970|timeIntervalSinceReferenceDate",
               [](Runtime& rt, u32 recv, const ObjCArgs&) {
                   auto* t = rt.objc().get_payload<double>(recv);
                   u64 bits = f64_to_bits(t ? *t : now_epoch_seconds());
                   return ObjCRet::pair(u32(bits & 0xFFFFFFFFu), u32(bits >> 32));
               });
    reg.add_cm("NSLocale", "currentLocale|autoupdatingCurrentLocale", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().new_obj("NSLocale"));
    });
    reg.add_im("NSLocale", "objectForKey:", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().nsstring("US"));
    });
    reg.add_cm("NSLocale", "preferredLanguages", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().new_obj_with("NSArray", ObjCArray{rt.objc().nsstring("en")}));
    });
    reg.add_cm("NSNull", "null", [](Runtime& rt, u32, const ObjCArgs&) {
        return ObjCRet::val(rt.objc().new_obj("NSNull"));
    });
    reg.add_cm("NSThread", "isMainThread", [](Runtime&, u32, const ObjCArgs&) { return ObjCRet::val(1); });
    reg.add_cm("NSThread", "sleepForTimeInterval:", [](Runtime&, u32, const ObjCArgs&) { return ObjCRet::val(0); });
}

void register_hle_foundation_runtime_funcs(Runtime& rt, HleRegistry& reg) {
    reg.add_func("_NSLog", [&rt](Machine& m) {
        GuestArgReader rd(m, 1, false);
        m.log("NSLog: " + format_guest_c(rt, rt.objc().get_str(m.reg(0)), rd, true));
        return TrapResult::val(0u);
    });
    reg.add_func("_CFStringCreateWithCString", [&rt](Machine& m) {
        return TrapResult::val(rt.objc().nsstring(m.cstr(m.reg(1))));
    });
    reg.add_func("_CFStringGetCString", [&rt](Machine& m) {
        std::string s = rt.objc().get_str(m.reg(0));
        u32 cap = m.reg(2);
        if (s.size() + 1u > cap) return TrapResult::val(0u);
        Bytes b(s.begin(), s.end());
        b.push_back(0);
        m.wrbytes(m.reg(1), b);
        return TrapResult::val(1u);
    });
    reg.add_func("_CFStringAppendCharacters", [&rt](Machine& m) {
        u32 obj = m.reg(0), ptr = m.reg(1), count = m.reg(2);
        Bytes raw = m.rdbytes(ptr, size_t(count) * 2u);
        std::string add;
        for (size_t i = 0; i + 1 < raw.size(); i += 2) {
            u16 ch = read_u16_le(raw.data() + i);
            if (ch < 0x80u) add.push_back(static_cast<char>(ch));
            else if (ch < 0x800u) {
                add.push_back(static_cast<char>(0xC0u | (ch >> 6)));
                add.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
            } else {
                add.push_back(static_cast<char>(0xE0u | (ch >> 12)));
                add.push_back(static_cast<char>(0x80u | ((ch >> 6) & 0x3Fu)));
                add.push_back(static_cast<char>(0x80u | (ch & 0x3Fu)));
            }
        }
        rt.objc().set_payload(obj, rt.objc().get_str(obj) + add);
        return TrapResult::val(0u);
    });
    reg.add_func("_CFBundleGetMainBundle", [&rt](Machine&) {
        if (rt.cfbundle_obj == 0) rt.cfbundle_obj = rt.objc().new_obj("NSBundle");
        return TrapResult::val(rt.cfbundle_obj);
    });
    reg.add_func("_CFBundleCopyResourceURL", [&rt](Machine& m) {
        std::string name = trim_slashes(rt.objc().get_str(m.reg(1)));
        std::string ext = rt.objc().get_str(m.reg(2));
        std::string d = trim_slashes(rt.objc().get_str(m.reg(3)));
        std::string rel = (d.empty() ? "" : (d + "/")) + name + (ext.empty() ? "" : ("." + ext));
        for (const std::string& cand : {rel, name, "en.lproj/" + rel}) {
            if (!cand.empty() && rt.bundle().has_asset(cand)) {
                return TrapResult::val(rt.objc().new_obj_with("NSURL", std::string(kGuestAppPath) + "/" + cand));
            }
        }
        return TrapResult::val(0u);
    });
    reg.add_func("_CFURLCopyFileSystemPath", [&rt](Machine& m) {
        return TrapResult::val(rt.objc().nsstring(rt.objc().get_str(m.reg(0))));
    });
    reg.add_func("_NSHomeDirectory", [&rt](Machine&) {
        std::string docs(kGuestDocsPath);
        size_t pos = docs.rfind('/');
        return TrapResult::val(rt.objc().nsstring(pos == std::string::npos ? docs : docs.substr(0, pos)));
    });
    reg.add_func("_NSTemporaryDirectory", [&rt](Machine&) {
        std::string docs(kGuestDocsPath);
        size_t pos = docs.rfind('/');
        return TrapResult::val(rt.objc().nsstring((pos == std::string::npos ? docs : docs.substr(0, pos)) + "/tmp"));
    });
    reg.add_func("_NSSearchPathForDirectoriesInDomains", [&rt](Machine&) {
        return TrapResult::val(rt.objc().new_obj_with("NSArray", ObjCArray{rt.objc().nsstring(kGuestDocsPath)}));
    });
}

}  // namespace hl3
