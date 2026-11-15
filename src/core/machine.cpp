#include "machine.h"
#include "arm_fast_cpu.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__SWITCH__)
#include <malloc.h>
#else
#include <sys/mman.h>
#endif

namespace hl3 {

namespace {

constexpr u32 kArmLdrPcMinus4 = 0xE51FF004u;
constexpr u32 kArmBxLr = 0xE12FFF1Eu;

u8* alloc_zero_4k(size_t size) {
    if (size == 0) return nullptr;
#ifdef _WIN32
    void* p = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) throw std::bad_alloc();
    return static_cast<u8*>(p);
#elif defined(__SWITCH__)
    void* p = memalign(4096u, size);
    if (!p) throw std::bad_alloc();
    std::memset(p, 0, size);
    return static_cast<u8*>(p);
#else
    void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) throw std::bad_alloc();
    return static_cast<u8*>(p);
#endif
}

void free_4k(u8* p, size_t size) {
    if (!p) return;
#ifdef _WIN32
    (void)size;
    VirtualFree(p, 0, MEM_RELEASE);
#elif defined(__SWITCH__)
    (void)size;
    std::free(p);
#else
    munmap(p, size);
#endif
}

u32 b_cond(u32 cond, u32 from_addr, u32 to_addr, bool link) {
    i32 offset = static_cast<i32>(to_addr - (from_addr + 8u)) >> 2;
    return ((cond & 0xFu) << 28) | (0x5u << 25) | ((link ? 1u : 0u) << 24) | (static_cast<u32>(offset) & 0xFFFFFFu);
}

}  // namespace

EmulationWatchdog::EmulationWatchdog(uc_engine* engine) : engine_(engine) {
    thread_ = std::thread([this]() { worker(); });
}

EmulationWatchdog::~EmulationWatchdog() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closing_ = true;
    }
    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void EmulationWatchdog::worker() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!closing_) {
        if (!armed_) {
            cv_.wait(lock);
            continue;
        }
        auto now = std::chrono::steady_clock::now();
        if (now < deadline_) {
            cv_.wait_until(lock, deadline_);
            continue;
        }
        expired_ = true;
        uc_emu_stop(engine_);
        cv_.wait_for(lock, std::chrono::milliseconds(1));
    }
}

void EmulationWatchdog::arm(u64 microseconds) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        deadline_ = std::chrono::steady_clock::now() + std::chrono::microseconds(microseconds);
        expired_ = false;
        armed_ = true;
    }
    cv_.notify_one();
}

bool EmulationWatchdog::disarm() {
    std::lock_guard<std::mutex> lock(mutex_);
    bool res = expired_;
    armed_ = false;
    cv_.notify_one();
    return res;
}

Machine::Machine(bool native_fast_traps) : native_fast_traps_(native_fast_traps) {
    uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc_);
    if (err != UC_ERR_OK) {
        throw std::runtime_error(std::string("uc_open failed: ") + uc_strerror(err));
    }
    uc_ctl_set_cpu_model(uc_, UC_CPU_ARM_1176);
    watchdog_ = new EmulationWatchdog(uc_);
    map_memory();
    enable_vfp();
    fast_cpu_ = std::make_unique<ArmFastCpu>(*this);
    if (use_fast_cpu_) {
        fast_cpu_->sync_from_unicorn();
    }
}

Machine::~Machine() {
    remove_all_user_hooks();
    fast_cpu_.reset();
    delete watchdog_;
    watchdog_ = nullptr;
    if (uc_) {
        uc_close(uc_);
        uc_ = nullptr;
    }
    free_memory();
}

void Machine::free_memory() {
    for (auto& r : regions_) {
        if (r.host) {
            free_4k(r.host, r.size);
            r.host = nullptr;
            r.size = 0;
        }
    }
}

