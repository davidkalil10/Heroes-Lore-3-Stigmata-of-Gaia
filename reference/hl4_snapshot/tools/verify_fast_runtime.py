"""Compare original Unicorn execution and generated regions on disposable saves."""
import argparse,hashlib,json,os,re,subprocess,time,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('exe');p.add_argument('output');p.add_argument('--benchmark',action='store_true');a=p.parse_args()
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
def run(name,flags):
 dest=out if name=='fast' else out.with_name(out.name+'-'+name)
 dest.mkdir(parents=True,exist_ok=True)
 temp=tempfile.TemporaryDirectory(prefix='hl4-fast-saves-',dir=out.parent)
 args=[str(Path(a.exe).resolve()),'--smoke','72','--deterministic','--script','tests/boot_navigation.txt','--save-dir',temp.name,'--capture-dir',str(dest),*flags]
 before=time.perf_counter()
 with (dest/'runtime.log').open('w',encoding='utf8') as log:subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 elapsed=time.perf_counter()-before
 temp.cleanup()
 return dest,json.loads((dest/'stats.json').read_text()),elapsed
# Separate synthetic combat kernels cover rotated blending and damage overlays
# without uploading a personal save or pretending that a town smoke is combat.
core_log=out/'combat-kernels.log'
with tempfile.TemporaryDirectory(prefix='hl4-kernel-saves-',dir=out.parent) as saves:
 with core_log.open('w',encoding='utf8') as log:
  subprocess.run([str(Path(a.exe).resolve()),'--verify-core','--verify-fast','--save-dir',saves],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
assert 'PASS combat rotate/blend kernels: 4 independent register/full-memory checks' in core_log.read_text()
original,base,base_time=run('original',['--no-fast'])
fast,result,verify_time=run('fast',['--verify-fast'])
assert base['states']==result['states'] and base['frames']==result['frames'],'Gameplay state/frame mismatch'
assert base['audio_nonzero']>0 and result['audio_nonzero']>0,'Missing real audio'
names=sorted(p.name for p in original.glob('*.bmp'))
assert names==sorted(p.name for p in fast.glob('*.bmp'))
for name in names:assert (original/name).read_bytes()==(fast/name).read_bytes(),name
log=(fast/'runtime.log').read_text(encoding='utf8')
regions=int(re.search(r'regionCount=(\d+)',Path('src/arm_hot_blocks.inc').read_text()).group(1))
checks={int(k):int(v) for k,v in re.findall(r'r(\d+)=\d+/(\d+)',log)}
assert regions==10 and len(checks)==regions and all(checks[i]>=2 for i in range(8)),checks
assert all(v==0 or v>=2 for v in checks.values()),checks
report={'frames':result['frames'],'equal_states':len(base['states']),'equal_bmps':len(names),'registers':'R0-R15 and complete CPSR','oracle_memory_bytes':167772160,'checks_per_region':checks,'separate_synthetic_combat_checks':4,'original_audio_nonzero':base['audio_nonzero'],'fast_audio_nonzero':result['audio_nonzero'],'bmp_sha256':{n:hashlib.sha256((fast/n).read_bytes()).hexdigest() for n in names},'timing_scope':'Headless host elapsed time including boot/captures; not physical Switch FPS. Verification run excluded from benchmark.'}
if a.benchmark:
 _,bench,fast_time=run('benchmark',['--fast'])
 assert bench['states']==base['states'] and bench['frames']==base['frames']
 report['single_paired_host_benchmark']={'original_seconds':base_time,'fast_seconds':fast_time,'ratio':base_time/fast_time,'repetitions':1}
(out/'fast-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS',regions,'regions:',sum(checks.values())+4,'independent register/full-memory checks;',len(names),'equal BMPs;',len(base['states']),'equal gameplay states')
if a.benchmark:print('HOST PAIRED BENCHMARK',report['single_paired_host_benchmark'])
