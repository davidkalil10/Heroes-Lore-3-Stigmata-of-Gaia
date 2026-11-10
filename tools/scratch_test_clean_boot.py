import sys, os
sys.path.insert(0, os.path.abspath("."))

from tools.hl3rt.runtime import Runtime
from tools.hl3rt.machine import HOST_STOP
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR

rt = Runtime()
m = rt.m
rt.run_mod_inits()
sp = rt.setup_main_stack()
m.phase = "boot"
m.uc.reg_write(UC_ARM_REG_SP, sp)
m.uc.reg_write(UC_ARM_REG_LR, HOST_STOP)
m.stop_reason = None

# Temporarily remove emu_stop from _ui_app_main
from tools.hl3rt import hle_uikit
orig_app_main = hle_uikit._ui_app_main
def safe_app_main(r):
    mm = r.m
    r.pending_app_main = {
        "argc": mm.reg(0),
        "argv": mm.reg(1),
        "principal": r.objc.pystr(mm.reg(2)),
        "delegate": r.objc.pystr(mm.reg(3)),
    }
    mm.stop_reason = None
    return 0

# Replace handler for _UIApplicationMain in trap_handlers
idx = m.trap_names.index("_UIApplicationMain")
m.trap_handlers[idx] = lambda mm: safe_app_main(rt)

print(f"Starting emu from 0x{m.entry:08x} until HOST_STOP=0x{HOST_STOP:08x}...")
m.uc.emu_start(m.entry, HOST_STOP)
print("Emu stopped naturally! Pending:", rt.pending_app_main)
