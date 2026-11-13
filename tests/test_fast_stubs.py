"""Unicorn oracle verification for native ARMv6 fast-path stubs."""

from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

from unicorn import UC_ARCH_ARM, UC_MODE_ARM, Uc
from unicorn.arm_const import UC_ARM_REG_CPSR, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_SP

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from hl3rt.fast_stubs import SJLJ_TOP_SLOT, install_native_fast_stubs  # noqa: E402
from hl3rt.hle_libc import _trunc_div  # noqa: E402


HOST_STOP = 0x7F100010
STACK_BASE = 0xBFF00000
STACK_SIZE = 0x00100000


class FastStubsOracleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        cls.uc.mem_map(0x7F100000, 0x1000)
        cls.uc.mem_map(STACK_BASE, STACK_SIZE)
        cls.uc.mem_map(0x10000000, 0x1000)  # test heap scratch
        cls.uc.mem_write(HOST_STOP, struct.pack("<I", 0xE12FFF1E))
        cls.stubs = install_native_fast_stubs(cls.uc)

    def _call(self, name: str, r0: int = 0, r1: int = 0) -> tuple[int, int]:
        uc = self.uc
        uc.reg_write(UC_ARM_REG_R0, r0 & 0xFFFFFFFF)
        uc.reg_write(UC_ARM_REG_R1, r1 & 0xFFFFFFFF)
        uc.reg_write(UC_ARM_REG_SP, STACK_BASE + STACK_SIZE - 0x100)
        uc.reg_write(UC_ARM_REG_LR, HOST_STOP)
        cpsr = uc.reg_read(UC_ARM_REG_CPSR) & ~0x20  # ARM mode
        uc.reg_write(UC_ARM_REG_CPSR, cpsr)
        uc.emu_start(self.stubs[name], HOST_STOP, count=500)
        return uc.reg_read(UC_ARM_REG_R0), uc.reg_read(UC_ARM_REG_R1)

    def test_gl_get_error(self):
        r0, _ = self._call("_glGetError", 123, 456)
        self.assertEqual(r0, 0)

    def test_sjlj_register_unregister_chain(self):
        self.uc.mem_write(SJLJ_TOP_SLOT, struct.pack("<I", 0))
        fc1, fc2 = 0x10000100, 0x10000200
        self.uc.mem_write(fc1, struct.pack("<I", 0xDEADBEEF))
        self.uc.mem_write(fc2, struct.pack("<I", 0xCAFEBABE))

        r0, _ = self._call("__Unwind_SjLj_Register", fc1)
        self.assertEqual(r0, 0)
        self.assertEqual(struct.unpack("<I", self.uc.mem_read(fc1, 4))[0], 0)
        self.assertEqual(struct.unpack("<I", self.uc.mem_read(SJLJ_TOP_SLOT, 4))[0], fc1)

        r0, _ = self._call("__Unwind_SjLj_Register", fc2)
        self.assertEqual(r0, 0)
        self.assertEqual(struct.unpack("<I", self.uc.mem_read(fc2, 4))[0], fc1)
        self.assertEqual(struct.unpack("<I", self.uc.mem_read(SJLJ_TOP_SLOT, 4))[0], fc2)

        r0, _ = self._call("__Unwind_SjLj_Unregister", fc2)
        self.assertEqual(r0, 0)
        self.assertEqual(struct.unpack("<I", self.uc.mem_read(SJLJ_TOP_SLOT, 4))[0], fc1)

        r0, _ = self._call("__Unwind_SjLj_Unregister", fc1)
        self.assertEqual(r0, 0)
        self.assertEqual(struct.unpack("<I", self.uc.mem_read(SJLJ_TOP_SLOT, 4))[0], 0)

    def test_udiv_umod_oracle(self):
        cases = [
            (0, 0),
            (42, 0),
            (0, 1),
            (0, 7),
            (1, 1),
            (5, 10),
            (10, 5),
            (480, 24),
            (481, 24),
            (0xFFFFFFFF, 1),
            (0xFFFFFFFF, 2),
            (0xFFFFFFFF, 0xFFFFFFFF),
            (0xFFFFFFFF, 0x80000000),
            (0x80000000, 0x80000001),
            (0x12345678, 0x9ABC),
            (1000000, 33),
            (1000, 60),
        ]
        for n, d in cases:
            expected_q = 0 if d == 0 else (n // d)
            expected_r = 0 if d == 0 else (n % d)
            got_q, got_r_pair = self._call("___udivsi3", n, d)
            self.assertEqual((got_q, got_r_pair), (expected_q, expected_r), f"udivsi3({n}, {d})")
            got_umod, _ = self._call("___umodsi3", n, d)
            self.assertEqual(got_umod, expected_r, f"umodsi3({n}, {d})")

    def test_divsi3_modsi3_oracle(self):
        vals = [0, 1, -1, 2, -2, 7, -7, 24, -24, 480, -480, 0x7FFFFFFF, -0x7FFFFFFF, -0x80000000, 123456, -123456]
        for a in vals:
            for b in vals:
                expected_q = 0 if b == 0 else (_trunc_div(a, b) & 0xFFFFFFFF)
                expected_r = 0 if b == 0 else ((a - _trunc_div(a, b) * b) & 0xFFFFFFFF)
                got_q, _ = self._call("___divsi3", a, b)
                self.assertEqual(got_q, expected_q, f"divsi3({a}, {b})")
                got_r, _ = self._call("___modsi3", a, b)
                self.assertEqual(got_r, expected_r, f"modsi3({a}, {b})")


if __name__ == "__main__":
    unittest.main()

