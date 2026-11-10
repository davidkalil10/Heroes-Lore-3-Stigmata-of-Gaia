import sys, os
sys.path.insert(0, os.path.abspath("."))
from tools.hl3rt.runtime import Runtime
import capstone

rt = Runtime()
cs = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM)
code = rt.m.rdbytes(0xbe5c, 0x250)

for ins in cs.disasm(code, 0xbe5c):
    extra = ''
    if ins.mnemonic == 'ldr' and '[pc,' in ins.op_str:
        try:
            parts = ins.op_str.split(',')
            offset_str = parts[1].split('#')[1].rstrip(']').strip()
            offset = int(offset_str, 16)
            target_addr = ins.address + 8 + offset
            val = rt.m.rd32(target_addr)
            try:
                val_deref = rt.m.rd32(val)
                s = rt.objc.sel_name(val_deref)
                extra = f'-> [0x{target_addr:x}]=0x{val:x} -> sel "{s}"'
            except Exception:
                extra = f'-> [0x{target_addr:x}]=0x{val:x}'
        except Exception as e:
            extra = f'-> err {e}'
    print(f'  0x{ins.address:x}: {ins.mnemonic:8s} {ins.op_str:25s} {extra}')

