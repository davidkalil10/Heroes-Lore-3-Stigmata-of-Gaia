#include "runtime.h"

#include <cstring>

namespace hl3 {

// Forward declarations for HLE module installers (linked incrementally).
void register_hle_libc(Runtime& rt, HleRegistry& reg);
void register_hle_foundation(Runtime& rt, HleRegistry& reg);
void register_hle_uikit(Runtime& rt, HleRegistry& reg);
void register_hle_gl(Runtime& rt, HleRegistry& reg);
void register_hle_audio(Runtime& rt, HleRegistry& reg);

namespace {

const std::unordered_map<std::string, u32>& data_zero_sizes() {
    static const std::unordered_map<std::string, u32> kZero = {
        {"__objc_empty_cache", 16u},
        {"__objc_empty_vtable", 16u},
        {"___stderrp", 4u},
        {"_CGRectZero", 16u},
        {"__DefaultRuneLocale", 0x2000u},
    };
    return kZero;
}

std::string lstrip_slashes(std::string s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == '/' || s[i] == '\\')) ++i;
    return s.substr(i);
}

}  // namespace

Runtime::Runtime() = default;
Runtime::~Runtime() = default;

bool Runtime::init(const std::filesystem::path& root_or_ipa, std::filesystem::path host_docs_override, bool trace,
                   std::string* err) {
    if (!bundle_.open(root_or_ipa, err)) {
        return false;
    }
    if (!host_docs_override.empty()) {
        host_docs_ = std::move(host_docs_override);
    } else {
        host_docs_ = find_writable_data_root() / "data" / "internal" / "Documents";
    }

    MachOImage img;
    if (!parse_macho_image(bundle_.macho_bytes(), img, err)) {
        return false;
    }

    m_ = std::make_unique<Machine>(true);
    m_->set_trace(trace);
    if (!m_->load_image(std::move(img), err)) {
        return false;
    }

    registry_ = HleRegistry{};
    register_hle_libc(*this, registry_);
    register_hle_foundation(*this, registry_);
    register_hle_uikit(*this, registry_);
    register_hle_gl(*this, registry_);
    register_hle_audio(*this, registry_);

    objc_ = std::make_unique<ObjCRuntime>(*m_, registry_);
    objc_->register_image_classes();

    m_->install_traps([this](const std::string& sym) { return resolve_import(sym); });
    return true;
}

ResolvedSymbol Runtime::resolve_import(const std::string& name) {
    u32 cls_addr = 0;
    if (objc_->try_class_for_symbol(name, cls_addr)) {
        return {ResolvedSymbol::Kind::Data, {}, cls_addr};
    }
    if (name == "___CFConstantStringClassReference") {
        return {ResolvedSymbol::Kind::Data, {}, objc_->hle_class("NSConstantString")->addr};
    }
    if (auto it = registry_.data_strings.find(name); it != registry_.data_strings.end()) {
        u32 cell = m_->malloc_bytes(8);
        m_->wr32(cell, objc_->nsstring(it->second));
        return {ResolvedSymbol::Kind::Data, {}, cell};
    }
    const auto& zmap = data_zero_sizes();
    if (auto it = zmap.find(name); it != zmap.end()) {
        return {ResolvedSymbol::Kind::Data, {}, m_->malloc_bytes(it->second)};
    }
    if (name.rfind("__ZTV", 0) == 0 || name.rfind("__ZTI", 0) == 0) {
        return {ResolvedSymbol::Kind::Data, {}, m_->malloc_bytes(64)};
    }
    if (name == "_objc_msgSend") {
        return {ResolvedSymbol::Kind::Func,
                [this](Machine&) {
                    objc_->msg_send(*this, false, false);
                    return TrapResult::none();
                },
                0};
    }
    if (name == "_objc_msgSend_stret") {
        return {ResolvedSymbol::Kind::Func,
                [this](Machine&) {
                    objc_->msg_send(*this, true, false);
                    return TrapResult::none();
                },
                0};
    }
    if (name == "_objc_msgSendSuper2") {
        return {ResolvedSymbol::Kind::Func,
                [this](Machine&) {
                    objc_->msg_send(*this, false, true);
                    return TrapResult::none();
                },
                0};
    }
    if (auto it = registry_.funcs.find(name); it != registry_.funcs.end()) {
        return {ResolvedSymbol::Kind::Func, it->second, 0};
    }
    if (name.rfind("_gl", 0) == 0 || name.rfind("_al", 0) == 0 || name.rfind("_AudioSession", 0) == 0) {
        return {ResolvedSymbol::Kind::Func, [](Machine&) { return TrapResult::val(0); }, 0};
    }

    return {ResolvedSymbol::Kind::Func,
            [name](Machine& m) {
                m.record_unimplemented(name);
                if (m.unimplemented_counts().at(name) == 1u) {
                    m.log("UNIMPL import " + name + " from lr=" + hex32(m.lr()));
                }
                return TrapResult::val(0);
            },
            0};
}

