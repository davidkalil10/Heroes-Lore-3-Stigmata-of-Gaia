#include "machine.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace hl3 {

namespace {

constexpr u32 kArmLdrPcMinus4 = 0xE51FF004u;
constexpr u32 kArmBxLr = 0xE12FFF1Eu;

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
}

Machine::~Machine() {
    remove_all_user_hooks();
    delete watchdog_;
    watchdog_ = nullptr;
    if (uc_) {
        uc_close(uc_);
        uc_ = nullptr;
    }
}

void Machine::map_memory() {
    auto must_map = [&](u32 addr, u32 size, const char* label) {
        uc_err err = uc_mem_map(uc_, addr, size, UC_PROT_ALL);
        if (err != UC_ERR_OK) {
            throw std::runtime_error(std::string("uc_mem_map ") + label + " failed: " + uc_strerror(err));
        }
    };
    must_map(kImageBase, kImageEnd - kImageBase, "IMAGE");
    must_map(kHeapBase, kHeapSize, "HEAP");
    must_map(kClassBase, kClassSize, "CLASS");
    must_map(kTrapBase, kTrapSize, "TRAP");
    must_map(kRetStub, 0x1000u, "STUB");
    must_map(kStackBase, kStackMapEnd - kStackBase, "STACK");

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

u32 Machine::rd32(u32 addr) const {
    u8 buf[4]{};
    uc_err err = uc_mem_read(uc_, addr, buf, 4);
    if (err != UC_ERR_OK) {
        throw GuestFault("rd32 failed at " + hex32(addr) + ": " + uc_strerror(err));
    }
    return read_u32_le(buf);
}

u16 Machine::rd16(u32 addr) const {
    u8 buf[2]{};
    uc_err err = uc_mem_read(uc_, addr, buf, 2);
    if (err != UC_ERR_OK) {
        throw GuestFault("rd16 failed at " + hex32(addr));
    }
    return read_u16_le(buf);
}

i32 Machine::rds16(u32 addr) const {
    u16 v = rd16(addr);
    return (v & 0x8000u) ? i32(u32(v) | 0xFFFF0000u) : i32(v);
}

u8 Machine::rd8(u32 addr) const {
    u8 v = 0;
    uc_err err = uc_mem_read(uc_, addr, &v, 1);
    if (err != UC_ERR_OK) {
        throw GuestFault("rd8 failed at " + hex32(addr));
    }
    return v;
}

i32 Machine::rds8(u32 addr) const {
    u8 v = rd8(addr);
    return (v & 0x80u) ? i32(u32(v) | 0xFFFFFF00u) : i32(v);
}

float Machine::rdf32(u32 addr) const {
    return bits_to_f32(rd32(addr));
}

Bytes Machine::rdbytes(u32 addr, size_t len) const {
    Bytes out(len);
    if (len == 0) return out;
    uc_err err = uc_mem_read(uc_, addr, out.data(), len);
    if (err != UC_ERR_OK) {
        throw GuestFault("rdbytes failed at " + hex32(addr) + " len=" + std::to_string(len));
    }
    return out;
}

bool Machine::rdbytes(u32 addr, size_t len, void* dst) const {
    if (len == 0) return true;
    if (!dst || addr == 0) return false;
    return uc_mem_read(uc_, addr, dst, len) == UC_ERR_OK;
}

std::string Machine::cstr(u32 addr) const {
    if (addr == 0) return {};
    std::string out;
    u8 chunk[64];
    for (u32 cur = addr;; cur += 64) {
        size_t chunk_len = 64;
        uc_err err = uc_mem_read(uc_, cur, chunk, chunk_len);
        if (err != UC_ERR_OK) {
            // Fall back byte-by-byte near page end.
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

void Machine::wr32(u32 addr, u32 val) {
    u8 buf[4];
    write_u32_le(buf, val);
    uc_err err = uc_mem_write(uc_, addr, buf, 4);
    if (err != UC_ERR_OK) {
        throw GuestFault("wr32 failed at " + hex32(addr));
    }
}

void Machine::wr16(u32 addr, u16 val) {
    u8 buf[2];
    write_u16_le(buf, val);
    uc_err err = uc_mem_write(uc_, addr, buf, 2);
    if (err != UC_ERR_OK) {
        throw GuestFault("wr16 failed at " + hex32(addr));
    }
}

void Machine::wr8(u32 addr, u8 val) {
    uc_err err = uc_mem_write(uc_, addr, &val, 1);
    if (err != UC_ERR_OK) {
        throw GuestFault("wr8 failed at " + hex32(addr));
    }
}

void Machine::wrf32(u32 addr, float val) {
    wr32(addr, f32_to_bits(val));
}

void Machine::wrbytes(u32 addr, const void* data, size_t len) {
    if (len == 0) return;
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
        std::vector<u8> zeros(aligned, 0);
        wrbytes(addr, zeros);
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
    u32 addr = malloc_bytes(static_cast<u32>(total));
    std::vector<u8> zeros(alloc_sizes_[addr], 0);
    wrbytes(addr, zeros);
    return addr;
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
        Bytes prev = rdbytes(ptr, old_size);
        wrbytes(np, prev);
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
    u32 addr = malloc_bytes(static_cast<u32>(text.size() + 1u));
    Bytes buf(text.begin(), text.end());
    buf.push_back(0);
    wrbytes(addr, buf);
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
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_R0 + n, &val);
    return val;
}

void Machine::set_reg(int n, u32 val) {
    uc_reg_write(uc_, UC_ARM_REG_R0 + n, &val);
}

u32 Machine::sp() const {
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_SP, &val);
    return val;
}

void Machine::set_sp(u32 val) {
    uc_reg_write(uc_, UC_ARM_REG_SP, &val);
}

u32 Machine::lr() const {
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_LR, &val);
    return val;
}

void Machine::set_lr(u32 val) {
    uc_reg_write(uc_, UC_ARM_REG_LR, &val);
}

u32 Machine::pc() const {
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_PC, &val);
    return val;
}

