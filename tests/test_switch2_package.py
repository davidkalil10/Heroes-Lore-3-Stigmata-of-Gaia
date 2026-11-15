import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_switch2_package import verify_link_map, verify_switch2_startup


def startup(platform=2, version=1):
    raw = bytearray(0x1000)
    struct.pack_into("<I", raw, 4, 0xf0)
    raw[0x10:0x14] = b"NRO0"
    raw[0xf0:0xf4] = b"MOD0"
    at = 0x10c
    for magic in [b"LNY0", b"LNY1", b"LNY2", b"OUN0"]:
        struct.pack_into("<4sII", raw, at, magic, platform, version)
        at += 12
    return raw


class Switch2PackageTests(unittest.TestCase):
    def test_current_switch2(self):
        verify_switch2_startup(startup())

    def test_switch1_and_wrong_version(self):
        for platform, version in [(1, 1), (2, 0), (2, 2)]:
            with self.assertRaises(ValueError):
                verify_switch2_startup(startup(platform, version))

    def test_old_startup_cannot_be_fixed_by_appending_marker(self):
        raw = startup()
        raw[0x130:0x13c] = bytes(12)
        raw += struct.pack("<4sII", b"OUN0", 2, 1)
        with self.assertRaises(ValueError):
            verify_switch2_startup(raw)

    def test_truncated_and_invalid_headers(self):
        for raw in [b"", startup()[:0x135], bytes(0x1000)]:
            with self.assertRaises(ValueError):
                verify_switch2_startup(raw)

    def test_map_keeps_reusable_c_libraries(self):
        good = "/opt/devkitpro/libounce/lib/libnx.a /opt/devkitpro/portlibs/switch2/lib/libouncevk-gl.a /opt/devkitpro/portlibs/switch/lib/libSDL2.a"
        verify_link_map(good)
        for bad in ["/libnx/lib/libnx.a", "/portlibs/switch/lib/libEGL.a", "/portlibs/switch/lib/libglapi.a", "/portlibs/switch/lib/libdrm_nouveau.a"]:
            with self.assertRaises(ValueError):
                verify_link_map(good + " /opt/devkitpro" + bad)


if __name__ == "__main__":
    unittest.main()