void Machine::map_memory() {
    // Ordered with most-frequently accessed host regions (HEAP, IMAGE, STACK) first.
    struct Spec {
        u32 base;
        u32 end;
        const char* label;
    };
    const Spec specs[6] = {
        {kHeapBase, kHeapBase + kHeapSize, "HEAP"},
        {kImageBase, kImageEnd, "IMAGE"},
        {kStackBase, kStackMapEnd, "STACK"},
        {kClassBase, kClassBase + kClassSize, "CLASS"},
        {kTrapBase, kTrapBase + kTrapSize, "TRAP"},
        {kRetStub, kRetStub + 0x1000u, "STUB"},
    };
    for (size_t i = 0; i < 6; ++i) {
        u32 size = specs[i].end - specs[i].base;
        u8* host = alloc_zero_4k(size);
        regions_[i] = {specs[i].base, specs[i].end, host, size};
        uc_err err = uc_mem_map_ptr(uc_, specs[i].base, size, UC_PROT_ALL, host);
        if (err != UC_ERR_OK) {
            throw std::runtime_error(std::string("uc_mem_map_ptr ") + specs[i].label + " failed: " +
                                     uc_strerror(err));
        }
    }

    wr32(kRetStub, kArmBxLr);
    wr32(kHostStop, kArmBxLr);
    wr32(kUiFlushReturnStub, kArmBxLr);
    if (native_fast_traps_) {
        install_native_fast_stubs();
    }
}

void Machine::install_native_fast_stubs() {
    std::vector<u32> words;
    auto pc = [&]() -> u32 { return kNativeStubBase + static_cast<u32>(words.size() * 4u); };
    auto emit = [&](std::initializer_list<u32> insns) { words.insert(words.end(), insns); };

    native_stubs_["_glGetError"] = pc();
    emit({0xE3A00000u, 0xE12FFF1Eu});

    native_stubs_["__Unwind_SjLj_Register"] = pc();
    emit({0xE59F1010u, 0xE5912000u, 0xE5802000u, 0xE5810000u, 0xE3A00000u, 0xE12FFF1Eu, kSjljTopSlot});

    native_stubs_["__Unwind_SjLj_Unregister"] = pc();
    emit({0xE59F100Cu, 0xE5902000u, 0xE5812000u, 0xE3A00000u, 0xE12FFF1Eu, kSjljTopSlot});

    u32 udiv_addr = pc();
    native_stubs_["___udivsi3"] = udiv_addr;
    emit({
        0xE3510000u, 0x03A00000u, 0x03A01000u, 0x012FFF1Eu,
        0xE1500001u, 0x31A01000u, 0x33A00000u, 0x312FFF1Eu,
        0xE16F2F11u, 0xE16F3F10u, 0xE0422003u, 0xE1A0C211u,
        0xE3A03001u, 0xE1A03213u, 0xE3A02000u,
    });
    u32 loop_addr = pc();
    emit({0xE150000Cu, 0x2040000Cu, 0x20822003u, 0xE1A0C0ACu, 0xE1B030A3u});
    words.push_back(b_cond(0x1u, pc(), loop_addr, false));
    emit({0xE1A01000u, 0xE1A00002u, 0xE12FFF1Eu});

    native_stubs_["___umodsi3"] = pc();
    words.push_back(0xE92D4010u);
    words.push_back(b_cond(0xEu, pc(), udiv_addr, true));
    emit({0xE1A00001u, 0xE8BD8010u});

    native_stubs_["___divsi3"] = pc();
    emit({0xE92D4010u, 0xE0204001u, 0xE3500000u, 0xB2600000u, 0xE3510000u, 0xB2611000u});
    words.push_back(b_cond(0xEu, pc(), udiv_addr, true));
    emit({0xE3540000u, 0xB2600000u, 0xE8BD8010u});

    native_stubs_["___modsi3"] = pc();
    emit({0xE92D4010u, 0xE1A04000u, 0xE3500000u, 0xB2600000u, 0xE3510000u, 0xB2611000u});
    words.push_back(b_cond(0xEu, pc(), udiv_addr, true));
    emit({0xE3540000u, 0xB2610000u, 0xA1A00001u, 0xE8BD8010u});

    std::vector<u8> raw(words.size() * 4u);
    for (size_t i = 0; i < words.size(); ++i) {
        write_u32_le(raw.data() + i * 4u, words[i]);
    }
    wrbytes(kNativeStubBase, raw);
    wr32(kSjljTopSlot, 0u);
}

