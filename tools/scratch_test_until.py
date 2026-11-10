import sys, os
sys.path.insert(0, os.path.abspath("."))

from tools.hl3rt.runtime import Runtime
rt = Runtime()
m = rt.m

rt.run_mod_inits()
sp = rt.setup_main_stack()
m.phase = "boot"
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR
m.uc.reg_write(UC_ARM_REG_SP, sp)
m.uc.reg_write(UC_ARM_REG_LR, 0)
m.stop_reason = None

trap_addr = m.import_addr["_UIApplicationMain"]
print(f"Starting emu until 0x{trap_addr:08x}...")
m.uc.emu_start(m.entry, trap_addr)
print("Emu stopped at until! PC=0x{:08x}".format(m.pc))
print("pending_app_main:", rt.pending_app_main)
