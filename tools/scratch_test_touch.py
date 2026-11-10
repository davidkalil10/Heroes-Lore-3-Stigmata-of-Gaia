import sys, os
sys.path.insert(0, os.path.abspath("."))

from tools.hl3rt.runtime import Runtime
from tools.hl3rt.hle_uikit import Touch

print("Booting runtime...")
rt = Runtime()
rt.boot_to_app_main()
rt.run_launch()

touch_responder = 0
for k, obj in rt.objc.hobj.items():
    cls, _ = rt.objc.class_of(k)
    if cls and cls.name == "TouchResponder":
        touch_responder = k
        break

print(f"TouchResponder: 0x{touch_responder:08x}")

# Step 10 frames
print("Stepping 10 timer frames...")
for f in range(10):
    rt.send(rt.state["delegate"], "onTimer:", 0)

print("Sending touchesBegan at (240, 160)...")
t = Touch(240.0, 160.0, 0)
t_obj = rt.objc.new_obj("UITouch", t)
evt_obj = rt.objc.new_obj("UIEvent", {"touches": [t_obj]})
set_obj = rt.objc.new_obj("NSSet", [t_obj])

res_began = rt.send(touch_responder, "touchesBegan:withEvent:", set_obj, evt_obj)
print(f"touchesBegan result: {res_began}")

print("Sending touchesEnded at (240, 160)...")
t_end = Touch(240.0, 160.0, 3)
t_end_obj = rt.objc.new_obj("UITouch", t_end)
evt_end_obj = rt.objc.new_obj("UIEvent", {"touches": [t_end_obj]})
set_end_obj = rt.objc.new_obj("NSSet", [t_end_obj])

res_ended = rt.send(touch_responder, "touchesEnded:withEvent:", set_end_obj, evt_end_obj)
print(f"touchesEnded result: {res_ended}")

print("Stepping 10 more timer frames...")
for f in range(10):
    rt.send(rt.state["delegate"], "onTimer:", 0)

print("Touch test finished successfully!")

