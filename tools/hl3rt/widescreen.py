"""Guest Mach-O true-widescreen patching, UI centering, and HUD anchoring for Heroes Lore 3."""

from __future__ import annotations

import struct
from unicorn import UC_HOOK_CODE, arm_const

from .display import BASE_H, BASE_W, ViewportLayout

APP_MANAGER_PTR = 0x000D7DD4
GAME_CTX_ADDR = 0x000D8270

UI_ROOT_DRAW_ADDR = 0x00074DE0
UI_ROOT_DRAW_FORM_ADDR = 0x00074E1C
UI_ROOT_DRAW_DIMMER_ADDR = 0x00074E54
UI_ROOT_DRAW_POPUP_ADDR = 0x00074E8C
UI_ROOT_DRAW_EXIT_ADDR = 0x00074F00
UI_MAIN_DRAW_RET1 = 0x00051F90
UI_MAIN_DRAW_RET2 = 0x00052140
UI_GFX_FLUSH_ADDR = 0x0000C688
UI_FLUSH_RETURN_STUB = 0x7F100020
UI_ROOT_TOUCH_ADDR = 0x0007575C
ORTHO_H_POOL_ADDR = 0x0000CBC0
ORTHO_W_POOL_ADDR = 0x0000CBC4

# Verified Mach-O ARMv6 instructions comparing/loading screen width (0x1e0 = 480)
WIDTH_IMM_SITES = (
    0x0000CE04,
    0x0000CE0C,
    0x0000CE6C,
    0x0000CE94,
    0x0001878C,
    0x00019714,
    0x00019A24,
    0x0003985C,
    0x0004F8D0,
    0x00065C9C,
    0x00065CF0,
    0x00074E10,
    0x00074E68,
)

# Verified `add r3, r3, #0xa0` (computing 320 + 160 = 480) -> replaced with `mov r3, #W`
WIDTH_MOV_R3_SITES = (
    0x00039940,
    0x00039A14,
    0x00051CA8,
    0x00051E78,
    0x0006D808,
)

# Verified Mach-O ARMv6 instructions comparing/loading/subtracting screen height (0x140 = 320)
HEIGHT_IMM_SITES = (
    0x0000CDD8,
    0x0000CE10,
    0x0000CE18,
    0x0000CE70,
    0x0000CE9C,
    0x0001971C,
    0x00039868,
    0x00039930,
    0x00039A04,
    0x0004F8D8,
    0x00051C94,
    0x00051E68,
    0x00065C70,
    0x00065CC0,
    0x0006D7F8,
    0x00074E08,
)

# Verified `sub r3, r3, #0xa0` (computing 480 - 160 = 320) -> replaced with `mov r3, #H`
HEIGHT_MOV_R3_SITES = (
    0x00018794,
    0x00019A2C,
)

# Modal dimmer start origin inside UI_Root::draw (0x74de0): force (0, 0) so dimmer covers whole canvas
UI_DIMMER_ORIGIN_FIXES = (
    (0x00074E60, 0xE3A01000),  # mov r1, #0
    (0x00074E64, 0xE3A02000),  # mov r2, #0
    (0x00074E70, 0xE3A02000),  # mov r2, #0
    (0x00074E74, 0xE3A01000),  # mov r1, #0
)


def encode_arm_rot_imm(val: int) -> int:
    """Encode an integer into an ARM 12-bit rotated 8-bit immediate (`(rot4 << 8) | imm8`)."""
    v = int(val) & 0xFFFFFFFF
    for rot4 in range(16):
        rot = rot4 * 2
        # Inverse of ROR(imm8, rot) is ROL(v, rot)
        imm8 = ((v << rot) | (v >> (32 - rot))) & 0xFFFFFFFF if rot else v
        if imm8 <= 0xFF:
            return (rot4 << 8) | imm8
    raise ValueError(f"Value {val} (0x{val:x}) is not representable as an ARM rotated immediate")


def _s32(val: int) -> int:
    val &= 0xFFFFFFFF
    return val - 0x100000000 if (val & 0x80000000) else val


