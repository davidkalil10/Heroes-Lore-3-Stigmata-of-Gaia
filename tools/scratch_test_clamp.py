import sys, os
sys.path.insert(0, os.path.abspath("."))

import pygame
from pygame.locals import *
from OpenGL.GL import *
from PIL import Image

from tools.hl3rt.runtime import Runtime
from tools.hl3rt.gl_bridge import GLBridge

pygame.init()
pygame.display.set_mode((960, 640), DOUBLEBUF | OPENGL | HIDDEN)

rt = Runtime()
rt.gl_bridge = GLBridge(rt.m, 960, 640)

# Hook bind_texture to enforce GL_CLAMP_TO_EDGE
orig_bind = rt.gl_bridge.bind_texture
def clamp_bind(target, tex_id):
    orig_bind(target, tex_id)
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)

rt.gl_bridge.bind_texture = clamp_bind

rt.boot_to_app_main()
rt.run_launch()

for f in range(60):
    rt.gl_bridge.begin_frame()
    rt.send(rt.state['delegate'], 'onTimer:', 0)
    pygame.display.flip()

pixels = glReadPixels(0, 0, 960, 640, GL_RGBA, GL_UNSIGNED_BYTE)
img = Image.frombytes("RGBA", (960, 640), pixels).transpose(Image.FLIP_TOP_BOTTOM)
img.save("reference/analysis/test_clamped_textures.png")
print("Saved reference/analysis/test_clamped_textures.png")

