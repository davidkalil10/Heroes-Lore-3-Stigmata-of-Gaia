import sys, os
sys.path.insert(0, os.path.abspath("."))

from tools.hl3rt.runtime import Runtime
import struct

rt = Runtime()
rt.boot_to_app_main()
rt.run_launch()

for f in range(60):
    rt.send(rt.state['delegate'], 'onTimer:', 0)

# Capture the draw calls in frame 61
draws = []
m = rt.m

state = {
    'last_vert': None,
    'last_tex': None,
    'last_tex_id': 0
}

orig_handlers = list(m.trap_handlers)
for idx, name in enumerate(m.trap_names):
    if name == '_glVertexPointer':
        def make_v(orig):
            def w_v(mm):
                sz, tp, strd, ptr = mm.reg(0), mm.reg(1), mm.reg(2), mm.reg(3)
                state['last_vert'] = (sz, tp, strd, ptr)
                return orig(mm)
            return w_v
        m.trap_handlers[idx] = make_v(orig_handlers[idx])
    elif name == '_glTexCoordPointer':
        def make_t(orig):
            def w_t(mm):
                sz, tp, strd, ptr = mm.reg(0), mm.reg(1), mm.reg(2), mm.reg(3)
                state['last_tex'] = (sz, tp, strd, ptr)
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
                draws.append({
                    'tex': state['last_tex_id'],
                    'first': first,
                    'count': count,
                    'verts': verts,
                    'uvs': uvs
                })
                return orig(mm)
            return w_d
        m.trap_handlers[idx] = make_d(orig_handlers[idx])


rt.send(rt.state['delegate'], 'onTimer:', 0)

print(f"Captured {len(draws)} draw calls:")
for i, d in enumerate(draws):
    print(f"\n--- Draw {i}: tex={d['tex']} first={d['first']} count={d['count']} ---")
    for v, uv in zip(d['verts'], d['uvs']):
        print(f"  V: ({v[0]:6.1f}, {v[1]:6.1f})   UV: ({uv[0]:6.3f}, {uv[1]:6.3f})")
