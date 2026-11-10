#!/usr/bin/env python3
"""Check the scaffold and reference provenance; this is not a game runtime test."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
from datetime import datetime,timezone
import project_status
ROOT=Path(__file__).resolve().parents[1]
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--record',action='store_true');args=parser.parse_args()
 config=json.loads((ROOT/'project.json').read_text(encoding='utf-8'))
 apk=ROOT/config['original_apk']['path']
 if hashlib.sha256(apk.read_bytes()).hexdigest()!=config['original_apk']['sha256']:raise ValueError('Original APK changed')
 manifest=json.loads((ROOT/'reference/HL5_SNAPSHOT.json').read_text(encoding='utf-8'))
 base=(ROOT/'reference/hl5_snapshot').resolve()
 for entry in manifest['files']:
  path=(base/entry['path']).resolve()
  if not path.is_relative_to(base) or not path.is_file():raise ValueError('Missing/unsafe snapshot file')
  if path.stat().st_size!=entry['bytes'] or hashlib.sha256(path.read_bytes()).hexdigest()!=entry['sha256']:raise ValueError('Changed snapshot: '+entry['path'])
 expected={e['path'] for e in manifest['files']}
 actual={p.relative_to(base).as_posix() for p in base.rglob('*') if p.is_file()}
 if expected!=actual:raise ValueError('Unexpected snapshot files')
 previous=json.loads((ROOT/'reference/JAR_PORTS_DOCS.json').read_text(encoding='utf-8'))
 previous_count=0
 for project in previous['projects']:
  base_docs=(ROOT/'reference/jar_ports_docs'/project['project']).resolve()
  for entry in project['files']:
   path=(base_docs/entry['path']).resolve()
   if not path.is_relative_to(base_docs) or hashlib.sha256(path.read_bytes()).hexdigest()!=entry['sha256']:
    raise ValueError('Changed/missing JAR-port reference document')
   previous_count+=1
 for name in ('AGENTS.md','README.md','PROMPT_NOVA_SESSAO.md','docs/ROADMAP.md','docs/HANDOFF.md','docs/DECISIONS.md',
              'docs/RESEARCH_LOG.md','docs/HL5_LESSONS.md','docs/PORTING_MAP.md','docs/VALIDATION.md','docs/SESSION_PROTOCOL.md',
              'docs/tasks/S02_RUNTIME_CONTRACT.md','src','tests','platforms','research/sessions'):
  if not (ROOT/name).exists():raise ValueError('Missing scaffold item: '+name)
 data=project_status.check()
 subprocess.run([sys.executable,str(ROOT/'tools/inspect_apk.py'),'--check'],check=True,cwd=ROOT)
 result={'recorded_at_utc':datetime.now(timezone.utc).isoformat(),'scope':'workspace and static input analysis only; no HL4 runtime executed',
         'apk_sha256_ok':True,'snapshot_files_checked':len(expected),'snapshot_commit':manifest['commit'],
         'jar_port_documents_checked':previous_count,'status_consistent':True,'inventory_matches':True,'next_step':data['current_step']}
 if args.record:(ROOT/'reference/analysis/BOOTSTRAP_VALIDATION.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print(f"PASS: input preserved; {len(expected)} snapshot files verified; structure/status/inventory consistent. No gameplay claim.")
if __name__=='__main__':main()
