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

# Clear unimplemented
rt.m.unimplemented.clear()

def send_touch(x, y, phase_name):
    phase_code = {'began': 0, 'moved': 1, 'ended': 3}[phase_name]
    t = Touch(x, y, phase_code)
    t_obj = rt.objc.new_obj('UITouch', t)
    evt_obj = rt.objc.new_obj('UIEvent', {'touches': [t_obj]})
    set_obj = rt.objc.new_obj('NSSet', [t_obj])
    sel = f'touches{phase_name.capitalize()}:withEvent:'
    rt.send(touch_responder, sel, set_obj, evt_obj)

send_touch(240.0, 280.0, 'began')
rt.send(rt.state['delegate'], 'onTimer:', 0)
send_touch(240.0, 280.0, 'ended')
for f in range(30):
    rt.send(rt.state['delegate'], 'onTimer:', 0)

print("Unimplemented after click:")
for k, v in rt.m.unimplemented.items():
    print(f"  {k}: {v}")

