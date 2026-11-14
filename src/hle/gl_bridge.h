#pragma once

#include "../core/common.h"
#include "../platform/display_config.h"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace hl3 {

class Machine;
class GLBridge;

struct Mat4 {
    std::array<float, 16> m{};
    static Mat4 identity();
    static Mat4 ortho(float l, float r, float b, float t, float n, float f);
    static Mat4 mul(const Mat4& a, const Mat4& b);
};

class WidescreenController {
public:
    WidescreenController(Machine& m, GLBridge* gl_bridge = nullptr);

    void install_hooks();
    void apply_layout(const ViewportLayout& layout);
    void reset_frame_state();

    int ui_dx() const { return ui_dx_; }
    int ui_dy() const { return ui_dy_; }

    static u32 encode_arm_rot_imm(u32 val);

private:
    struct PendingFlush {
        bool active = false;
        std::array<u32, 8> regs{};  // r0, r1, r2, r3, r12, sp, lr, cpsr
        u32 resume_pc = 0;
        int ox = 0;
        int oy = 0;
    };

    struct HudNodeState {
        i32 orig_x = 0;
        i32 orig_y = 0;
        float bcx = 0.0f;
        float bcy = 0.0f;
        int cur_dx = 0;
        int cur_dy = 0;
    };

    void set_gl_ui_offset(int ox, int oy);
    bool flush_then_set_offset(u32 resume_pc, u32 gfx, int ox, int oy);
    void write_code32(u32 va, u32 word);
    void patch_imm(u32 va, u32 val);
    void patch_mov_rd(u32 va, u32 rd, u32 val);
    void patch_tooltip_clamp(int patch_w);
    void sync_live_structs(const ViewportLayout& layout);
    void sync_hud_nodes(const ViewportLayout& layout);
    void walk_and_anchor_hud(u32 head_ptr, const ViewportLayout& layout);

    Machine& m_;
    GLBridge* gl_bridge_ = nullptr;
    int ui_dx_ = 0;
    int ui_dy_ = 0;
    bool in_ui_main_draw_ = false;
    PendingFlush pending_flush_;
    bool has_last_dims_ = false;
    int last_patch_w_ = 0;
    int last_patch_h_ = 0;
    float last_ortho_w_ = 0.0f;
    float last_ortho_h_ = 0.0f;
    std::unordered_map<u32, u32> orig_words_;
    bool dimmer_fixed_ = false;
    bool hooks_installed_ = false;
    u32 hud_form_ptr_ = 0;
    std::unordered_map<u32, HudNodeState> hud_nodes_;
};

class GLBridge {
public:
    GLBridge(Machine* m, int win_w = 960, int win_h = 640, const std::string& aspect_mode = kDefaultAspectMode);
    ~GLBridge();

    GLBridge(const GLBridge&) = delete;
    GLBridge& operator=(const GLBridge&) = delete;

    bool init_gl();
    void shutdown_gl();
    bool is_gl_ready() const { return gl_ready_; }

    // Viewport & layout
    void set_aspect_mode(const std::string& aspect_name, int widescreen_override = -1);  // -1 auto, 0 off, 1 on
    void set_world_active(bool world_active);
    void update_viewport(int win_w, int win_h);
    void sync_guest();
    void set_ui_offset(int ox, int oy);
    std::pair<int, int> ui_offset() const { return {ui_ox_, ui_oy_}; }

    std::pair<float, float> window_to_game_coord(int wx, int wy) const;

    int win_w() const { return win_w_; }
    int win_h() const { return win_h_; }
    float game_w() const { return game_w_; }
    float game_h() const { return game_h_; }
    const std::string& aspect_name() const { return aspect_name_; }
    bool true_widescreen() const { return true_widescreen_; }
    bool world_active() const { return world_active_; }
    const RectI& viewport_rect() const { return viewport_rect_; }
    const ViewportLayout& layout() const { return layout_; }
    WidescreenController* widescreen() { return widescreen_.get(); }

    void begin_frame();