Runtime::ResolvedPath Runtime::resolve_path(const std::string& path) const {
    if (path.rfind(kGuestAppPath, 0) == 0) {
        return {ResolvedPath::Kind::Bundle, lstrip_slashes(path.substr(std::strlen(kGuestAppPath))), {}};
    }
    if (path.rfind(kGuestDocsPath, 0) == 0) {
        std::string rel = lstrip_slashes(path.substr(std::strlen(kGuestDocsPath)));
        return {ResolvedPath::Kind::Docs, {}, rel.empty() ? host_docs_ : (host_docs_ / std::filesystem::u8path(rel))};
    }
    static constexpr const char* kAppTmpPrefix = "/var/mobile/Applications/HL3/tmp";
    if (path.rfind("/tmp", 0) == 0 || path.rfind(kAppTmpPrefix, 0) == 0) {
        size_t pos = path.find("/tmp");
        std::string rel = lstrip_slashes(path.substr(pos + 4));
        return {ResolvedPath::Kind::Docs, {}, host_docs_ / "tmp" / std::filesystem::u8path(rel)};
    }
    return {};
}

bool Runtime::read_guest_file(const std::string& path, Bytes& out) const {
    auto r = resolve_path(path);
    if (r.kind == ResolvedPath::Kind::Bundle) {
        return bundle_.read_asset(r.bundle_rel, out);
    }
    if (r.kind == ResolvedPath::Kind::Docs) {
        return read_file_bytes(r.host_path, out);
    }
    return false;
}

void Runtime::set_pending_app_main(PendingAppMain info) {
    pending_app_main_ = std::move(info);
    has_pending_app_main_ = true;
}

u32 Runtime::setup_main_stack() {
    u32 sp = kStackTop - 0x400u;
    u32 arg0 = m_->new_cstr(std::string(kGuestAppPath) + "/Heroes Lore");
    u32 apple = m_->new_cstr(std::string(kGuestAppPath) + "/Heroes Lore");
    const u32 words[6] = {1u, arg0, 0u, 0u, apple, 0u};
    sp -= static_cast<u32>(sizeof(words));
    sp &= ~7u;
    for (u32 i = 0; i < 6; ++i) {
        m_->wr32(sp + i * 4u, words[i]);
    }
    return sp;
}

void Runtime::run_mod_inits() {
    u32 sp = kStackTop - 0x2000u;
    for (u32 fn : m_->image().mod_init_funcs) {
        m_->call(fn, {}, sp);
    }
}

void Runtime::boot_to_app_main() {
    run_mod_inits();
    u32 sp = setup_main_stack();
    m_->set_phase("boot");
    m_->stop_reason.clear();
    has_pending_app_main_ = false;

    try {
        m_->call(m_->image().entry_pc, {}, sp);
    } catch (const GuestFault& exc) {
        if (!has_pending_app_main_) {
            throw GuestFault(std::string("boot failed: ") + exc.what());
        }
    }
    if (!has_pending_app_main_) {
        throw GuestFault("boot ended without UIApplicationMain: " + m_->stop_reason);
    }
}

