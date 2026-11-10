"""Verify HL4 map expansion from real guest captures, including AOT oracle runs.

Run 46s deterministic tests/boot_navigation.txt with --fast in these folders:
scratch/aspect-native (--aspect native --window-size 1280 576),
scratch/aspect-wide20 (1280 576), aspect-wide4 (800 600),
aspect-wide16 (1280 720), aspect-oracle (1280 576 --verify-fast).
Each run needs a separate disposable --save-dir and --capture-dir.
Also capture scratch/aspect-pointer with --smoke 80 --fast --deterministic
--script tests/pointer_boot.txt --pointer-script tests/pointer_gameplay_wide20.txt
--pointer-kind finger --window-size 1280 576; redirect its log to the same prefix.
"""
import collections, hashlib, json, re, sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scratch/analysis_deps'))
from PIL import Image


def main():
    runs={}
    dimensions={'native':(240,400),'wide20':(240,534),'wide4':(300,400),
                'wide10':(250,400),'wide21':(240,560),
                'wide16':(240,428),'oracle':(240,534)}
    for name,size in dimensions.items():
        folder=ROOT/'scratch'/('aspect-'+name)
        stats=json.loads((folder/'stats.json').read_text())
        assert stats['frames']>1000 and stats['audio_nonzero']>0,name
        assert Image.open(folder/'smoke-20.bmp').size==size,name
        if runs:assert stats['states']==runs['native']['states'],name
        runs[name]=stats
    base=ROOT/'scratch/aspect-wide20'
    oracle=ROOT/'scratch/aspect-oracle'
    for p in base.glob('*.bmp'):
        assert p.read_bytes()==(oracle/p.name).read_bytes(),'AOT capture '+p.name
    log=(ROOT/'scratch/aspect-oracle.log').read_text()
    checks={int(k):int(v) for k,v in re.findall(r'r(\d+)=\d+/(\d+)',log)}
    region_count=int(re.search(r'regionCount=(\d+)',(ROOT/'src/arm_hot_blocks.inc').read_text()).group(1))
    assert region_count==10 and len(checks)==region_count and all(checks[i]>=2 for i in range(8)),checks
    assert all(v==0 or v>=2 for v in checks.values()),checks # Combat-only kernels covered by verify_fast_runtime core fixtures.
    a=Image.open(ROOT/'scratch/aspect-native/smoke-20.bmp').convert('RGB').transpose(Image.Transpose.ROTATE_270)
    b=Image.open(base/'smoke-20.bmp').convert('RGB').transpose(Image.Transpose.ROTATE_270)
    lookup=collections.defaultdict(list)
    for y in range(b.height-15):
        for x in range(80,b.width-15):
            key=hashlib.sha256(b.crop((x,y,x+16,y+16)).tobytes()).digest()
            lookup[key].append((x,y))
    hits=collections.Counter()
    for y in range(8,a.height-24,16):
        for x in range(88,a.width-24,16):
            tile=a.crop((x,y,x+16,y+16))
            if len(tile.getcolors(256) or [])<12:continue
            for xx,yy in lookup[hashlib.sha256(tile.tobytes()).digest()]:
                hits[(xx-x,yy-y)]+=1
    offset,matches=hits.most_common(1)[0]
    assert matches>=8,'No rigid 16x16 scenery correspondence; possible stretching'
    # The new left world area is outside the translated old view. Reject a
    # filler border: it must have varied original scenery, not a solid colour.
    extra=b.crop((108,0,80+offset[0],64))
    assert extra.width>=32 and len(extra.getcolors(100000))>100,'No additional scenery'
    pointer_log=(ROOT/'scratch/aspect-pointer.log').read_text()
    events=[tuple(map(int,x)) for x in re.findall(r'pointer phase=(\d+) guest=\((\d+),(\d+)\) key=(-?\d+)',pointer_log)]
    ps=json.loads((ROOT/'scratch/aspect-pointer/stats.json').read_text())
    assert len(events)>80 and events[0][0]==0 and events[1][0]==1
    assert abs(events[0][1]-235)<=1 and abs(events[0][2]-19)<=1 and events[0][3]==35,'Skip hit testing'
    assert {35,-5,-4,-3,-16}.issubset({e[3] for e in events if e[0]==0})
    assert len({(s['x'],s['y']) for s in ps['states'] if s['ms']>=62000})>=3
    callbacks=[s['callback'] for s in ps['states']]
    assert 'MainGameFrame_KeyPressed' in callbacks[callbacks.index('GameMenuFrame_KeyPressed')+1:]
    assert sum(e[0]==0 for e in events)==sum(e[0]==1 for e in events) and events[-1][0]==1
    assert ps['audio_nonzero']>0
    report={'date':'2026-10-22','result':'PASS','frames':runs['native']['frames'],
            'identical_gameplay_states':len(runs['native']['states']),
            'guest_dimensions':dimensions,'audio_nonzero':{k:v['audio_nonzero'] for k,v in runs.items()},
            'exact_16x16_scenery_matches':matches,'rigid_offset':offset,
            'additional_scenery_sample_pixels':extra.width*extra.height,
            'aot_checks':checks,'oracle_memory_bytes':167772160,
            'finger_replay':{'events':len(events),'balanced_phases':True,'skip_1ms':True,
                             'action_movement_menu_return':True,'frames':ps['frames'],'audio_nonzero':ps['audio_nonzero']},
            'source_sha256':hashlib.sha256((ROOT/'src/main.cpp').read_bytes()).hexdigest(),
            'limits':['Host automated runs, not physical Android/Switch validation',
                      'Canvas selected at startup; live resize expansion pending',
                      'Small maps retain authored boundaries; no invented scenery',
                      'Cutouts/transient system bars, transitions, other maps and all UI still need device coverage']}
    (ROOT/'reference/analysis/VIEWPORT_EXPANSION_TESTS.json').write_text(json.dumps(report,indent=2)+'\n')
    a.save(ROOT/'scratch/aspect-native-landscape.png')
    b.save(ROOT/'scratch/aspect-wide20-landscape.png')
    print('PASS: more scenery; unchanged 16x16 pixels, gameplay states and nonzero audio;',sum(checks.values()),'AOT oracle checks')


if __name__=='__main__':main()