    // Guest GLES 1.1 bridge
    std::vector<u32> gen_textures(int n);
    void bind_texture(u32 target, u32 tex_id);
    void delete_textures(const std::vector<u32>& tex_ids);
    void tex_image_2d(u32 target, int level, u32 ifmt, int w, int h, int border, u32 fmt, u32 tp, u32 pix_ptr);
    void tex_parameter_i(u32 target, u32 pname, int param);
    void tex_parameter_f(u32 target, u32 pname, float param);
    void tex_env_i(u32 target, u32 pname, int param);
    void matrix_mode(u32 mode);
    void load_identity();
    void push_matrix();
    void pop_matrix();
    void ortho_f(float l, float r, float b, float t, float n, float f);
    void enable(u32 cap);
    void disable(u32 cap);
    void enable_client_state(u32 cap);
    void disable_client_state(u32 cap);
    void blend_func(u32 sfactor, u32 dfactor);
    void alpha_func(u32 func, float ref);
    void clear_color(float r, float g, float b, float a);
    void clear(u32 mask);
    void scissor(int x, int y, int w, int h);
    void viewport(int x, int y, int w, int h);
    void vertex_pointer(int sz, u32 tp, int stride, u32 ptr);
    void tex_coord_pointer(int sz, u32 tp, int stride, u32 ptr);
    void color_pointer(int sz, u32 tp, int stride, u32 ptr);
    void draw_arrays(u32 mode, int first, int count);

    // Host overlay & texture utilities for UI/cursor/modal rendering
    u32 create_rgba_texture(int w, int h, const u8* rgba, bool linear_filter = true);
    void update_rgba_texture(u32 tex_id, int w, int h, const u8* rgba);
    void destroy_texture(u32 tex_id);

    void overlay_begin_game(float ortho_w, float ortho_h, bool in_viewport = true);
    void overlay_begin_window(int win_w, int win_h);
    void overlay_end();
    void overlay_set_scissor_topdown(int x, int y, int w, int h);  // Top-down window pixels
    void overlay_clear_scissor();

    void overlay_fill_rect(float x, float y, float w, float h, float r, float g, float b, float a);
    void overlay_outline_rect(float x, float y, float w, float h, float line_w, float r, float g, float b, float a);
    void overlay_triangle(float x0, float y0, float x1, float y1, float x2, float y2,
                          float r, float g, float b, float a);
    void overlay_textured_rect(u32 tex_id, float x, float y, float w, float h,
                               float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f,
                               float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

    bool read_framebuffer_rgba(int& out_w, int& out_h, Bytes& out_rgba) const;

private:
    struct Vertex {
        float x, y, z;
        float u, v;
        float r, g, b, a;
    };

    struct GlImpl;
    std::unique_ptr<GlImpl> impl_;

    void apply_alpha_state_uniforms();
    Mat4 current_mvp() const;
    void draw_vertices(u32 mode, const Vertex* verts, size_t count, bool use_tex, u32 tex_id);

    Machine* m_ = nullptr;
    int win_w_ = 960;
    int win_h_ = 640;
    float game_w_ = 480.0f;
    float game_h_ = 320.0f;
    std::string aspect_name_ = kDefaultAspectMode;
    int widescreen_override_ = -1;
    bool true_widescreen_ = true;
    bool world_active_ = true;
    RectI viewport_rect_{0, 0, 960, 640};
    ViewportLayout layout_;
    std::unique_ptr<WidescreenController> widescreen_;

    int ui_ox_ = 0;
    int ui_oy_ = 0;
    bool has_last_ortho_ = false;
    float last_ortho_[6]{};

    int v_size_ = 2;
    u32 v_type_ = 0x1406u;
    int v_stride_ = 0;
    u32 v_ptr_ = 0;

    int t_size_ = 2;
    u32 t_type_ = 0x1406u;
    int t_stride_ = 0;
    u32 t_ptr_ = 0;

    int c_size_ = 4;
    u32 c_type_ = 0x1401u;
    int c_stride_ = 0;
    u32 c_ptr_ = 0;

    bool client_vert_ = false;
    bool client_tex_ = false;
    bool client_col_ = false;

    u32 cur_matrix_mode_ = 0x1700u;  // GL_MODELVIEW
    std::vector<Mat4> mv_stack_;
    std::vector<Mat4> proj_stack_;

    u32 bound_tex_ = 0;
    std::unordered_map<u32, std::unordered_map<u32, int>> tex_params_;
    std::unordered_map<u32, bool> enabled_caps_;
    u32 blend_s_ = 1;
    u32 blend_d_ = 0;
    u32 alpha_func_ = 0x0207u;  // GL_ALWAYS
    float alpha_ref_ = 0.0f;
    bool tex_replace_ = false;
    bool gl_ready_ = false;

    std::vector<Vertex> scratch_verts_;
    Bytes scratch_raw_;
};

}  // namespace hl3
