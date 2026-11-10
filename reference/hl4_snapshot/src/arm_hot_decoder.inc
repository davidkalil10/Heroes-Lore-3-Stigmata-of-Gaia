// Pure ARM decoder adapted from the frozen HL5 cpu.cpp (reference/HL5_SNAPSHOT.json).
// No HL5 addresses/hooks/game data. Generated HL4 blocks specialize this decoder;
// Unicorn remains the independent oracle and fallback. See docs/PERFORMANCE.md.
#if defined(_MSC_VER)
#define HL4_HOT_INLINE __forceinline
#define HL4_HOT_NOINLINE __declspec(noinline)
#else
#define HL4_HOT_INLINE inline __attribute__((always_inline))
#define HL4_HOT_NOINLINE __attribute__((noinline))
#endif
struct HotArm {
 using u8=uint8_t;using u16=uint16_t;using s64=int64_t;
 u32 r[16]{};bool N=false,Z=false,C=false,V=false;u64 icount=0;uint8_t* memory;
 explicit HotArm(uint8_t* p):memory(p){}
 uint8_t* address(u32 a,size_t n){bool valid=(a>=BASE&&u64(a)+n<=BASE+0x200000)||(a>=TEMP&&u64(a)+n<=TEMP+0x100000)||(a>=TRAP&&u64(a)+n<=TRAP+0x10000)||(a>=HEAP&&u64(a)+n<=HEAP+0x4000000);if(!valid)throw std::runtime_error("Hot ARM memory bounds");return memory+a;}
 u8 load8(u32 a){return *address(a,1);}u16 load16(u32 a){u16 v;memcpy(&v,address(a,2),2);return v;}u32 load32(u32 a){u32 v;memcpy(&v,address(a,4),4);return v;}
 void store8(u32 a,u8 v){*address(a,1)=v;}void store16(u32 a,u16 v){memcpy(address(a,2),&v,2);}void store32(u32 a,u32 v){memcpy(address(a,4),&v,4);}
 static u32 countLeading(u32 v){u32 n=0;while(v&&!(v&0x80000000)){n++;v<<=1;}return v?n:32;}
 static u32 countBits(u32 v){u32 n=0;while(v){n+=v&1;v>>=1;}return n;}
 [[noreturn]] void undef(u32 pc,u32 ins,const char* why){std::ostringstream out;out<<why<<" pc="<<std::hex<<pc<<" opcode="<<ins;throw std::runtime_error(out.str());}
bool cond(u32 c) const {
    switch (c) {
        case 0: return Z;
        case 1: return !Z;
        case 2: return C;
        case 3: return !C;
        case 4: return N;
        case 5: return !N;
        case 6: return V;
        case 7: return !V;
        case 8: return C && !Z;
        case 9: return !C || Z;
        case 10: return N == V;
        case 11: return N != V;
        case 12: return !Z && (N == V);
        case 13: return Z || (N != V);
        default: return true;
    }
}

static inline u32 ror32(u32 v, u32 n) { n &= 31; return n ? (v >> n) | (v << (32 - n)) : v; }

u32 shiftImm(u32 v, u32 type, u32 amt, bool& c) {
    switch (type) {
        case 0:  // LSL
            if (amt == 0) return v;
            c = (v >> (32 - amt)) & 1;
            return v << amt;
        case 1:  // LSR
            if (amt == 0) { c = v >> 31; return 0; }
            c = (v >> (amt - 1)) & 1;
            return v >> amt;
        case 2:  // ASR
            if (amt == 0) { c = v >> 31; return (v >> 31) ? 0xFFFFFFFFu : 0; }
            c = (v >> (amt - 1)) & 1;
            return (u32)((s32)v >> amt);
        default:  // ROR / RRX
            if (amt == 0) { u32 res = ((u32)C << 31) | (v >> 1); c = v & 1; return res; }
            c = (v >> (amt - 1)) & 1;
            return ror32(v, amt);
    }
}

u32 shiftReg(u32 v, u32 type, u32 amt, bool& c) {
    amt &= 0xFF;
    if (amt == 0) return v;
    switch (type) {
        case 0:
            if (amt < 32) { c = (v >> (32 - amt)) & 1; return v << amt; }
            if (amt == 32) { c = v & 1; return 0; }
            c = false; return 0;
        case 1:
            if (amt < 32) { c = (v >> (amt - 1)) & 1; return v >> amt; }
            if (amt == 32) { c = v >> 31; return 0; }
            c = false; return 0;
        case 2:
            if (amt < 32) { c = (v >> (amt - 1)) & 1; return (u32)((s32)v >> amt); }
            c = v >> 31; return (v >> 31) ? 0xFFFFFFFFu : 0;
        default:
            if ((amt & 31) == 0) { c = v >> 31; return v; }
            c = (v >> ((amt & 31) - 1)) & 1;
            return ror32(v, amt);
    }
}

u32 addc(u32 a, u32 b, u32 cin, bool setf) {
    u64 s = (u64)a + b + cin;
    u32 res = (u32)s;
    if (setf) { C = (s >> 32) & 1; V = (((~(a ^ b)) & (a ^ res)) >> 31) & 1; }
    return res;
}

