"""AOT Accelerated Hot Blocks with Strict Opcode Guards for Heroes Lore 3.

Generated deterministically by tools/generate_aot_blocks.py.
"""

from __future__ import annotations

import struct
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .machine import Machine


class HotBlockRegistry:
    def __init__(self):
        self.blocks = {}
        self.guards = {}
        self.hit_counts = {}

    def register(self, pc: int, guard_fn, exec_fn):
        self.guards[pc] = guard_fn
        self.blocks[pc] = exec_fn
        self.hit_counts[pc] = 0

    def try_execute(self, m: Machine, pc: int) -> int | None:
        guard = self.guards.get(pc)
        if guard and guard(m):
            exec_fn = self.blocks[pc]
            self.hit_counts[pc] += 1
            return exec_fn(m)
        return None


AOT_REGISTRY = HotBlockRegistry()

def _guard_0x0000d944(m: Machine) -> bool:
    try:
        if m.rd32(0x0000d944) != 0xe1a03841:  # asr r3, r1, #0x10
            return False
        if m.rd32(0x0000d948) != 0xe5c03000:  # strb r3, [r0]
            return False
        if m.rd32(0x0000d94c) != 0xe5c01002:  # strb r1, [r0, #2]
            return False
        if m.rd32(0x0000d950) != 0xe1a03441:  # asr r3, r1, #8
            return False
        if m.rd32(0x0000d954) != 0xe1a01c41:  # asr r1, r1, #0x18
            return False
        if m.rd32(0x0000d958) != 0xe5c03001:  # strb r3, [r0, #1]
            return False
        if m.rd32(0x0000d95c) != 0xe5c01003:  # strb r1, [r0, #3]
            return False
        if m.rd32(0x0000d960) != 0xe2800004:  # add r0, r0, #4
            return False
        return True
    except Exception:
        return False

def _exec_0x0000d944(m: Machine) -> int:
    # Hot block 0x0000d944 .. 0x0000d964 (8 insns)
    # 0x0000d944: asr r3, r1, #0x10
    # Fallback instruction execution via unicorn for asr
    m.uc.emu_start(0x0000d944, 0x0000d948, count=1)
    # 0x0000d948: strb r3, [r0]
    # Fallback instruction execution via unicorn for strb
    m.uc.emu_start(0x0000d948, 0x0000d94c, count=1)
    # 0x0000d94c: strb r1, [r0, #2]
    # Fallback instruction execution via unicorn for strb
    m.uc.emu_start(0x0000d94c, 0x0000d950, count=1)
    # 0x0000d950: asr r3, r1, #8
    # Fallback instruction execution via unicorn for asr
    m.uc.emu_start(0x0000d950, 0x0000d954, count=1)
    # 0x0000d954: asr r1, r1, #0x18
    # Fallback instruction execution via unicorn for asr
    m.uc.emu_start(0x0000d954, 0x0000d958, count=1)
    # 0x0000d958: strb r3, [r0, #1]
    # Fallback instruction execution via unicorn for strb
    m.uc.emu_start(0x0000d958, 0x0000d95c, count=1)
    # 0x0000d95c: strb r1, [r0, #3]
    # Fallback instruction execution via unicorn for strb
    m.uc.emu_start(0x0000d95c, 0x0000d960, count=1)
    # 0x0000d960: add r0, r0, #4
    m.set_reg(0, (m.reg(0) + 4) & 0xFFFFFFFF)
    return 0x0000d964

AOT_REGISTRY.register(0x0000d944, _guard_0x0000d944, _exec_0x0000d944)

def _guard_0x0000f6a0(m: Machine) -> bool:
    try:
        if m.rd32(0x0000f6a0) != 0xe92d40b0:  # push {r4, r5, r7, lr}
            return False
        if m.rd32(0x0000f6a4) != 0xe28d7008:  # add r7, sp, #8
            return False
        if m.rd32(0x0000f6a8) != 0xe24dd008:  # sub sp, sp, #8
            return False
        if m.rd32(0x0000f6ac) != 0xe59d9018:  # ldr sb, [sp, #0x18]
            return False
        if m.rd32(0x0000f6b0) != 0xe1dde1bc:  # ldrh lr, [sp, #0x1c]
            return False
        if m.rd32(0x0000f6b4) != 0xe1a05001:  # mov r5, r1
            return False
        if m.rd32(0x0000f6b8) != 0xe1530009:  # cmp r3, sb
            return False
        if m.rd32(0x0000f6bc) != 0xe6ff2072:  # uxth r2, r2
            return False
        return True
    except Exception:
        return False

def _exec_0x0000f6a0(m: Machine) -> int:
    # Hot block 0x0000f6a0 .. 0x0000f6c0 (8 insns)
    # 0x0000f6a0: push {r4, r5, r7, lr}
    # Fallback instruction execution via unicorn for push
    m.uc.emu_start(0x0000f6a0, 0x0000f6a4, count=1)
    # 0x0000f6a4: add r7, sp, #8
    m.set_reg(7, (m.reg(13) + 8) & 0xFFFFFFFF)
    # 0x0000f6a8: sub sp, sp, #8
    m.set_reg(13, (m.reg(13) - 8) & 0xFFFFFFFF)
    # 0x0000f6ac: ldr sb, [sp, #0x18]
    m.set_reg(9, m.rd32(m.sp + 24))
    # 0x0000f6b0: ldrh lr, [sp, #0x1c]
    # Fallback instruction execution via unicorn for ldrh
    m.uc.emu_start(0x0000f6b0, 0x0000f6b4, count=1)
    # 0x0000f6b4: mov r5, r1
    m.set_reg(5, m.reg(1))
    # 0x0000f6b8: cmp r3, sb
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0000f6b8, 0x0000f6bc, count=1)
    # 0x0000f6bc: uxth r2, r2
    # Fallback instruction execution via unicorn for uxth
    m.uc.emu_start(0x0000f6bc, 0x0000f6c0, count=1)
    return 0x0000f6c0

AOT_REGISTRY.register(0x0000f6a0, _guard_0x0000f6a0, _exec_0x0000f6a0)

def _guard_0x0000f6c8(m: Machine) -> bool:
    try:
        if m.rd32(0x0000f6c8) != 0xe089c003:  # add ip, sb, r3
            return False
        if m.rd32(0x0000f6cc) != 0xe1a040cc:  # asr r4, ip, #1
            return False
        if m.rd32(0x0000f6d0) != 0xe3ccc001:  # bic ip, ip, #1
            return False
        if m.rd32(0x0000f6d4) != 0xe19cc0b1:  # ldrh ip, [ip, r1]
            return False
        if m.rd32(0x0000f6d8) != 0xe152000c:  # cmp r2, ip
            return False
        if m.rd32(0x0000f6dc) != 0x06ff0074:  # uxtheq r0, r4
            return False
        if m.rd32(0x0000f6e0) != 0x0a000008:  # beq #0xf708
            return False
        if m.rd32(0x0000f6e4) != 0xe15c0002:  # cmp ip, r2
            return False
        return True
    except Exception:
        return False

