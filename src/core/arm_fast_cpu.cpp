#include "arm_fast_cpu.h"
#include "machine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <unicorn/unicorn.h>

namespace hl3 {

namespace {

inline u32 rotr32(u32 v, u32 n) {
    n &= 31u;
    if (n == 0) return v;
    return (v >> n) | (v << (32u - n));
}

inline u32 sign_extend_24(u32 v) {
    v &= 0x00FFFFFFu;
    return (v & 0x00800000u) ? (v | 0xFF000000u) : v;
}

inline u32 count_leading_zeros(u32 v) {
    if (v == 0) return 32u;
    u32 n = 0;
    if ((v & 0xFFFF0000u) == 0) { n += 16; v <<= 16; }
    if ((v & 0xFF000000u) == 0) { n += 8; v <<= 8; }
    if ((v & 0xF0000000u) == 0) { n += 4; v <<= 4; }
    if ((v & 0xC0000000u) == 0) { n += 2; v <<= 2; }
    if ((v & 0x80000000u) == 0) { n += 1; }
    return n;
}

inline u32 popcount16(u32 mask) {
    u32 c = 0;
    for (u32 m = mask & 0xFFFFu; m; m &= (m - 1u)) ++c;
    return c;
}

inline bool cond_passed(u32 cond, u32 cpsr) {
    if (cond >= 14u) return true;
    const bool n = (cpsr >> 31) & 1u;
    const bool z = (cpsr >> 30) & 1u;
    const bool c = (cpsr >> 29) & 1u;
    const bool v = (cpsr >> 28) & 1u;
    switch (cond) {
        case 0: return z;
        case 1: return !z;
        case 2: return c;
        case 3: return !c;
        case 4: return n;
        case 5: return !n;
        case 6: return v;
        case 7: return !v;
        case 8: return c && !z;
        case 9: return !c || z;
        case 10: return n == v;
        case 11: return n != v;
        case 12: return !z && (n == v);
        case 13: return z || (n != v);
        default: return true;
    }
}

inline void dp_imm_shifter(u32 imm12, u32 old_c, u32& val, u32& cout) {
    u32 rot = ((imm12 >> 8) & 0xFu) * 2u;
    u32 imm8 = imm12 & 0xFFu;
    if (rot == 0) {
        val = imm8;
        cout = old_c;
    } else {
        val = rotr32(imm8, rot);
        cout = (val >> 31) & 1u;
    }
}

inline void dp_imm_shift_reg(u32 rm_val, u32 stype, u32 samt, u32 old_c, u32& val, u32& cout) {
    switch (stype) {
        case 0:  // LSL
            if (samt == 0) {
                val = rm_val;
                cout = old_c;
            } else {
                val = rm_val << samt;
                cout = (rm_val >> (32u - samt)) & 1u;
            }
            return;
        case 1:  // LSR
            if (samt == 0) {
                val = 0;
                cout = (rm_val >> 31) & 1u;
            } else {
                val = rm_val >> samt;
                cout = (rm_val >> (samt - 1u)) & 1u;
            }
            return;
        case 2:  // ASR
            if (samt == 0) {
                u32 b31 = (rm_val >> 31) & 1u;
                val = b31 ? 0xFFFFFFFFu : 0u;
                cout = b31;
            } else {
                val = static_cast<u32>(static_cast<i32>(rm_val) >> samt);
                cout = (rm_val >> (samt - 1u)) & 1u;
            }
            return;
        default:  // ROR / RRX
            if (samt == 0) {
                val = (old_c << 31) | (rm_val >> 1);
                cout = rm_val & 1u;
            } else {
                val = rotr32(rm_val, samt);
                cout = (val >> 31) & 1u;
            }
            return;
    }
}

inline void dp_reg_shift_reg(u32 rm_val, u32 stype, u32 rs_val, u32 old_c, u32& val, u32& cout) {
    u32 samt = rs_val & 0xFFu;
    if (samt == 0) {
        val = rm_val;
        cout = old_c;
        return;
    }
    switch (stype) {
        case 0:  // LSL
            if (samt < 32u) {
                val = rm_val << samt;
                cout = (rm_val >> (32u - samt)) & 1u;
            } else if (samt == 32u) {
                val = 0;
                cout = rm_val & 1u;
            } else {
                val = 0;
                cout = 0;
            }
            return;
        case 1:  // LSR
            if (samt < 32u) {
                val = rm_val >> samt;
                cout = (rm_val >> (samt - 1u)) & 1u;
            } else if (samt == 32u) {
                val = 0;
                cout = (rm_val >> 31) & 1u;
            } else {
                val = 0;
                cout = 0;
            }
            return;
        case 2:  // ASR
            if (samt < 32u) {
                val = static_cast<u32>(static_cast<i32>(rm_val) >> samt);
                cout = (rm_val >> (samt - 1u)) & 1u;
            } else {
                u32 b31 = (rm_val >> 31) & 1u;
                val = b31 ? 0xFFFFFFFFu : 0u;
                cout = b31;
            }
            return;
        default: {  // ROR
            u32 r5 = samt & 31u;
            if (r5 == 0) {
                val = rm_val;
                cout = (rm_val >> 31) & 1u;
            } else {
                val = rotr32(rm_val, r5);
                cout = (val >> 31) & 1u;
            }
            return;
        }
    }
}

inline void set_nzcv(u32& cpsr, u32 n, u32 z, u32 c, u32 v) {
    cpsr = (cpsr & 0x0FFFFFFFu) | ((n & 1u) << 31) | ((z & 1u) << 30) | ((c & 1u) << 29) | ((v & 1u) << 28);
}

inline void set_nzc(u32& cpsr, u32 res, u32 c) {
    cpsr = (cpsr & 0x1FFFFFFFu) | (res & 0x80000000u) | (res == 0 ? (1u << 30) : 0u) | ((c & 1u) << 29);
}

inline void add_with_carry(u32 a, u32 b, u32 cin, u32& res, u32& c_out, u32& v_out) {
    u64 sum64 = static_cast<u64>(a) + static_cast<u64>(b) + static_cast<u64>(cin);
    res = static_cast<u32>(sum64);
    c_out = static_cast<u32>(sum64 >> 32) & 1u;
    v_out = ((~(a ^ b) & (a ^ res)) >> 31) & 1u;
}

inline u32 vfp_cmp_flags(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) return 0x30000000u;
    if (a < b) return 0x80000000u;
    if (a == b) return 0x60000000u;
    return 0x20000000u;
}

inline double round_by_fpscr(double v, u32 fpscr, bool force_rz) {
    if (force_rz) return std::trunc(v);
    u32 rmode = (fpscr >> 22) & 3u;
    switch (rmode) {
        case 0: return std::nearbyint(v);
        case 1: return std::ceil(v);
        case 2: return std::floor(v);
        case 3:
        default: return std::trunc(v);
    }
}

inline i32 f64_to_i32_sat(double v, u32 fpscr, bool force_rz) {
    if (std::isnan(v)) return 0;
    double r = round_by_fpscr(v, fpscr, force_rz);
    if (r >= 2147483648.0) return std::numeric_limits<i32>::max();
    if (r <= -2147483648.0) return std::numeric_limits<i32>::min();
    return static_cast<i32>(r);
}

inline u32 f64_to_u32_sat(double v, u32 fpscr, bool force_rz) {
    if (std::isnan(v)) return 0;
    double r = round_by_fpscr(v, fpscr, force_rz);
    if (r <= 0.0) return 0u;
    if (r >= 4294967296.0) return std::numeric_limits<u32>::max();
    return static_cast<u32>(r);
}

constexpr u32 kGprUcIds[16] = {
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2,  UC_ARM_REG_R3,  UC_ARM_REG_R4,  UC_ARM_REG_R5,
    UC_ARM_REG_R6, UC_ARM_REG_R7, UC_ARM_REG_R8,  UC_ARM_REG_R9,  UC_ARM_REG_R10, UC_ARM_REG_R11,
    UC_ARM_REG_R12, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
};

constexpr u32 kDregUcIds[16] = {
    UC_ARM_REG_D0,  UC_ARM_REG_D1,  UC_ARM_REG_D2,  UC_ARM_REG_D3,  UC_ARM_REG_D4,  UC_ARM_REG_D5,
    UC_ARM_REG_D6,  UC_ARM_REG_D7,  UC_ARM_REG_D8,  UC_ARM_REG_D9,  UC_ARM_REG_D10, UC_ARM_REG_D11,
    UC_ARM_REG_D12, UC_ARM_REG_D13, UC_ARM_REG_D14, UC_ARM_REG_D15,
};

struct ThumbReturnProbe {
    uc_engine* uc = nullptr;
    uc_hook hook = 0;
    bool stop_on_next_arm = false;
};

void on_thumb_fallback_block(uc_engine* uc, uint64_t address, uint32_t /*size*/, void* user_data) {
    auto* probe = static_cast<ThumbReturnProbe*>(user_data);
    u32 cpsr = 0;
    if (uc_reg_read(uc, UC_ARM_REG_CPSR, &cpsr) == UC_ERR_OK && (cpsr & 0x20u) == 0) {
        // Check we are back in fast ARM-decodable image or stub region and finished at least 1 Thumb block.
        if (probe->stop_on_next_arm &&
            ((address >= kImageBase && address < kImageEnd) || (address >= kRetStub && address < kRetStub + 0x1000u))) {
            uc_emu_stop(uc);
            return;
        }
    }
    probe->stop_on_next_arm = true;
}

}  // namespace