void Machine::set_pc(u32 val) {
    uc_reg_write(uc_, UC_ARM_REG_PC, &val);
}

u32 Machine::cpsr() const {
    u32 val = 0;
    uc_reg_read(uc_, UC_ARM_REG_CPSR, &val);
    return val;
}

void Machine::set_cpsr(u32 val) {
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
}

void Machine::forward_to(u32 trap_addr_for_slot, u32 target) {
    wr32(trap_addr_for_slot + 4u, target);
}

void Machine::reset_forward(u32 trap_addr_for_slot) {
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
    const std::string& name = m->trap_names_[idx];
    m->counters_[m->phase_][name] += 1u;
    try {
        TrapResult res = m->trap_handlers_[idx](*m);
        if (res.has_value) {
            m->set_reg(0, res.r0);
            if (res.two_regs) m->set_reg(1, res.r1);
        }
    } catch (const std::exception& exc) {
        m->stop_reason = name + ": " + exc.what();
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

void Machine::add_code_hook(u32 begin, u32 end, CodeHookFn fn) {
    auto entry = std::make_unique<CodeHookEntry>();
    entry->fn = std::move(fn);
    entry->machine = this;
    uc_err err = uc_hook_add(uc_, &entry->handle, UC_HOOK_CODE,
                             reinterpret_cast<void*>(&Machine::on_user_code_hook), entry.get(), begin, end);
    if (err != UC_ERR_OK) {
        throw GuestFault("add_code_hook failed: " + std::string(uc_strerror(err)));
    }
    user_hooks_.push_back(std::move(entry));
}

void Machine::remove_all_user_hooks() {
    for (auto& h : user_hooks_) {
        if (h->handle) uc_hook_del(uc_, h->handle);
    }
    user_hooks_.clear();
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

const std::unordered_map<std::string, u64>& Machine::trap_counts(const std::string& p) const {
    auto it = counters_.find(p);
    return (it == counters_.end()) ? empty_counter_ : it->second;
}

}  // namespace hl3
