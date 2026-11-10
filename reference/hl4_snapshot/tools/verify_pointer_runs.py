#!/usr/bin/env python3
"""Assert original guest outcomes after SDL mouse/finger pixel input."""
import hashlib,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    specs=[('mouse-final','pointer_mouse_final.txt','mouse',800,480),('mouse-2','pointer_mouse_2.txt','mouse',1000,700),('finger-final','pointer_finger_final.txt','finger',1000,700)]
    runs=[]
    for suffix,log,kind,w,h in specs:
        folder=ROOT/('scratch/pointer-'+suffix)
        stats=json.loads((folder/'stats.json').read_text())
        text=(ROOT/'scratch'/log).read_text()
        events=[tuple(map(int,x)) for x in re.findall(r'pointer phase=(\d+) guest=\((\d+),(\d+)\) key=(-?\d+)',text)]
        assert len(events)>80 and events[0][0]==0 and events[1][0]==1,'Fast click lost'
        assert abs(events[0][1]-235)<=1 and abs(events[0][2]-19)<=1 and events[0][3]==35,'Skip mapped incorrectly'
        assert {35,-5,-4,-3,-16}.issubset({e[3] for e in events if e[0]==0})
        states=stats['states'];positions={(s['x'],s['y']) for s in states if s['ms']>=62000};assert len(positions)>=3
        callbacks=[s['callback'] for s in states];i=callbacks.index('GameMenuFrame_KeyPressed');assert 'MainGameFrame_KeyPressed' in callbacks[i+1:]
        assert sum(e[0]==0 for e in events)==sum(e[0]==1 for e in events) and events[-1][0]==1
        runs.append({'kind':kind,'window':[w,h],'events':len(events),'stats':stats,'log_sha256':sha(ROOT/'scratch'/log),'window_sha256':sha(folder/'window.bmp')})
    assert all(r['stats']['states']==runs[0]['stats']['states'] for r in runs)
    result={'date':'2026-10-21','source_sha256':sha(ROOT/'src/main.cpp'),'exe_sha256':sha(ROOT/'dist/windows/heroes_lore_4.exe'),
        'fixture_sha256':sha(ROOT/'tests/pointer_gameplay.txt'),'result':'PASS','runs':runs,
        'scope':'SDL event injection with real window ID, renderer event scaling, inverse rotation, original guest touch hit testing and timer consumption; native test clock',
        'limits':'physical touch screen not exercised; initial boot by keyboard, gameplay by pointer'}
    (ROOT/'reference/analysis/POINTER_TESTS.json').write_text(json.dumps(result,indent=2)+'\n')
    print('PASS: mouse/finger, normal/resized/letterboxed windows, 1ms Skip press, action, movement, menu/return, balanced releases; identical guest states')
if __name__=='__main__':main()