ArmFastCpu::ArmFastCpu(Machine& machine)
    : machine_(machine),
      image_cache_((kImageEnd - kImageBase) >> 2),
      stub_cache_(0x1000u >> 2) {}

void ArmFastCpu::sync_from_unicorn() {
    uc_engine* uc = machine_.uc();
    for (int i = 0; i < 16; ++i) {
        uc_reg_read(uc, static_cast<int>(kGprUcIds[i]), &state_.regs[i]);
    }
    uc_reg_read(uc, UC_ARM_REG_CPSR, &state_.cpsr);
    for (int i = 0; i < 16; ++i) {
        uc_reg_read(uc, static_cast<int>(kDregUcIds[i]), &state_.dregs[i]);
    }
    uc_reg_read(uc, UC_ARM_REG_FPSCR, &state_.fpscr);
    uc_reg_read(uc, UC_ARM_REG_FPEXC, &state_.fpexc);
}

void ArmFastCpu::sync_to_unicorn() {
    uc_engine* uc = machine_.uc();
    for (int i = 0; i < 16; ++i) {
        uc_reg_write(uc, static_cast<int>(kGprUcIds[i]), &state_.regs[i]);
    }
    uc_reg_write(uc, UC_ARM_REG_CPSR, &state_.cpsr);
    for (int i = 0; i < 16; ++i) {
        uc_reg_write(uc, static_cast<int>(kDregUcIds[i]), &state_.dregs[i]);
    }
    uc_reg_write(uc, UC_ARM_REG_FPSCR, &state_.fpscr);
    uc_reg_write(uc, UC_ARM_REG_FPEXC, &state_.fpexc);
}

DecodedInsn* ArmFastCpu::slot_for_pc(u32 pc) {
    if ((pc & 3u) != 0) return nullptr;
    if (pc >= kImageBase && pc < kImageEnd) {
        return &image_cache_[(pc - kImageBase) >> 2];
    }
    if (pc >= kRetStub && pc < kRetStub + 0x1000u) {
        return &stub_cache_[(pc - kRetStub) >> 2];
    }
    return nullptr;
}

const DecodedInsn* ArmFastCpu::slot_for_pc(u32 pc) const {
    if ((pc & 3u) != 0) return nullptr;
    if (pc >= kImageBase && pc < kImageEnd) {
        return &image_cache_[(pc - kImageBase) >> 2];
    }
    if (pc >= kRetStub && pc < kRetStub + 0x1000u) {
        return &stub_cache_[(pc - kRetStub) >> 2];
    }
    return nullptr;
}

void ArmFastCpu::invalidate_word(u32 va) {
    if (DecodedInsn* slot = slot_for_pc(va & ~3u)) {
        u8 hook_bit = slot->flags & kFastFlagHook;
        slot->raw = 0;
        slot->op = FastOp::kFallback;
        slot->flags = hook_bit;
    }
}

void ArmFastCpu::invalidate_range(u32 start_va, u32 end_va) {
    u32 a = start_va & ~3u;
    while (a < end_va) {
        invalidate_word(a);
        if (a > 0xFFFFFFFBu) break;
        a += 4u;
    }
}

void ArmFastCpu::refresh_hooks() {
    for (auto& d : image_cache_) d.flags &= ~kFastFlagHook;
    for (auto& d : stub_cache_) d.flags &= ~kFastFlagHook;
    for (const auto& h : machine_.user_hooks_) {
        u32 a = h->begin & ~3u;
        u32 b = h->end;
        if (a < kImageEnd && b >= kImageBase) {
            u32 lo = std::max(a, kImageBase);
            u32 hi = std::min(b, kImageEnd - 4u);
            for (u32 va = lo; va <= hi; va += 4u) {
                image_cache_[(va - kImageBase) >> 2].flags |= kFastFlagHook;
            }
        }
        if (a < kRetStub + 0x1000u && b >= kRetStub) {
            u32 lo = std::max(a, kRetStub);
            u32 hi = std::min(b, kRetStub + 0x1000u - 4u);
            for (u32 va = lo; va <= hi; va += 4u) {
                stub_cache_[(va - kRetStub) >> 2].flags |= kFastFlagHook;
            }
        }
    }
}

bool ArmFastCpu::addr_has_hook(u32 va) const {
    if (const DecodedInsn* slot = slot_for_pc(va)) {
        return (slot->flags & kFastFlagHook) != 0;
    }
    return machine_.has_user_hook_at(va);
}

