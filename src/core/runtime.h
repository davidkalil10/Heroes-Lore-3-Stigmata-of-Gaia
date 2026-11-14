#pragma once

#include "bundle.h"
#include "common.h"
#include "machine.h"
#include "objc.h"

#include <any>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hl3 {

inline constexpr const char* kGuestAppPath = "/var/mobile/Applications/HL3/Heroes Lore.app";
inline constexpr const char* kGuestDocsPath = "/var/mobile/Applications/HL3/Documents";

class GLBridge;
class AudioMixer;
class WidescreenController;

struct PendingAppMain {
    std::string principal;
    std::string delegate;
};

struct TouchPoint {
    float x = 0.0f;
    float y = 0.0f;
    float prev_x = 0.0f;
    float prev_y = 0.0f;
    u32 phase = 0;
    double timestamp = 0.0;
};
struct RuntimeTimer {
    u32 obj = 0;
    double interval = 1.0 / 60.0;
    u32 target = 0;
    std::string sel;
    u32 user_info = 0;
    bool repeats = false;
    bool valid = true;
};

struct VirtualFile {
    std::string path;
    std::string mode;
    Bytes data;
    std::filesystem::path host_path;
    bool has_host_path = false;
    size_t pos = 0;
    bool dirty = false;
};

class Runtime {
public:
    Runtime();
    ~Runtime();

    bool init(const std::filesystem::path& root_or_ipa = {}, std::filesystem::path host_docs_override = {},
              bool trace = false, std::string* err = nullptr);

    void boot_to_app_main();
    void run_launch();

    u32 sel(const std::string& name);
    u32 send(u32 recv, const std::string& selector, const std::vector<u32>& args = {},
             const char* phase_override = nullptr);

    struct ResolvedPath {
        enum class Kind { None, Bundle, Docs };
        Kind kind = Kind::None;
        std::string bundle_rel;
        std::filesystem::path host_path;
    };

    ResolvedPath resolve_path(const std::string& path) const;
    bool read_guest_file(const std::string& path, Bytes& out) const;

    Bundle& bundle() { return bundle_; }
    const Bundle& bundle() const { return bundle_; }
    Machine& machine() { return *m_; }
    const Machine& machine() const { return *m_; }
    ObjCRuntime& objc() { return *objc_; }
    const ObjCRuntime& objc() const { return *objc_; }

    const std::filesystem::path& host_docs() const { return host_docs_; }
    void set_host_docs(std::filesystem::path p) { host_docs_ = std::move(p); }
    void flush_open_files();

    bool has_pending_app_main() const { return has_pending_app_main_; }
    const PendingAppMain& pending_app_main() const { return pending_app_main_; }
    void set_pending_app_main(PendingAppMain info);

    u32 app_obj() const { return app_obj_; }
    u32 delegate_obj() const { return delegate_obj_; }
    u32 host_sp() const { return host_sp_; }

    // Subsystems registered during Runtime init
    void set_gl_bridge(GLBridge* gl) { gl_ = gl; }
    GLBridge* gl_bridge() const { return gl_; }
    void set_audio_mixer(AudioMixer* am) { audio_ = am; }
    AudioMixer* audio_mixer() const { return audio_; }

    // Shared runtime state containers for Foundation/UIKit/libc HLE
    std::vector<RuntimeTimer> timers;
    std::unordered_map<u32, std::unique_ptr<VirtualFile>> files;
    std::vector<u32> windows;
    u32 main_gl_view = 0;
    u32 main_window = 0;
    u32 touch_responder = 0;
    u32 eagl_context = 0;
    bool active = true;
    int exit_code = 0;
    bool exited = false;
    u32 sjlj_top = 0;
    std::function<void()> on_before_runloop_stop;

    // Key-value user defaults / misc host state
    u32 orientation = 3;  // UIInterfaceOrientationLandscapeRight
    u32 frames_presented = 0;
    u32 gl_context = 0;
    u32 screen_obj = 0;
    u32 device_obj = 0;
    u32 main_bundle_obj = 0;
    u32 cfbundle_obj = 0;
    u32 file_manager_obj = 0;
    u32 user_defaults_obj = 0;
    u32 info_dict_obj = 0;
    u32 mutations_dummy = 0;
    std::function<void()> present_hook;
    std::unordered_map<std::string, std::any> user_defaults;
    std::unordered_map<std::string, std::any> kv_state;

private:
    ResolvedSymbol resolve_import(const std::string& name);
    u32 setup_main_stack();
    void run_mod_inits();

    Bundle bundle_;
    std::filesystem::path host_docs_;
    std::unique_ptr<Machine> m_;
    HleRegistry registry_;
    std::unique_ptr<ObjCRuntime> objc_;

    GLBridge* gl_ = nullptr;
    AudioMixer* audio_ = nullptr;

    PendingAppMain pending_app_main_;
    bool has_pending_app_main_ = false;
    u32 app_obj_ = 0;
    u32 delegate_obj_ = 0;
    u32 host_sp_ = kStackTop - 0x400u;
    std::unordered_map<std::string, u32> sel_cache_;
};

}  // namespace hl3
