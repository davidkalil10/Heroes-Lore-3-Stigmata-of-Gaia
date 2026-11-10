"""Attribute an opt-in HL4 block histogram to original ELF function symbols."""
import argparse,bisect,collections,csv,hashlib,io,json,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scratch/analysis_deps'))
from elftools.elf.elffile import ELFFile
p=argparse.ArgumentParser();p.add_argument('csv');p.add_argument('--output',required=True);p.add_argument('--scope',default='HL4 opt-in block histogram; execution flags must be recorded by caller');a=p.parse_args()
cfg=json.loads((ROOT/'project.json').read_text(encoding='utf8'));raw=(ROOT/cfg['original_apk']['path']).read_bytes();assert hashlib.sha256(raw).hexdigest()==cfg['original_apk']['sha256']
with zipfile.ZipFile(io.BytesIO(raw)) as z:blob=z.read('lib/armeabi/libbinary.so')
elf=ELFFile(io.BytesIO(blob));symbols=sorted({(s['st_value']&~1,s['st_size'],s.name) for s in elf.get_section_by_name('.dynsym').iter_symbols() if s['st_info']['type']=='STT_FUNC' and s['st_size']});starts=[s[0] for s in symbols];totals=collections.Counter()
for row in csv.DictReader(Path(a.csv).open()):
 pc=int(row['va']);i=bisect.bisect_right(starts,pc)-1
 name=symbols[i][2] if i>=0 and pc<symbols[i][0]+symbols[i][1] else hex(pc)
 totals[name]+=int(row['block_bytes'])
total=sum(totals.values());out={'scope':a.scope+'; block bytes weighted by executions, not wall time or measured Switch FPS. No profiler in performance benchmark.','functions':[{'name':n,'block_bytes':v,'percent':100*v/total} for n,v in totals.most_common(25)],'total_block_bytes':total}
Path(a.output).write_text(json.dumps(out,indent=2)+'\n');print('PASS profile attribution; top five functions',sum(v for _,v in totals.most_common(5))*100/total,'% weighted ARM bytes')
