#pragma once

#include "common.h"
#include "macho_loader.h"

#include <unicorn/unicorn.h>

#include <array>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace hl3 {

inline constexpr u32 kImageBase = 0x00001000u;
inline constexpr u32 kImageEnd = 0x000EA000u;
inline constexpr u32 kHeapBase = 0x10000000u;
inline constexpr u32 kHeapSize = 0x08000000u;  // 128 MB
inline constexpr u32 kClassBase = 0x38000000u;
inline constexpr u32 kClassSize = 0x00400000u;  // 4 MB
inline constexpr u32 kTrapBase = 0x7F000000u;
inline constexpr u32 kTrapSize = 0x00100000u;  // 1 MB
inline constexpr u32 kRetStub = 0x7F100000u;
inline constexpr u32 kHostStop = 0x7F100010u;
inline constexpr u32 kUiFlushReturnStub = 0x7F100020u;
inline constexpr u32 kNativeStubBase = 0x7F100100u;
inline constexpr u32 kSjljTopSlot = 0x7F100800u;
inline constexpr u32 kStackBase = 0xBFF00000u;
inline constexpr u32 kStackTop = 0xBFFFF000u;
inline constexpr u32 kStackMapEnd = 0xC0000000u;

class GuestFault : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Machine;
class ArmFastCpu;

struct TrapResult {
    bool has_value = false;
    bool two_regs = false;
    u32 r0 = 0;
    u32 r1 = 0;

    static TrapResult none() { return {}; }
    static TrapResult val(u32 v) { return {true, false, v, 0}; }
    static TrapResult pair(u32 a, u32 b) { return {true, true, a, b}; }
};

using TrapFn = std::function<TrapResult(Machine&)>;
using CodeHookFn = std::function<void(Machine&, u32 addr, u32 size)>;

struct ResolvedSymbol {
    enum class Kind { Func, Data };
    Kind kind = Kind::Func;
    TrapFn handler;
    u32 data_addr = 0;
};

using SymbolResolver = std::function<ResolvedSymbol(const std::string& name)>;

// Persistent sleeping watchdog per Unicorn engine (avoids per-call thread creation churn).
class EmulationWatchdog {
public:
    explicit EmulationWatchdog(uc_engine* engine);
    ~EmulationWatchdog();

    EmulationWatchdog(const EmulationWatchdog&) = delete;
    EmulationWatchdog& operator=(const EmulationWatchdog&) = delete;

    void arm(u64 microseconds);
    bool disarm();

private:
    void worker();

    uc_engine* engine_ = nullptr;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread thread_;
    bool closing_ = false;
    bool armed_ = false;
    bool expired_ = false;
    std::chrono::steady_clock::time_point deadline_{};
};

class Machine {
public:
    struct MemRegion {
        u32 base = 0;
        u32 end = 0;
        u8* host = nullptr;
        size_t size = 0;
    };

    explicit Machine(bool native_fast_traps = true);
    ~Machine();

    Machine(const Machine&) = delete;
    Machine& operator=(const Machine&) = delete;

    uc_engine* uc() const { return uc_; }
    const MachOImage& image() const { return image_; }

    // Direct host backing pointers for mapped guest regions
    const u8* guest_rptr(u32 addr, size_t len) const {
        for (const auto& r : regions_) {
            if (addr >= r.base && addr < r.end && len <= static_cast<size_t>(r.end - addr)) {
                return r.host + (addr - r.base);
            }
        }
        return nullptr;
    }
    u8* guest_wptr(u32 addr, size_t len) {
        for (const auto& r : regions_) {
            if (addr >= r.base && addr < r.end && len <= static_cast<size_t>(r.end - addr)) {
                return r.host + (addr - r.base);
            }
        }
        return nullptr;
    }
    const char* guest_cstr_ptr(u32 addr, size_t* max_avail = nullptr) const {
        for (const auto& r : regions_) {
            if (addr >= r.base && addr < r.end) {
                if (max_avail) *max_avail = static_cast<size_t>(r.end - addr);
                return reinterpret_cast<const char*>(r.host + (addr - r.base));
            }
        }
        if (max_avail) *max_avail = 0;
        return nullptr;
    }

    // Memory access (fast inline host pointer path with strict bounds checks)
    u32 rd32(u32 addr) const {
        if (const u8* p = guest_rptr(addr, 4)) return read_u32_le(p);
        throw GuestFault("rd32 failed at " + hex32(addr));
    }
    u16 rd16(u32 addr) const {
        if (const u8* p = guest_rptr(addr, 2)) return read_u16_le(p);
        throw GuestFault("rd16 failed at " + hex32(addr));
    }
    i32 rds16(u32 addr) const {
        u16 v = rd16(addr);
        return (v & 0x8000u) ? i32(u32(v) | 0xFFFF0000u) : i32(v);
    }
    u8 rd8(u32 addr) const {
        if (const u8* p = guest_rptr(addr, 1)) return *p;
        throw GuestFault("rd8 failed at " + hex32(addr));
    }
    i32 rds8(u32 addr) const {
        u8 v = rd8(addr);
        return (v & 0x80u) ? i32(u32(v) | 0xFFFFFF00u) : i32(v);
    }
    float rdf32(u32 addr) const { return bits_to_f32(rd32(addr)); }
    Bytes rdbytes(u32 addr, size_t len) const;
    bool rdbytes(u32 addr, size_t len, void* dst) const;
    std::string cstr(u32 addr) const;

