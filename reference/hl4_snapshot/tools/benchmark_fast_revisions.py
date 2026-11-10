"""One paired host sample of two AOT revisions; not a physical Switch FPS test."""
import argparse,hashlib,json,os,subprocess,tempfile,time
from pathlib import Path

def main():
    p=argparse.ArgumentParser();p.add_argument('baseline');p.add_argument('candidate');p.add_argument('output');p.add_argument('--baseline-flag',action='append',default=[]);a=p.parse_args()
    out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy');runs={}
    for name,exe in [('baseline',a.baseline),('candidate',a.candidate)]:
        dest=out/name;dest.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='hl4-paired-saves-',dir=out) as saves:
            args=[str(Path(exe).resolve()),'--smoke','72','--fast','--aspect','native','--deterministic',
                  '--script','tests/boot_navigation.txt','--save-dir',saves,'--capture-dir',str(dest)]
            if name=='baseline':args+=a.baseline_flag
            before=time.perf_counter()
            with (dest/'runtime.log').open('w',encoding='utf8') as log:subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
            elapsed=time.perf_counter()-before
        stats=json.loads((dest/'stats.json').read_text());assert stats['audio_nonzero']>0
        runs[name]={'seconds':elapsed,'extra_flags':a.baseline_flag if name=='baseline' else [],'exe_sha256':hashlib.sha256(Path(exe).read_bytes()).hexdigest(),'stats':stats}
    assert runs['baseline']['stats']['states']==runs['candidate']['stats']['states']
    assert runs['baseline']['stats']['frames']==runs['candidate']['stats']['frames']
    names=sorted(p.name for p in (out/'baseline').glob('*.bmp'))
    assert names==sorted(p.name for p in (out/'candidate').glob('*.bmp'))
    for n in names:assert (out/'baseline'/n).read_bytes()==(out/'candidate'/n).read_bytes(),n
    report={'scope':'One paired Windows/JIT-host sample, both --fast, no profiler/oracle; includes boot and captures. Not Switch FPS.',
            'baseline':runs['baseline'],'candidate':runs['candidate'],'ratio':runs['baseline']['seconds']/runs['candidate']['seconds'],
            'equal_bmps':len(names),'equal_states':len(runs['candidate']['stats']['states'])}
    (out/'paired.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS paired host revisions:',{k:round(v['seconds'],3) for k,v in runs.items()},'ratio',round(report['ratio'],3))

if __name__=='__main__':main()
