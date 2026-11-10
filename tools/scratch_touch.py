import sys, os
sys.path.insert(0, os.path.abspath("."))
from tools.hl3rt.runtime import Runtime
import capstone

rt = Runtime()

# Read PC-relative values in touchesBegan:
# 0xc0b4: ldr r1, [pc, #0x230] -> pc is 0xc0b4 + 8 = 0xc0bc + 0x230 = 0xc2ec
targets = [
    (0xc0b4, 0x230),
    (0xc0c8, 0x220),
    (0xc11c, 0x1d0),
    (0xc130, 0x1c0),
    (0xc138, 0x1bc),
    (0xc148, 0x1b0),
    (0xc158, 0x1a4),
]

for ins_addr, off in targets:
    t_addr = ins_addr + 8 + off
    ptr_val = rt.m.rd32(t_addr)
    deref = rt.m.rd32(ptr_val)
    name = rt.m.cstr(deref)
    print(f"At 0x{ins_addr:x} ldr from 0x{t_addr:x}: ptr=0x{ptr_val:x} -> sel='{name}'")