DecodedInsn ArmFastCpu::decode_arm(u32 w) {
    DecodedInsn d{};
    d.raw = w;
    u8 cond = static_cast<u8>((w >> 28) & 0xFu);
    d.cond = cond;

    if (cond == 0xFu) {
        if ((w & 0xFE000000u) == 0xFA000000u) {
            d.op = FastOp::kBlxImm;
            return d;
        }
        if ((w & 0x0D70F000u) == 0x0550F000u) {
            d.op = FastOp::kNop;  // PLD
            return d;
        }
        d.op = FastOp::kFallback;
        return d;
    }

    u32 op1 = (w >> 25) & 0x7u;

    // Branches: B / BL
    if (op1 == 5u) {
        d.op = ((w >> 24) & 1u) ? FastOp::kBranchLink : FastOp::kBranch;
        return d;
    }

    // Load/Store word/byte
    if (op1 == 2u) {
        u8 p = (w >> 24) & 1u;
        u8 u = (w >> 23) & 1u;
        u8 b = (w >> 22) & 1u;
        u8 wb = (w >> 21) & 1u;
        u8 l = (w >> 20) & 1u;
        if (!p && wb) {
            d.op = FastOp::kFallback;  // unprivileged LDRT/STRT
            return d;
        }
        u8 rn = (w >> 16) & 0xFu;
        u8 rd = (w >> 12) & 0xFu;
        if ((!p || wb) && rn == 15u) {
            d.op = FastOp::kFallback;
            return d;
        }
        d.op = FastOp::kLdrStrImm;
        d.rn = rn;
        d.rd = rd;
        d.aux = static_cast<u16>(w & 0xFFFu);
        d.flags = (p ? kFastFlagP : 0) | (u ? kFastFlagU : 0) | (b ? kFastFlagB : 0) |
                  (wb ? kFastFlagW : 0) | (l ? kFastFlagL : 0);
        return d;
    }

    if (op1 == 3u) {
        if (((w >> 4) & 1u) == 0u) {
            u8 p = (w >> 24) & 1u;
            u8 u = (w >> 23) & 1u;
            u8 b = (w >> 22) & 1u;
            u8 wb = (w >> 21) & 1u;
            u8 l = (w >> 20) & 1u;
            u8 rm = w & 0xFu;
            if ((!p && wb) || rm == 15u) {
                d.op = FastOp::kFallback;
                return d;
            }
            u8 rn = (w >> 16) & 0xFu;
            if ((!p || wb) && rn == 15u) {
                d.op = FastOp::kFallback;
                return d;
            }
            d.op = FastOp::kLdrStrReg;
            d.rn = rn;
            d.rd = (w >> 12) & 0xFu;
            d.rm = rm;
            d.rs = static_cast<u8>((w >> 5) & 3u);
            d.aux = static_cast<u16>((w >> 7) & 0x1Fu);
            d.flags = (p ? kFastFlagP : 0) | (u ? kFastFlagU : 0) | (b ? kFastFlagB : 0) |
                      (wb ? kFastFlagW : 0) | (l ? kFastFlagL : 0);
            return d;
        }
        // Media (op1 == 3, bit4 == 1)
        u8 rd = (w >> 12) & 0xFu;
        u8 rn = (w >> 16) & 0xFu;
        u8 rm = w & 0xFu;
        if (rd != 15u && rm != 15u) {
            if ((w & 0x0FF003F0u) == 0x06A00070u || (w & 0x0FF003F0u) == 0x06E00070u) {
                d.op = FastOp::kExtend8;
                d.rd = rd;
                d.rn = rn;
                d.rm = rm;
                d.flags = ((w & 0x00400000u) == 0) ? kFastFlagS : 0;  // 0x06A = signed, 0x06E = unsigned
                d.aux = static_cast<u16>(((w >> 10) & 3u) * 8u);
                return d;
            }
            if ((w & 0x0FF003F0u) == 0x06B00070u || (w & 0x0FF003F0u) == 0x06F00070u) {
                d.op = FastOp::kExtend16;
                d.rd = rd;
                d.rn = rn;
                d.rm = rm;
                d.flags = ((w & 0x00400000u) == 0) ? kFastFlagS : 0;
                d.aux = static_cast<u16>(((w >> 10) & 3u) * 8u);
                return d;
            }
            if ((w & 0x0FFF0FF0u) == 0x06BF0F30u) {
                d.op = FastOp::kRev;
                d.rd = rd;
                d.rm = rm;
                return d;
            }
            if ((w & 0x0FFF0FF0u) == 0x06BF0FB0u) {
                d.op = FastOp::kRev16;
                d.rd = rd;
                d.rm = rm;
                return d;
            }
            if ((w & 0x0FFF0FF0u) == 0x06FF0FB0u) {
                d.op = FastOp::kRevsh;
                d.rd = rd;
                d.rm = rm;
                return d;
            }
        }
        d.op = FastOp::kFallback;
        return d;
    }

    // LDM / STM (S == 0)
    if (op1 == 4u) {
        if ((w & 0x00400000u) != 0) {
            d.op = FastOp::kFallback;  // S-bit system LDM/STM
            return d;
        }
        u8 rn = (w >> 16) & 0xFu;
        u16 mask = static_cast<u16>(w & 0xFFFFu);
        if (rn == 15u || mask == 0) {
            d.op = FastOp::kFallback;
            return d;
        }
        d.op = FastOp::kLdmStm;
        d.rn = rn;
        d.aux = mask;
        u8 p = (w >> 24) & 1u;
        u8 u = (w >> 23) & 1u;
        u8 wb = (w >> 21) & 1u;
        u8 l = (w >> 20) & 1u;
        d.flags = (p ? kFastFlagP : 0) | (u ? kFastFlagU : 0) | (wb ? kFastFlagW : 0) | (l ? kFastFlagL : 0);
        return d;
    }

    // Data Processing Immediate (op1 == 1)
    if (op1 == 1u) {
        u8 opcode = static_cast<u8>((w >> 21) & 0xFu);
        u8 s = static_cast<u8>((w >> 20) & 1u);
        u8 rn = static_cast<u8>((w >> 16) & 0xFu);
        u8 rd = static_cast<u8>((w >> 12) & 0xFu);
        if ((opcode & 0xCu) == 0x8u && s == 0) {
            // Hints / MSR imm / MOVW / MOVT -> let MOVW/MOVT or NOP hints if exact, else fallback
            if ((w & 0x0FFFFFF0u) == 0x0320F000u && (w & 0xFu) == 0u) {
                d.op = FastOp::kNop;
                return d;
            }
            d.op = FastOp::kFallback;
            return d;
        }
        if (s && rd == 15u && (opcode < 8u || opcode > 11u)) {
            d.op = FastOp::kFallback;
            return d;
        }
        d.op = FastOp::kDpImm;
        d.rn = rn;
        d.rd = rd;
        d.rs = opcode;
        d.aux = static_cast<u16>(w & 0xFFFu);
        d.flags = s ? kFastFlagS : 0;
        return d;
    }

    // op1 == 0
    if (op1 == 0u) {
        if ((w & 0x0FFFFFF0u) == 0x012FFF10u) {
            d.op = FastOp::kBxReg;
            d.rm = w & 0xFu;
            return d;
        }
        if ((w & 0x0FFFFFF0u) == 0x012FFF30u) {
            d.op = FastOp::kBlxReg;
            d.rm = w & 0xFu;
            return d;
        }
        if ((w & 0x0FFF0FF0u) == 0x016F0F10u) {
            u8 rd = (w >> 12) & 0xFu;
            u8 rm = w & 0xFu;
            if (rd != 15u && rm != 15u) {
                d.op = FastOp::kClz;
                d.rd = rd;
                d.rm = rm;
                return d;
            }
            d.op = FastOp::kFallback;
            return d;
        }
        if ((w & 0x0FFF0FFFu) == 0x010F0000u) {
            u8 rd = (w >> 12) & 0xFu;
            if (rd != 15u) {
                d.op = FastOp::kMrsCpsr;
                d.rd = rd;
                return d;
            }
            d.op = FastOp::kFallback;
            return d;
        }
        if ((w & 0x0E000090u) == 0x00000090u) {
            u32 op2 = (w >> 5) & 3u;
            if (op2 == 0u) {
                // Multiply family
                if ((w & 0x0FC000F0u) == 0x00000090u) {
                    u8 rd = (w >> 16) & 0xFu;
                    u8 rn = (w >> 12) & 0xFu;
                    u8 rs = (w >> 8) & 0xFu;
                    u8 rm = w & 0xFu;
                    if (rd == 15u || rs == 15u || rm == 15u || (((w >> 21) & 1u) && rn == 15u)) {
                        d.op = FastOp::kFallback;
                        return d;
                    }
                    d.op = ((w >> 21) & 1u) ? FastOp::kMla : FastOp::kMul;
                    d.rd = rd;
                    d.rn = rn;
                    d.rs = rs;
                    d.rm = rm;
                    d.flags = ((w >> 20) & 1u) ? kFastFlagS : 0;
                    return d;
                }
                if ((w & 0x0F8000F0u) == 0x00800090u) {
                    u8 rdhi = (w >> 16) & 0xFu;
                    u8 rdlo = (w >> 12) & 0xFu;
                    u8 rs = (w >> 8) & 0xFu;
                    u8 rm = w & 0xFu;
                    if (rdhi == 15u || rdlo == 15u || rs == 15u || rm == 15u || rdhi == rdlo) {
                        d.op = FastOp::kFallback;
                        return d;
                    }
                    bool sig = ((w >> 22) & 1u) != 0;
                    bool acc = ((w >> 21) & 1u) != 0;
                    d.op = sig ? (acc ? FastOp::kSmlal : FastOp::kSmull)
                               : (acc ? FastOp::kUmlal : FastOp::kUmull);
                    d.rd = rdlo;
                    d.rn = rdhi;
                    d.rs = rs;
                    d.rm = rm;
                    d.flags = ((w >> 20) & 1u) ? kFastFlagS : 0;
                    return d;
                }
                d.op = FastOp::kFallback;
                return d;
            }
            // Extra Load / Store (LDRH/STRH/LDRSB/LDRSH/LDRD/STRD)
            u8 p = (w >> 24) & 1u;
            u8 u = (w >> 23) & 1u;
            u8 b = (w >> 22) & 1u;  // I=1 imm
            u8 wb = (w >> 21) & 1u;
            u8 l = (w >> 20) & 1u;
            u8 rn = (w >> 16) & 0xFu;
            u8 rd = (w >> 12) & 0xFu;
            u8 rm = w & 0xFu;
            if (!p && wb) {
                d.op = FastOp::kFallback;
                return d;
            }
            if ((!p || wb) && rn == 15u) {
                d.op = FastOp::kFallback;
                return d;
            }
            if (!b && (((w >> 8) & 0xFu) != 0 || rm == 15u)) {
                d.op = FastOp::kFallback;
                return d;
            }
            u8 subop = 0;
            if (!l && op2 == 1u) subop = 0;       // STRH
            else if (l && op2 == 1u) subop = 1;   // LDRH
            else if (l && op2 == 2u) subop = 2;   // LDRSB
            else if (l && op2 == 3u) subop = 3;   // LDRSH
            else if (!l && op2 == 2u) subop = 4;  // LDRD
            else if (!l && op2 == 3u) subop = 5;  // STRD
            if ((subop == 1 || subop == 2 || subop == 3) && rd == 15u) {
                d.op = FastOp::kFallback;
                return d;
            }
            if ((subop == 4 || subop == 5) && ((rd & 1u) != 0 || rd >= 14u)) {
                d.op = FastOp::kFallback;
                return d;
            }
            d.op = b ? FastOp::kLdrStrExtraImm : FastOp::kLdrStrExtraReg;
            d.rn = rn;
            d.rd = rd;
            d.rm = rm;
            d.rs = subop;
            d.aux = static_cast<u16>(((w >> 4) & 0xF0u) | (w & 0xFu));
            d.flags = (p ? kFastFlagP : 0) | (u ? kFastFlagU : 0) | (wb ? kFastFlagW : 0);
            return d;
        }

        // Standard DP reg
        bool reg_shift = ((w >> 4) & 1u) != 0u;
        if (reg_shift && ((w >> 7) & 1u) != 0u) {
            d.op = FastOp::kFallback;
            return d;
        }
        u8 opcode = static_cast<u8>((w >> 21) & 0xFu);
        u8 s = static_cast<u8>((w >> 20) & 1u);
        u8 rn = static_cast<u8>((w >> 16) & 0xFu);
        u8 rd = static_cast<u8>((w >> 12) & 0xFu);
        u8 rm = static_cast<u8>(w & 0xFu);
        if ((opcode & 0xCu) == 0x8u && s == 0) {
            d.op = FastOp::kFallback;
            return d;
        }
        if (s && rd == 15u && (opcode < 8u || opcode > 11u)) {
            d.op = FastOp::kFallback;
            return d;
        }
        if (!reg_shift) {
            d.op = FastOp::kDpRegImmShift;
            d.rn = rn;
            d.rd = rd;
            d.rm = rm;
            d.rs = opcode;
            // aux packs stype (bits 6:5 -> bits 7:6? keep clear): low 5 bits = samt, bits 6:5 = stype
            d.aux = static_cast<u16>(((w >> 5) & 0x7Fu));
            d.flags = s ? kFastFlagS : 0;
            return d;
        } else {
            u8 rs_reg = static_cast<u8>((w >> 8) & 0xFu);
            if (rn == 15u || rd == 15u || rm == 15u || rs_reg == 15u) {
                d.op = FastOp::kFallback;
                return d;
            }
            d.op = FastOp::kDpRegRegShift;
            d.rn = rn;
            d.rd = rd;
            d.rm = rm;
            d.rs = opcode;
            d.aux = static_cast<u16>((rs_reg << 2) | ((w >> 5) & 3u));
            d.flags = s ? kFastFlagS : 0;
            return d;
        }
    }

    // VFPv2 (op1 == 6 or op1 == 7) with cp10 / cp11
    if ((op1 == 6u || op1 == 7u) && ((w & 0x00000E00u) == 0x00000A00u)) {
        bool dbl = ((w >> 8) & 1u) != 0u;
        if (op1 == 6u) {
            u8 p = (w >> 24) & 1u;
            u8 u = (w >> 23) & 1u;
            u8 d_bit = (w >> 22) & 1u;
            u8 wb = (w >> 21) & 1u;
            u8 l = (w >> 20) & 1u;
            u8 rn = (w >> 16) & 0xFu;
            u8 vd4 = (w >> 12) & 0xFu;
            u8 imm8 = w & 0xFFu;
            if (dbl && d_bit) {
                d.op = FastOp::kFallback;
                return d;
            }
            u8 vd = dbl ? vd4 : static_cast<u8>((vd4 << 1) | d_bit);
            if (p && !wb) {
                // VLDR / VSTR
                d.op = FastOp::kVfpLdrStr;
                d.rn = rn;
                d.rd = vd;
                d.aux = imm8;
                d.flags = (u ? kFastFlagU : 0) | (l ? kFastFlagL : 0) | (dbl ? kFastFlagDouble : 0);
                return d;
            }
            // VMOV Dd <-> Rt, Rt2 (0x0C400B10 / 0x0C500B10)
            if (dbl && !p && u && !wb && ((w & 0x0FE00FF0u) == 0x0C400B10u)) {
                u8 rt = vd4;
                u8 rt2 = rn;
                u8 vm = w & 0xFu;
                if (rt == 15u || rt2 == 15u || ((w >> 5) & 1u) != 0) {
                    d.op = FastOp::kFallback;
                    return d;
                }
                d.op = FastOp::kVfpMovArmDouble;
                d.rd = rt;
                d.rn = rt2;
                d.rm = vm;
                d.flags = l ? kFastFlagL : 0;
                return d;
            }
            // VLDM / VSTM / VPUSH / VPOP
            bool valid_ldm = (!p && u) || (p && !u && wb);
            if (valid_ldm && rn != 15u && imm8 > 0) {
                u32 count = dbl ? (imm8 >> 1) : imm8;
                if (dbl && ((imm8 & 1u) != 0)) {
                    d.op = FastOp::kFallback;
                    return d;
                }
                if (count == 0 || (dbl ? (vd + count > 16u) : (vd + count > 32u))) {
                    d.op = FastOp::kFallback;
                    return d;
                }
                d.op = FastOp::kVfpLdmStm;
                d.rn = rn;
                d.rd = vd;
                d.aux = static_cast<u16>(count);
                d.flags = (p ? kFastFlagP : 0) | (u ? kFastFlagU : 0) | (wb ? kFastFlagW : 0) |
                          (l ? kFastFlagL : 0) | (dbl ? kFastFlagDouble : 0);
                return d;
            }
            d.op = FastOp::kFallback;
            return d;
        }

        // op1 == 7
        if ((w & 0x01000010u) == 0x00000010u) {
            if (!dbl && (w & 0x0FE00F7Fu) == 0x0E000A10u) {
                u8 vn = static_cast<u8>(((w >> 16) & 0xFu) << 1) | ((w >> 7) & 1u);
                u8 rt = (w >> 12) & 0xFu;
                if (rt == 15u) {
                    d.op = FastOp::kFallback;
                    return d;
                }
                d.op = FastOp::kVfpMovArmSingle;
                d.rd = rt;
                d.rn = vn;
                d.flags = ((w >> 20) & 1u) ? kFastFlagL : 0;
                return d;
            }
            if (!dbl && (w & 0x0FFF0FFFu) == 0x0EF10A10u) {
                d.op = FastOp::kVfpVmrs;
                d.rd = (w >> 12) & 0xFu;
                return d;
            }
            if (!dbl && (w & 0x0FFF0FFFu) == 0x0EE10A10u) {
                u8 rt = (w >> 12) & 0xFu;
                if (rt == 15u) {
                    d.op = FastOp::kFallback;
                    return d;
                }
                d.op = FastOp::kVfpVmsr;
                d.rd = rt;
                return d;
            }
            d.op = FastOp::kFallback;
            return d;
        }

        // VFP Data Processing (bit24 == 0, bit4 == 0)
        if ((w & 0x0F000E10u) == 0x0E000A00u) {
            u8 opc1 = static_cast<u8>(((w >> 21) & 0x4u) | ((w >> 20) & 0x3u));
            u8 d_bit = (w >> 22) & 1u;
            u8 n_bit = (w >> 7) & 1u;
            u8 op_bit6 = (w >> 6) & 1u;
            u8 m_bit = (w >> 5) & 1u;
            u8 vd4 = (w >> 12) & 0xFu;
            u8 vn4 = (w >> 16) & 0xFu;
            u8 vm4 = w & 0xFu;

            if (opc1 <= 4u) {
                if (opc1 == 4u && op_bit6 != 0) {
                    d.op = FastOp::kFallback;
                    return d;
                }
                if (dbl && (d_bit || n_bit || m_bit)) {
                    d.op = FastOp::kFallback;
                    return d;
                }
                d.op = FastOp::kVfpDp;
                d.rd = dbl ? vd4 : static_cast<u8>((vd4 << 1) | d_bit);
                d.rn = dbl ? vn4 : static_cast<u8>((vn4 << 1) | n_bit);
                d.rm = dbl ? vm4 : static_cast<u8>((vm4 << 1) | m_bit);
                d.rs = static_cast<u8>((opc1 << 1) | op_bit6);  // 0..8
                d.flags = dbl ? kFastFlagDouble : 0;
                return d;
            }

            if (opc1 == 7u && op_bit6 == 1u) {
                u8 opc2 = vn4;
                // Subop codes >= 16 stored in d.rs:
                // 16: VMOV reg, 17: VABS, 18: VNEG, 19: VSQRT,
                // 20: VCMP, 21: VCMP #0, 22: VCVT f32<->f64,
                // 23: VCVT F<-U32, 24: VCVT F<-S32, 25: VCVT U32<-F, 26: VCVT S32<-F
                u8 sub = 0;
                if (opc2 == 0u && !n_bit) sub = 16u;
                else if (opc2 == 0u && n_bit) sub = 17u;
                else if (opc2 == 1u && !n_bit) sub = 18u;
                else if (opc2 == 1u && n_bit) sub = 19u;
                else if (opc2 == 4u) sub = 20u;
                else if (opc2 == 5u) sub = 21u;
                else if (opc2 == 7u && n_bit) sub = 22u;
                else if (opc2 == 8u && !n_bit) sub = 23u;
                else if (opc2 == 8u && n_bit) sub = 24u;
                else if (opc2 == 12u) sub = 25u;
                else if (opc2 == 13u) sub = 26u;
                else {
                    d.op = FastOp::kFallback;
                    return d;
                }

                if (sub <= 21u) {
                    if (dbl && (d_bit || m_bit)) {
                        d.op = FastOp::kFallback;
                        return d;
                    }
                    d.rd = dbl ? vd4 : static_cast<u8>((vd4 << 1) | d_bit);
                    d.rm = dbl ? vm4 : static_cast<u8>((vm4 << 1) | m_bit);
                } else if (sub == 22u) {
                    // dbl=false: f32 -> f64 (Dd, Sm); dbl=true: f64 -> f32 (Sd, Dm)
                    if (!dbl) {
                        if (d_bit) { d.op = FastOp::kFallback; return d; }
                        d.rd = vd4;
                        d.rm = static_cast<u8>((vm4 << 1) | m_bit);
                    } else {
                        if (m_bit) { d.op = FastOp::kFallback; return d; }
                        d.rd = static_cast<u8>((vd4 << 1) | d_bit);
                        d.rm = vm4;
                    }
                } else if (sub == 23u || sub == 24u) {
                    // Int (always S) -> Float (Sd or Dd)
                    if (dbl && d_bit) { d.op = FastOp::kFallback; return d; }
                    d.rd = dbl ? vd4 : static_cast<u8>((vd4 << 1) | d_bit);
                    d.rm = static_cast<u8>((vm4 << 1) | m_bit);
                } else {
                    // Float (Sm or Dm) -> Int (always Sd)
                    if (dbl && m_bit) { d.op = FastOp::kFallback; return d; }
                    d.rd = static_cast<u8>((vd4 << 1) | d_bit);
                    d.rm = dbl ? vm4 : static_cast<u8>((vm4 << 1) | m_bit);
                    if (n_bit) d.aux = 1u;  // force_rz (ftosiz/ftouiz)
                }
                d.op = FastOp::kVfpDp;
                d.rs = sub;
                d.flags = dbl ? kFastFlagDouble : 0;
                return d;
            }
        }
        d.op = FastOp::kFallback;
        return d;
    }

    d.op = FastOp::kFallback;
    return d;
}

