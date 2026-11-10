#!/usr/bin/env python3
"""Offline, deterministic inventory/extraction of the project's pinned input APK."""
import argparse,csv,hashlib,io,json,struct,zipfile
from pathlib import Path,PurePosixPath
ROOT=Path(__file__).resolve().parents[1]
def sha(data):return hashlib.sha256(data).hexdigest()
def encoded(value):return (json.dumps(value,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
def cstr(data,offset):return data[offset:].split(b'\0',1)[0].decode('utf-8',errors='replace')
def uleb(data,offset):
 value=0;shift=0
 while True:
  byte=data[offset];offset+=1;value|=(byte&127)<<shift
  if not byte&128:return value,offset
  shift+=7
  if shift>35:raise ValueError('Invalid ULEB128')
def dex_strings(data):
 if not data.startswith(b'dex\n'):raise ValueError('Not a DEX file')
 count,offset=struct.unpack_from('<II',data,56);strings=[]
 for i in range(count):
  start=struct.unpack_from('<I',data,offset+i*4)[0];_,start=uleb(data,start)
  strings.append(cstr(data,start))
 return strings
def axml(data):
 """Read string pools/start tags; preserve resource refs without guessing labels."""
 strings=[];tags=[];pos=struct.unpack_from('<H',data,2)[0]
 def string(index):return strings[index] if index<len(strings) else None
 while pos+8<=len(data):
  kind,header,size=struct.unpack_from('<HHI',data,pos)
  if size<8 or pos+size>len(data):raise ValueError('Malformed AXML chunk')
  if kind==1:
   count,styles,flags,start,_=struct.unpack_from('<5I',data,pos+8)
   strings=[]
   for i in range(count):
    cursor=pos+start+struct.unpack_from('<I',data,pos+header+i*4)[0]
    if flags&256:
     def length8(at):
      val=data[at];return (((val&127)<<8)|data[at+1],at+2) if val&128 else (val,at+1)
     _,cursor=length8(cursor);length,cursor=length8(cursor)
     strings.append(data[cursor:cursor+length].decode('utf-8',errors='replace'))
    else:
     length=struct.unpack_from('<H',data,cursor)[0];cursor+=2
     if length&0x8000:length=((length&0x7fff)<<16)|struct.unpack_from('<H',data,cursor)[0];cursor+=2
     strings.append(data[cursor:cursor+length*2].decode('utf-16le',errors='replace'))
  elif kind==0x102:
   ns,name,start,stride,count=struct.unpack_from('<IIHHH',data,pos+16)
   attrs={}
   for i in range(count):
    at=pos+16+start+i*stride
    ans,aname,raw,tsize,zero,typ,value=struct.unpack_from('<IIIHBBI',data,at)
    resolved=string(raw)
    if resolved is None:
     resolved=string(value) if typ==3 else bool(value) if typ==0x12 else value if typ in (0x10,0x11) else f'@0x{value:08x}'
    attrs[string(aname) or str(aname)]=resolved
   tags.append({'tag':string(name),'attributes':attrs})
  pos+=size
 return {'tags':tags,'string_pool':strings}
def elf(data,label):
 if data[:6]!=b'\x7fELF\x01\x01':raise ValueError('Expected ELF32 little-endian: '+label)
 h=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
 sections=[struct.unpack_from('<10I',data,h[6]+i*h[11]) for i in range(h[12])]
 names_section=sections[h[13]];names=data[names_section[4]:names_section[4]+names_section[5]]
 result={'input':label,'bytes':len(data),'sha256':sha(data),'elf_class':32,'endian':'little',
         'machine':h[2],'type':h[1],'entry_virtual_hex':f'0x{h[4]:x}','flags_hex':f'0x{h[7]:x}',
         'sections':[],'segments':[],'dependencies':[]};symbols=[]
 for section in sections:
  name=cstr(names,section[0]);result['sections'].append({'name':name,'type':section[1],
   'virtual_address_hex':f'0x{section[3]:x}','offset':section[4],'bytes':section[5]})
  if section[1] in (2,11):
   strings=sections[section[6]];strings=data[strings[4]:strings[4]+strings[5]]
   for off in range(section[4],section[4]+section[5],section[9]):
    index,value,size,info,other,defined=struct.unpack_from('<IIIBBH',data,off)
    symbol=cstr(strings,index)
    if symbol:symbols.append({'table':name,'name':symbol,'value_hex':f'0x{value:x}','bytes':size,
     'binding':info>>4,'type':info&15,'section_index':defined,'defined':defined!=0,
     'value_lsb':value&1})
  if section[1]==6:
   linked=sections[section[6]];strings=data[linked[4]:linked[4]+linked[5]]
   for off in range(section[4],section[4]+section[5],8):
    tag,value=struct.unpack_from('<II',data,off)
    if tag==1:result['dependencies'].append(cstr(strings,value))
 for i in range(h[10]):
  typ,off,va,pa,filesz,memsz,flags,align=struct.unpack_from('<8I',data,h[5]+i*h[9])
  if off+filesz>len(data) or filesz>memsz:raise ValueError('Bad ELF segment')
  result['segments'].append({'type':typ,'offset':off,'virtual_address_hex':f'0x{va:x}',
   'file_bytes':filesz,'memory_bytes':memsz,'flags':flags,'alignment':align,'sha256':sha(data[off:off+filesz])})
 dyn=[s for s in symbols if s['table']=='.dynsym']
 result['dynamic_symbol_count']=len(dyn)
 result['imports']=[s['name'] for s in dyn if not s['defined']]
 result['jni_exports']=[s['name'] for s in dyn if s['defined'] and (s['name'].startswith('Java_') or s['name']=='JNI_OnLoad')]
 result['candidate_exports']=[s for s in dyn if s['name'] in
  ('init','_init_module','startClet','paintClet','onStartApp','onHandleEvent','MainFrame_Process','MainFrame_Render','SaveDataInit')]
 return result,symbols
def entries(archive):return [{'path':i.filename,'bytes':i.file_size,'compressed_bytes':i.compress_size,
 'crc32_hex':f'{i.CRC:08x}','directory':i.is_dir()} for i in archive.infolist()]
def safe_extract(archive,destination):
 total=sum(i.file_size for i in archive.infolist())
 if total>512*1024*1024:raise ValueError('Extraction exceeds project limit')
 destination=destination.resolve()
 if not destination.is_relative_to(ROOT.resolve()):raise ValueError("Extraction root outside project")
 destination.mkdir(parents=True,exist_ok=True)
 for info in archive.infolist():
  raw_name=info.orig_filename
  name=PurePosixPath(raw_name)
  if name.is_absolute() or '..' in name.parts or chr(92) in raw_name or ':' in info.filename:
   raise ValueError('Unsafe archive path: '+info.filename)
  if (info.external_attr>>16)&0o170000==0o120000:raise ValueError('Archive symlink refused')
  path=(destination/Path(*name.parts)).resolve()
  if not path.is_relative_to(destination):raise ValueError('Extraction outside destination')
  if info.is_dir():path.mkdir(parents=True,exist_ok=True);continue
  payload=archive.read(info)
  if path.exists():
   if path.read_bytes()!=payload:raise ValueError('Existing different extraction: '+str(path))
  else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(payload)
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');parser.add_argument('--extract',action='store_true');args=parser.parse_args()
 if args.check and args.extract:parser.error('--check and --extract are mutually exclusive')
 config=json.loads((ROOT/'project.json').read_text(encoding='utf-8'));path=(ROOT/config['original_apk']['path']).resolve()
 if not path.is_relative_to(ROOT.resolve()):raise ValueError('Input APK outside project')
 payload=path.read_bytes()
 if sha(payload)!=config['original_apk']['sha256']:raise ValueError('Input APK hash mismatch')
 reports={}
 with zipfile.ZipFile(io.BytesIO(payload)) as apk:
  bad=apk.testzip()
  if bad:raise ValueError('APK CRC failed: '+bad)
  reports['apk_inventory.json']=encoded({'file':path.name,'bytes':len(payload),'sha256':sha(payload),'entries':entries(apk)})
  reports['android_manifest.json']=encoded(axml(apk.read('AndroidManifest.xml')))
  reports['dex_strings.json']=encoded({'note':'Inspection strings; DEX MUTF-8 decoded with replacement for nonstandard sequences','strings':dex_strings(apk.read('classes.dex'))})
  native=apk.read('lib/armeabi/libbinary.so');el,syms=elf(native,'lib/armeabi/libbinary.so')
  reports['elf_inventory.json']=encoded(el)
  output=io.StringIO(newline='');writer=csv.DictWriter(output,fieldnames=list(syms[0]));writer.writeheader();writer.writerows(syms)
  reports['elf_symbols.csv']=output.getvalue().encode('utf-8')
  jarbytes=apk.read('res/raw/jarfile.mp3')
  with zipfile.ZipFile(io.BytesIO(jarbytes)) as jar:
   bad=jar.testzip()
   if bad:raise ValueError('Nested JAR CRC failed: '+bad)
   module=jar.read('binary.mod');mod,_=elf(module,'JAR:binary.mod')
   reports['module_elf_inventory.json']=encoded(mod)
   related={'native_is_module_prefix':module.startswith(native),'trailing_bytes':len(module)-len(native),
            'trailer_sha256':sha(module[len(native):]) if module.startswith(native) else None}
   jarreport={'apk_entry':'res/raw/jarfile.mp3','bytes':len(jarbytes),'sha256':sha(jarbytes),
              'class_files':sum(i.filename.endswith('.class') for i in jar.infolist()),'entries':entries(jar),'module_relationship':related}
   reports['jar_inventory.json']=encoded(jarreport)
   if args.extract:
    safe_extract(apk,ROOT/'reference/extracted_apk');safe_extract(jar,ROOT/'reference/extracted_jar')
  reports['INITIAL_FINDINGS.md']=f'''# Inventário inicial do HL4

Gerado offline por tools/inspect_apk.py; nenhuma execução do jogo.

- APK: `{path.name}`; {len(payload)} bytes; SHA-256 `{sha(payload)}`.
- ZIP externo: {len(apk.infolist())} entradas, CRCs válidos.
- ELF: {el['elf_class']} bits, {el['endian']}, machine {el['machine']}, entrada virtual `{el['entry_virtual_hex']}`.
- Dynsym: {el['dynamic_symbol_count']} nomes, {len(el['imports'])} imports; exports JNI: {len(el['jni_exports'])}.
- JAR de recursos: {len(jarreport['entries'])} entradas; {jarreport['class_files']} arquivos .class.
- binary.mod tem o SO completo como prefixo: {related['native_is_module_prefix']}; bytes adicionais: {related['trailing_bytes']}.

Manifesto: android_manifest.json. Strings DEX: dex_strings.json. Símbolos/endereços
antes do rebasing: elf_symbols.csv. Imports e segmentos: elf_inventory.json.
Inventário de recursos: jar_inventory.json. O JAR é um container de recursos e
módulo ARM; não confundir com o executável Java dos ports HL1/HL2/Zero.

Próxima ação: docs/tasks/S02_RUNTIME_CONTRACT.md. Assinaturas/ordem de chamadas,
contrato WIPI, estruturas, formatos de recursos e significado do trailer são
questões de pesquisa, não fatos provados por este inventário.
'''.encode('utf-8')
 destination=ROOT/'reference/analysis';destination.mkdir(parents=True,exist_ok=True)
 for name,content in reports.items():
  target=destination/name
  if args.check:
   if not target.exists() or target.read_bytes()!=content:raise ValueError('Stale/missing report: '+name)
  else:target.write_bytes(content)
 print(f"PASS: pinned APK, CRCs, manifest/DEX, ELF/symbols and {len(jarreport['entries'])} JAR entries; "+('reports match' if args.check else 'reports generated'))
 if args.extract:print('PASS: APK and nested JAR safely extracted; existing equal files preserved.')
if __name__=='__main__':main()
