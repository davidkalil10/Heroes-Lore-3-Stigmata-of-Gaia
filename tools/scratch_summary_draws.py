import sys, os
sys.path.insert(0, os.path.abspath("."))

from tools.hl3rt.runtime import Runtime
import struct

rt = Runtime()
rt.boot_to_app_main()
rt.run_launch()

for f in range(60):
    rt.send(rt.state['delegate'], 'onTimer:', 0)

state = {
    'last_vert': None,
    'last_tex': None,
    'last_tex_id': 0
}

draws = []
m = rt.m
orig_handlers = list(m.trap_handlers)

for idx, name in enumerate(m.trap_names):
    if name == '_glVertexPointer':
        def make_v(orig):
            def w_v(mm):
                state['last_vert'] = (mm.reg(0), mm.reg(1), mm.reg(2), mm.reg(3))
                return orig(mm)
            return w_v
        m.trap_handlers[idx] = make_v(orig_handlers[idx])
    elif name == '_glTexCoordPointer':
        def make_t(orig):
            def w_t(mm):
                state['last_tex'] = (mm.reg(0), mm.reg(1), mm.reg(2), mm.reg(3))
                return orig(mm)
            return w_t
        m.trap_handlers[idx] = make_t(orig_handlers[idx])
    elif name == '_glBindTexture':
        def make_b(orig):
            def w_b(mm):
                state['last_tex_id'] = mm.reg(1)
                return orig(mm)
            return w_b
        m.trap_handlers[idx] = make_b(orig_handlers[idx])
    elif name == '_glDrawArrays':
        def make_d(orig):
            def w_d(mm):
                mode, first, count = mm.reg(0), mm.reg(1), mm.reg(2)
                v_sz, v_tp, v_strd, v_ptr = state['last_vert']
                t_sz, t_tp, t_strd, t_ptr = state['last_tex']
                v_bytes = mm.rdbytes(v_ptr + first * 8, count * 8)
                t_bytes = mm.rdbytes(t_ptr + first * 8, count * 8)
                verts = [struct.unpack('<ff', v_bytes[i*8:(i+1)*8]) for i in range(count)]
                uvs = [struct.unpack('<ff', t_bytes[i*8:(i+1)*8]) for i in range(count)]
                # compute bounding box of vertices
                min_x = min(v[0] for v in verts)
                max_x = max(v[0] for v in verts)
                min_y = min(v[1] for v in verts)
                max_y = max(v[1] for v in verts)
                min_u = min(u[0] for u in uvs)
                max_u = max(u[0] for u in uvs)
                min_v = min(u[1] for u in uvs)
                max_v = max(u[1] for u in uvs)
                draws.append((state['last_tex_id'], first, count, (min_x, min_y, max_x, max_y), (min_u, min_v, max_u, max_v)))
                return orig(mm)
            return w_d
        m.trap_handlers[idx] = make_d(orig_handlers[idx])

rt.send(rt.state['delegate'], 'onTimer:', 0)

print(f"Total draws in frame 60: {len(draws)}")
for i, d in enumerate(draws):
    print(f"Draw {i}: tex={d[0]} first={d[1]:3d} count={d[2]:3d} V=({d[3][0]:.1f},{d[3][1]:.1f} to {d[3][2]:.1f},{d[3][3]:.1f}) UV=({d[4][0]:.3f},{d[4][1]:.3f} to {d[4][2]:.3f},{d[4][3]:.3f})")