void Machine::enable_vfp() {
    u32 c1 = 0;
    if (uc_reg_read(uc_, UC_ARM_REG_C1_C0_2, &c1) == UC_ERR_OK) {
        c1 |= (0xFu << 20);
        uc_reg_write(uc_, UC_ARM_REG_C1_C0_2, &c1);
    }
    u32 fpexc = 0x40000000u;
    uc_reg_write(uc_, UC_ARM_REG_FPEXC, &fpexc);
}

Bytes Machine::rdbytes(u32 addr, size_t len) const {
    Bytes out(len);
    if (len == 0) return out;
    if (const u8* p = guest_rptr(addr, len)) {
        std::memcpy(out.data(), p, len);
        return out;
    }
    uc_err err = uc_mem_read(uc_, addr, out.data(), len);
    if (err != UC_ERR_OK) {
        throw GuestFault("rdbytes failed at " + hex32(addr) + " len=" + std::to_string(len));
    }
    return out;
}

bool Machine::rdbytes(u32 addr, size_t len, void* dst) const {
    if (len == 0) return true;
    if (!dst || addr == 0) return false;
    if (const u8* p = guest_rptr(addr, len)) {
        std::memcpy(dst, p, len);
        return true;
    }
    return uc_mem_read(uc_, addr, dst, len) == UC_ERR_OK;
}

std::string Machine::cstr(u32 addr) const {
    if (addr == 0) return {};
    size_t max_avail = 0;
    if (const char* p = guest_cstr_ptr(addr, &max_avail)) {
        size_t limit = std::min<size_t>(max_avail, 1u << 20);
        const void* nul = std::memchr(p, '\0', limit);
        size_t len = nul ? static_cast<size_t>(static_cast<const char*>(nul) - p) : limit;
        return std::string(p, len);
    }
    std::string out;
    u8 chunk[64];
    for (u32 cur = addr;; cur += 64) {
        size_t chunk_len = 64;
        uc_err err = uc_mem_read(uc_, cur, chunk, chunk_len);
        if (err != UC_ERR_OK) {
            for (size_t i = 0; i < 64; ++i) {
                u8 b = 0;
                if (uc_mem_read(uc_, cur + i, &b, 1) != UC_ERR_OK || b == 0) return out;
                out.push_back(static_cast<char>(b));
            }
            return out;
        }
        for (size_t i = 0; i < chunk_len; ++i) {
            if (chunk[i] == 0) return out;
            out.push_back(static_cast<char>(chunk[i]));
        }
        if (out.size() > (1u << 20)) break;
    }
    return out;
}

void Machine::wrbytes(u32 addr, const void* data, size_t len) {
    if (len == 0) return;
    if (u8* p = guest_wptr(addr, len)) {
        std::memcpy(p, data, len);
        return;
    }
    uc_err err = uc_mem_write(uc_, addr, data, len);
    if (err != UC_ERR_OK) {
        throw GuestFault("wrbytes failed at " + hex32(addr) + " len=" + std::to_string(len));
    }
}

u32 Machine::alloc_size(u32 addr) const {
    auto it = alloc_sizes_.find(addr);
    return (it != alloc_sizes_.end()) ? it->second : 0u;
}

u32 Machine::malloc_bytes(u32 size) {
    u32 aligned = std::max<u32>(8u, (size + 15u) & ~15u);
    auto it = free_bins_.find(aligned);
    if (it != free_bins_.end() && !it->second.empty()) {
        u32 addr = it->second.back();
        it->second.pop_back();
        alloc_sizes_[addr] = aligned;
        if (u8* p = guest_wptr(addr, aligned)) {
            std::memset(p, 0, aligned);
        } else {
            std::vector<u8> zeros(aligned, 0);
            wrbytes(addr, zeros);
        }
        return addr;
    }
    if (u64(heap_ptr_) + aligned > u64(kHeapBase) + kHeapSize) {
        throw GuestFault("guest heap exhausted");
    }
    u32 addr = heap_ptr_;
    heap_ptr_ += aligned;
    alloc_sizes_[addr] = aligned;
    return addr;
}