u32 Runtime::sel(const std::string& name) {
    if (auto it = sel_cache_.find(name); it != sel_cache_.end()) {
        return it->second;
    }
    u32 ptr = 0;
    for (const auto& ref : m_->image().objc_selrefs) {
        if (ref.selector == name) {
            ptr = m_->rd32(ref.slot);
            break;
        }
    }
    if (ptr == 0) {
        ptr = m_->new_cstr(name);
    }
    sel_cache_[name] = ptr;
    return ptr;
}

u32 Runtime::send(u32 recv, const std::string& selector, const std::vector<u32>& args, const char* phase_override) {
    std::string prev_phase;
    if (phase_override) {
        prev_phase = m_->phase();
        m_->set_phase(phase_override);
    }
    auto restore_phase = [&]() {
        if (phase_override) m_->set_phase(prev_phase);
    };

    auto [cls, is_class] = objc_->class_of(recv);
    const HMethodEntry* target = cls ? objc_->lookup(cls, selector, is_class) : nullptr;
    if (!target) {
        restore_phase();
        return 0;
    }
    if (target->is_guest) {
        std::vector<u32> full_args;
        full_args.reserve(2 + args.size());
        full_args.push_back(recv);
        full_args.push_back(sel(selector));
        full_args.insert(full_args.end(), args.begin(), args.end());
        u32 res = m_->call(target->guest_imp, full_args, host_sp_);
        restore_phase();
        return res;
    }
    // Host-implemented ObjC method called directly via send()
    u32 base_sp = host_sp_;
    std::vector<u32> words = args;
    u32 sp_after = base_sp;
    if (words.size() > 2) {
        u32 extra = static_cast<u32>(words.size() - 2);
        sp_after = (base_sp - 4u * extra) & ~7u;
        for (u32 i = 0; i < extra; ++i) {
            m_->wr32(sp_after + 4u * i, words[2 + i]);
        }
    }
    m_->set_sp(sp_after);
    m_->set_reg(0, recv);
    m_->set_reg(1, sel(selector));
    if (words.size() > 0) m_->set_reg(2, words[0]);
    if (words.size() > 1) m_->set_reg(3, words[1]);
    ObjCArgs oa(*m_, 2);
    ObjCRet r = target->host_fn(*this, recv, oa);
    restore_phase();
    return r.r0;
}

void Runtime::run_launch() {
    host_sp_ = (m_->sp() - 0x200u) & ~7u;
    app_obj_ = objc_->new_obj("UIApplication");
    HClass* del_cls = objc_->find_class_by_name(pending_app_main_.delegate);
    if (!del_cls) {
        throw GuestFault("Delegate class not found: " + pending_app_main_.delegate);
    }
    delegate_obj_ = objc_->alloc_instance(del_cls);
    m_->set_phase("launch");

    u32 opts = objc_->new_obj_with("NSDictionary", std::unordered_map<std::string, u32>{});
    auto [dcls, is_c] = objc_->class_of(delegate_obj_);
    if (objc_->lookup(dcls, "application:didFinishLaunchingWithOptions:", false)) {
        send(delegate_obj_, "application:didFinishLaunchingWithOptions:", {app_obj_, opts});
    } else {
        send(delegate_obj_, "applicationDidFinishLaunching:", {app_obj_});
    }
    send(delegate_obj_, "applicationDidBecomeActive:", {app_obj_});
}

void Runtime::flush_open_files() {
    for (auto& [handle, vf] : files) {
        (void)handle;
        if (vf && vf->dirty && vf->has_host_path) {
            if (write_file_bytes(vf->host_path, vf->data.data(), vf->data.size())) {
                vf->dirty = false;
            }
        }
    }
}

}  // namespace hl3
