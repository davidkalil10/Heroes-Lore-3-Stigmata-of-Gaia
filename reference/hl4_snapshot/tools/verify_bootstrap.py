#!/usr/bin/env python3
"""Exercise extraction safety and status gates without touching original files."""
import argparse,copy,importlib.util,io,json,re,tempfile,zipfile
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(__file__).resolve().parents[1]
def load(name,filename):
 spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/filename);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--record',action='store_true');args=parser.parse_args()
 inspector=load('hl4_inspect_test','inspect_apk.py');status=load('hl4_status_test','project_status.py')
 passed=[]
 def archive(name,payload=b'test',symlink=False):
  data=io.BytesIO()
  with zipfile.ZipFile(data,'w') as z:
   info=zipfile.ZipInfo("fixture");info.filename=name
   if symlink:info.create_system=3;info.external_attr=0o120777<<16
   z.writestr(info,payload)
  data.seek(0);return zipfile.ZipFile(data)
 with tempfile.TemporaryDirectory(prefix='hl4-bootstrap-test-') as tmp:
  fake=Path(tmp);inspector.ROOT=fake
  for i,name in enumerate(('../escape','/absolute','C:/outside','dir\\outside')):
   try:
    with archive(name) as z:inspector.safe_extract(z,fake/str(i))
   except ValueError:passed.append('reject_unsafe_path_'+str(i))
   else:raise AssertionError('Unsafe archive path accepted: '+name)
  try:
   with archive('link',symlink=True) as z:inspector.safe_extract(z,fake/'links')
  except ValueError:passed.append('reject_symlink_entry')
  else:raise AssertionError('Symlink accepted')
  try:
   with archive('file') as z:inspector.safe_extract(z,fake.parent/('outside-'+fake.name))
  except ValueError:passed.append('reject_destination_outside_project')
  else:raise AssertionError('Outside destination accepted')
  dest=fake/'valid'
  for _ in range(2):
   with archive('res/MAP/example',b'original') as z:inspector.safe_extract(z,dest)
  assert (dest/'res/MAP/example').read_bytes()==b'original';passed.append('repeat_extraction_preserves_equal_files')
  try:
   with archive('res/MAP/example',b'different') as z:inspector.safe_extract(z,dest)
  except ValueError:passed.append('reject_overwrite_different_file')
  else:raise AssertionError('Different existing file overwritten')
  assert (dest/'res/MAP/example').read_bytes()==b'original'
  data=status.read();bad=copy.deepcopy(data);bad['steps'][2]['status']='done';bad['steps'][2]['evidence']=[]
  try:status.validate(bad)
  except ValueError:passed.append('done_requires_evidence')
  else:raise AssertionError('DONE without evidence accepted')
  bad=copy.deepcopy(data);bad['steps'][2]['evidence']=['../outside.md']
  try:status.validate(bad)
  except ValueError:passed.append('status_evidence_confined_to_project')
  else:raise AssertionError('Outside evidence accepted')
  status.ROOT=fake;(fake/'docs').mkdir()
  (fake/'docs/STATUS.json').write_text(json.dumps({'steps':[{'id':'S00','title':'test','status':'pending','summary':'test','evidence':[]}],
     'updated_on':'2026-10-21','current_step':'S00'}),encoding='utf-8')
  (fake/'docs/STATUS.md').write_text('stale',encoding='utf-8')
  try:status.check()
  except ValueError:passed.append('detect_stale_markdown_status')
  else:raise AssertionError('Stale status view accepted')
 # Check links only in active documentation, not frozen historical references.
 documents=[ROOT/'README.md',ROOT/'AGENTS.md',ROOT/'PROMPT_NOVA_SESSAO.md',*list((ROOT/'docs').rglob('*.md'))]
 links=0
 for path in documents:
  for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)',path.read_text(encoding='utf-8')):
   if target.startswith(('https:','http:','mailto:','#')):continue
   target=target.split('#',1)[0]
   if not (path.parent/target).resolve().exists():raise AssertionError('Broken documentation link: '+str(path)+' -> '+target)
   links+=1
 passed.append('active_documentation_links')
 report={'recorded_at_utc':datetime.now(timezone.utc).isoformat(),'checks_passed':passed,'local_links_checked':links,
         'scope':'Scaffold tools only, temporary fixtures; no game/runtime/hardware execution'}
 if args.record:(ROOT/'reference/analysis/BOOTSTRAP_TESTS.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(f'PASS: {len(passed)} scaffold-tool checks, {links} documentation links; all fixtures disposable.')
if __name__=='__main__':main()
