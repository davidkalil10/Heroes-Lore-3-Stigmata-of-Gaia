#include "../../src/core/runtime.h"

#include <cstdio>
#include <string>

#include "../../src/core/arm_fast_cpu.h"
#include "../../src/core/runtime.h"

#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using hl3::u32;
using hl3::u64;

int test_arm_fast_cpu_oracle() {
    struct Case {
        const char* name;
        std::vector<u32> words;
        std::array<u32, 4> args;
        std::array<u32, 4> scratch_init;
    };

    const u32 scratch = hl3::kHeapBase + 0x40000u;
    const std::vector<Case> cases = {
        {
            "alu_flags_shifts",
            {
                0xE3A04000u,  // mov r4, #0
                0xE0905001u,  // adds r5, r0, r1 (sets C/V)
                0xE0A26003u,  // adc r6, r2, r3
                0xE0557081u,  // subs r7, r5, r1, lsl #1
                0xE0C68122u,  // sbc r8, r6, r2, lsr #2
                0xE0619042u,  // rsb r9, r1, r2, asr #0 (-> asr #32)
                0xE1E0A060u,  // mvn r10, r0, rrx (uses C!)
                0xE1C5B00Au,  // bic r11, r5, r10
                0xE1160007u,  // tst r6, r7
                0x03A04001u,  // moveq r4, #1
                0xE0250406u,  // eor r0, r5, r6
                0xE12FFF1Eu,  // bx lr
            },
            {0x80000005u, 0x80000003u, 0x12345678u, 0x000000FFu},
            {0, 0, 0, 0},
        },
        {
            "reg_shift_and_mul64",
            {
                0xE3A04003u,  // mov r4, #3
                0xE1A05410u,  // mov r5, r0, lsl r4
                0xE3A04021u,  // mov r4, #33
                0xE1B06431u,  // movs r6, r1, lsr r4
                0xE1A07450u,  // mov r7, r0, asr r4
                0xE0080291u,  // mul r8, r1, r2
                0xE0293190u,  // mla r9, r0, r1, r3
                0xE0C54392u,  // smull r4, r5, r2, r3  (RdLo=r4, RdHi=r5)
                0xE0A76190u,  // umlal r6, r7, r0, r1
                0xE16FAF18u,  // clz r10, r8
                0xE12FFF1Eu,  // bx lr
            },
            {0xFFFFFFF0u, 0x12345678u, 0xFFFFFFFCu, 0x00010003u},
            {0, 0, 0, 0},
        },
        {
            "media_and_mem_extra",
            {
                0xE6AF4070u,  // sxtb r4, r0
                0xE6BF5471u,  // sxth r5, r1, ror #8
                0xE6EF6070u,  // uxtb r6, r0
                0xE6FF7871u,  // uxth r7, r1, ror #16
                0xE6BF8F31u,  // rev r8, r1
                0xE6BF9FB1u,  // rev16 r9, r1
                0xE6FFAFB1u,  // revsh r10, r1
                0xE5828000u,  // str r8, [r2]
                0xE1C240F0u,  // strd r4, [r2, #0]  (writes r4, r5)
                0xE1C260B8u,  // strh r6, [r2, #8]
                0xE1D2B0D1u,  // ldrsb r11, [r2, #1]
                0xE1D2C0F8u,  // ldrsh r12, [r2, #8]
                0xE92D0FF0u,  // push {r4-r11}
                0xE8BD0FF0u,  // pop {r4-r11}
                0xE0840008u,  // add r0, r4, r8
                0xE12FFF1Eu,  // bx lr
            },
            {0x000000F2u, 0x89ABCDEFu, scratch, 0},
            {0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u},
        },
        {
            "vfp_math_cmp_cvt",
            {
                0xEE000A10u,  // vmov s0, r0
                0xEE001A90u,  // vmov s1, r1
                0xEEB82AC0u,  // vcvt.f32.s32 s4, s0  (12.0f)
                0xEEF82AE0u,  // vcvt.f32.s32 s5, s1  (-5.0f)
                0xEE322A22u,  // vadd.f32 s4, s4, s5  (7.0f)
                0xEE223A22u,  // vmul.f32 s6, s4, s5  (-35.0f)
                0xEEB43A42u,  // vcmp.f32 s6, s4      (-35 < 7)
                0xEEF1FA10u,  // vmrs apsr_nzcv, fpscr
                0xB3A04009u,  // movlt r4, #9
                0xEEB74AC3u,  // vcvt.f64.f32 d4, s6  (-35.0)
                0xEEB05AC4u,  // vabs.f64 d5, d4      (35.0)
                0xEEB15AC5u,  // vsqrt.f64 d5, d5
                0xEEF74BC5u,  // vcvt.f32.f64 s9, d5
                0xEEBD0AC3u,  // vcvt.s32.f32 s0, s6  (-35)
                0xEE100A10u,  // vmov r0, s0
                0xEE145A90u,  // vmov r5, s9
                0xE12FFF1Eu,  // bx lr
            },
            {12u, static_cast<u32>(-5), 0, 0},
            {0, 0, 0, 0},
        },
    };

    const u32 code_addr = hl3::kNativeStubBase + 0x400u;
    for (const auto& c : cases) {
        hl3::Machine m_uc(true);
        hl3::Machine m_fast(true);
        m_uc.set_fast_cpu(false);
        m_fast.set_fast_cpu(true);

        auto setup_case = [&](hl3::Machine& m) {
            for (size_t i = 0; i < c.words.size(); ++i) {
                u32 va = code_addr + static_cast<u32>(i * 4u);
                m.wr32(va, c.words[i]);
                m.invalidate_code_word(va);
            }
            for (size_t i = 0; i < 4; ++i) {
                m.wr32(scratch + static_cast<u32>(i * 4u), c.scratch_init[i]);
            }
            // Clear VFP d0..d15 and FPSCR cleanly before call
            u32 cpsr0 = 0x10u;
            m.set_cpsr(cpsr0);
            for (int r = 4; r <= 12; ++r) m.set_reg(r, 0u);
        };

        setup_case(m_uc);
        setup_case(m_fast);

        std::vector<u32> call_args(c.args.begin(), c.args.end());
        u32 r_uc = 0;
        u32 r_fast = 0;
        try {
            r_uc = m_uc.call(code_addr, call_args, hl3::kStackTop - 0x400u);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Unicorn threw in '%s': %s\n", c.name, e.what());
            return 8;
        }
        try {
            r_fast = m_fast.call(code_addr, call_args, hl3::kStackTop - 0x400u);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "ArmFastCpu threw in '%s': %s\n", c.name, e.what());
            return 9;
        }

        if (r_uc != r_fast) {
            std::fprintf(stderr, "Oracle mismatch in '%s': r0 uc=%08x fast=%08x\n", c.name, r_uc, r_fast);
            return 10;
        }
        for (int r = 0; r <= 12; ++r) {
            if (m_uc.reg(r) != m_fast.reg(r)) {
                std::fprintf(stderr, "Oracle mismatch in '%s': r%d uc=%08x fast=%08x\n", c.name, r, m_uc.reg(r),
                             m_fast.reg(r));
                return 11;
            }
        }
        if ((m_uc.cpsr() & 0xF0000020u) != (m_fast.cpsr() & 0xF0000020u)) {
            std::fprintf(stderr, "Oracle mismatch in '%s': cpsr uc=%08x fast=%08x\n", c.name, m_uc.cpsr(),
                         m_fast.cpsr());
            return 12;
        }
        for (size_t i = 0; i < 4; ++i) {
            u32 wu = m_uc.rd32(scratch + static_cast<u32>(i * 4u));
            u32 wf = m_fast.rd32(scratch + static_cast<u32>(i * 4u));
            if (wu != wf) {
                std::fprintf(stderr, "Oracle mem mismatch in '%s' word %zu: uc=%08x fast=%08x\n", c.name, i, wu, wf);
                return 13;
            }
        }
    }
    return 0;
}

}  // namespace