def _exec_0x0000f6c8(m: Machine) -> int:
    # Hot block 0x0000f6c8 .. 0x0000f6e8 (8 insns)
    # 0x0000f6c8: add ip, sb, r3
    m.set_reg(12, (m.reg(9) + m.reg(3)) & 0xFFFFFFFF)
    # 0x0000f6cc: asr r4, ip, #1
    # Fallback instruction execution via unicorn for asr
    m.uc.emu_start(0x0000f6cc, 0x0000f6d0, count=1)
    # 0x0000f6d0: bic ip, ip, #1
    # Fallback instruction execution via unicorn for bic
    m.uc.emu_start(0x0000f6d0, 0x0000f6d4, count=1)
    # 0x0000f6d4: ldrh ip, [ip, r1]
    # Fallback instruction execution via unicorn for ldrh
    m.uc.emu_start(0x0000f6d4, 0x0000f6d8, count=1)
    # 0x0000f6d8: cmp r2, ip
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0000f6d8, 0x0000f6dc, count=1)
    # 0x0000f6dc: uxtheq r0, r4
    # Fallback instruction execution via unicorn for uxtheq
    m.uc.emu_start(0x0000f6dc, 0x0000f6e0, count=1)
    # 0x0000f6e0: beq #0xf708
    # Fallback instruction execution via unicorn for beq
    m.uc.emu_start(0x0000f6e0, 0x0000f6e4, count=1)
    # 0x0000f6e4: cmp ip, r2
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0000f6e4, 0x0000f6e8, count=1)
    return 0x0000f6e8

AOT_REGISTRY.register(0x0000f6c8, _guard_0x0000f6c8, _exec_0x0000f6c8)

def _guard_0x0000f6e4(m: Machine) -> bool:
    try:
        if m.rd32(0x0000f6e4) != 0xe15c0002:  # cmp ip, r2
            return False
        if m.rd32(0x0000f6e8) != 0x9a000003:  # bls #0xf6fc
            return False
        if m.rd32(0x0000f6ec) != 0xe244c001:  # sub ip, r4, #1
            return False
        if m.rd32(0x0000f6f0) != 0xe88d5000:  # stm sp, {ip, lr}
            return False
        return True
    except Exception:
        return False

def _exec_0x0000f6e4(m: Machine) -> int:
    # Hot block 0x0000f6e4 .. 0x0000f6f4 (4 insns)
    # 0x0000f6e4: cmp ip, r2
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0000f6e4, 0x0000f6e8, count=1)
    # 0x0000f6e8: bls #0xf6fc
    # Fallback instruction execution via unicorn for bls
    m.uc.emu_start(0x0000f6e8, 0x0000f6ec, count=1)
    # 0x0000f6ec: sub ip, r4, #1
    m.set_reg(12, (m.reg(4) - 1) & 0xFFFFFFFF)
    # 0x0000f6f0: stm sp, {ip, lr}
    # Fallback instruction execution via unicorn for stm
    m.uc.emu_start(0x0000f6f0, 0x0000f6f4, count=1)
    return 0x0000f6f4

AOT_REGISTRY.register(0x0000f6e4, _guard_0x0000f6e4, _exec_0x0000f6e4)

def _guard_0x0000f6ec(m: Machine) -> bool:
    try:
        if m.rd32(0x0000f6ec) != 0xe244c001:  # sub ip, r4, #1
            return False
        if m.rd32(0x0000f6f0) != 0xe88d5000:  # stm sp, {ip, lr}
            return False
        return True
    except Exception:
        return False

def _exec_0x0000f6ec(m: Machine) -> int:
    # Hot block 0x0000f6ec .. 0x0000f6f4 (2 insns)
    # 0x0000f6ec: sub ip, r4, #1
    m.set_reg(12, (m.reg(4) - 1) & 0xFFFFFFFF)
    # 0x0000f6f0: stm sp, {ip, lr}
    # Fallback instruction execution via unicorn for stm
    m.uc.emu_start(0x0000f6f0, 0x0000f6f4, count=1)
    return 0x0000f6f4

AOT_REGISTRY.register(0x0000f6ec, _guard_0x0000f6ec, _exec_0x0000f6ec)

def _guard_0x0000f6fc(m: Machine) -> bool:
    try:
        if m.rd32(0x0000f6fc) != 0xe88d4200:  # stm sp, {sb, lr}
            return False
        if m.rd32(0x0000f700) != 0xe2843001:  # add r3, r4, #1
            return False
        return True
    except Exception:
        return False

def _exec_0x0000f6fc(m: Machine) -> int:
    # Hot block 0x0000f6fc .. 0x0000f704 (2 insns)
    # 0x0000f6fc: stm sp, {sb, lr}
    # Fallback instruction execution via unicorn for stm
    m.uc.emu_start(0x0000f6fc, 0x0000f700, count=1)
    # 0x0000f700: add r3, r4, #1
    m.set_reg(3, (m.reg(4) + 1) & 0xFFFFFFFF)
    return 0x0000f704

AOT_REGISTRY.register(0x0000f6fc, _guard_0x0000f6fc, _exec_0x0000f6fc)

def _guard_0x00012dd8(m: Machine) -> bool:
    try:
        if m.rd32(0x00012dd8) != 0xe1d030d4:  # ldrsb r3, [r0, #4]
            return False
        if m.rd32(0x00012ddc) != 0xe3530000:  # cmp r3, #0
            return False
        if m.rd32(0x00012de0) != 0x1a000004:  # bne #0x12df8
            return False
        if m.rd32(0x00012de4) != 0xe5903008:  # ldr r3, [r0, #8]
            return False
        if m.rd32(0x00012de8) != 0xe590200c:  # ldr r2, [r0, #0xc]
            return False
        if m.rd32(0x00012dec) != 0xe2833001:  # add r3, r3, #1
            return False
        if m.rd32(0x00012df0) != 0xe1530002:  # cmp r3, r2
            return False
        if m.rd32(0x00012df4) != 0xda000003:  # ble #0x12e08
            return False
        return True
    except Exception:
        return False

def _exec_0x00012dd8(m: Machine) -> int:
    # Hot block 0x00012dd8 .. 0x00012df8 (8 insns)
    # 0x00012dd8: ldrsb r3, [r0, #4]
    # Fallback instruction execution via unicorn for ldrsb
    m.uc.emu_start(0x00012dd8, 0x00012ddc, count=1)
    # 0x00012ddc: cmp r3, #0
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x00012ddc, 0x00012de0, count=1)
    # 0x00012de0: bne #0x12df8
    # Fallback instruction execution via unicorn for bne
    m.uc.emu_start(0x00012de0, 0x00012de4, count=1)
    # 0x00012de4: ldr r3, [r0, #8]
    m.set_reg(3, m.rd32(m.reg(0) + 8))
    # 0x00012de8: ldr r2, [r0, #0xc]
    m.set_reg(2, m.rd32(m.reg(0) + 12))
    # 0x00012dec: add r3, r3, #1
    m.set_reg(3, (m.reg(3) + 1) & 0xFFFFFFFF)
    # 0x00012df0: cmp r3, r2
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x00012df0, 0x00012df4, count=1)
    # 0x00012df4: ble #0x12e08
    # Fallback instruction execution via unicorn for ble
    m.uc.emu_start(0x00012df4, 0x00012df8, count=1)
    return 0x00012df8

AOT_REGISTRY.register(0x00012dd8, _guard_0x00012dd8, _exec_0x00012dd8)