class WidescreenController:
    """Keeps Mach-O viewport constants, GL ortho bounds, ui_main centering, and HUD anchoring in sync."""

    def __init__(self, m, gl_bridge=None):
        self.m = m
        self.gl_bridge = gl_bridge
        self.ui_dx = 0
        self.ui_dy = 0
        self._in_ui_main_draw = False
        self._pending_flush = None
        self._last_dims: tuple[int, int, float, float] | None = None
        self._orig_words: dict[int, int] = {}
        self._dimmer_fixed = False
        self._hooks_installed = False
        self._hud_form_ptr = 0
        self._hud_nodes: dict[int, tuple[int, int, float, float, int, int]] = {}  # ptr -> (orig_x, orig_y, bcx, bcy, cur_dx, cur_dy)

    def install_hooks(self):
        if self._hooks_installed:
            return
        self._hooks_installed = True
        uc = self.m.uc
        self.m.wr32(UI_FLUSH_RETURN_STUB, 0xE1A00000)  # ARM nop return target
        uc.hook_add(UC_HOOK_CODE, self._on_ui_flush_done, begin=UI_FLUSH_RETURN_STUB, end=UI_FLUSH_RETURN_STUB)
        uc.hook_add(UC_HOOK_CODE, self._on_ui_root_draw, begin=UI_ROOT_DRAW_ADDR, end=UI_ROOT_DRAW_ADDR)
        uc.hook_add(UC_HOOK_CODE, self._on_ui_draw_enter_content, begin=UI_ROOT_DRAW_FORM_ADDR, end=UI_ROOT_DRAW_FORM_ADDR)
        uc.hook_add(UC_HOOK_CODE, self._on_ui_draw_dimmer, begin=UI_ROOT_DRAW_DIMMER_ADDR, end=UI_ROOT_DRAW_DIMMER_ADDR)
        uc.hook_add(UC_HOOK_CODE, self._on_ui_draw_enter_content, begin=UI_ROOT_DRAW_POPUP_ADDR, end=UI_ROOT_DRAW_POPUP_ADDR)
        uc.hook_add(UC_HOOK_CODE, self._on_ui_draw_exit, begin=UI_ROOT_DRAW_EXIT_ADDR, end=UI_ROOT_DRAW_EXIT_ADDR)
        uc.hook_add(UC_HOOK_CODE, self._on_ui_draw_exit_caller, begin=UI_MAIN_DRAW_RET1, end=UI_MAIN_DRAW_RET1)
        uc.hook_add(UC_HOOK_CODE, self._on_ui_draw_exit_caller, begin=UI_MAIN_DRAW_RET2, end=UI_MAIN_DRAW_RET2)
        uc.hook_add(UC_HOOK_CODE, self._on_ui_root_touch, begin=UI_ROOT_TOUCH_ADDR, end=UI_ROOT_TOUCH_ADDR)

    def _set_gl_ui_offset(self, ox: int, oy: int):
        if self.gl_bridge is not None:
            self.gl_bridge.set_ui_offset(ox, oy)

    def _flush_then_set_offset(self, uc, resume_pc: int, gfx: int, ox: int, oy: int) -> bool:
        if self.gl_bridge is None or (int(ox), int(oy)) == self.gl_bridge._ui_offset:
            return False
        if not (0x10000000 <= gfx < 0x18000000):
            self._set_gl_ui_offset(ox, oy)
            return False
        regs = (
            uc.reg_read(arm_const.UC_ARM_REG_R0),
            uc.reg_read(arm_const.UC_ARM_REG_R1),
            uc.reg_read(arm_const.UC_ARM_REG_R2),
            uc.reg_read(arm_const.UC_ARM_REG_R3),
            uc.reg_read(arm_const.UC_ARM_REG_R12),
            uc.reg_read(arm_const.UC_ARM_REG_SP),
            uc.reg_read(arm_const.UC_ARM_REG_LR),
            uc.reg_read(arm_const.UC_ARM_REG_CPSR),
        )
        self._pending_flush = (regs, resume_pc, int(ox), int(oy))
        uc.reg_write(arm_const.UC_ARM_REG_R0, gfx)
        uc.reg_write(arm_const.UC_ARM_REG_SP, regs[5] & ~7)
        uc.reg_write(arm_const.UC_ARM_REG_LR, UI_FLUSH_RETURN_STUB)
        uc.reg_write(arm_const.UC_ARM_REG_CPSR, regs[7] & ~0x20)
        uc.reg_write(arm_const.UC_ARM_REG_PC, UI_GFX_FLUSH_ADDR)
        return True

    def _on_ui_flush_done(self, uc, address, size, user):
        pf = self._pending_flush
        if pf is None:
            return
        self._pending_flush = None
        regs, resume_pc, ox, oy = pf
        uc.reg_write(arm_const.UC_ARM_REG_R0, regs[0])
        uc.reg_write(arm_const.UC_ARM_REG_R1, regs[1])
        uc.reg_write(arm_const.UC_ARM_REG_R2, regs[2])
        uc.reg_write(arm_const.UC_ARM_REG_R3, regs[3])
        uc.reg_write(arm_const.UC_ARM_REG_R12, regs[4])
        uc.reg_write(arm_const.UC_ARM_REG_SP, regs[5])
        uc.reg_write(arm_const.UC_ARM_REG_LR, regs[6])
        uc.reg_write(arm_const.UC_ARM_REG_CPSR, regs[7])
        self._set_gl_ui_offset(ox, oy)
        uc.reg_write(arm_const.UC_ARM_REG_PC, resume_pc)

    def _on_ui_root_draw(self, uc, address, size, user):
        self._in_ui_main_draw = False
        dx, dy = self.ui_dx, self.ui_dy
        if dx == 0 and dy == 0:
            return
        m = self.m
        app = m.rd32(APP_MANAGER_PTR)
        if not app:
            return
        if m.reg(0) != m.rd32(app + 0x40):
            return
        if m.rd32(app + 0x78) in (0, 1, 256):
            return
        self._in_ui_main_draw = True

    def _on_ui_draw_enter_content(self, uc, address, size, user):
        if self._in_ui_main_draw:
            gfx = uc.reg_read(arm_const.UC_ARM_REG_R5)
            self._flush_then_set_offset(uc, address, gfx, self.ui_dx, self.ui_dy)

    def _on_ui_draw_dimmer(self, uc, address, size, user):
        if self._in_ui_main_draw:
            gfx = uc.reg_read(arm_const.UC_ARM_REG_R5)
            self._flush_then_set_offset(uc, address, gfx, 0, 0)

    def _on_ui_draw_exit(self, uc, address, size, user):
        if self._in_ui_main_draw:
            self._in_ui_main_draw = False
            gfx = uc.reg_read(arm_const.UC_ARM_REG_R5)
            self._flush_then_set_offset(uc, address, gfx, 0, 0)

    def _on_ui_draw_exit_caller(self, uc, address, size, user):
        if self._in_ui_main_draw:
            self._in_ui_main_draw = False
            gfx = self.m.rd32(uc.reg_read(arm_const.UC_ARM_REG_R5) + 0x14)
            self._flush_then_set_offset(uc, address, gfx, 0, 0)

    def _on_ui_root_touch(self, uc, address, size, user):
        dx, dy = self.ui_dx, self.ui_dy
        if dx == 0 and dy == 0:
            return
        m = self.m
        app = m.rd32(APP_MANAGER_PTR)
        if not app:
            return
        if m.reg(0) != m.rd32(app + 0x40):
            return
        if m.rd32(app + 0x78) in (0, 1, 256):
            return
        m.set_reg(3, (_s32(m.reg(3)) - dx) & 0xFFFFFFFF)
        sp = m.sp
        m.wr32(sp, (_s32(m.rd32(sp)) - dy) & 0xFFFFFFFF)

    def _write_code32(self, va: int, word: int):
        if va not in self._orig_words:
            self._orig_words[va] = self.m.rd32(va)
        self.m.wr32(va, word)
        if hasattr(self.m.uc, "ctl_remove_cache"):
            try:
                self.m.uc.ctl_remove_cache(va, va + 4)
            except Exception:
                pass

    def _patch_imm(self, va: int, val: int):
        if va not in self._orig_words:
            self._orig_words[va] = self.m.rd32(va)
        orig = self._orig_words[va]
        enc = encode_arm_rot_imm(val)
        self._write_code32(va, (orig & 0xFFFFF000) | enc)

    def _patch_mov_rd(self, va: int, rd: int, val: int):
        if va not in self._orig_words:
            self._orig_words[va] = self.m.rd32(va)
        orig = self._orig_words[va]
        cond = orig & 0xF0000000
        enc = encode_arm_rot_imm(val)
        self._write_code32(va, cond | 0x03A00000 | ((rd & 0xF) << 12) | enc)

    def _patch_tooltip_clamp(self, patch_w: int):
        enc_w = encode_arm_rot_imm(patch_w)
        self._patch_imm(0x0003B96C, patch_w)
        # gt condition (0xC): addgt r3, r1, #patch_w ; subgt r3, r3, #1
        self._write_code32(0x0003B970, 0xC2813000 | enc_w)
        self._write_code32(0x0003B974, 0xC2433001)

    def apply_layout(self, layout: ViewportLayout):
        """Sync guest binary constants, ortho float pool, GameCtx/Map bounds, ui_main offset, and HUD nodes."""
        self.install_hooks()
        self.ui_dx = layout.ui_dx
        self.ui_dy = layout.ui_dy

        dims = (layout.patch_w, layout.patch_h, layout.ortho_w, layout.ortho_h)
        if self._last_dims != dims:
            self._last_dims = dims
            pw, ph = layout.patch_w, layout.patch_h

            if not self._dimmer_fixed:
                self._dimmer_fixed = True
                for va, word in UI_DIMMER_ORIGIN_FIXES:
                    self._write_code32(va, word)

            for va in WIDTH_IMM_SITES:
                self._patch_imm(va, pw)
            for va in WIDTH_MOV_R3_SITES:
                self._patch_mov_rd(va, 3, pw)
            for va in HEIGHT_IMM_SITES:
                self._patch_imm(va, ph)
            for va in HEIGHT_MOV_R3_SITES:
                self._patch_mov_rd(va, 3, ph)
            self._patch_tooltip_clamp(pw)

            self.m.wrbytes(ORTHO_H_POOL_ADDR, struct.pack("<f", float(layout.ortho_h)))
            self.m.wrbytes(ORTHO_W_POOL_ADDR, struct.pack("<f", float(layout.ortho_w)))

        self._sync_live_structs(layout)
        self._sync_hud_nodes(layout)

    def _sync_live_structs(self, layout: ViewportLayout):
        m = self.m
        ctx = GAME_CTX_ADDR
        pw, ph = layout.patch_w, layout.patch_h
        cur_w = m.rd32(ctx + 0x40)
        cur_h = m.rd32(ctx + 0x44)
        if cur_w == 0 and cur_h == 0:
            return
        if cur_w != pw or cur_h != ph:
            m.wr32(ctx + 0x40, pw)
            m.wr32(ctx + 0x44, ph)
        cam_base = ctx + 0x5000
        map_ptr = m.rd32(cam_base + 0xC88)
        if 0x10000000 <= map_ptr < 0x18000000:
            vh_map = max(1, ph - 33)
            cam_cx = max(0, (pw - 24) // 2)
            cam_cy = max(0, vh_map // 2)
            if m.rd32(cam_base + 0xE60) != pw or m.rd32(cam_base + 0xE64) != vh_map:
                m.wr32(cam_base + 0xE60, pw)
                m.wr32(cam_base + 0xE64, vh_map)
                m.wr32(cam_base + 0xE78, cam_cx)
                m.wr32(cam_base + 0xE7C, cam_cy)

    def _sync_hud_nodes(self, layout: ViewportLayout):
        m = self.m
        app = m.rd32(APP_MANAGER_PTR)
        if not (0x10000000 <= app < 0x18000000):
            return
        ui_hud = m.rd32(app + 0x44)
        if not (0x10000000 <= ui_hud < 0x18000000):
            return
        form_ptr = m.rd32(ui_hud + 0x98)
        if not (0x10000000 <= form_ptr < 0x18000000):
            return
        if m.rd16(form_ptr + 0x14) != 70:
            return

        if self._hud_form_ptr != form_ptr:
            self._hud_form_ptr = form_ptr
            self._hud_nodes.clear()

        self._walk_and_anchor_hud(m.rd32(form_ptr + 0x38), layout)

    def _walk_and_anchor_hud(self, head_ptr: int, layout: ViewportLayout):
        m = self.m
        seen = set()
        stack = [head_ptr]
        while stack:
            ptr = stack.pop()
            if not (0x10000000 <= ptr < 0x18000000) or ptr in seen:
                continue
            seen.add(ptr)

            if ptr not in self._hud_nodes:
                px, py, pw, ph = struct.unpack("<4h", m.rdbytes(ptr + 0x08, 8))
                fx0, fx1, fy0, fy1 = struct.unpack("<4f", m.rdbytes(ptr + 0x18, 16))
                bcx = (fx0 + fx1) * 0.5 if (fx1 - fx0) >= 2.0 else (px + pw * 0.5)
                bcy = (fy0 + fy1) * 0.5 if (fy1 - fy0) >= 2.0 else (py + ph * 0.5)
                self._hud_nodes[ptr] = (px, py, bcx, bcy, 0, 0)

            orig_px, orig_py, bcx, bcy, cur_dx, cur_dy = self._hud_nodes[ptr]
            _, _, pw, ph = struct.unpack("<4h", m.rdbytes(ptr + 0x08, 8))
            fx0_chk, fx1_chk, _, _ = struct.unpack("<4f", m.rdbytes(ptr + 0x18, 16))
            if pw >= 350 or (fx1_chk - fx0_chk) >= 350.0:
                nxt = m.rd32(ptr + 0x30)
                cld = m.rd32(ptr + 0x38)
                if nxt:
                    stack.append(nxt)
                if cld:
                    stack.append(cld)
                continue

            if bcx < 150.0:
                dx = 0
            elif bcx <= 330.0:
                dx = layout.hud_dx_center
            else:
                dx = layout.hud_dx_right

            if bcy < 100.0:
                dy = 0
            elif bcy < 180.0:
                dy = layout.hud_dy_bottom // 2
            else:
                dy = layout.hud_dy_bottom

            px, py = struct.unpack("<2h", m.rdbytes(ptr + 0x08, 4))
            # If the game rebuilt/reset this node back to its unshifted origin, reset tracked delta
            if cur_dx != 0 and px == orig_px and py == orig_py:
                cur_dx, cur_dy = 0, 0

            ddx = dx - cur_dx
            ddy = dy - cur_dy
            if ddx != 0 or ddy != 0:
                m.wrbytes(ptr + 0x08, struct.pack("<2h", px + ddx, py + ddy))
                fx0, fx1, fy0, fy1 = struct.unpack("<4f", m.rdbytes(ptr + 0x18, 16))
                if (fx1 - fx0) >= 1.0 or (fy1 - fy0) >= 1.0:
                    m.wrbytes(ptr + 0x18, struct.pack("<4f", fx0 + ddx, fx1 + ddx, fy0 + ddy, fy1 + ddy))
                self._hud_nodes[ptr] = (orig_px, orig_py, bcx, bcy, dx, dy)

            nxt = m.rd32(ptr + 0x30)
            cld = m.rd32(ptr + 0x38)
            if nxt:
                stack.append(nxt)
            if cld:
                stack.append(cld)