    void wr32(u32 addr, u32 val) {
        if (u8* p = guest_wptr(addr, 4)) {
            write_u32_le(p, val);
            return;
        }
        throw GuestFault("wr32 failed at " + hex32(addr));
    }
    void wr16(u32 addr, u16 val) {
        if (u8* p = guest_wptr(addr, 2)) {
            write_u16_le(p, val);
            return;
        }
        throw GuestFault("wr16 failed at " + hex32(addr));
    }
    void wr8(u32 addr, u8 val) {
        if (u8* p = guest_wptr(addr, 1)) {
            *p = val;
            return;
        }
        throw GuestFault("wr8 failed at " + hex32(addr));
    }
    void wrf32(u32 addr, float val) { wr32(addr, f32_to_bits(val)); }
    void wrbytes(u32 addr, const void* data, size_t len);
    void wrbytes(u32 addr, const Bytes& data) { wrbytes(addr, data.data(), data.size()); }

    // Heap / Class allocators
    u32 malloc_bytes(u32 size);
    u32 calloc_bytes(u32 count, u32 elem_size);
    u32 realloc_bytes(u32 ptr, u32 new_size);
    void free_bytes(u32 ptr);
    u32 new_cstr(const std::string& text);
    u32 class_alloc(u32 size);
    u32 alloc_size(u32 addr) const;
    u32 heap_used() const { return heap_ptr_ - (kHeapBase + 0x1000u); }

    // ARM Registers
    u32 reg(int n) const;
    void set_reg(int n, u32 val);
    u32 sp() const;
    void set_sp(u32 val);
    u32 lr() const;
    void set_lr(u32 val);
    u32 pc() const;
    void set_pc(u32 val);
    u32 cpsr() const;
    void set_cpsr(u32 val);
    u32 arg(int n) const;

    // Image & Trap installation
    bool load_image(MachOImage img, std::string* err = nullptr);
    void install_traps(const SymbolResolver& resolver);
    void forward_to(u32 trap_addr_for_slot, u32 target);
    void reset_forward(u32 trap_addr_for_slot);

    u32 import_addr(const std::string& name) const;
    bool has_import_addr(const std::string& name) const;
    u32 data_symbol(const std::string& name) const;

    // Host -> Guest execution
    u32 call(u32 addr, const std::vector<u32>& args = {}, u32 sp_override = 0, u64 max_insn = 0,
             u64 timeout_us = 0);
    void request_stop();
    bool last_timed_out() const { return last_timed_out_; }

    // Code hook registration
    void add_code_hook(u32 begin, u32 end, CodeHookFn fn);
    void remove_all_user_hooks();

    // Fast ARMv6 CPU executor & cache invalidation
    void set_fast_cpu(bool enabled);
    bool fast_cpu_enabled() const { return use_fast_cpu_; }
    ArmFastCpu* fast_cpu() { return fast_cpu_.get(); }
    const ArmFastCpu* fast_cpu() const { return fast_cpu_.get(); }
    void invalidate_code_word(u32 va);

    // Diagnostics & stats
    void set_phase(const std::string& p);
    const std::string& phase() const { return phase_; }
    void record_unimplemented(const std::string& key);
    void log(const std::string& msg);
    void set_trace(bool on) { trace_enabled_ = on; }
    bool trace_enabled() const { return trace_enabled_; }
    std::unordered_map<std::string, u64> trap_counts(const std::string& p) const;
    const std::unordered_map<std::string, u64>& unimplemented_counts() const { return unimplemented_; }

    std::string stop_reason;

private:
    struct CodeHookEntry {
        uc_hook handle = 0;
        u32 begin = 0;
        u32 end = 0;
        CodeHookFn fn;
        Machine* machine = nullptr;
    };

    friend class ArmFastCpu;

    u32 dispatch_fast_trap(u32 trap_addr);
    bool has_user_hook_at(u32 addr) const;
    void run_user_hooks_at(u32 addr, u32 size);
    void set_in_unicorn_fallback(bool v) { in_unicorn_fallback_ = v; }
    bool use_fast_regs() const { return use_fast_cpu_ && fast_cpu_ && !in_unicorn_fallback_; }

    void map_memory();
    void free_memory();
    void install_native_fast_stubs();
    void enable_vfp();

    static void on_trap_hook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data);
    static bool on_fault_hook(uc_engine* uc, uc_mem_type type, uint64_t address, int size, int64_t value,
                              void* user_data);
    static void on_user_code_hook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data);

    uc_engine* uc_ = nullptr;
    EmulationWatchdog* watchdog_ = nullptr;
    std::unique_ptr<ArmFastCpu> fast_cpu_;
#if defined(HL3_USE_FAST_CPU)
    bool use_fast_cpu_ = true;
#else
    bool use_fast_cpu_ = false;
#endif
    bool in_unicorn_fallback_ = false;
    bool native_fast_traps_ = true;
    MachOImage image_;
    std::array<MemRegion, 6> regions_{};

    u32 heap_ptr_ = kHeapBase + 0x1000u;
    std::unordered_map<u32, u32> alloc_sizes_;
    std::map<u32, std::vector<u32>> free_bins_;
    u32 class_ptr_ = kClassBase;

    std::vector<std::string> trap_names_;
    std::vector<TrapFn> trap_handlers_;
    std::unordered_map<std::string, u32> native_stubs_;
    std::unordered_map<std::string, u32> import_addrs_;
    std::unordered_map<std::string, u32> data_symbols_;

    std::vector<std::unique_ptr<CodeHookEntry>> user_hooks_;
    uc_hook trap_hook_ = 0;
    uc_hook fault_hook_ = 0;

    std::string phase_ = "boot";
    std::unordered_map<std::string, std::vector<u64>> phase_trap_counts_;
    std::vector<u64>* active_trap_counts_ = nullptr;
    std::unordered_map<std::string, u64> unimplemented_;
    std::vector<std::string> log_lines_;
    bool trace_enabled_ = false;
    bool last_timed_out_ = false;
};

}  // namespace hl3