def _guard_0x00012de4(m: Machine) -> bool:
    try:
        if m.rd32(0x00012de4) != 0xe5903008:  # ldr r3, [r0, #8]
            return False
        if m.rd32(0x00012de8) != 0xe590200c:  # ldr r2, [r0, #0xc]
            return False
        if m.rd32(0x00012dec) != 0xe2833001:  # add r3, r3, #1
            return False
        if m.rd32(0x00012df0) != 0xe1530002:  # cmp r3, r2
            return False
        if m.rd32(0x00012df4) != 0xda000003:  # ble #0x12e08
            return False
        if m.rd32(0x00012df8) != 0xe3a03001:  # mov r3, #1
            return False
        if m.rd32(0x00012dfc) != 0xe5c03004:  # strb r3, [r0, #4]
            return False
        if m.rd32(0x00012e00) != 0xe3a00000:  # mov r0, #0
            return False
        return True
    except Exception:
        return False

def _exec_0x00012de4(m: Machine) -> int:
    # Hot block 0x00012de4 .. 0x00012e04 (8 insns)
    # 0x00012de4: ldr r3, [r0, #8]
    m.set_reg(3, m.rd32(m.reg(0) + 8))
    # 0x00012de8: ldr r2, [r0, #0xc]
    m.set_reg(2, m.rd32(m.reg(0) + 12))
    # 0x00012dec: add r3, r3, #1
    m.set_reg(3, (m.reg(3) + 1) & 0xFFFFFFFF)
    # 0x00012df0: cmp r3, r2
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x00012df0, 0x00012df4, count=1)
    # 0x00012df4: ble #0x12e08
    # Fallback instruction execution via unicorn for ble
    m.uc.emu_start(0x00012df4, 0x00012df8, count=1)
    # 0x00012df8: mov r3, #1
    m.set_reg(3, 1)
    # 0x00012dfc: strb r3, [r0, #4]
    # Fallback instruction execution via unicorn for strb
    m.uc.emu_start(0x00012dfc, 0x00012e00, count=1)
    # 0x00012e00: mov r0, #0
    m.set_reg(0, 0)
    return 0x00012e04

AOT_REGISTRY.register(0x00012de4, _guard_0x00012de4, _exec_0x00012de4)

def _guard_0x00075934(m: Machine) -> bool:
    try:
        if m.rd32(0x00075934) != 0xe92d4080:  # push {r7, lr}
            return False
        if m.rd32(0x00075938) != 0xe28d7000:  # add r7, sp, #0
            return False
        if m.rd32(0x0007593c) != 0xe1a00001:  # mov r0, r1
            return False
        if m.rd32(0x00075940) != 0xe1a01002:  # mov r1, r2
            return False
        return True
    except Exception:
        return False

def _exec_0x00075934(m: Machine) -> int:
    # Hot block 0x00075934 .. 0x00075944 (4 insns)
    # 0x00075934: push {r7, lr}
    # Fallback instruction execution via unicorn for push
    m.uc.emu_start(0x00075934, 0x00075938, count=1)
    # 0x00075938: add r7, sp, #0
    m.set_reg(7, (m.reg(13) + 0) & 0xFFFFFFFF)
    # 0x0007593c: mov r0, r1
    m.set_reg(0, m.reg(1))
    # 0x00075940: mov r1, r2
    m.set_reg(1, m.reg(2))
    return 0x00075944

AOT_REGISTRY.register(0x00075934, _guard_0x00075934, _exec_0x00075934)

def _guard_0x0008bfe0(m: Machine) -> bool:
    try:
        if m.rd32(0x0008bfe0) != 0xe92d40f0:  # push {r4, r5, r6, r7, lr}
            return False
        if m.rd32(0x0008bfe4) != 0xe28d700c:  # add r7, sp, #0xc
            return False
        if m.rd32(0x0008bfe8) != 0xed2d8b02:  # vpush {d8}
            return False
        if m.rd32(0x0008bfec) != 0xe24dd034:  # sub sp, sp, #0x34
            return False
        if m.rd32(0x0008bff0) != 0xee083a90:  # vmov s17, r3
            return False
        if m.rd32(0x0008bff4) != 0xe5903070:  # ldr r3, [r0, #0x70]
            return False
        if m.rd32(0x0008bff8) != 0xee082a10:  # vmov s16, r2
            return False
        if m.rd32(0x0008bffc) != 0xe1a05001:  # mov r5, r1
            return False
        return True
    except Exception:
        return False

def _exec_0x0008bfe0(m: Machine) -> int:
    # Hot block 0x0008bfe0 .. 0x0008c000 (8 insns)
    # 0x0008bfe0: push {r4, r5, r6, r7, lr}
    # Fallback instruction execution via unicorn for push
    m.uc.emu_start(0x0008bfe0, 0x0008bfe4, count=1)
    # 0x0008bfe4: add r7, sp, #0xc
    m.set_reg(7, (m.reg(13) + 12) & 0xFFFFFFFF)
    # 0x0008bfe8: vpush {d8}
    # Fallback instruction execution via unicorn for vpush
    m.uc.emu_start(0x0008bfe8, 0x0008bfec, count=1)
    # 0x0008bfec: sub sp, sp, #0x34
    m.set_reg(13, (m.reg(13) - 52) & 0xFFFFFFFF)
    # 0x0008bff0: vmov s17, r3
    # Fallback instruction execution via unicorn for vmov
    m.uc.emu_start(0x0008bff0, 0x0008bff4, count=1)
    # 0x0008bff4: ldr r3, [r0, #0x70]
    m.set_reg(3, m.rd32(m.reg(0) + 112))
    # 0x0008bff8: vmov s16, r2
    # Fallback instruction execution via unicorn for vmov
    m.uc.emu_start(0x0008bff8, 0x0008bffc, count=1)
    # 0x0008bffc: mov r5, r1
    m.set_reg(5, m.reg(1))
    return 0x0008c000

AOT_REGISTRY.register(0x0008bfe0, _guard_0x0008bfe0, _exec_0x0008bfe0)

def _guard_0x0008c000(m: Machine) -> bool:
    try:
        if m.rd32(0x0008c000) != 0xe3530000:  # cmp r3, #0
            return False
        if m.rd32(0x0008c004) != 0xe1a04000:  # mov r4, r0
            return False
        if m.rd32(0x0008c008) != 0xa5902074:  # ldrge r2, [r0, #0x74]
            return False
        if m.rd32(0x0008c00c) != 0xa7923103:  # ldrge r3, [r2, r3, lsl #2]
            return False
        if m.rd32(0x0008c010) != 0xa7d35001:  # ldrbge r5, [r3, r1]
            return False
        if m.rd32(0x0008c014) != 0xe59f3118:  # ldr r3, [pc, #0x118]
            return False
        if m.rd32(0x0008c018) != 0xe5933000:  # ldr r3, [r3]
            return False
        if m.rd32(0x0008c01c) != 0xe3530000:  # cmp r3, #0
            return False
        return True
    except Exception:
        return False

