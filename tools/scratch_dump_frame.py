import sys, os
sys.path.insert(0, os.path.abspath("."))

import pygame
from pygame.locals import *
from OpenGL.GL import *
from PIL import Image

from tools.hl3rt.runtime import Runtime
from tools.hl3rt.gl_bridge import GLBridge

# Initialize hidden pygame opengl context
pygame.init()
pygame.display.set_mode((960, 640), DOUBLEBUF | OPENGL | HIDDEN)

rt = Runtime()
rt.gl_bridge = GLBridge(rt.m, 960, 640)

rt.boot_to_app_main()
rt.run_launch()

# Step 60 frames
for f in range(60):
    rt.gl_bridge.begin_frame()
    rt.send(rt.state['delegate'], 'onTimer:', 0)
    pygame.display.flip()

# Read pixels
pixels = glReadPixels(0, 0, 960, 640, GL_RGBA, GL_UNSIGNED_BYTE)
img = Image.frombytes("RGBA", (960, 640), pixels).transpose(Image.FLIP_TOP_BOTTOM)
img.save("reference/analysis/frame60_test.png")
print("Saved reference/analysis/frame60_test.png")

