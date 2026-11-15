#pragma once

#include "common.h"
#include "machine.h"

#include <any>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hl3 {

class Runtime;

class ObjCArgs {
public:
    ObjCArgs(Machine& m, int first_word) : m_(m), first_(first_word) {}

    u32 operator[](int i) const { return m_.arg(first_ + i); }
    float f32(int i) const { return bits_to_f32((*this)[i]); }
    double f64(int i) const {
        u64 lo = (*this)[i];
        u64 hi = (*this)[i + 1];
        return bits_to_f64(lo | (hi << 32));
    }

private:
    Machine& m_;
    int first_;
};

struct ObjCRet {
    bool is_pair = false;
    bool is_bytes = false;
    u32 r0 = 0;
    u32 r1 = 0;
    Bytes bytes;

    static ObjCRet none() { return {}; }
    static ObjCRet val(u32 v) {
        ObjCRet r;
        r.r0 = v;
        return r;
    }
    static ObjCRet pair(u32 a, u32 b) {
        ObjCRet r;
        r.is_pair = true;
        r.r0 = a;
        r.r1 = b;
        return r;
    }
    static ObjCRet stret_bytes(Bytes b) {
        ObjCRet r;
        r.is_bytes = true;
        r.bytes = std::move(b);
        return r;
    }
};

using MethodFn = std::function<ObjCRet(Runtime&, u32 recv, const ObjCArgs&)>;

struct HMethodEntry {
    bool is_guest = false;
    u32 guest_imp = 0;
    MethodFn host_fn;
};

struct HClass {
    std::string name;
    u32 addr = 0;
    u32 meta = 0;
    HClass* sup = nullptr;
    u32 instance_size = 4;
    bool from_image = false;
    std::unordered_map<std::string, HMethodEntry> imethods;
    std::unordered_map<std::string, HMethodEntry> cmethods;
    std::unordered_map<std::string, u32> ivars;
};

struct HleRegistry {
    std::unordered_map<std::string, MethodFn> imethods;  // key: "Class|sel"
    std::unordered_map<std::string, MethodFn> cmethods;  // key: "Class|sel"
    std::unordered_map<std::string, TrapFn> funcs;
    std::unordered_map<std::string, std::string> data_strings;
    std::unordered_map<std::string, u32> data_zero;

    void add_im(const std::string& cls, const std::string& sels, MethodFn fn);
    void add_cm(const std::string& cls, const std::string& sels, MethodFn fn);
    void add_func(const std::string& names, TrapFn fn);
};

class ObjCRuntime {
public:
    explicit ObjCRuntime(Machine& m, const HleRegistry& reg);

    void register_image_classes();
    HClass* hle_class(const std::string& name);
    HClass* find_class_by_name(const std::string& name) const;
    HClass* find_class_by_addr(u32 addr) const;
    bool try_class_for_symbol(const std::string& sym, u32& out_addr);

    u32 alloc_instance(HClass* cls);
    u32 new_obj(const std::string& clsname);
    template <typename T>
    u32 new_obj_with(const std::string& clsname, T&& payload) {
        u32 addr = new_obj(clsname);
        hobj_[addr] = std::any(std::forward<T>(payload));
        return addr;
    }

    template <typename T>
    T* get_payload(u32 obj) {
        auto it = hobj_.find(obj);
        if (it == hobj_.end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    template <typename T>
    void set_payload(u32 obj, T&& payload) {
        hobj_[obj] = std::any(std::forward<T>(payload));
    }

    bool has_payload(u32 obj) const { return hobj_.find(obj) != hobj_.end(); }
    void erase_payload(u32 obj) { hobj_.erase(obj); }

    std::pair<HClass*, bool> class_of(u32 obj) const;
    const HMethodEntry* lookup(HClass* cls, const std::string& sel, bool is_class) const;
    std::string sel_name(u32 ptr);

    // Returns empty string if nil or non-string; caller can check has_str().
    bool try_get_str(u32 obj, std::string& out) const;
    std::string get_str(u32 obj) const;
    u32 nsstring(const std::string& text);

    void msg_send(Runtime& rt, bool stret = false, bool super_ = false);
    void set_msg_trace(bool on) { msg_trace_ = on; }

private:
    struct DispatchCacheEntry {
        u32 isa = 0;
        u32 sel_ptr = 0;
        HClass* cls = nullptr;
        bool is_class = false;
        const HMethodEntry* method = nullptr;
    };

    const std::string& sel_name_ref(u32 ptr);
    u32 resolve_send_slot(bool stret, bool super_);

    Machine& m_;
    const HleRegistry& reg_;
    std::unordered_map<std::string, std::unique_ptr<HClass>> owned_;
    std::unordered_map<std::string, HClass*> by_name_;
    std::unordered_map<u32, HClass*> by_addr_;
    std::unordered_map<u32, HClass*> by_meta_;
    std::unordered_map<u32, std::any> hobj_;
    std::unordered_map<u32, std::string> sel_cache_;
    std::unordered_set<std::string> logged_unimpl_;
    std::vector<DispatchCacheEntry> dispatch_cache_;
    u32 slot_msg_send_ = 0xFFFFFFFFu;
    u32 slot_msg_send_stret_ = 0xFFFFFFFFu;
    u32 slot_msg_send_super2_ = 0xFFFFFFFFu;
    bool msg_trace_ = false;
};

}  // namespace hl3

