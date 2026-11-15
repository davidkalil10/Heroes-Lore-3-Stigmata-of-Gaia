#pragma once

#include "common.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace hl3 {

class Machine;

struct ArmCpuState {
    std::array<u32, 16> regs{};
    u32 cpsr = 0x10u;
    std::array<u64, 16> dregs{};
    u32 fpscr = 0;
    u32 fpexc = 0x40000000u;
};

enum class FastOp : u8 {
    kFallback = 0,
    kNop,
    // Branch & misc
    kBranch,
    kBranchLink,
    kBlxImm,
    kBxReg,
    kBlxReg,
    kClz,
    kMovw,
    kMovt,
    kMrsCpsr,
    // Multiply
    kMul,
    kMla,
    kUmull,
    kUmlal,
    kSmull,
    kSmlal,
    // Data processing
    kDpImm,
    kDpRegImmShift,
    kDpRegRegShift,
    // Load / store word / byte
    kLdrStrImm,
    kLdrStrReg,
    // Load / store extra (H / SB / SH / D)
    kLdrStrExtraImm,
    kLdrStrExtraReg,
    // Block transfer
    kLdmStm,
    // Media (ARMv6)
    kExtend8,
    kExtend16,
    kRev,
    kRev16,
    kRevsh,
    // VFPv2
    kVfpLdrStr,
    kVfpLdmStm,
    kVfpMovArmSingle,
    kVfpMovArmDouble,
    kVfpVmrs,
    kVfpVmsr,
    kVfpDp,
};

struct DecodedInsn {
    u32 raw = 0;
    FastOp op = FastOp::kFallback;
    u8 cond = 0;
    u8 flags = 0;  // kFlag* bitmask + opcode-specific subop
    u8 rd = 0;
    u8 rn = 0;
    u8 rm = 0;
    u8 rs = 0;
    u16 aux = 0;
};

inline constexpr u8 kFastFlagS = 1u << 0;
inline constexpr u8 kFastFlagP = 1u << 1;
inline constexpr u8 kFastFlagU = 1u << 2;
inline constexpr u8 kFastFlagW = 1u << 3;
inline constexpr u8 kFastFlagL = 1u << 4;
inline constexpr u8 kFastFlagB = 1u << 5;
inline constexpr u8 kFastFlagDouble = 1u << 6;
inline constexpr u8 kFastFlagHook = 1u << 7;

class ArmFastCpu {
public:
    explicit ArmFastCpu(Machine& machine);

    ArmCpuState& state() { return state_; }
    const ArmCpuState& state() const { return state_; }

    void sync_from_unicorn();
    void sync_to_unicorn();

    void invalidate_range(u32 start_va, u32 end_va);
    void invalidate_word(u32 va);
    void refresh_hooks();

    void request_stop() { stop_requested_.store(true, std::memory_order_relaxed); }
    u64 fallback_count() const { return fallback_count_; }
    u64 executed_count() const { return executed_count_; }
    const std::unordered_map<u32, u64>& fallback_words() const { return fallback_words_; }

    // Execute starting from state_.regs[15] until stop_addr, fault, max_insn, or timeout.
    bool execute(u32 stop_addr, u64 max_insn, u64 timeout_us, bool* timed_out);

    static DecodedInsn decode_arm(u32 word);

private:
    bool lookup_word(u32 pc, DecodedInsn& out);
    DecodedInsn* slot_for_pc(u32 pc);
    const DecodedInsn* slot_for_pc(u32 pc) const;
    bool addr_has_hook(u32 va) const;
    bool fallback_one_or_thumb(u32 stop_addr, u64 remaining_insn);

    Machine& machine_;
    ArmCpuState state_{};
    std::vector<DecodedInsn> image_cache_;
    std::vector<DecodedInsn> stub_cache_;
    std::atomic<bool> stop_requested_{false};
    u64 fallback_count_ = 0;
    u64 executed_count_ = 0;
    std::unordered_map<u32, u64> fallback_words_;
};

}  // namespace hl3