bool ArmFastCpu::lookup_word(u32 pc, DecodedInsn& out) {
    const u8* p = machine_.guest_rptr(pc, 4);
    if (!p) return false;
    u32 live = read_u32_le(p);
    if (DecodedInsn* slot = slot_for_pc(pc)) {
        if (slot->raw != live) {
            u8 hook_bit = slot->flags & kFastFlagHook;
            *slot = decode_arm(live);
            slot->flags |= hook_bit;
        }
        out = *slot;
        return true;
    }
    out = decode_arm(live);
    if (machine_.has_user_hook_at(pc)) out.flags |= kFastFlagHook;
    return true;
}

bool ArmFastCpu::fallback_one_or_thumb(u32 stop_addr, u64 remaining_insn) {
    fallback_count_ += 1u;
    if (const u8* pw = machine_.guest_rptr(state_.regs[15] & ~3u, 4u)) {
        fallback_words_[read_u32_le(pw)] += 1u;
    }
    sync_to_unicorn();
    uc_engine* uc = machine_.uc();
    bool is_thumb = (state_.cpsr & 0x20u) != 0;

    ThumbReturnProbe probe{};
    probe.uc = uc;
    if (is_thumb) {
        uc_err herr = uc_hook_add(uc, &probe.hook, UC_HOOK_BLOCK, reinterpret_cast<void*>(&on_thumb_fallback_block),
                                  &probe, 1, 0);
        if (herr != UC_ERR_OK) {
            machine_.stop_reason = std::string("thumb fallback hook failed: ") + uc_strerror(herr);
            return false;
        }
    }

    u64 count = is_thumb ? remaining_insn : 1u;
    u32 start = is_thumb ? (state_.regs[15] | 1u) : state_.regs[15];
    machine_.set_in_unicorn_fallback(true);
    uc_err err = uc_emu_start(uc, start, stop_addr, 0, static_cast<size_t>(count));
    machine_.set_in_unicorn_fallback(false);

    if (probe.hook) {
        uc_hook_del(uc, probe.hook);
    }
    sync_from_unicorn();

    if (err != UC_ERR_OK) {
        machine_.stop_reason = std::string(uc_strerror(err)) + ": pc=" + hex32(state_.regs[15]) +
                               " lr=" + hex32(state_.regs[14]) + " sp=" + hex32(state_.regs[13]);
        return false;
    }
    if (!machine_.stop_reason.empty()) return false;
    return true;
}