int main() {
    if (int rc = test_arm_fast_cpu_oracle(); rc != 0) {
        return rc;
    }

    // Compare Unicorn vs ArmFastCpu full Mach-O boot + launch parity
    hl3::u32 uc_heap = 0;
    size_t uc_unimpl = 0;
    for (bool fast : {false, true}) {
        hl3::Runtime rt;
        std::string err;
        if (!rt.init({}, {}, false, &err)) {
            std::fprintf(stderr, "Runtime init failed (fast=%d): %s\n", fast ? 1 : 0, err.c_str());
            return 1;
        }
        rt.machine().set_fast_cpu(fast);

        const auto& img = rt.machine().image();
        std::printf("[fast=%d] Loaded Mach-O: %zu segments, %zu imports, %zu ObjC classes, entry=%s\n",
                    fast ? 1 : 0, img.segments.size(), img.imports.size(), img.objc_classes.size(),
                    hl3::hex32(img.entry_pc).c_str());

        if (img.objc_classes.size() != 22) {
            std::fprintf(stderr, "Expected 22 ObjC classes, got %zu\n", img.objc_classes.size());
            return 2;
        }

        rt.boot_to_app_main();
        if (!rt.has_pending_app_main()) {
            std::fprintf(stderr, "Boot did not reach UIApplicationMain\n");
            return 3;
        }
        const auto& pm = rt.pending_app_main();
        if (pm.delegate != "AppDelegate") {
            std::fprintf(stderr, "Unexpected delegate: %s\n", pm.delegate.c_str());
            return 4;
        }

        rt.run_launch();
        hl3::u64 fallbacks = rt.machine().fast_cpu() ? rt.machine().fast_cpu()->fallback_count() : 0;
        hl3::u64 executed = rt.machine().fast_cpu() ? rt.machine().fast_cpu()->executed_count() : 0;
        std::printf("[fast=%d] AppDelegate launch succeeded! unimplemented=%zu heap_used=%u executed=%llu fallbacks=%llu\n",
                    fast ? 1 : 0, rt.machine().unimplemented_counts().size(), rt.machine().heap_used(),
                    static_cast<unsigned long long>(executed), static_cast<unsigned long long>(fallbacks));
        if (!fast) {
            uc_heap = rt.machine().heap_used();
            uc_unimpl = rt.machine().unimplemented_counts().size();
        } else {
            if (executed == 0) {
                std::fprintf(stderr, "ArmFastCpu did not execute any instructions!\n");
                return 5;
            }
            if (rt.machine().heap_used() != uc_heap || rt.machine().unimplemented_counts().size() != uc_unimpl) {
                std::fprintf(stderr, "ArmFastCpu parity mismatch: heap %u vs %u, unimpl %zu vs %zu\n",
                             rt.machine().heap_used(), uc_heap, rt.machine().unimplemented_counts().size(), uc_unimpl);
                return 6;
            }
            if (fallbacks > 2000) {
                std::fprintf(stderr, "Too many ArmFastCpu fallbacks during launch: %llu\n",
                             static_cast<unsigned long long>(fallbacks));
                return 7;
            }
        }
    }
    return 0;
}