def _exec_0x0008c000(m: Machine) -> int:
    # Hot block 0x0008c000 .. 0x0008c020 (8 insns)
    # 0x0008c000: cmp r3, #0
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0008c000, 0x0008c004, count=1)
    # 0x0008c004: mov r4, r0
    m.set_reg(4, m.reg(0))
    # 0x0008c008: ldrge r2, [r0, #0x74]
    # Fallback instruction execution via unicorn for ldrge
    m.uc.emu_start(0x0008c008, 0x0008c00c, count=1)
    # 0x0008c00c: ldrge r3, [r2, r3, lsl #2]
    # Fallback instruction execution via unicorn for ldrge
    m.uc.emu_start(0x0008c00c, 0x0008c010, count=1)
    # 0x0008c010: ldrbge r5, [r3, r1]
    # Fallback instruction execution via unicorn for ldrbge
    m.uc.emu_start(0x0008c010, 0x0008c014, count=1)
    # 0x0008c014: ldr r3, [pc, #0x118]
    m.set_reg(3, m.rd32(m.reg(15) + 280))
    # 0x0008c018: ldr r3, [r3]
    m.set_reg(3, m.rd32(m.reg(3) + 0))
    # 0x0008c01c: cmp r3, #0
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0008c01c, 0x0008c020, count=1)
    return 0x0008c020

AOT_REGISTRY.register(0x0008c000, _guard_0x0008c000, _exec_0x0008c000)

def _guard_0x0008c0ac(m: Machine) -> bool:
    try:
        if m.rd32(0x0008c0ac) != 0xe3530002:  # cmp r3, #2
            return False
        if m.rd32(0x0008c0b0) != 0x1a00001b:  # bne #0x8c124
            return False
        if m.rd32(0x0008c0b4) != 0xe5903018:  # ldr r3, [r0, #0x18]
            return False
        if m.rd32(0x0008c0b8) != 0xe590001c:  # ldr r0, [r0, #0x1c]
            return False
        if m.rd32(0x0008c0bc) != 0xe1a01085:  # lsl r1, r5, #1
            return False
        if m.rd32(0x0008c0c0) != 0xe59d2050:  # ldr r2, [sp, #0x50]
            return False
        if m.rd32(0x0008c0c4) != 0xe19330b1:  # ldrh r3, [r3, r1]
            return False
        if m.rd32(0x0008c0c8) != 0xe19100b0:  # ldrh r0, [r1, r0]
            return False
        return True
    except Exception:
        return False

def _exec_0x0008c0ac(m: Machine) -> int:
    # Hot block 0x0008c0ac .. 0x0008c0cc (8 insns)
    # 0x0008c0ac: cmp r3, #2
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0008c0ac, 0x0008c0b0, count=1)
    # 0x0008c0b0: bne #0x8c124
    # Fallback instruction execution via unicorn for bne
    m.uc.emu_start(0x0008c0b0, 0x0008c0b4, count=1)
    # 0x0008c0b4: ldr r3, [r0, #0x18]
    m.set_reg(3, m.rd32(m.reg(0) + 24))
    # 0x0008c0b8: ldr r0, [r0, #0x1c]
    m.set_reg(0, m.rd32(m.reg(0) + 28))
    # 0x0008c0bc: lsl r1, r5, #1
    # Fallback instruction execution via unicorn for lsl
    m.uc.emu_start(0x0008c0bc, 0x0008c0c0, count=1)
    # 0x0008c0c0: ldr r2, [sp, #0x50]
    m.set_reg(2, m.rd32(m.sp + 80))
    # 0x0008c0c4: ldrh r3, [r3, r1]
    # Fallback instruction execution via unicorn for ldrh
    m.uc.emu_start(0x0008c0c4, 0x0008c0c8, count=1)
    # 0x0008c0c8: ldrh r0, [r1, r0]
    # Fallback instruction execution via unicorn for ldrh
    m.uc.emu_start(0x0008c0c8, 0x0008c0cc, count=1)
    return 0x0008c0cc

AOT_REGISTRY.register(0x0008c0ac, _guard_0x0008c0ac, _exec_0x0008c0ac)

def _guard_0x0008c0b4(m: Machine) -> bool:
    try:
        if m.rd32(0x0008c0b4) != 0xe5903018:  # ldr r3, [r0, #0x18]
            return False
        if m.rd32(0x0008c0b8) != 0xe590001c:  # ldr r0, [r0, #0x1c]
            return False
        if m.rd32(0x0008c0bc) != 0xe1a01085:  # lsl r1, r5, #1
            return False
        if m.rd32(0x0008c0c0) != 0xe59d2050:  # ldr r2, [sp, #0x50]
            return False
        if m.rd32(0x0008c0c4) != 0xe19330b1:  # ldrh r3, [r3, r1]
            return False
        if m.rd32(0x0008c0c8) != 0xe19100b0:  # ldrh r0, [r1, r0]
            return False
        if m.rd32(0x0008c0cc) != 0xe3120004:  # tst r2, #4
            return False
        if m.rd32(0x0008c0d0) != 0x11a02003:  # movne r2, r3
            return False
        return True
    except Exception:
        return False

def _exec_0x0008c0b4(m: Machine) -> int:
    # Hot block 0x0008c0b4 .. 0x0008c0d4 (8 insns)
    # 0x0008c0b4: ldr r3, [r0, #0x18]
    m.set_reg(3, m.rd32(m.reg(0) + 24))
    # 0x0008c0b8: ldr r0, [r0, #0x1c]
    m.set_reg(0, m.rd32(m.reg(0) + 28))
    # 0x0008c0bc: lsl r1, r5, #1
    # Fallback instruction execution via unicorn for lsl
    m.uc.emu_start(0x0008c0bc, 0x0008c0c0, count=1)
    # 0x0008c0c0: ldr r2, [sp, #0x50]
    m.set_reg(2, m.rd32(m.sp + 80))
    # 0x0008c0c4: ldrh r3, [r3, r1]
    # Fallback instruction execution via unicorn for ldrh
    m.uc.emu_start(0x0008c0c4, 0x0008c0c8, count=1)
    # 0x0008c0c8: ldrh r0, [r1, r0]
    # Fallback instruction execution via unicorn for ldrh
    m.uc.emu_start(0x0008c0c8, 0x0008c0cc, count=1)
    # 0x0008c0cc: tst r2, #4
    res = m.reg(2) & 4
    cpsr = m.cpsr
    if res == 0: cpsr |= (1 << 30)
    else: cpsr &= ~(1 << 30)
    m.cpsr = cpsr
    # 0x0008c0d0: movne r2, r3
    # Fallback instruction execution via unicorn for movne
    m.uc.emu_start(0x0008c0d0, 0x0008c0d4, count=1)
    return 0x0008c0d4

AOT_REGISTRY.register(0x0008c0b4, _guard_0x0008c0b4, _exec_0x0008c0b4)

def _guard_0x0008c11c(m: Machine) -> bool:
    try:
        if m.rd32(0x0008c11c) != 0xe894000f:  # ldm r4, {r0, r1, r2, r3}
            return False
        if m.rd32(0x0008c120) != 0xe885000f:  # stm r5, {r0, r1, r2, r3}
            return False
        if m.rd32(0x0008c124) != 0xe247d014:  # sub sp, r7, #0x14
            return False
        if m.rd32(0x0008c128) != 0xecbd8b02:  # vpop {d8}
            return False
        if m.rd32(0x0008c12c) != 0xe247d00c:  # sub sp, r7, #0xc
            return False
        return True
    except Exception:
        return False

