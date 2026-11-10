import sys, os
sys.path.insert(0, os.path.abspath("."))

import pygame
from pygame.locals import *
from OpenGL.GL import *
from PIL import Image

from tools.run_pc import PCRunner

# Run PCRunner in headless mode
runner = PCRunner(scale=2, headless_frames=150)

# Advance 80 frames to title screen
for f in range(80):
    runner.poll_input()
    if hasattr(runner.rt, "gl_bridge") and runner.rt.gl_bridge:
        runner.rt.gl_bridge.begin_frame()
    runner.rt.send(runner.delegate, "onTimer:", 0)

# Now simulate pressing Enter (touch_down on center button)
print("Simulating press of START button...")
runner.touch_down("center", (240.0, 280.0))
for f in range(4):
    runner.poll_input()
    if hasattr(runner.rt, "gl_bridge") and runner.rt.gl_bridge:
        runner.rt.gl_bridge.begin_frame()
    runner.rt.send(runner.delegate, "onTimer:", 0)

# Now release Enter (touch_up on center button)
print("Simulating release of START button...")
runner.touch_up("center")

# Step 60 frames after release
for f in range(60):
    runner.poll_input()
    if hasattr(runner.rt, "gl_bridge") and runner.rt.gl_bridge:
        runner.rt.gl_bridge.begin_frame()
    runner.rt.send(runner.delegate, "onTimer:", 0)

out_path = runner.capture_screenshot()
print(f"Captured screenshot to {out_path}")

