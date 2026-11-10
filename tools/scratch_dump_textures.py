import sys, os
sys.path.insert(0, os.path.abspath("."))

from tools.hl3rt.runtime import Runtime
from PIL import Image

rt = Runtime()
rt.boot_to_app_main()
rt.run_launch()

# Let's inspect glTexImage2D calls
m = rt.m
orig_handlers = list(m.trap_handlers)

tex_data = {}

for idx, name in enumerate(m.trap_names):
    if name == '_glTexImage2D':
        def make_t(orig):
            def w(mm):
                target, level, ifmt, w, h = [mm.reg(i) for i in range(4)] + [mm.rd32(mm.sp)]
                border, fmt, tp, pix = [mm.rd32(mm.sp + 4 * i) for i in range(1, 5)]
                print(f"glTexImage2D: {w}x{h} fmt=0x{fmt:x} tp=0x{tp:x} ptr=0x{pix:x}")
                if pix != 0:
                    b = mm.rdbytes(pix, w * h * 4)
                    tex_data[f"{w}x{h}_{len(tex_data)}"] = (w, h, b)
                return orig(mm)
            return w
        m.trap_handlers[idx] = make_t(orig_handlers[idx])

for f in range(60):
    rt.send(rt.state['delegate'], 'onTimer:', 0)

out_dir = "reference/analysis/dumped_textures"
os.makedirs(out_dir, exist_ok=True)
for k, (w, h, b) in tex_data.items():
    img = Image.frombytes("RGBA", (w, h), b)
    p = os.path.join(out_dir, f"tex_{k}.png")
    img.save(p)
    print(f"Saved {p}")