def _exec_0x0008c11c(m: Machine) -> int:
    # Hot block 0x0008c11c .. 0x0008c130 (5 insns)
    # 0x0008c11c: ldm r4, {r0, r1, r2, r3}
    # Fallback instruction execution via unicorn for ldm
    m.uc.emu_start(0x0008c11c, 0x0008c120, count=1)
    # 0x0008c120: stm r5, {r0, r1, r2, r3}
    # Fallback instruction execution via unicorn for stm
    m.uc.emu_start(0x0008c120, 0x0008c124, count=1)
    # 0x0008c124: sub sp, r7, #0x14
    m.set_reg(13, (m.reg(7) - 20) & 0xFFFFFFFF)
    # 0x0008c128: vpop {d8}
    # Fallback instruction execution via unicorn for vpop
    m.uc.emu_start(0x0008c128, 0x0008c12c, count=1)
    # 0x0008c12c: sub sp, r7, #0xc
    m.set_reg(13, (m.reg(7) - 12) & 0xFFFFFFFF)
    return 0x0008c130

AOT_REGISTRY.register(0x0008c11c, _guard_0x0008c11c, _exec_0x0008c11c)

def _guard_0x0008c140(m: Machine) -> bool:
    try:
        if m.rd32(0x0008c140) != 0xe92d40f0:  # push {r4, r5, r6, r7, lr}
            return False
        if m.rd32(0x0008c144) != 0xe28d700c:  # add r7, sp, #0xc
            return False
        if m.rd32(0x0008c148) != 0xe52d8004:  # str r8, [sp, #-4]!
            return False
        if m.rd32(0x0008c14c) != 0xed2d8b02:  # vpush {d8}
            return False
        if m.rd32(0x0008c150) != 0xee083a10:  # vmov s16, r3
            return False
        if m.rd32(0x0008c154) != 0xe5903024:  # ldr r3, [r0, #0x24]
            return False
        if m.rd32(0x0008c158) != 0xe1a01081:  # lsl r1, r1, #1
            return False
        if m.rd32(0x0008c15c) != 0xe59dc024:  # ldr ip, [sp, #0x24]
            return False
        return True
    except Exception:
        return False

def _exec_0x0008c140(m: Machine) -> int:
    # Hot block 0x0008c140 .. 0x0008c160 (8 insns)
    # 0x0008c140: push {r4, r5, r6, r7, lr}
    # Fallback instruction execution via unicorn for push
    m.uc.emu_start(0x0008c140, 0x0008c144, count=1)
    # 0x0008c144: add r7, sp, #0xc
    m.set_reg(7, (m.reg(13) + 12) & 0xFFFFFFFF)
    # 0x0008c148: str r8, [sp, #-4]!
    m.wr32(m.sp + -4, m.reg(8))
    # 0x0008c14c: vpush {d8}
    # Fallback instruction execution via unicorn for vpush
    m.uc.emu_start(0x0008c14c, 0x0008c150, count=1)
    # 0x0008c150: vmov s16, r3
    # Fallback instruction execution via unicorn for vmov
    m.uc.emu_start(0x0008c150, 0x0008c154, count=1)
    # 0x0008c154: ldr r3, [r0, #0x24]
    m.set_reg(3, m.rd32(m.reg(0) + 36))
    # 0x0008c158: lsl r1, r1, #1
    # Fallback instruction execution via unicorn for lsl
    m.uc.emu_start(0x0008c158, 0x0008c15c, count=1)
    # 0x0008c15c: ldr ip, [sp, #0x24]
    m.set_reg(12, m.rd32(m.sp + 36))
    return 0x0008c160

AOT_REGISTRY.register(0x0008c140, _guard_0x0008c140, _exec_0x0008c140)

def _guard_0x0008c208(m: Machine) -> bool:
    try:
        if m.rd32(0x0008c208) != 0xee066a90:  # vmov s13, r6
            return False
        if m.rd32(0x0008c20c) != 0xe58d0020:  # str r0, [sp, #0x20]
            return False
        if m.rd32(0x0008c210) != 0xe1a01008:  # mov r1, r8
            return False
        if m.rd32(0x0008c214) != 0xe1a00004:  # mov r0, r4
            return False
        if m.rd32(0x0008c218) != 0xeef87ae6:  # vcvt.f32.s32 s15, s13
            return False
        if m.rd32(0x0008c21c) != 0xee065a90:  # vmov s13, r5
            return False
        if m.rd32(0x0008c220) != 0xeeb87ae6:  # vcvt.f32.s32 s14, s13
            return False
        if m.rd32(0x0008c224) != 0xee777a88:  # vadd.f32 s15, s15, s16
            return False
        return True
    except Exception:
        return False

def _exec_0x0008c208(m: Machine) -> int:
    # Hot block 0x0008c208 .. 0x0008c228 (8 insns)
    # 0x0008c208: vmov s13, r6
    # Fallback instruction execution via unicorn for vmov
    m.uc.emu_start(0x0008c208, 0x0008c20c, count=1)
    # 0x0008c20c: str r0, [sp, #0x20]
    m.wr32(m.sp + 32, m.reg(0))
    # 0x0008c210: mov r1, r8
    m.set_reg(1, m.reg(8))
    # 0x0008c214: mov r0, r4
    m.set_reg(0, m.reg(4))
    # 0x0008c218: vcvt.f32.s32 s15, s13
    # Fallback instruction execution via unicorn for vcvt.f32.s32
    m.uc.emu_start(0x0008c218, 0x0008c21c, count=1)
    # 0x0008c21c: vmov s13, r5
    # Fallback instruction execution via unicorn for vmov
    m.uc.emu_start(0x0008c21c, 0x0008c220, count=1)
    # 0x0008c220: vcvt.f32.s32 s14, s13
    # Fallback instruction execution via unicorn for vcvt.f32.s32
    m.uc.emu_start(0x0008c220, 0x0008c224, count=1)
    # 0x0008c224: vadd.f32 s15, s15, s16
    # Fallback instruction execution via unicorn for vadd.f32
    m.uc.emu_start(0x0008c224, 0x0008c228, count=1)
    return 0x0008c228

AOT_REGISTRY.register(0x0008c208, _guard_0x0008c208, _exec_0x0008c208)

def _guard_0x0008c31c(m: Machine) -> bool:
    try:
        if m.rd32(0x0008c31c) != 0xe59d3028:  # ldr r3, [sp, #0x28]
            return False
        if m.rd32(0x0008c320) != 0xe1a02004:  # mov r2, r4
            return False
        if m.rd32(0x0008c324) != 0xe1a0000a:  # mov r0, sl
            return False
        if m.rd32(0x0008c328) != 0xe58d3004:  # str r3, [sp, #4]
            return False
        if m.rd32(0x0008c32c) != 0xe1a01006:  # mov r1, r6
            return False
        if m.rd32(0x0008c330) != 0xe1a0300b:  # mov r3, fp
            return False
        if m.rd32(0x0008c334) != 0xe58d8000:  # str r8, [sp]
            return False
        return True
    except Exception:
        return False

