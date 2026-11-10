import sys, os
sys.path.insert(0, os.path.abspath("."))

import pygame
from pygame.locals import *
from OpenGL.GL import *
from PIL import Image

from tools.hl3rt.runtime import Runtime
from tools.hl3rt.gl_bridge import GLBridge
from tools.hl3rt.hle_uikit import Touch

pygame.init()
pygame.display.set_mode((960, 640), DOUBLEBUF | OPENGL | HIDDEN)

rt = Runtime()
rt.gl_bridge = GLBridge(rt.m, 960, 640)
rt.boot_to_app_main()
rt.run_launch()

touch_responder = 0
for k, obj in rt.objc.hobj.items():
    cls, _ = rt.objc.class_of(k)
    if cls and cls.name == "TouchResponder":
        touch_responder = k
        break

print(f"TouchResponder: 0x{touch_responder:08x}")

def send_touch(x, y, phase_name):
    phase_code = {"began": 0, "moved": 1, "ended": 3}[phase_name]
    t = Touch(x, y, phase_code)
    t_obj = rt.objc.new_obj("UITouch", t)
    evt_obj = rt.objc.new_obj("UIEvent", {"touches": [t_obj]})
    set_obj = rt.objc.new_obj("NSSet", [t_obj])
    sel = f"touches{phase_name.capitalize()}:withEvent:"
    rt.send(touch_responder, sel, set_obj, evt_obj)

# Run 60 frames to reach title screen
print("Advancing to frame 60...")
for f in range(60):
    rt.gl_bridge.begin_frame()
    rt.send(rt.state['delegate'], 'onTimer:', 0)
    pygame.display.flip()

print("Clicking START GAME button at (240, 280)...")
send_touch(240.0, 280.0, "began")
for f in range(5):
    rt.gl_bridge.begin_frame()
    rt.send(rt.state['delegate'], 'onTimer:', 0)
    pygame.display.flip()

send_touch(240.0, 280.0, "ended")
print("Advancing 60 more frames after click...")
for f in range(60):
    rt.gl_bridge.begin_frame()
    rt.send(rt.state['delegate'], 'onTimer:', 0)
    pygame.display.flip()

pixels = glReadPixels(0, 0, 960, 640, GL_RGBA, GL_UNSIGNED_BYTE)
img = Image.frombytes("RGBA", (960, 640), pixels).transpose(Image.FLIP_TOP_BOTTOM)
img.save("reference/analysis/after_click_start.png")
print("Saved reference/analysis/after_click_start.png")

