import sys, os
sys.path.insert(0, os.path.abspath("."))

from tools.hl3rt.runtime import Runtime
from tools.hl3rt.hle_uikit import Touch

rt = Runtime()
rt.boot_to_app_main()
rt.run_launch()

touch_responder = 0
for k, obj in rt.objc.hobj.items():
    cls, _ = rt.objc.class_of(k)
    if cls and cls.name == 'TouchResponder':
        touch_responder = k
        break

for f in range(60):
    rt.send(rt.state['delegate'], 'onTimer:', 0)

# Hook at 0xc26c (touchesBegan blx ip) and 0xc004 (touchesEnded blx ip)
from unicorn import UC_HOOK_CODE

from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_R12

def hook_began_call(uc, address, size, user):
    r0 = uc.reg_read(UC_ARM_REG_R0)
    r1 = uc.reg_read(UC_ARM_REG_R1)
    r2 = uc.reg_read(UC_ARM_REG_R2)
    r3 = uc.reg_read(UC_ARM_REG_R3)
    ip = uc.reg_read(UC_ARM_REG_R12)
    print(f"touchesBegan -> engine call to 0x{ip:08x}: r0=0x{r0:08x} r1={r1} r2={r2} r3=0x{r3:08x}")

def hook_ended_call(uc, address, size, user):
    r0 = uc.reg_read(UC_ARM_REG_R0)
    r1 = uc.reg_read(UC_ARM_REG_R1)
    r2 = uc.reg_read(UC_ARM_REG_R2)
    r3 = uc.reg_read(UC_ARM_REG_R3)
    ip = uc.reg_read(UC_ARM_REG_R12)
    print(f"touchesEnded -> engine call to 0x{ip:08x}: r0=0x{r0:08x} r1={r1} r2={r2} r3=0x{r3:08x}")


rt.m.uc.hook_add(UC_HOOK_CODE, hook_began_call, begin=0xc26c, end=0xc26c)
rt.m.uc.hook_add(UC_HOOK_CODE, hook_ended_call, begin=0xc004, end=0xc004)

def send_touch(x, y, phase_name):
    phase_code = {'began': 0, 'moved': 1, 'ended': 3}[phase_name]
    t = Touch(x, y, phase_code)
    t_obj = rt.objc.new_obj('UITouch', t)
    evt_obj = rt.objc.new_obj('UIEvent', {'touches': [t_obj]})
    set_obj = rt.objc.new_obj('NSSet', [t_obj])
    sel = f'touches{phase_name.capitalize()}:withEvent:'
    rt.send(touch_responder, sel, set_obj, evt_obj)

print("Calling touchesBegan...")
send_touch(240.0, 280.0, 'began')
print("Calling touchesEnded...")
send_touch(240.0, 280.0, 'ended')