def _exec_0x0008c31c(m: Machine) -> int:
    # Hot block 0x0008c31c .. 0x0008c338 (7 insns)
    # 0x0008c31c: ldr r3, [sp, #0x28]
    m.set_reg(3, m.rd32(m.sp + 40))
    # 0x0008c320: mov r2, r4
    m.set_reg(2, m.reg(4))
    # 0x0008c324: mov r0, sl
    m.set_reg(0, m.reg(10))
    # 0x0008c328: str r3, [sp, #4]
    m.wr32(m.sp + 4, m.reg(3))
    # 0x0008c32c: mov r1, r6
    m.set_reg(1, m.reg(6))
    # 0x0008c330: mov r3, fp
    m.set_reg(3, m.reg(11))
    # 0x0008c334: str r8, [sp]
    m.wr32(m.sp + 0, m.reg(8))
    return 0x0008c338

AOT_REGISTRY.register(0x0008c31c, _guard_0x0008c31c, _exec_0x0008c31c)

def _guard_0x0008c33c(m: Machine) -> bool:
    try:
        if m.rd32(0x0008c33c) != 0xe2844001:  # add r4, r4, #1
            return False
        if m.rd32(0x0008c340) != 0xe1540005:  # cmp r4, r5
            return False
        if m.rd32(0x0008c344) != 0xbafffff4:  # blt #0x8c31c
            return False
        if m.rd32(0x0008c348) != 0xe247d018:  # sub sp, r7, #0x18
            return False
        return True
    except Exception:
        return False

def _exec_0x0008c33c(m: Machine) -> int:
    # Hot block 0x0008c33c .. 0x0008c34c (4 insns)
    # 0x0008c33c: add r4, r4, #1
    m.set_reg(4, (m.reg(4) + 1) & 0xFFFFFFFF)
    # 0x0008c340: cmp r4, r5
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0008c340, 0x0008c344, count=1)
    # 0x0008c344: blt #0x8c31c
    # Fallback instruction execution via unicorn for blt
    m.uc.emu_start(0x0008c344, 0x0008c348, count=1)
    # 0x0008c348: sub sp, r7, #0x18
    m.set_reg(13, (m.reg(7) - 24) & 0xFFFFFFFF)
    return 0x0008c34c

AOT_REGISTRY.register(0x0008c33c, _guard_0x0008c33c, _exec_0x0008c33c)

def _guard_0x0008cd0c(m: Machine) -> bool:
    try:
        if m.rd32(0x0008cd0c) != 0xedd27a00:  # vldr s15, [r2]
            return False
        if m.rd32(0x0008cd10) != 0xedd15a00:  # vldr s11, [r1]
            return False
        if m.rd32(0x0008cd14) != 0xed916a02:  # vldr s12, [r1, #8]
            return False
        if m.rd32(0x0008cd18) != 0xed927a01:  # vldr s14, [r2, #4]
            return False
        if m.rd32(0x0008cd1c) != 0xedd16a03:  # vldr s13, [r1, #0xc]
            return False
        if m.rd32(0x0008cd20) != 0xeef45ae7:  # vcmpe.f32 s11, s15
            return False
        if m.rd32(0x0008cd24) != 0xeef1fa10:  # vmrs apsr_nzcv, fpscr
            return False
        if m.rd32(0x0008cd28) != 0x5ef05a67:  # vmovpl.f32 s11, s15
            return False
        return True
    except Exception:
        return False

def _exec_0x0008cd0c(m: Machine) -> int:
    # Hot block 0x0008cd0c .. 0x0008cd2c (8 insns)
    # 0x0008cd0c: vldr s15, [r2]
    # Fallback instruction execution via unicorn for vldr
    m.uc.emu_start(0x0008cd0c, 0x0008cd10, count=1)
    # 0x0008cd10: vldr s11, [r1]
    # Fallback instruction execution via unicorn for vldr
    m.uc.emu_start(0x0008cd10, 0x0008cd14, count=1)
    # 0x0008cd14: vldr s12, [r1, #8]
    # Fallback instruction execution via unicorn for vldr
    m.uc.emu_start(0x0008cd14, 0x0008cd18, count=1)
    # 0x0008cd18: vldr s14, [r2, #4]
    # Fallback instruction execution via unicorn for vldr
    m.uc.emu_start(0x0008cd18, 0x0008cd1c, count=1)
    # 0x0008cd1c: vldr s13, [r1, #0xc]
    # Fallback instruction execution via unicorn for vldr
    m.uc.emu_start(0x0008cd1c, 0x0008cd20, count=1)
    # 0x0008cd20: vcmpe.f32 s11, s15
    # Fallback instruction execution via unicorn for vcmpe.f32
    m.uc.emu_start(0x0008cd20, 0x0008cd24, count=1)
    # 0x0008cd24: vmrs apsr_nzcv, fpscr
    # Fallback instruction execution via unicorn for vmrs
    m.uc.emu_start(0x0008cd24, 0x0008cd28, count=1)
    # 0x0008cd28: vmovpl.f32 s11, s15
    # Fallback instruction execution via unicorn for vmovpl.f32
    m.uc.emu_start(0x0008cd28, 0x0008cd2c, count=1)
    return 0x0008cd2c

AOT_REGISTRY.register(0x0008cd0c, _guard_0x0008cd0c, _exec_0x0008cd0c)

def _guard_0x0008e3f0(m: Machine) -> bool:
    try:
        if m.rd32(0x0008e3f0) != 0xe92d4090:  # push {r4, r7, lr}
            return False
        if m.rd32(0x0008e3f4) != 0xe28d7004:  # add r7, sp, #4
            return False
        if m.rd32(0x0008e3f8) != 0xe24dd00c:  # sub sp, sp, #0xc
            return False
        if m.rd32(0x0008e3fc) != 0xe1a04000:  # mov r4, r0
            return False
        if m.rd32(0x0008e400) != 0xe58d1000:  # str r1, [sp]
            return False
        if m.rd32(0x0008e404) != 0xe1a0200d:  # mov r2, sp
            return False
        if m.rd32(0x0008e408) != 0xe5901000:  # ldr r1, [r0]
            return False
        if m.rd32(0x0008e40c) != 0xe28d0004:  # add r0, sp, #4
            return False
        return True
    except Exception:
        return False

def _exec_0x0008e3f0(m: Machine) -> int:
    # Hot block 0x0008e3f0 .. 0x0008e410 (8 insns)
    # 0x0008e3f0: push {r4, r7, lr}
    # Fallback instruction execution via unicorn for push
    m.uc.emu_start(0x0008e3f0, 0x0008e3f4, count=1)
    # 0x0008e3f4: add r7, sp, #4
    m.set_reg(7, (m.reg(13) + 4) & 0xFFFFFFFF)
    # 0x0008e3f8: sub sp, sp, #0xc
    m.set_reg(13, (m.reg(13) - 12) & 0xFFFFFFFF)
    # 0x0008e3fc: mov r4, r0
    m.set_reg(4, m.reg(0))
    # 0x0008e400: str r1, [sp]
    m.wr32(m.sp + 0, m.reg(1))
    # 0x0008e404: mov r2, sp
    m.set_reg(2, m.reg(13))
    # 0x0008e408: ldr r1, [r0]
    m.set_reg(1, m.rd32(m.reg(0) + 0))
    # 0x0008e40c: add r0, sp, #4
    m.set_reg(0, (m.reg(13) + 4) & 0xFFFFFFFF)
    return 0x0008e410

