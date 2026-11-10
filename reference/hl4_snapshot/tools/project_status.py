#!/usr/bin/env python3
"""Single-source progress ledger; render or check the portable Markdown view."""
import argparse,json
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parents[1]
VALID={'pending','in_progress','blocked','done'}
def evidence_path(name):
 path=(ROOT/name).resolve()
 if not path.is_relative_to(ROOT.resolve()) or not path.is_file():raise ValueError('Evidence must be an existing project file: '+name)
 return path
def read():return json.loads((ROOT/'docs/STATUS.json').read_text(encoding='utf-8'))
def render(data):
 def cell(s):return str(s).replace('|','/').replace('\n',' ')
 lines=['# Status do projeto HL4','',f"Atualizado: {data['updated_on']}. Próxima etapa: **{data['current_step']}**.",
        '', '**Preparação e inventário concluídos; nenhum runtime HL4 jogável implementado ainda.**' if all(s['status']=='pending' for s in data['steps'][2:]) else 'Consulte as evidências e a matriz de validação para saber o que foi realmente executado.',
        '', '| Etapa | Entrega | Estado | Resumo | Evidência |','| --- | --- | --- | --- | --- |']
 for step in data['steps']:
  ev=', '.join(f'[{Path(p).name}](../{p})' for p in step['evidence']) or '—'
  lines.append('| '+' | '.join([step['id'],cell(step['title']),step['status'],cell(step['summary']),ev])+' |')
 lines+=['','Fonte única: `docs/STATUS.json`. Editar com `python tools/project_status.py set ...`;','critérios em ROADMAP.md. DONE de pesquisa não significa que o port esteja pronto.']
 return '\n'.join(lines)+'\n'
def validate(data):
 ids=[s['id'] for s in data['steps']]
 if len(ids)!=len(set(ids)) or data['current_step'] not in ids:raise ValueError('Invalid step IDs/current step')
 for step in data['steps']:
  if step['status'] not in VALID:raise ValueError('Invalid status')
  if step['status']=='done' and not step['evidence']:raise ValueError('DONE needs evidence: '+step['id'])
  for name in step['evidence']:evidence_path(name)
def check():
 data=read();validate(data)
 if (ROOT/'docs/STATUS.md').read_text(encoding='utf-8')!=render(data):raise ValueError('STATUS.md differs from STATUS.json')
 return data
def main():
 parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='cmd',required=True)
 for name in ('show','check','render'):sub.add_parser(name)
 update=sub.add_parser('set');update.add_argument('id');update.add_argument('status',choices=sorted(VALID));update.add_argument('--summary',required=True);update.add_argument('--evidence',action='append')
 args=parser.parse_args();data=read()
 if args.cmd=='set':
  step=next((s for s in data['steps'] if s['id']==args.id),None)
  if step is None:raise ValueError('Unknown step')
  step['status']=args.status;step['summary']=args.summary
  if args.evidence is not None:step['evidence']=args.evidence
  data['updated_on']=datetime.now().astimezone().date().isoformat()
  data['current_step']=next((s['id'] for s in data['steps'] if s['status']!='done'),data['steps'][-1]['id'])
  validate(data)
  (ROOT/'docs/STATUS.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 if args.cmd in ('set','render'):
  validate(data);(ROOT/'docs/STATUS.md').write_text(render(data),encoding='utf-8')
 else:check()
 print(render(data) if args.cmd=='show' else 'PASS: project status and evidence references are consistent.')
if __name__=='__main__':main()
