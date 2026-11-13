"""Native ARMv6 fast-path stubs installed inside Unicorn RAM for ultra-hot leaf imports.

These execute directly inside Unicorn's JIT without triggering Python `UC_HOOK_CODE` exits
on every call, preserving exact ABI semantics and 8-byte stack alignment.
"""

from __future__ import annotations

import struct

NATIVE_STUB_BASE = 0x7F100100
SJLJ_TOP_SLOT = 0x7F100800


def _b_cond(cond: int, from_addr: int, to_addr: int, link: bool = False) -> int:
    offset = (to_addr - (from_addr + 8)) >> 2
    return ((cond & 0xF) << 28) | (0x5 << 25) | ((1 if link else 0) << 24) | (offset & 0xFFFFFF)


def install_native_fast_stubs(uc, base_addr: int = NATIVE_STUB_BASE, sjlj_slot: int = SJLJ_TOP_SLOT) -> dict[str, int]:
    """Assemble and write native ARMv6 leaf stubs into Unicorn memory, returning name -> addr."""
    words: list[int] = []
    stubs: dict[str, int] = {}

    def pc() -> int:
        return base_addr + len(words) * 4

    def emit(*insns: int) -> None:
        words.extend(insns)

    # _glGetError -> returns 0
    stubs["_glGetError"] = pc()
    emit(
        0xE3A00000,  # mov r0, #0
        0xE12FFF1E,  # bx lr
    )

    # __Unwind_SjLj_Register(fc=r0) -> links fc onto SJLJ_TOP_SLOT, returns 0
    stubs["__Unwind_SjLj_Register"] = pc()
    emit(
        0xE59F1010,  # ldr r1, [pc, #16] -> sjlj_slot literal below
        0xE5912000,  # ldr r2, [r1]
        0xE5802000,  # str r2, [r0]
        0xE5810000,  # str r0, [r1]
        0xE3A00000,  # mov r0, #0
        0xE12FFF1E,  # bx lr
        sjlj_slot & 0xFFFFFFFF,
    )

    # __Unwind_SjLj_Unregister(fc=r0) -> restores SJLJ_TOP_SLOT from *fc, returns 0
    stubs["__Unwind_SjLj_Unregister"] = pc()
    emit(
        0xE59F100C,  # ldr r1, [pc, #12] -> sjlj_slot literal below
        0xE5902000,  # ldr r2, [r0]
        0xE5812000,  # str r2, [r1]
        0xE3A00000,  # mov r0, #0
        0xE12FFF1E,  # bx lr
        sjlj_slot & 0xFFFFFFFF,
    )

    # ___udivsi3(n=r0, d=r1) -> r0 = q, r1 = rem (internal shared helper)
    udiv_addr = pc()
    stubs["___udivsi3"] = udiv_addr
    emit(
        0xE3510000,  # cmp r1, #0
        0x03A00000,  # moveq r0, #0
        0x03A01000,  # moveq r1, #0
        0x012FFF1E,  # bxeq lr
        0xE1500001,  # cmp r0, r1
        0x31A01000,  # movlo r1, r0
        0x33A00000,  # movlo r0, #0
        0x312FFF1E,  # bxlo lr
        0xE16F2F11,  # clz r2, r1
        0xE16F3F10,  # clz r3, r0
        0xE0422003,  # sub r2, r2, r3
        0xE1A0C211,  # mov ip, r1, lsl r2
        0xE3A03001,  # mov r3, #1
        0xE1A03213,  # mov r3, r3, lsl r2
        0xE3A02000,  # mov r2, #0
    )
    loop_addr = pc()
    emit(
        0xE150000C,  # cmp r0, ip
        0x2040000C,  # subhs r0, r0, ip
        0x20822003,  # addhs r2, r2, r3
        0xE1A0C0AC,  # mov ip, ip, lsr #1
        0xE1B030A3,  # movs r3, r3, lsr #1
    )
    emit(_b_cond(0x1, pc(), loop_addr, link=False))  # bne loop
    emit(
        0xE1A01000,  # mov r1, r0
        0xE1A00002,  # mov r0, r2
        0xE12FFF1E,  # bx lr
    )

    # ___umodsi3(n=r0, d=r1) -> r0 = rem
    stubs["___umodsi3"] = pc()
    emit(0xE92D4010)  # push {r4, lr}
    emit(_b_cond(0xE, pc(), udiv_addr, link=True))
    emit(
        0xE1A00001,  # mov r0, r1
        0xE8BD8010,  # pop {r4, pc}
    )

    # ___divsi3(a=r0, b=r1) -> r0 = signed truncating quotient
    stubs["___divsi3"] = pc()
    emit(
        0xE92D4010,  # push {r4, lr}
        0xE0204001,  # eor r4, r0, r1
        0xE3500000,  # cmp r0, #0
        0xB2600000,  # rsblt r0, r0, #0
        0xE3510000,  # cmp r1, #0
        0xB2611000,  # rsblt r1, r1, #0
    )
    emit(_b_cond(0xE, pc(), udiv_addr, link=True))
    emit(
        0xE3540000,  # cmp r4, #0
        0xB2600000,  # rsblt r0, r0, #0
        0xE8BD8010,  # pop {r4, pc}
    )

    # ___modsi3(a=r0, b=r1) -> r0 = signed truncating remainder
    stubs["___modsi3"] = pc()
    emit(
        0xE92D4010,  # push {r4, lr}
        0xE1A04000,  # mov r4, r0
        0xE3500000,  # cmp r0, #0
        0xB2600000,  # rsblt r0, r0, #0
        0xE3510000,  # cmp r1, #0
        0xB2611000,  # rsblt r1, r1, #0
    )
    emit(_b_cond(0xE, pc(), udiv_addr, link=True))
    emit(
        0xE3540000,  # cmp r4, #0
        0xB2610000,  # rsblt r0, r1, #0
        0xA1A00001,  # movge r0, r1
        0xE8BD8010,  # pop {r4, pc}
    )

    uc.mem_write(base_addr, struct.pack(f"<{len(words)}I", *words))
    uc.mem_write(sjlj_slot, struct.pack("<I", 0))
    return stubs