AOT_REGISTRY.register(0x0008e3f0, _guard_0x0008e3f0, _exec_0x0008e3f0)

def _guard_0x0008e438(m: Machine) -> bool:
    try:
        if m.rd32(0x0008e438) != 0xe92d40f0:  # push {r4, r5, r6, r7, lr}
            return False
        if m.rd32(0x0008e43c) != 0xe28d700c:  # add r7, sp, #0xc
            return False
        if m.rd32(0x0008e440) != 0xe92d0d00:  # push {r8, sl, fp}
            return False
        if m.rd32(0x0008e444) != 0xed2d8b10:  # vpush {d8, d9, d10, d11, d12, d13, d14, d15}
            return False
        if m.rd32(0x0008e448) != 0xe24dd040:  # sub sp, sp, #0x40
            return False
        if m.rd32(0x0008e44c) != 0xe59f30b0:  # ldr r3, [pc, #0xb0]
            return False
        if m.rd32(0x0008e450) != 0xe28d000c:  # add r0, sp, #0xc
            return False
        if m.rd32(0x0008e454) != 0xe58d702c:  # str r7, [sp, #0x2c]
            return False
        return True
    except Exception:
        return False

def _exec_0x0008e438(m: Machine) -> int:
    # Hot block 0x0008e438 .. 0x0008e458 (8 insns)
    # 0x0008e438: push {r4, r5, r6, r7, lr}
    # Fallback instruction execution via unicorn for push
    m.uc.emu_start(0x0008e438, 0x0008e43c, count=1)
    # 0x0008e43c: add r7, sp, #0xc
    m.set_reg(7, (m.reg(13) + 12) & 0xFFFFFFFF)
    # 0x0008e440: push {r8, sl, fp}
    # Fallback instruction execution via unicorn for push
    m.uc.emu_start(0x0008e440, 0x0008e444, count=1)
    # 0x0008e444: vpush {d8, d9, d10, d11, d12, d13, d14, d15}
    # Fallback instruction execution via unicorn for vpush
    m.uc.emu_start(0x0008e444, 0x0008e448, count=1)
    # 0x0008e448: sub sp, sp, #0x40
    m.set_reg(13, (m.reg(13) - 64) & 0xFFFFFFFF)
    # 0x0008e44c: ldr r3, [pc, #0xb0]
    m.set_reg(3, m.rd32(m.reg(15) + 176))
    # 0x0008e450: add r0, sp, #0xc
    m.set_reg(0, (m.reg(13) + 12) & 0xFFFFFFFF)
    # 0x0008e454: str r7, [sp, #0x2c]
    m.wr32(m.sp + 44, m.reg(7))
    return 0x0008e458

AOT_REGISTRY.register(0x0008e438, _guard_0x0008e438, _exec_0x0008e438)

def _guard_0x0008e478(m: Machine) -> bool:
    try:
        if m.rd32(0x0008e478) != 0xe59f3090:  # ldr r3, [pc, #0x90]
            return False
        if m.rd32(0x0008e47c) != 0xe5933000:  # ldr r3, [r3]
            return False
        if m.rd32(0x0008e480) != 0xe3530000:  # cmp r3, #0
            return False
        if m.rd32(0x0008e484) != 0x1a000013:  # bne #0x8e4d8
            return False
        if m.rd32(0x0008e488) != 0xe2433001:  # sub r3, r3, #1
            return False
        if m.rd32(0x0008e48c) != 0xe3a00014:  # mov r0, #0x14
            return False
        if m.rd32(0x0008e490) != 0xe58d3010:  # str r3, [sp, #0x10]
            return False
        return True
    except Exception:
        return False

def _exec_0x0008e478(m: Machine) -> int:
    # Hot block 0x0008e478 .. 0x0008e494 (7 insns)
    # 0x0008e478: ldr r3, [pc, #0x90]
    m.set_reg(3, m.rd32(m.reg(15) + 144))
    # 0x0008e47c: ldr r3, [r3]
    m.set_reg(3, m.rd32(m.reg(3) + 0))
    # 0x0008e480: cmp r3, #0
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0008e480, 0x0008e484, count=1)
    # 0x0008e484: bne #0x8e4d8
    # Fallback instruction execution via unicorn for bne
    m.uc.emu_start(0x0008e484, 0x0008e488, count=1)
    # 0x0008e488: sub r3, r3, #1
    m.set_reg(3, (m.reg(3) - 1) & 0xFFFFFFFF)
    # 0x0008e48c: mov r0, #0x14
    m.set_reg(0, 20)
    # 0x0008e490: str r3, [sp, #0x10]
    m.wr32(m.sp + 16, m.reg(3))
    return 0x0008e494

AOT_REGISTRY.register(0x0008e478, _guard_0x0008e478, _exec_0x0008e478)

def _guard_0x0008e4d8(m: Machine) -> bool:
    try:
        if m.rd32(0x0008e4d8) != 0xe59f3030:  # ldr r3, [pc, #0x30]
            return False
        if m.rd32(0x0008e4dc) != 0xe28d000c:  # add r0, sp, #0xc
            return False
        if m.rd32(0x0008e4e0) != 0xe5933000:  # ldr r3, [r3]
            return False
        if m.rd32(0x0008e4e4) != 0xe58d3000:  # str r3, [sp]
            return False
        return True
    except Exception:
        return False

def _exec_0x0008e4d8(m: Machine) -> int:
    # Hot block 0x0008e4d8 .. 0x0008e4e8 (4 insns)
    # 0x0008e4d8: ldr r3, [pc, #0x30]
    m.set_reg(3, m.rd32(m.reg(15) + 48))
    # 0x0008e4dc: add r0, sp, #0xc
    m.set_reg(0, (m.reg(13) + 12) & 0xFFFFFFFF)
    # 0x0008e4e0: ldr r3, [r3]
    m.set_reg(3, m.rd32(m.reg(3) + 0))
    # 0x0008e4e4: str r3, [sp]
    m.wr32(m.sp + 0, m.reg(3))
    return 0x0008e4e8

AOT_REGISTRY.register(0x0008e4d8, _guard_0x0008e4d8, _exec_0x0008e4d8)

def _guard_0x0008e4ec(m: Machine) -> bool:
    try:
        if m.rd32(0x0008e4ec) != 0xe59d0000:  # ldr r0, [sp]
            return False
        if m.rd32(0x0008e4f0) != 0xe247d058:  # sub sp, r7, #0x58
            return False
        if m.rd32(0x0008e4f4) != 0xecbd8b10:  # vpop {d8, d9, d10, d11, d12, d13, d14, d15}
            return False
        if m.rd32(0x0008e4f8) != 0xe247d018:  # sub sp, r7, #0x18
            return False
        return True
    except Exception:
        return False

def _exec_0x0008e4ec(m: Machine) -> int:
    # Hot block 0x0008e4ec .. 0x0008e4fc (4 insns)
    # 0x0008e4ec: ldr r0, [sp]
    m.set_reg(0, m.rd32(m.sp + 0))
    # 0x0008e4f0: sub sp, r7, #0x58
    m.set_reg(13, (m.reg(7) - 88) & 0xFFFFFFFF)
    # 0x0008e4f4: vpop {d8, d9, d10, d11, d12, d13, d14, d15}
    # Fallback instruction execution via unicorn for vpop
    m.uc.emu_start(0x0008e4f4, 0x0008e4f8, count=1)
    # 0x0008e4f8: sub sp, r7, #0x18
    m.set_reg(13, (m.reg(7) - 24) & 0xFFFFFFFF)
    return 0x0008e4fc