u32 Machine::calloc_bytes(u32 count, u32 elem_size) {
    u64 total = u64(count) * elem_size;
    if (total > kHeapSize) throw GuestFault("calloc overflow");
    // Fresh bump allocations are zero-initialized by OS backing, and recycled blocks are memset to 0 in malloc_bytes.
    return malloc_bytes(static_cast<u32>(total));
}

u32 Machine::realloc_bytes(u32 ptr, u32 new_size) {
    if (ptr == 0) return malloc_bytes(new_size);
    if (new_size == 0) {
        free_bytes(ptr);
        return 0;
    }
    auto it = alloc_sizes_.find(ptr);
    u32 old_size = (it != alloc_sizes_.end()) ? it->second : 0u;
    if (new_size <= old_size) return ptr;
    u32 np = malloc_bytes(new_size);
    if (old_size > 0) {
        const u8* src = guest_rptr(ptr, old_size);
        u8* dst = guest_wptr(np, old_size);
        if (src && dst) {
            std::memcpy(dst, src, old_size);
        } else {
            Bytes prev = rdbytes(ptr, old_size);
            wrbytes(np, prev);
        }
    }
    free_bytes(ptr);
    return np;
}

void Machine::free_bytes(u32 ptr) {
    if (ptr == 0) return;
    auto it = alloc_sizes_.find(ptr);
    if (it == alloc_sizes_.end()) return;
    u32 sz = it->second;
    alloc_sizes_.erase(it);
    free_bins_[sz].push_back(ptr);
}

u32 Machine::new_cstr(const std::string& text) {
    u32 len = static_cast<u32>(text.size() + 1u);
    u32 addr = malloc_bytes(len);
    if (u8* p = guest_wptr(addr, len)) {
        if (!text.empty()) std::memcpy(p, text.data(), text.size());
        p[text.size()] = 0;
    } else {
        Bytes buf(text.begin(), text.end());
        buf.push_back(0);
        wrbytes(addr, buf);
    }
    return addr;
}

u32 Machine::class_alloc(u32 size) {
    u32 aligned = (size + 15u) & ~15u;
    if (u64(class_ptr_) + aligned > u64(kClassBase) + kClassSize) {
        throw GuestFault("class region exhausted");
    }
    u32 addr = class_ptr_;
    class_ptr_ += aligned;
    return addr;
}

u32 Machine::reg(int n) const {
    if (use_fast_regs() && n >= 0 && n < 16) return fast_cpu_->state().regs[static_cast<size_t>(n)];
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_R0 + n, &val);
    return val;
}

void Machine::set_reg(int n, u32 val) {
    if (use_fast_regs() && n >= 0 && n < 16) {
        fast_cpu_->state().regs[static_cast<size_t>(n)] = val;
        return;
    }
    uc_reg_write(uc_, UC_ARM_REG_R0 + n, &val);
}

u32 Machine::sp() const {
    if (use_fast_regs()) return fast_cpu_->state().regs[13];
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_SP, &val);
    return val;
}

void Machine::set_sp(u32 val) {
    if (use_fast_regs()) {
        fast_cpu_->state().regs[13] = val;
        return;
    }
    uc_reg_write(uc_, UC_ARM_REG_SP, &val);
}

u32 Machine::lr() const {
    if (use_fast_regs()) return fast_cpu_->state().regs[14];
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_LR, &val);
    return val;
}

void Machine::set_lr(u32 val) {
    if (use_fast_regs()) {
        fast_cpu_->state().regs[14] = val;
        return;
    }
    uc_reg_write(uc_, UC_ARM_REG_LR, &val);
}

u32 Machine::pc() const {
    if (use_fast_regs()) return fast_cpu_->state().regs[15];
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_PC, &val);
    return val;
}

void Machine::set_pc(u32 val) {
    if (use_fast_regs()) {
        fast_cpu_->state().regs[15] = val;
        return;
    }
    uc_reg_write(uc_, UC_ARM_REG_PC, &val);
}

u32 Machine::cpsr() const {
    if (use_fast_regs()) return fast_cpu_->state().cpsr;
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_CPSR, &val);
    return val;
}

