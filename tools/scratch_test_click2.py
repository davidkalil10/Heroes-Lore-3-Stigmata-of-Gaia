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
    if cls and cls.name == 'TouchResponder':
        touch_responder = k
        break

print(f"TouchResponder: 0x{touch_responder:08x}")

for f in range(60):
    rt.send(rt.state['delegate'], 'onTimer:', 0)

print("Starting touch with persistent UITouch instance...")
# Create a single persistent Touch and UITouch object!
t = Touch(240.0, 280.0, phase=0)
t_obj = rt.objc.new_obj('UITouch', t)
evt_obj = rt.objc.new_obj('UIEvent', {'touches': [t_obj]})
set_obj = rt.objc.new_obj('NSSet', [t_obj])

# 1. touchesBegan
rt.send(touch_responder, "touchesBegan:withEvent:", set_obj, evt_obj)
print("touchesBegan sent.")

# 2. Step 3 timer frames
for f in range(3):
    rt.send(rt.state['delegate'], 'onTimer:', 0)

# 3. Update the SAME touch to ended (phase = 3)
t.phase = 3
rt.send(touch_responder, "touchesEnded:withEvent:", set_obj, evt_obj)
print("touchesEnded sent.")

# 4. Step 60 frames after release
for f in range(60):
    rt.send(rt.state['delegate'], 'onTimer:', 0)

pixels = glReadPixels(0, 0, 960, 640, GL_RGBA, GL_UNSIGNED_BYTE)
img = Image.frombytes("RGBA", (960, 640), pixels).transpose(Image.FLIP_TOP_BOTTOM)
img.save("reference/analysis/after_reused_touch_ended.png")
print("Saved reference/analysis/after_reused_touch_ended.png")