 template<u32 pc,u32 ins> HL4_HOT_INLINE void step(){
    icount++;
    u32 cc = ins >> 28;
    u32 npc = pc + 4;
    auto rd = [&](u32 n) -> u32 { return n == 15 ? pc + 8 : r[n]; };

    if (cc == 0xF) {
        if ((ins & 0xFE000000) == 0xFA000000) {  // BLX immediate (thumb target)
            undef(pc, ins, "BLX imm to Thumb unsupported");
        }
        if ((ins & 0xFD70F000) == 0xF550F000) { r[15] = npc; return; }  // PLD
        undef(pc, ins, "unsupported unconditional instruction");
    }
    if (!cond(cc)) { r[15] = npc; return; }

    switch ((ins >> 25) & 7) {
        case 0:
        case 1: {
            bool imm = (ins >> 25) & 1;
            if (!imm && ((ins & 0x90) == 0x90 || (ins & 0x01900000) == 0x01000000)) {
                if ((ins & 0x0FFFFFF0) == 0x012FFF10) {  // BX
                    u32 t = rd(ins & 15);
                    if (t & 1) undef(pc, ins, "BX to Thumb unsupported");
                    r[15] = t; return;
                }
                if ((ins & 0x0FFFFFF0) == 0x012FFF30) {  // BLX reg
                    u32 t = rd(ins & 15);
                    if (t & 1) undef(pc, ins, "BLX to Thumb unsupported");
                    r[14] = pc + 4; r[15] = t; return;
                }
                if ((ins & 0x0FFF0FF0) == 0x016F0F10) {  // CLZ
                    u32 v = rd(ins & 15);
                    r[(ins >> 12) & 15] = v ? (u32)countLeading(v) : 32;
                    r[15] = npc; return;
                }
                if ((ins & 0x0FC000F0) == 0x00000090) {  // MUL / MLA
                    u32 rdn = (ins >> 16) & 15, rn = (ins >> 12) & 15, rs = (ins >> 8) & 15, rm = ins & 15;
                    u32 res = rd(rm) * rd(rs);
                    if (ins & (1 << 21)) res += rd(rn);
                    r[rdn] = res;
                    if (ins & (1 << 20)) { N = res >> 31; Z = res == 0; }
                    r[15] = npc; return;
                }
                if ((ins & 0x0F8000F0) == 0x00800090) {  // long multiply
                    u32 hi = (ins >> 16) & 15, lo = (ins >> 12) & 15, rs = (ins >> 8) & 15, rm = ins & 15;
                    u64 res;
                    if (ins & (1 << 22)) res = (u64)((s64)(s32)rd(rm) * (s64)(s32)rd(rs));
                    else res = (u64)rd(rm) * (u64)rd(rs);
                    if (ins & (1 << 21)) res += ((u64)r[hi] << 32) | r[lo];
                    r[lo] = (u32)res; r[hi] = (u32)(res >> 32);
                    if (ins & (1 << 20)) { N = (res >> 63) & 1; Z = res == 0; }
                    r[15] = npc; return;
                }
                if ((ins & 0x0FB00FF0) == 0x01000090) {  // SWP / SWPB
                    u32 addr = rd((ins >> 16) & 15), rm = rd(ins & 15), rdn = (ins >> 12) & 15;
                    if (ins & (1 << 22)) { u32 t = load8(addr); store8(addr, (u8)rm); r[rdn] = t; }
                    else { u32 t = load32(addr); store32(addr, rm); r[rdn] = t; }
                    r[15] = npc; return;
                }
                if ((ins & 0x90) == 0x90 && (ins & 0x60) != 0) {  // halfword / signed / dword transfers
                    u32 P = (ins >> 24) & 1, U = (ins >> 23) & 1, I = (ins >> 22) & 1, W = (ins >> 21) & 1, L = (ins >> 20) & 1;
                    u32 rn = (ins >> 16) & 15, rdn = (ins >> 12) & 15, sh = (ins >> 5) & 3;
                    u32 off = I ? (((ins >> 4) & 0xF0) | (ins & 15)) : rd(ins & 15);
                    u32 base = rd(rn);
                    u32 addr = P ? (U ? base + off : base - off) : base;
                    if (L) {
                        u32 v;
                        if (sh == 1) v = load16(addr);
                        else if (sh == 2) v = (u32)(s32)(int8_t)load8(addr);
                        else v = (u32)(s32)(int16_t)load16(addr);
                        if (P && W) r[rn] = addr; else if (!P) r[rn] = U ? base + off : base - off;
                        if (rdn == 15) { r[15] = v & ~1u; return; }
                        r[rdn] = v;
                    } else if (sh == 1) {
                        store16(addr, (u16)rd(rdn));
                        if (P && W) r[rn] = addr; else if (!P) r[rn] = U ? base + off : base - off;
                    } else if (sh == 2) {  // LDRD
                        r[rdn] = load32(addr); r[rdn + 1] = load32(addr + 4);
                        if (P && W) r[rn] = addr; else if (!P) r[rn] = U ? base + off : base - off;
                    } else {               // STRD
                        store32(addr, rd(rdn)); store32(addr + 4, rd(rdn + 1));
                        if (P && W) r[rn] = addr; else if (!P) r[rn] = U ? base + off : base - off;
                    }
                    r[15] = npc; return;
                }
                if ((ins & 0x0F900090) == 0x01000080) {  // DSP multiplies SMLAxy etc.
                    u32 op = (ins >> 21) & 3, rdn = (ins >> 16) & 15, rn = (ins >> 12) & 15, rs = (ins >> 8) & 15, rm = ins & 15;
                    bool y = (ins >> 6) & 1, x = (ins >> 5) & 1;
                    s32 a = x ? (s32)rd(rm) >> 16 : (s32)(int16_t)rd(rm);
                    s32 b = y ? (s32)rd(rs) >> 16 : (s32)(int16_t)rd(rs);
                    if (op == 0) r[rdn] = (u32)(a * b) + rd(rn);
                    else if (op == 1) {
                        s64 p = ((s64)(s32)rd(rm) * (s64)b) >> 16;
                        r[rdn] = x ? (u32)p : (u32)p + rd(rn);
                    } else if (op == 2) {
                        u64 acc = ((u64)r[rdn] << 32) | r[rn];
                        acc += (u64)(s64)((s64)a * b);
                        r[rn] = (u32)acc; r[rdn] = (u32)(acc >> 32);
                    } else r[rdn] = (u32)(a * b);
                    r[15] = npc; return;
                }
                if ((ins & 0x0F900FF0) == 0x01000050) {  // QADD/QSUB/QDADD/QDSUB
                    u32 op = (ins >> 21) & 3, rdn = (ins >> 12) & 15, rn = (ins >> 16) & 15, rm = ins & 15;
                    auto sat = [](s64 v) -> s32 { return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : (s32)v; };
                    s64 a = (s32)rd(rm), b = (s32)rd(rn);
                    if (op & 2) b = sat(b * 2);
                    r[rdn] = (u32)sat((op & 1) ? a - b : a + b);
                    r[15] = npc; return;
                }
                if ((ins & 0x0FBF0FFF) == 0x010F0000) {  // MRS
                    r[(ins >> 12) & 15] = ((u32)N << 31) | ((u32)Z << 30) | ((u32)C << 29) | ((u32)V << 28) | 0x10;
                    r[15] = npc; return;
                }
                if ((ins & 0x0FB0FFF0) == 0x0120F000) {  // MSR reg
                    if ((ins >> 19) & 1) { u32 v = rd(ins & 15); N = v >> 31; Z = (v >> 30) & 1; C = (v >> 29) & 1; V = (v >> 28) & 1; }
                    r[15] = npc; return;
                }
            } else if (imm && (ins & 0x01900000) == 0x01000000) {
                if ((ins & 0x0FB0F000) == 0x0320F000) {  // MSR imm / hints (NOP)
                    if ((ins & 0x0FB00000) == 0x03200000 && ((ins >> 19) & 1) && ((ins >> 22) & 1) == 0) {
                        u32 v = ror32(ins & 0xFF, ((ins >> 8) & 15) * 2);
                        N = v >> 31; Z = (v >> 30) & 1; C = (v >> 29) & 1; V = (v >> 28) & 1;
                    }
                    r[15] = npc; return;
                }
            }
            // ---- data processing ----
            u32 opc = (ins >> 21) & 15, S = (ins >> 20) & 1;
            u32 rn = (ins >> 16) & 15, rdn = (ins >> 12) & 15;
            bool sc = C;
            u32 op2;
            if (imm) {
                u32 rot = ((ins >> 8) & 15) * 2;
                op2 = ror32(ins & 0xFF, rot);
                if (rot) sc = op2 >> 31;
            } else {
                u32 rm = ins & 15, type = (ins >> 5) & 3;
                if (ins & 0x10) {
                    u32 v = rd(rm);
                    if (rm == 15) v += 4;  // reg-shift form reads pc+12
                    u32 amt = r[(ins >> 8) & 15];
                    op2 = shiftReg(v, type, amt, sc);
                } else {
                    op2 = shiftImm(rd(rm), type, (ins >> 7) & 31, sc);
                }
            }
            u32 a = rd(rn);
            if (!imm && (ins & 0x10) && rn == 15) a += 4;
            u32 res = 0;
            bool logical = false, write = true;
            switch (opc) {
                case 0: res = a & op2; logical = true; break;
                case 1: res = a ^ op2; logical = true; break;
                case 2: res = addc(a, ~op2, 1, S); break;
                case 3: res = addc(op2, ~a, 1, S); break;
                case 4: res = addc(a, op2, 0, S); break;
                case 5: res = addc(a, op2, C, S); break;
                case 6: res = addc(a, ~op2, C, S); break;
                case 7: res = addc(op2, ~a, C, S); break;
                case 8: res = a & op2; logical = true; write = false; break;
                case 9: res = a ^ op2; logical = true; write = false; break;
                case 10: res = addc(a, ~op2, 1, true); write = false; break;
                case 11: res = addc(a, op2, 0, true); write = false; break;
                case 12: res = a | op2; logical = true; break;
                case 13: res = op2; logical = true; break;
                case 14: res = a & ~op2; logical = true; break;
                default: res = ~op2; logical = true; break;
            }
            if (S || !write) {
                if (rdn != 15 || !write) { N = res >> 31; Z = res == 0; if (logical) C = sc; }
            }
            if (write) {
                if (rdn == 15) { if (res & 1) undef(pc, ins, "data-proc branch to Thumb"); npc = res; }
                else r[rdn] = res;
            }
            r[15] = npc; return;
        }
        case 2:
        case 3: {
            if ((ins >> 25) & 1 && (ins & 0x10)) undef(pc, ins, "media/undefined instruction");
            u32 I = (ins >> 25) & 1, P = (ins >> 24) & 1, U = (ins >> 23) & 1, B = (ins >> 22) & 1, W = (ins >> 21) & 1, L = (ins >> 20) & 1;
            u32 rn = (ins >> 16) & 15, rdn = (ins >> 12) & 15;
            u32 off;
            if (!I) off = ins & 0xFFF;
            else { bool dummy = C; off = shiftImm(rd(ins & 15), (ins >> 5) & 3, (ins >> 7) & 31, dummy); }
            u32 base = rd(rn);
            u32 eff = U ? base + off : base - off;
            u32 addr = P ? eff : base;
            if (L) {
                u32 v;
                if (B) v = load8(addr);
                else { v = load32(addr & ~3u); v = ror32(v, (addr & 3) * 8); }
                if (!P) r[rn] = eff; else if (W) r[rn] = eff;
                if (rdn == 15) { if (v & 1) undef(pc, ins, "LDR pc to Thumb"); r[15] = v; return; }
                r[rdn] = v;
            } else {
                u32 v = rdn == 15 ? pc + 8 : r[rdn];
                if (B) store8(addr, (u8)v); else store32(addr & ~3u, v);
                if (!P) r[rn] = eff; else if (W) r[rn] = eff;
            }
            r[15] = npc; return;
        }
        case 4: {  // LDM/STM
            u32 P = (ins >> 24) & 1, U = (ins >> 23) & 1, W = (ins >> 21) & 1, L = (ins >> 20) & 1;
            u32 rn = (ins >> 16) & 15, list = ins & 0xFFFF;
            u32 n = (u32)countBits(list);
            u32 base = r[rn];
            u32 start = U ? (P ? base + 4 : base) : (P ? base - 4 * n : base - 4 * n + 4);
            u32 wb = U ? base + 4 * n : base - 4 * n;
            u32 a = start;
            for (u32 i = 0; i < 16; i++) {
                if (!(list & (1u << i))) continue;
                if (L) {
                    u32 v = load32(a);
                    if (i == 15) { if (v & 1) undef(pc, ins, "LDM pc to Thumb"); npc = v; }
                    else r[i] = v;
                } else {
                    store32(a, i == 15 ? pc + 8 : r[i]);
                }
                a += 4;
            }
            if (W && !(L && (list & (1u << rn)))) r[rn] = wb;
            r[15] = npc; return;
        }
        case 5: {  // B / BL
            s32 off = (s32)(ins << 8) >> 6;
            if (ins & (1u << 24)) r[14] = pc + 4;
            r[15] = pc + 8 + off; return;
        }
        default: undef(pc,ins,"unsupported hot instruction");
    }

 }
#include "arm_hot_blocks.inc"
};
#undef HL4_HOT_INLINE
#undef HL4_HOT_NOINLINE