void Machine::set_cpsr(u32 val) {
    if (use_fast_regs()) {
        fast_cpu_->state().cpsr = val;
        return;
    }
    uc_reg_write(uc_, UC_ARM_REG_CPSR, &val);
}

u32 Machine::arg(int n) const {
    if (n < 4) return reg(n);
    return rd32(sp() + static_cast<u32>(4 * (n - 4)));
}

bool Machine::load_image(MachOImage img, std::string* err) {
    image_ = std::move(img);
    for (const auto& seg : image_.segments) {
        if (seg.segname == "__PAGEZERO" || seg.filesize == 0) continue;
        if (size_t(seg.fileoff) + seg.filesize > image_.data.size()) {
            if (err) *err = "Segment outside Mach-O file bounds: " + seg.segname;
            return false;
        }
        if (seg.vmaddr < kImageBase || u64(seg.vmaddr) + seg.filesize > kImageEnd) {
            if (err) *err = "Segment outside mapped image bounds: " + seg.segname;
            return false;
        }
        wrbytes(seg.vmaddr, image_.data.data() + seg.fileoff, seg.filesize);
    }
    return true;
}

void Machine::install_traps(const SymbolResolver& resolver) {
    trap_names_.clear();
    trap_handlers_.clear();
    import_addrs_.clear();
    data_symbols_.clear();

    for (const auto& imp : image_.imports) {
        ResolvedSymbol res = resolver(imp.name);
        u32 target = 0;
        if (res.kind == ResolvedSymbol::Kind::Func) {
            auto nit = native_stubs_.find(imp.name);
            if (native_fast_traps_ && nit != native_stubs_.end()) {
                target = nit->second;
            } else {
                u32 idx = static_cast<u32>(trap_names_.size());
                target = kTrapBase + idx * 8u;
                if (target + 8u > kTrapBase + kTrapSize) {
                    throw GuestFault("Trap table exhausted");
                }
                trap_names_.push_back(imp.name);
                trap_handlers_.push_back(std::move(res.handler));
                wr32(target, kArmLdrPcMinus4);
                wr32(target + 4u, kRetStub);
            }
            import_addrs_[imp.name] = target;
        } else {
            target = res.data_addr;
            data_symbols_[imp.name] = target;
        }

        for (u32 a : imp.nl_symbol_ptr_addrs) wr32(a, target);
        for (u32 a : imp.la_symbol_ptr_addrs) wr32(a, target);
        for (u32 a : imp.data_relocation_addrs) wr32(a, rd32(a) + target);
    }

    for (const auto& p : image_.defined_pointers) {
        wr32(p.slot, p.value);
    }

    if (trap_hook_) {
        uc_hook_del(uc_, trap_hook_);
        trap_hook_ = 0;
    }
    if (fault_hook_) {
        uc_hook_del(uc_, fault_hook_);
        fault_hook_ = 0;
    }

    u64 end_addr = kTrapBase + static_cast<u64>(trap_names_.size()) * 8u;
    if (end_addr > kTrapBase) {
        uc_hook_add(uc_, &trap_hook_, UC_HOOK_CODE, reinterpret_cast<void*>(&Machine::on_trap_hook), this,
                    kTrapBase, end_addr - 1u);
    }
    uc_hook_add(uc_, &fault_hook_,
                UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED,
                reinterpret_cast<void*>(&Machine::on_fault_hook), this, 1, 0);
    phase_trap_counts_.clear();
    set_phase(phase_);
}

void Machine::forward_to(u32 trap_addr_for_slot, u32 target) {
    wr32(trap_addr_for_slot + 4u, target);
}

void Machine::reset_forward(u32 trap_addr_for_slot) {
    if (u8* p = guest_wptr(trap_addr_for_slot + 4u, 4)) {
        if (read_u32_le(p) != kRetStub) write_u32_le(p, kRetStub);
        return;
    }
    wr32(trap_addr_for_slot + 4u, kRetStub);
}

u32 Machine::import_addr(const std::string& name) const {
    auto it = import_addrs_.find(name);
    if (it == import_addrs_.end()) {
        throw GuestFault("Missing import_addr: " + name);
    }
    return it->second;
}

bool Machine::has_import_addr(const std::string& name) const {
    return import_addrs_.find(name) != import_addrs_.end();
}