AOT_REGISTRY.register(0x0008e4ec, _guard_0x0008e4ec, _exec_0x0008e4ec)

def _guard_0x0008e9ac(m: Machine) -> bool:
    try:
        if m.rd32(0x0008e9ac) != 0xe5932000:  # ldr r2, [r3]
            return False
        if m.rd32(0x0008e9b0) != 0xe1540002:  # cmp r4, r2
            return False
        if m.rd32(0x0008e9b4) != 0x0a000003:  # beq #0x8e9c8
            return False
        if m.rd32(0x0008e9b8) != 0xe5933008:  # ldr r3, [r3, #8]
            return False
        if m.rd32(0x0008e9bc) != 0xe3530000:  # cmp r3, #0
            return False
        if m.rd32(0x0008e9c0) != 0x1afffff9:  # bne #0x8e9ac
            return False
        return True
    except Exception:
        return False

def _exec_0x0008e9ac(m: Machine) -> int:
    # Hot block 0x0008e9ac .. 0x0008e9c4 (6 insns)
    # 0x0008e9ac: ldr r2, [r3]
    m.set_reg(2, m.rd32(m.reg(3) + 0))
    # 0x0008e9b0: cmp r4, r2
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0008e9b0, 0x0008e9b4, count=1)
    # 0x0008e9b4: beq #0x8e9c8
    # Fallback instruction execution via unicorn for beq
    m.uc.emu_start(0x0008e9b4, 0x0008e9b8, count=1)
    # 0x0008e9b8: ldr r3, [r3, #8]
    m.set_reg(3, m.rd32(m.reg(3) + 8))
    # 0x0008e9bc: cmp r3, #0
    # Fallback instruction execution via unicorn for cmp
    m.uc.emu_start(0x0008e9bc, 0x0008e9c0, count=1)
    # 0x0008e9c0: bne #0x8e9ac
    # Fallback instruction execution via unicorn for bne
    m.uc.emu_start(0x0008e9c0, 0x0008e9c4, count=1)
    return 0x0008e9c4

AOT_REGISTRY.register(0x0008e9ac, _guard_0x0008e9ac, _exec_0x0008e9ac)

def _guard_0x000bdd28(m: Machine) -> bool:
    try:
        if m.rd32(0x000bdd28) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bdd2c) != 0xe59cf000:  # ldr pc, [ip]
            return False
        if m.rd32(0x000bdd30) != 0x000cc294:  # muleq ip, r4, r2
            return False
        if m.rd32(0x000bdd34) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bdd38) != 0xe59cf000:  # ldr pc, [ip]
            return False
        if m.rd32(0x000bdd3c) != 0x000cc298:  # muleq ip, r8, r2
            return False
        if m.rd32(0x000bdd40) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bdd44) != 0xe59cf000:  # ldr pc, [ip]
            return False
        return True
    except Exception:
        return False

def _exec_0x000bdd28(m: Machine) -> int:
    # Hot block 0x000bdd28 .. 0x000bdd48 (8 insns)
    # 0x000bdd28: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bdd2c: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    # 0x000bdd30: muleq ip, r4, r2
    # Fallback instruction execution via unicorn for muleq
    m.uc.emu_start(0x000bdd30, 0x000bdd34, count=1)
    # 0x000bdd34: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bdd38: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    # 0x000bdd3c: muleq ip, r8, r2
    # Fallback instruction execution via unicorn for muleq
    m.uc.emu_start(0x000bdd3c, 0x000bdd40, count=1)
    # 0x000bdd40: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bdd44: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    return 0x000bdd48

AOT_REGISTRY.register(0x000bdd28, _guard_0x000bdd28, _exec_0x000bdd28)

def _guard_0x000bdd40(m: Machine) -> bool:
    try:
        if m.rd32(0x000bdd40) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bdd44) != 0xe59cf000:  # ldr pc, [ip]
            return False
        if m.rd32(0x000bdd48) != 0x000cc29c:  # muleq ip, ip, r2
            return False
        if m.rd32(0x000bdd4c) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bdd50) != 0xe59cf000:  # ldr pc, [ip]
            return False
        if m.rd32(0x000bdd54) != 0x000cc2a0:  # andeq ip, ip, r0, lsr #5
            return False
        if m.rd32(0x000bdd58) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bdd5c) != 0xe59cf000:  # ldr pc, [ip]
            return False
        return True
    except Exception:
        return False

def _exec_0x000bdd40(m: Machine) -> int:
    # Hot block 0x000bdd40 .. 0x000bdd60 (8 insns)
    # 0x000bdd40: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bdd44: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    # 0x000bdd48: muleq ip, ip, r2
    # Fallback instruction execution via unicorn for muleq
    m.uc.emu_start(0x000bdd48, 0x000bdd4c, count=1)
    # 0x000bdd4c: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bdd50: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    # 0x000bdd54: andeq ip, ip, r0, lsr #5
    # Fallback instruction execution via unicorn for andeq
    m.uc.emu_start(0x000bdd54, 0x000bdd58, count=1)
    # 0x000bdd58: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bdd5c: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    return 0x000bdd60

AOT_REGISTRY.register(0x000bdd40, _guard_0x000bdd40, _exec_0x000bdd40)

def _guard_0x000bdec0(m: Machine) -> bool:
    try:
        if m.rd32(0x000bdec0) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bdec4) != 0xe59cf000:  # ldr pc, [ip]
            return False
        if m.rd32(0x000bdec8) != 0x000cc31c:  # andeq ip, ip, ip, lsl r3
            return False
        if m.rd32(0x000bdecc) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bded0) != 0xe59cf000:  # ldr pc, [ip]
            return False
        if m.rd32(0x000bded4) != 0x000cc320:  # andeq ip, ip, r0, lsr #6
            return False
        if m.rd32(0x000bded8) != 0xe59fc000:  # ldr ip, [pc]
            return False
        if m.rd32(0x000bdedc) != 0xe59cf000:  # ldr pc, [ip]
            return False
        return True
    except Exception:
        return False

def _exec_0x000bdec0(m: Machine) -> int:
    # Hot block 0x000bdec0 .. 0x000bdee0 (8 insns)
    # 0x000bdec0: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bdec4: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    # 0x000bdec8: andeq ip, ip, ip, lsl r3
    # Fallback instruction execution via unicorn for andeq
    m.uc.emu_start(0x000bdec8, 0x000bdecc, count=1)
    # 0x000bdecc: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bded0: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    # 0x000bded4: andeq ip, ip, r0, lsr #6
    # Fallback instruction execution via unicorn for andeq
    m.uc.emu_start(0x000bded4, 0x000bded8, count=1)
    # 0x000bded8: ldr ip, [pc]
    m.set_reg(12, m.rd32(m.reg(15) + 0))
    # 0x000bdedc: ldr pc, [ip]
    m.set_reg(15, m.rd32(m.reg(12) + 0))
    return 0x000bdee0

AOT_REGISTRY.register(0x000bdec0, _guard_0x000bdec0, _exec_0x000bdec0)

