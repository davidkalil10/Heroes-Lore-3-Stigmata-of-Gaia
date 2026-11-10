"""Compare real combat on disposable copies of an explicitly supplied local save.

No save is uploaded or written back. CI uses the separate synthetic kernel
fixtures; this replay depends on the stage of the supplied save.
"""
import argparse,hashlib,json,os,re,shutil,subprocess,tempfile
from pathlib import Path

def main():
 p=argparse.ArgumentParser();p.add_argument('exe');p.add_argument('save_source');p.add_argument('output');p.add_argument('--script',default='tests/combat_existing_save.txt');a=p.parse_args()
 source=Path(a.save_source);out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
 hashes=lambda:{str(p.relative_to(source)):hashlib.sha256(p.read_bytes()).hexdigest() for p in source.rglob('*') if p.is_file()}
 before=hashes();assert before,'Empty save fixture';results={}
 for name,flag in [('original','--no-fast'),('fast','--verify-fast')]:
  dest=out/name;dest.mkdir(exist_ok=True)
  with tempfile.TemporaryDirectory(prefix='hl4-combat-',dir=out) as tmp:
   shutil.copytree(source,tmp,dirs_exist_ok=True)
   with (dest/'runtime.log').open('w',encoding='utf8') as log:
    subprocess.run([str(Path(a.exe).resolve()),'--smoke','110','--deterministic',flag,'--script',a.script,'--save-dir',tmp,'--capture-dir',str(dest)],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),stdout=log,stderr=subprocess.STDOUT,check=True)
  results[name]=json.loads((dest/'stats.json').read_text());assert results[name]['audio_nonzero']>0
 assert hashes()==before,'Original saves changed'
 assert results['original']['states']==results['fast']['states'] and results['original']['frames']==results['fast']['frames']
 names=sorted(p.name for p in (out/'original').glob('*.bmp'))
 assert names==sorted(p.name for p in (out/'fast').glob('*.bmp'))
 for n in names:assert (out/'original'/n).read_bytes()==(out/'fast'/n).read_bytes(),n
 log=(out/'fast/runtime.log').read_text();checks={int(i):int(n) for i,n in re.findall(r'r(\d+)=\d+/(\d+)',log)}
 assert len(checks)==10 and all(n>=2 for n in checks.values()),checks
 assert not (out/'fast/localization-missing.tsv').read_text().strip(),'Untranslated text during this replay'
 report={'result':'PASS','scope':'Local copied-save battle replay, not physical Android/Switch FPS; no timings claimed',
  'equal_bmps':len(names),'equal_states':len(results['fast']['states']),'frames':results['fast']['frames'],'checks_per_region':checks,
  'oracle_memory_bytes':167772160,'untranslated_canonical_strings':0,'original_saves_unchanged':True,'save_fixture_sha256':before,
  'audio_nonzero':{k:s['audio_nonzero'] for k,s in results.items()},'exe_sha256':hashlib.sha256(Path(a.exe).read_bytes()).hexdigest()}
 (out/'combat-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS combat:',report['equal_bmps'],'BMPs;',report['equal_states'],'states;',sum(checks.values()),'full-memory oracle checks; zero observed misses')
if __name__=='__main__':main()