u32 Machine::data_symbol(const std::string& name) const {
    auto it = data_symbols_.find(name);
    if (it == data_symbols_.end()) {
        throw GuestFault("Missing data_symbol: " + name);
    }
    return it->second;
}

void Machine::on_trap_hook(uc_engine* /*uc*/, uint64_t address, uint32_t /*size*/, void* user_data) {
    auto* m = static_cast<Machine*>(user_data);
    if ((address & 7u) != 0) return;
    u32 idx = static_cast<u32>((address - kTrapBase) / 8u);
    if (idx >= m->trap_handlers_.size()) return;
    if (m->active_trap_counts_ && idx < m->active_trap_counts_->size()) {
        (*m->active_trap_counts_)[idx] += 1u;
    }
    try {
        TrapResult res = m->trap_handlers_[idx](*m);
        if (res.has_value) {
            m->set_reg(0, res.r0);
            if (res.two_regs) m->set_reg(1, res.r1);
        }
    } catch (const std::exception& exc) {
        m->stop_reason = m->trap_names_[idx] + ": " + exc.what();
        if (m->fast_cpu_ && m->use_fast_cpu_) {
            m->fast_cpu_->request_stop();
        }
        uc_emu_stop(m->uc_);
    }
}

bool Machine::on_fault_hook(uc_engine* /*uc*/, uc_mem_type type, uint64_t address, int size, int64_t /*value*/,
                            void* user_data) {
    auto* m = static_cast<Machine*>(user_data);
    m->stop_reason = "unmapped access type=" + std::to_string(static_cast<int>(type)) +
                     " addr=" + hex32(static_cast<u32>(address)) +
                     " size=" + std::to_string(size) +
                     " pc=" + hex32(m->pc()) + " lr=" + hex32(m->lr());
    return false;
}

void Machine::on_user_code_hook(uc_engine* /*uc*/, uint64_t address, uint32_t size, void* user_data) {
    auto* entry = static_cast<CodeHookEntry*>(user_data);
    entry->fn(*entry->machine, static_cast<u32>(address), size);
}

u32 Machine::dispatch_fast_trap(u32 trap_addr) {
    u32 idx = (trap_addr - kTrapBase) / 8u;
    if (idx < trap_handlers_.size()) {
        if (active_trap_counts_ && idx < active_trap_counts_->size()) {
            (*active_trap_counts_)[idx] += 1u;
        }
        try {
            TrapResult res = trap_handlers_[idx](*this);
            if (res.has_value) {
                set_reg(0, res.r0);
                if (res.two_regs) set_reg(1, res.r1);
            }
        } catch (const std::exception& exc) {
            stop_reason = trap_names_[idx] + ": " + exc.what();
            return kRetStub;
        }
    }
    return rd32(trap_addr + 4u);
}

bool Machine::has_user_hook_at(u32 addr) const {
    for (const auto& h : user_hooks_) {
        if (addr >= h->begin && addr <= h->end) return true;
    }
    return false;
}

void Machine::run_user_hooks_at(u32 addr, u32 size) {
    for (const auto& h : user_hooks_) {
        if (addr >= h->begin && addr <= h->end) {
            h->fn(*this, addr, size);
        }
    }
}

void Machine::set_fast_cpu(bool enabled) {
    if (use_fast_cpu_ == enabled) return;
    if (fast_cpu_) {
        if (enabled) {
            fast_cpu_->sync_from_unicorn();
        } else {
            fast_cpu_->sync_to_unicorn();
        }
    }
    use_fast_cpu_ = enabled;
}

void Machine::invalidate_code_word(u32 va) {
    if (fast_cpu_) {
        fast_cpu_->invalidate_word(va);
    }
}

void Machine::add_code_hook(u32 begin, u32 end, CodeHookFn fn) {
    auto entry = std::make_unique<CodeHookEntry>();
    entry->begin = begin;
    entry->end = end;
    entry->fn = std::move(fn);
    entry->machine = this;
    uc_err err = uc_hook_add(uc_, &entry->handle, UC_HOOK_CODE,
                             reinterpret_cast<void*>(&Machine::on_user_code_hook), entry.get(), begin, end);
    if (err != UC_ERR_OK) {
        throw GuestFault("add_code_hook failed: " + std::string(uc_strerror(err)));
    }
    user_hooks_.push_back(std::move(entry));
    if (fast_cpu_) fast_cpu_->refresh_hooks();
}