bool ArmFastCpu::execute(u32 stop_addr, u64 max_insn, u64 timeout_us, bool* timed_out) {
    if (timed_out) *timed_out = false;
    stop_requested_.store(false, std::memory_order_relaxed);

    const bool has_timeout = timeout_us > 0;
    std::chrono::steady_clock::time_point deadline{};
    if (has_timeout) {
        deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(timeout_us);
    }

    const u32 trap_end = kTrapBase + static_cast<u32>(machine_.trap_handlers_.size()) * 8u;
    u64 steps = 0;

    while (true) {
        u32 cur_pc = state_.regs[15];
        if (cur_pc == stop_addr) {
            return true;
        }
        if (max_insn > 0 && steps >= max_insn) {
            return true;
        }
        if (stop_requested_.load(std::memory_order_relaxed) || !machine_.stop_reason.empty()) {
            return machine_.stop_reason.empty();
        }
        if (has_timeout && (steps & 4095u) == 0u && steps > 0u) {
            if (std::chrono::steady_clock::now() >= deadline) {
                if (timed_out) *timed_out = true;
                return true;
            }
        }

        // Fast trap table interception before decoding [kTrapBase, trap_end)
        if (cur_pc >= kTrapBase && cur_pc < trap_end && (cur_pc & 7u) == 0u) {
            ++steps;
            ++executed_count_;
            u32 next_pc = machine_.dispatch_fast_trap(cur_pc);
            if (!machine_.stop_reason.empty() || stop_requested_.load(std::memory_order_relaxed)) {
                return machine_.stop_reason.empty();
            }
            if (next_pc == kRetStub && !addr_has_hook(kRetStub)) {
                u32 ret = state_.regs[14];
                if (ret & 1u) {
                    state_.cpsr |= 0x20u;
                    state_.regs[15] = ret & ~1u;
                } else {
                    state_.cpsr &= ~0x20u;
                    state_.regs[15] = ret & ~3u;
                }
            } else {
                if (next_pc & 1u) {
                    state_.cpsr |= 0x20u;
                    state_.regs[15] = next_pc & ~1u;
                } else {
                    state_.cpsr &= ~0x20u;
                    state_.regs[15] = next_pc & ~3u;
                }
            }
            continue;
        }

        if ((state_.cpsr & 0x20u) != 0u) {
            u64 rem = (max_insn > 0) ? (max_insn - steps) : 0u;
            if (!fallback_one_or_thumb(stop_addr, rem)) return false;
            ++steps;
            continue;
        }

        DecodedInsn d{};
        if (!lookup_word(cur_pc, d)) {
            machine_.stop_reason = "unmapped fetch at pc=" + hex32(cur_pc) + " lr=" + hex32(state_.regs[14]);
            return false;
        }

        if (d.flags & kFastFlagHook) {
            machine_.run_user_hooks_at(cur_pc, 4u);
            if (!machine_.stop_reason.empty() || stop_requested_.load(std::memory_order_relaxed)) {
                return machine_.stop_reason.empty();
            }
            if (state_.regs[15] != cur_pc) {
                continue;
            }
        }

        ++steps;
        ++executed_count_;

        if (d.op == FastOp::kFallback) {
            if (!fallback_one_or_thumb(stop_addr, 1u)) return false;
            continue;
        }

        if (!cond_passed(d.cond, state_.cpsr)) {
            state_.regs[15] = cur_pc + 4u;
            continue;
        }

        auto rd_gpr = [&](u8 r) -> u32 { return (r == 15u) ? (cur_pc + 8u) : state_.regs[r]; };
        auto wr_pc_interwork = [&](u32 target) {
            if (target & 1u) {
                state_.cpsr |= 0x20u;
                state_.regs[15] = target & ~1u;
            } else {
                state_.cpsr &= ~0x20u;
                state_.regs[15] = target & ~3u;
            }
        };

        bool wrote_pc = false;

        switch (d.op) {
            case FastOp::kNop:
                break;

            case FastOp::kBranch: {
                u32 off = sign_extend_24(d.raw) << 2;
                state_.regs[15] = cur_pc + 8u + off;
                wrote_pc = true;
                break;
            }

            case FastOp::kBranchLink: {
                u32 off = sign_extend_24(d.raw) << 2;
                state_.regs[14] = cur_pc + 4u;
                state_.regs[15] = cur_pc + 8u + off;
                wrote_pc = true;
                break;
            }

            case FastOp::kBlxImm: {
                u32 off = (sign_extend_24(d.raw) << 2) | (((d.raw >> 24) & 1u) << 1);
                state_.regs[14] = cur_pc + 4u;
                state_.cpsr |= 0x20u;
                state_.regs[15] = (cur_pc + 8u + off) & ~1u;
                wrote_pc = true;
                break;
            }

            case FastOp::kBxReg: {
                u32 target = rd_gpr(d.rm);
                wr_pc_interwork(target);
                wrote_pc = true;
                break;
            }

            case FastOp::kBlxReg: {
                u32 target = rd_gpr(d.rm);
                state_.regs[14] = cur_pc + 4u;
                wr_pc_interwork(target);
                wrote_pc = true;
                break;
            }

            case FastOp::kClz:
                state_.regs[d.rd] = count_leading_zeros(state_.regs[d.rm]);
                break;

            case FastOp::kMrsCpsr:
                state_.regs[d.rd] = state_.cpsr;
                break;

            case FastOp::kMul: {
                u32 res = state_.regs[d.rm] * state_.regs[d.rs];
                state_.regs[d.rd] = res;
                if (d.flags & kFastFlagS) {
                    state_.cpsr = (state_.cpsr & 0x3FFFFFFFu) | (res & 0x80000000u) | (res == 0 ? (1u << 30) : 0u);
                }
                break;
            }

            case FastOp::kMla: {
                u32 res = state_.regs[d.rm] * state_.regs[d.rs] + state_.regs[d.rn];
                state_.regs[d.rd] = res;
                if (d.flags & kFastFlagS) {
                    state_.cpsr = (state_.cpsr & 0x3FFFFFFFu) | (res & 0x80000000u) | (res == 0 ? (1u << 30) : 0u);
                }
                break;
            }

            case FastOp::kUmull:
            case FastOp::kUmlal:
            case FastOp::kSmull:
            case FastOp::kSmlal: {
                bool is_signed = (d.op == FastOp::kSmull || d.op == FastOp::kSmlal);
                bool is_acc = (d.op == FastOp::kUmlal || d.op == FastOp::kSmlal);
                u64 acc = is_acc ? ((static_cast<u64>(state_.regs[d.rn]) << 32) | state_.regs[d.rd]) : 0ull;
                u64 res64 = 0;
                if (is_signed) {
                    i64 p = static_cast<i64>(static_cast<i32>(state_.regs[d.rm])) *
                            static_cast<i64>(static_cast<i32>(state_.regs[d.rs]));
                    res64 = static_cast<u64>(p) + acc;
                } else {
                    res64 = static_cast<u64>(state_.regs[d.rm]) * static_cast<u64>(state_.regs[d.rs]) + acc;
                }
                state_.regs[d.rd] = static_cast<u32>(res64);
                state_.regs[d.rn] = static_cast<u32>(res64 >> 32);
                if (d.flags & kFastFlagS) {
                    u32 n = static_cast<u32>(res64 >> 63) & 1u;
                    u32 z = (res64 == 0) ? 1u : 0u;
                    state_.cpsr = (state_.cpsr & 0x3FFFFFFFu) | (n << 31) | (z << 30);
                }
                break;
            }

            case FastOp::kDpImm:
            case FastOp::kDpRegImmShift:
            case FastOp::kDpRegRegShift: {
                u32 old_c = (state_.cpsr >> 29) & 1u;
                u32 op2_val = 0;
                u32 scout = old_c;
                if (d.op == FastOp::kDpImm) {
                    dp_imm_shifter(d.aux, old_c, op2_val, scout);
                } else if (d.op == FastOp::kDpRegImmShift) {
                    dp_imm_shift_reg(rd_gpr(d.rm), (d.aux & 3u), (d.aux >> 2) & 0x1Fu, old_c, op2_val, scout);
                } else {
                    dp_reg_shift_reg(state_.regs[d.rm], (d.aux & 3u), state_.regs[(d.aux >> 2) & 0xFu], old_c,
                                     op2_val, scout);
                }
                u32 a = rd_gpr(d.rn);
                u32 b = op2_val;
                u32 res = 0;
                u32 c_out = scout;
                u32 v_out = (state_.cpsr >> 28) & 1u;
                bool write_rd = true;
                bool set_flags = (d.flags & kFastFlagS) != 0;

                switch (d.rs) {
                    case 0: res = a & b; break;                                   // AND
                    case 1: res = a ^ b; break;                                   // EOR
                    case 2: add_with_carry(a, ~b, 1u, res, c_out, v_out); break;  // SUB
                    case 3: add_with_carry(b, ~a, 1u, res, c_out, v_out); break;  // RSB
                    case 4: add_with_carry(a, b, 0u, res, c_out, v_out); break;   // ADD
                    case 5: add_with_carry(a, b, old_c, res, c_out, v_out); break;// ADC
                    case 6: add_with_carry(a, ~b, old_c, res, c_out, v_out); break;// SBC
                    case 7: add_with_carry(b, ~a, old_c, res, c_out, v_out); break;// RSC
                    case 8: res = a & b; write_rd = false; break;                 // TST
                    case 9: res = a ^ b; write_rd = false; break;                 // TEQ
                    case 10: add_with_carry(a, ~b, 1u, res, c_out, v_out); write_rd = false; break; // CMP
                    case 11: add_with_carry(a, b, 0u, res, c_out, v_out); write_rd = false; break;  // CMN
                    case 12: res = a | b; break;                                  // ORR
                    case 13: res = b; break;                                      // MOV
                    case 14: res = a & ~b; break;                                 // BIC
                    case 15: res = ~b; break;                                     // MVN
                    default: break;
                }
                if (set_flags) {
                    if (d.rs == 2 || d.rs == 3 || d.rs == 4 || d.rs == 5 || d.rs == 6 || d.rs == 7 || d.rs == 10 ||
                        d.rs == 11) {
                        set_nzcv(state_.cpsr, res >> 31, res == 0 ? 1u : 0u, c_out, v_out);
                    } else {
                        set_nzc(state_.cpsr, res, c_out);
                    }
                }
                if (write_rd) {
                    if (d.rd == 15u) {
                        state_.regs[15] = res & ~3u;
                        wrote_pc = true;
                    } else {
                        state_.regs[d.rd] = res;
                    }
                }
                break;
            }

            case FastOp::kLdrStrImm:
            case FastOp::kLdrStrReg: {
                u32 off = 0;
                if (d.op == FastOp::kLdrStrImm) {
                    off = d.aux;
                } else {
                    u32 dummy_c = 0;
                    dp_imm_shift_reg(state_.regs[d.rm], d.rs, d.aux, (state_.cpsr >> 29) & 1u, off, dummy_c);
                }
                bool pre = (d.flags & kFastFlagP) != 0;
                bool up = (d.flags & kFastFlagU) != 0;
                bool byte = (d.flags & kFastFlagB) != 0;
                bool wb = (d.flags & kFastFlagW) != 0;
                bool load = (d.flags & kFastFlagL) != 0;

                u32 base = rd_gpr(d.rn);
                u32 wb_addr = up ? (base + off) : (base - off);
                u32 ea = pre ? wb_addr : base;
                try {
                    if (load) {
                        u32 val = byte ? static_cast<u32>(machine_.rd8(ea)) : machine_.rd32(ea);
                        if (!pre || wb) state_.regs[d.rn] = wb_addr;
                        if (d.rd == 15u) {
                            wr_pc_interwork(val);
                            wrote_pc = true;
                        } else {
                            state_.regs[d.rd] = val;
                        }
                    } else {
                        u32 sval = rd_gpr(d.rd);
                        if (byte) machine_.wr8(ea, static_cast<u8>(sval & 0xFFu));
                        else machine_.wr32(ea, sval);
                        if (!pre || wb) state_.regs[d.rn] = wb_addr;
                    }
                } catch (const GuestFault& e) {
                    machine_.stop_reason = e.what();
                    return false;
                }
                break;
            }

            case FastOp::kLdrStrExtraImm:
            case FastOp::kLdrStrExtraReg: {
                u32 off = (d.op == FastOp::kLdrStrExtraImm) ? d.aux : state_.regs[d.rm];
                bool pre = (d.flags & kFastFlagP) != 0;
                bool up = (d.flags & kFastFlagU) != 0;
                bool wb = (d.flags & kFastFlagW) != 0;
                u32 base = rd_gpr(d.rn);
                u32 wb_addr = up ? (base + off) : (base - off);
                u32 ea = pre ? wb_addr : base;
                try {
                    switch (d.rs) {
                        case 0: {  // STRH
                            u32 v = rd_gpr(d.rd);
                            machine_.wr16(ea, static_cast<u16>(v & 0xFFFFu));
                            if (!pre || wb) state_.regs[d.rn] = wb_addr;
                            break;
                        }
                        case 1: {  // LDRH
                            u32 v = machine_.rd16(ea);
                            if (!pre || wb) state_.regs[d.rn] = wb_addr;
                            state_.regs[d.rd] = v;
                            break;
                        }
                        case 2: {  // LDRSB
                            u32 v = static_cast<u32>(machine_.rds8(ea));
                            if (!pre || wb) state_.regs[d.rn] = wb_addr;
                            state_.regs[d.rd] = v;
                            break;
                        }
                        case 3: {  // LDRSH
                            u32 v = static_cast<u32>(machine_.rds16(ea));
                            if (!pre || wb) state_.regs[d.rn] = wb_addr;
                            state_.regs[d.rd] = v;
                            break;
                        }
                        case 4: {  // LDRD
                            u32 lo = machine_.rd32(ea);
                            u32 hi = machine_.rd32(ea + 4u);
                            if (!pre || wb) state_.regs[d.rn] = wb_addr;
                            state_.regs[d.rd] = lo;
                            state_.regs[d.rd + 1u] = hi;
                            break;
                        }
                        case 5: {  // STRD
                            u32 lo = state_.regs[d.rd];
                            u32 hi = state_.regs[d.rd + 1u];
                            machine_.wr32(ea, lo);
                            machine_.wr32(ea + 4u, hi);
                            if (!pre || wb) state_.regs[d.rn] = wb_addr;
                            break;
                        }
                    }
                } catch (const GuestFault& e) {
                    machine_.stop_reason = e.what();
                    return false;
                }
                break;
            }

            case FastOp::kLdmStm: {
                bool pre = (d.flags & kFastFlagP) != 0;
                bool up = (d.flags & kFastFlagU) != 0;
                bool wb = (d.flags & kFastFlagW) != 0;
                bool load = (d.flags & kFastFlagL) != 0;
                u32 mask = d.aux;
                u32 count = popcount16(mask);
                u32 base = state_.regs[d.rn];
                u32 addr = up ? (pre ? base + 4u : base) : (base - 4u * count + (pre ? 0u : 4u));
                u32 wb_addr = up ? (base + 4u * count) : (base - 4u * count);
                try {
                    if (load) {
                        u32 tmp[16];
                        for (u32 i = 0; i < 16u; ++i) {
                            if (mask & (1u << i)) {
                                tmp[i] = machine_.rd32(addr);
                                addr += 4u;
                            }
                        }
                        if (wb) state_.regs[d.rn] = wb_addr;
                        for (u32 i = 0; i < 15u; ++i) {
                            if (mask & (1u << i)) state_.regs[i] = tmp[i];
                        }
                        if (mask & (1u << 15)) {
                            wr_pc_interwork(tmp[15]);
                            wrote_pc = true;
                        }
                    } else {
                        for (u32 i = 0; i < 16u; ++i) {
                            if (mask & (1u << i)) {
                                u32 v = (i == 15u) ? (cur_pc + 8u) : state_.regs[i];
                                machine_.wr32(addr, v);
                                addr += 4u;
                            }
                        }
                        if (wb) state_.regs[d.rn] = wb_addr;
                    }
                } catch (const GuestFault& e) {
                    machine_.stop_reason = e.what();
                    return false;
                }
                break;
            }

            case FastOp::kExtend8: {
                u32 r = rotr32(state_.regs[d.rm], d.aux);
                u8 low = static_cast<u8>(r & 0xFFu);
                u32 ext = (d.flags & kFastFlagS) ? static_cast<u32>(static_cast<i32>(static_cast<int8_t>(low)))
                                                 : static_cast<u32>(low);
                state_.regs[d.rd] = (d.rn == 15u) ? ext : (state_.regs[d.rn] + ext);
                break;
            }

            case FastOp::kExtend16: {
                u32 r = rotr32(state_.regs[d.rm], d.aux);
                u16 low = static_cast<u16>(r & 0xFFFFu);
                u32 ext = (d.flags & kFastFlagS) ? static_cast<u32>(static_cast<i32>(static_cast<int16_t>(low)))
                                                 : static_cast<u32>(low);
                state_.regs[d.rd] = (d.rn == 15u) ? ext : (state_.regs[d.rn] + ext);
                break;
            }

            case FastOp::kRev: {
                u32 v = state_.regs[d.rm];
                state_.regs[d.rd] =
                    ((v & 0xFFu) << 24) | ((v & 0xFF00u) << 8) | ((v >> 8) & 0xFF00u) | ((v >> 24) & 0xFFu);
                break;
            }

            case FastOp::kRev16: {
                u32 v = state_.regs[d.rm];
                state_.regs[d.rd] = ((v & 0x00FF00FFu) << 8) | ((v & 0xFF00FF00u) >> 8);
                break;
            }

            case FastOp::kRevsh: {
                u32 v = state_.regs[d.rm];
                u16 lo = static_cast<u16>(((v & 0xFFu) << 8) | ((v >> 8) & 0xFFu));
                state_.regs[d.rd] = static_cast<u32>(static_cast<i32>(static_cast<int16_t>(lo)));
                break;
            }

            case FastOp::kVfpLdrStr: {
                bool up = (d.flags & kFastFlagU) != 0;
                bool load = (d.flags & kFastFlagL) != 0;
                bool dbl = (d.flags & kFastFlagDouble) != 0;
                u32 base = rd_gpr(d.rn);
                u32 off = static_cast<u32>(d.aux) << 2;
                u32 ea = up ? (base + off) : (base - off);
                auto* sregs = reinterpret_cast<u32*>(state_.dregs.data());
                try {
                    if (dbl) {
                        if (load) {
                            u32 lo = machine_.rd32(ea);
                            u32 hi = machine_.rd32(ea + 4u);
                            state_.dregs[d.rd] = static_cast<u64>(lo) | (static_cast<u64>(hi) << 32);
                        } else {
                            u64 val = state_.dregs[d.rd];
                            machine_.wr32(ea, static_cast<u32>(val));
                            machine_.wr32(ea + 4u, static_cast<u32>(val >> 32));
                        }
                    } else {
                        if (load) sregs[d.rd] = machine_.rd32(ea);
                        else machine_.wr32(ea, sregs[d.rd]);
                    }
                } catch (const GuestFault& e) {
                    machine_.stop_reason = e.what();
                    return false;
                }
                break;
            }

            case FastOp::kVfpLdmStm: {
                bool pre = (d.flags & kFastFlagP) != 0;
                bool wb = (d.flags & kFastFlagW) != 0;
                bool load = (d.flags & kFastFlagL) != 0;
                bool dbl = (d.flags & kFastFlagDouble) != 0;
                u32 count = d.aux;
                u32 bytes = count * (dbl ? 8u : 4u);
                u32 base = state_.regs[d.rn];
                u32 ea = pre ? (base - bytes) : base;
                u32 wb_addr = pre ? (base - bytes) : (base + bytes);
                auto* sregs = reinterpret_cast<u32*>(state_.dregs.data());
                try {
                    if (dbl) {
                        for (u32 i = 0; i < count; ++i) {
                            if (load) {
                                u32 lo = machine_.rd32(ea);
                                u32 hi = machine_.rd32(ea + 4u);
                                state_.dregs[d.rd + i] = static_cast<u64>(lo) | (static_cast<u64>(hi) << 32);
                            } else {
                                u64 val = state_.dregs[d.rd + i];
                                machine_.wr32(ea, static_cast<u32>(val));
                                machine_.wr32(ea + 4u, static_cast<u32>(val >> 32));
                            }
                            ea += 8u;
                        }
                    } else {
                        for (u32 i = 0; i < count; ++i) {
                            if (load) sregs[d.rd + i] = machine_.rd32(ea);
                            else machine_.wr32(ea, sregs[d.rd + i]);
                            ea += 4u;
                        }
                    }
                    if (wb) state_.regs[d.rn] = wb_addr;
                } catch (const GuestFault& e) {
                    machine_.stop_reason = e.what();
                    return false;
                }
                break;
            }

            case FastOp::kVfpMovArmSingle: {
                auto* sregs = reinterpret_cast<u32*>(state_.dregs.data());
                if (d.flags & kFastFlagL) state_.regs[d.rd] = sregs[d.rn];
                else sregs[d.rn] = state_.regs[d.rd];
                break;
            }

            case FastOp::kVfpMovArmDouble: {
                if (d.flags & kFastFlagL) {
                    u64 val = state_.dregs[d.rm];
                    state_.regs[d.rd] = static_cast<u32>(val);
                    state_.regs[d.rn] = static_cast<u32>(val >> 32);
                } else {
                    state_.dregs[d.rm] =
                        static_cast<u64>(state_.regs[d.rd]) | (static_cast<u64>(state_.regs[d.rn]) << 32);
                }
                break;
            }

            case FastOp::kVfpVmrs:
                if (d.rd == 15u) {
                    state_.cpsr = (state_.cpsr & 0x0FFFFFFFu) | (state_.fpscr & 0xF0000000u);
                } else {
                    state_.regs[d.rd] = state_.fpscr;
                }
                break;

            case FastOp::kVfpVmsr:
                state_.fpscr = state_.regs[d.rd];
                break;

            case FastOp::kVfpDp: {
                bool dbl = (d.flags & kFastFlagDouble) != 0;
                auto* sregs = reinterpret_cast<u32*>(state_.dregs.data());
                auto rd_f32 = [&](u8 idx) -> float { return bits_to_f32(sregs[idx]); };
                auto wr_f32 = [&](u8 idx, float v) { sregs[idx] = f32_to_bits(v); };
                auto rd_f64 = [&](u8 idx) -> double {
                    double res = 0.0;
                    std::memcpy(&res, &state_.dregs[idx], sizeof(double));
                    return res;
                };
                auto wr_f64 = [&](u8 idx, double v) {
                    std::memcpy(&state_.dregs[idx], &v, sizeof(double));
                };

                switch (d.rs) {
                    case 0:  // VMLA
                        if (dbl) wr_f64(d.rd, rd_f64(d.rd) + rd_f64(d.rn) * rd_f64(d.rm));
                        else wr_f32(d.rd, rd_f32(d.rd) + rd_f32(d.rn) * rd_f32(d.rm));
                        break;
                    case 1:  // VMLS
                        if (dbl) wr_f64(d.rd, rd_f64(d.rd) - rd_f64(d.rn) * rd_f64(d.rm));
                        else wr_f32(d.rd, rd_f32(d.rd) - rd_f32(d.rn) * rd_f32(d.rm));
                        break;
                    case 2:  // VNMLS
                        if (dbl) wr_f64(d.rd, -(rd_f64(d.rd) - rd_f64(d.rn) * rd_f64(d.rm)));
                        else wr_f32(d.rd, -(rd_f32(d.rd) - rd_f32(d.rn) * rd_f32(d.rm)));
                        break;
                    case 3:  // VNMLA
                        if (dbl) wr_f64(d.rd, -(rd_f64(d.rd) + rd_f64(d.rn) * rd_f64(d.rm)));
                        else wr_f32(d.rd, -(rd_f32(d.rd) + rd_f32(d.rn) * rd_f32(d.rm)));
                        break;
                    case 4:  // VMUL
                        if (dbl) wr_f64(d.rd, rd_f64(d.rn) * rd_f64(d.rm));
                        else wr_f32(d.rd, rd_f32(d.rn) * rd_f32(d.rm));
                        break;
                    case 5:  // VNMUL
                        if (dbl) wr_f64(d.rd, -(rd_f64(d.rn) * rd_f64(d.rm)));
                        else wr_f32(d.rd, -(rd_f32(d.rn) * rd_f32(d.rm)));
                        break;
                    case 6:  // VADD
                        if (dbl) wr_f64(d.rd, rd_f64(d.rn) + rd_f64(d.rm));
                        else wr_f32(d.rd, rd_f32(d.rn) + rd_f32(d.rm));
                        break;
                    case 7:  // VSUB
                        if (dbl) wr_f64(d.rd, rd_f64(d.rn) - rd_f64(d.rm));
                        else wr_f32(d.rd, rd_f32(d.rn) - rd_f32(d.rm));
                        break;
                    case 8:  // VDIV
                        if (dbl) wr_f64(d.rd, rd_f64(d.rn) / rd_f64(d.rm));
                        else wr_f32(d.rd, rd_f32(d.rn) / rd_f32(d.rm));
                        break;
                    case 16:  // VMOV reg
                        if (dbl) state_.dregs[d.rd] = state_.dregs[d.rm];
                        else sregs[d.rd] = sregs[d.rm];
                        break;
                    case 17:  // VABS
                        if (dbl) state_.dregs[d.rd] = state_.dregs[d.rm] & 0x7FFFFFFFFFFFFFFFull;
                        else sregs[d.rd] = sregs[d.rm] & 0x7FFFFFFFu;
                        break;
                    case 18:  // VNEG
                        if (dbl) state_.dregs[d.rd] = state_.dregs[d.rm] ^ 0x8000000000000000ull;
                        else sregs[d.rd] = sregs[d.rm] ^ 0x80000000u;
                        break;
                    case 19:  // VSQRT
                        if (dbl) wr_f64(d.rd, std::sqrt(rd_f64(d.rm)));
                        else wr_f32(d.rd, std::sqrt(rd_f32(d.rm)));
                        break;
                    case 20: {  // VCMP
                        u32 flags = dbl ? vfp_cmp_flags(rd_f64(d.rd), rd_f64(d.rm))
                                        : vfp_cmp_flags(rd_f32(d.rd), rd_f32(d.rm));
                        state_.fpscr = (state_.fpscr & 0x0FFFFFFFu) | flags;
                        break;
                    }
                    case 21: {  // VCMP #0.0
                        u32 flags = dbl ? vfp_cmp_flags(rd_f64(d.rd), 0.0) : vfp_cmp_flags(rd_f32(d.rd), 0.0);
                        state_.fpscr = (state_.fpscr & 0x0FFFFFFFu) | flags;
                        break;
                    }
                    case 22:  // VCVT F64<->F32
                        if (!dbl) wr_f64(d.rd, static_cast<double>(rd_f32(d.rm)));
                        else wr_f32(d.rd, static_cast<float>(rd_f64(d.rm)));
                        break;
                    case 23:  // VCVT F<-U32
                        if (dbl) wr_f64(d.rd, static_cast<double>(sregs[d.rm]));
                        else wr_f32(d.rd, static_cast<float>(sregs[d.rm]));
                        break;
                    case 24:  // VCVT F<-S32
                        if (dbl) wr_f64(d.rd, static_cast<double>(static_cast<i32>(sregs[d.rm])));
                        else wr_f32(d.rd, static_cast<float>(static_cast<i32>(sregs[d.rm])));
                        break;
                    case 25: {  // VCVT U32<-F
                        double src = dbl ? rd_f64(d.rm) : static_cast<double>(rd_f32(d.rm));
                        sregs[d.rd] = f64_to_u32_sat(src, state_.fpscr, d.aux != 0);
                        break;
                    }
                    case 26: {  // VCVT S32<-F
                        double src = dbl ? rd_f64(d.rm) : static_cast<double>(rd_f32(d.rm));
                        sregs[d.rd] = static_cast<u32>(f64_to_i32_sat(src, state_.fpscr, d.aux != 0));
                        break;
                    }
                }
                break;
            }

            case FastOp::kMovw:
            case FastOp::kMovt:
            case FastOp::kFallback:
                if (!fallback_one_or_thumb(stop_addr, 1u)) return false;
                wrote_pc = true;
                break;
        }

        if (!wrote_pc) {
            state_.regs[15] = cur_pc + 4u;
        }
    }
}

}  // namespace hl3
