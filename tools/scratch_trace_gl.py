import sys, os
sys.path.insert(0, os.path.abspath("."))

from tools.hl3rt.runtime import Runtime
import struct

rt = Runtime()
rt.boot_to_app_main()
rt.run_launch()

# Step 5 frames
for _ in range(5):
    rt.send(rt.state["delegate"], "onTimer:", 0)

# Now trace the 6th frame
calls = []
m = rt.m

# Hook GL functions
orig_trap_handlers = list(m.trap_handlers)

for idx, name in enumerate(m.trap_names):
    if name.startswith("_gl"):
        def make_wrapper(orig_fn, name=name):
            def wrapper(mm):
                r0, r1, r2, r3 = mm.reg(0), mm.reg(1), mm.reg(2), mm.reg(3)
                calls.append((name, r0, r1, r2, r3, mm.rd32(mm.sp), mm.rd32(mm.sp+4)))
                return orig_fn(mm)
            return wrapper
        m.trap_handlers[idx] = make_wrapper(orig_trap_handlers[idx], name)

rt.send(rt.state["delegate"], "onTimer:", 0)

print(f"Captured {len(calls)} GL calls in 1 frame.")
for i, c in enumerate(calls):
    name = c[0]
    print(f"[{i:02d}] {name:25s} r0=0x{c[1]:x} r1=0x{c[2]:x} r2=0x{c[3]:x} r3=0x{c[4]:x}")