void Machine::remove_all_user_hooks() {
    for (auto& h : user_hooks_) {
        if (h->handle) uc_hook_del(uc_, h->handle);
    }
    user_hooks_.clear();
    if (fast_cpu_) fast_cpu_->refresh_hooks();
}

void Machine::request_stop() {
    if (fast_cpu_) fast_cpu_->request_stop();
    uc_emu_stop(uc_);
}

u32 Machine::call(u32 addr, const std::vector<u32>& args, u32 sp_override, u64 max_insn, u64 timeout_us) {
    for (size_t i = 0; i < args.size() && i < 4; ++i) {
        set_reg(static_cast<int>(i), args[i]);
    }
    if (sp_override != 0) {
        set_sp(sp_override);
    }
    if (args.size() > 4) {
        u32 sp0 = sp();
        u32 extra = static_cast<u32>(args.size() - 4);
        sp0 -= 4u * extra;
        sp0 &= ~7u;
        for (u32 i = 0; i < extra; ++i) {
            wr32(sp0 + 4u * i, args[4 + i]);
        }
        set_sp(sp0);
    }
    set_lr(kHostStop);
    bool thumb = (addr & 1u) != 0;
    u32 cur_cpsr = cpsr();
    set_cpsr(thumb ? (cur_cpsr | 0x20u) : (cur_cpsr & ~0x20u));
    stop_reason.clear();
    last_timed_out_ = false;

    if (use_fast_regs()) {
        fast_cpu_->state().regs[15] = addr & ~1u;
        bool timed_out = false;
        bool ok = fast_cpu_->execute(kHostStop, max_insn, timeout_us, &timed_out);
        last_timed_out_ = timed_out;
        if (!ok || !stop_reason.empty()) {
            std::string msg = stop_reason.empty() ? "fast_cpu fault" : stop_reason;
            msg += ": pc=" + hex32(pc()) + " lr=" + hex32(lr()) + " sp=" + hex32(sp());
            throw GuestFault(msg);
        }
        return reg(0);
    }

    if (timeout_us > 0) watchdog_->arm(timeout_us);
    uc_err err = uc_emu_start(uc_, addr & ~1u, kHostStop, 0, static_cast<size_t>(max_insn));
    if (timeout_us > 0) last_timed_out_ = watchdog_->disarm();

    if (err != UC_ERR_OK) {
        std::string msg = std::string(uc_strerror(err)) + ": pc=" + hex32(pc()) + " lr=" + hex32(lr()) +
                          " sp=" + hex32(sp());
        if (!stop_reason.empty()) msg += " (" + stop_reason + ")";
        throw GuestFault(msg);
    }
    if (!stop_reason.empty()) {
        throw GuestFault(stop_reason);
    }
    return reg(0);
}

void Machine::record_unimplemented(const std::string& key) {
    unimplemented_[key] += 1u;
}

void Machine::log(const std::string& msg) {
    log_lines_.push_back(msg);
    if (trace_enabled_) {
        std::fprintf(stderr, "%s\n", msg.c_str());
    }
}

void Machine::set_phase(const std::string& p) {
    phase_ = p;
    auto& vec = phase_trap_counts_[phase_];
    if (vec.size() < trap_names_.size()) {
        vec.resize(trap_names_.size(), 0u);
    }
    active_trap_counts_ = &vec;
}

std::unordered_map<std::string, u64> Machine::trap_counts(const std::string& p) const {
    std::unordered_map<std::string, u64> out;
    auto it = phase_trap_counts_.find(p);
    if (it == phase_trap_counts_.end()) return out;
    const auto& vec = it->second;
    size_t n = std::min(vec.size(), trap_names_.size());
    for (size_t i = 0; i < n; ++i) {
        if (vec[i] > 0) out[trap_names_[i]] = vec[i];
    }
    return out;
}

}  // namespace hl3
