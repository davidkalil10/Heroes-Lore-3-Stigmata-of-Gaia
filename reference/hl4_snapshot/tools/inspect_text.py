#!/usr/bin/env python3
"""Offline HL4 text inventory; original ARM DES, exact container roundtrips.

Full game text stays in ignored scratch/localization. No service/network used.
"""
import contextlib,hashlib,io,json,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def sha(b):return hashlib.sha256(b).hexdigest()
def scene_layout(b):
    if len(b)<13 or any(w not in (1,2) for w in b[:3]):raise ValueError('Scene header')
    at=12
    for width in b[:2]:
        count=b[at];at+=1
        if at+count*width>len(b):raise ValueError('Script table bounds')
        size=sum(int.from_bytes(b[at+i*width:at+(i+1)*width],'little') for i in range(count))
        at+=count*width+size
        if at>=len(b):raise ValueError('Script payload bounds')
    count=b[at];at+=1;table=at;width=b[2]
    if at+count*width>len(b):raise ValueError('Text table bounds')
    lengths=[int.from_bytes(b[at+i*width:at+(i+1)*width],'little') for i in range(count)]
    at+=count*width;entries=[]
    for size in lengths:
        if size<1 or at+size>len(b):raise ValueError('Text payload bounds')
        raw=b[at:at+size]
        if raw[-1:]!=b'\0':raise ValueError('Missing text terminator')
        raw[:-1].decode('cp949')
        entries.append((at,raw));at+=size
    return table,width,entries,b[at:]

def rebuild_scene(b,strings=None):
    table,width,entries,padding=scene_layout(b)
    strings=[raw for _,raw in entries] if strings is None else strings
    if len(strings)!=len(entries):raise ValueError('Changed record count')
    if any(not s.endswith(b'\0') or len(s)>=1<<(8*width) for s in strings):raise ValueError('Text length overflow/terminator')
    return b[:table]+b''.join(len(s).to_bytes(width,'little') for s in strings)+b''.join(strings)+padding

def indexed_layout(b):
    if len(b)<6:return None
    total,count=struct.unpack_from('<HH',b)
    if total!=len(b) or not 0<count<1000 or 4+count*2>len(b):return None
    offsets=list(struct.unpack_from('<'+'H'*count,b,4))
    if offsets[0]!=4+count*2 or offsets!=sorted(set(offsets)) or offsets[-1]>=len(b):return None
    entries=[]
    for at,end in zip(offsets,offsets[1:]+[len(b)]):
        raw=b[at:end]
        if raw[-1:]!=b'\0':return None
        try:raw[:-1].decode('cp949')
        except UnicodeError:return None
        entries.append((at,raw))
    return entries

def merged_layout(b):
    if not b or not 0<b[0]<128:return None
    at=1;entries=[]
    for _ in range(b[0]):
        if at+2>len(b):return None
        size=int.from_bytes(b[at:at+2],'big');at+=2
        if size<1 or at+size>len(b):return None
        raw=b[at:at+size]
        try:text=raw.decode('cp949')
        except UnicodeError:return None
        if '\0' in text:return None
        entries.append((at,raw));at+=size
    return entries if at==len(b) else None

def main():
    sys.path.insert(0,str(ROOT/'tools'))
    from trace_boot import Wipi,TEMP
    with contextlib.redirect_stdout(io.StringIO()):
        r=Wipi();r.call('SaveDataInit')
    out=ROOT/'scratch/localization';out.mkdir(parents=True,exist_ok=True)
    catalog=[];files=[];failures=[]
    for name,data in sorted(r.resources.items()):
        if name.startswith('res/MAP/SC/') and name.endswith('_scn'):
            if len(data)>0x80000:raise ValueError('Research buffer limit')
            with contextlib.redirect_stdout(io.StringIO()):
                r.uc.mem_write(TEMP+0x10000,data);r.call('MX_desDecrypt',(TEMP+0x10000,len(data)))
                plain=bytes(r.uc.mem_read(TEMP+0x10000,len(data)))
                r.call('MX_desEncrypt',(TEMP+0x10000,len(data)))
            if bytes(r.uc.mem_read(TEMP+0x10000,len(data)))!=data:raise ValueError('DES roundtrip: '+name)
            try:
                table,width,entries,padding=scene_layout(plain)
                if rebuild_scene(plain)!=plain:raise ValueError('Container roundtrip')
            except (ValueError,UnicodeError,IndexError) as e:
                failures.append({'resource':name,'reason':str(e)});continue
            kind='scene-DES';extra={'length_width':width,'padding_bytes':len(padding),'roundtrip':'container + original DES decrypt/encrypt exact'}
        else:
            entries=indexed_layout(data)
            if entries is None:
                entries=merged_layout(data)
                if entries is None:continue
                if data[:1]+b''.join(len(raw).to_bytes(2,'big')+raw for _,raw in entries)!=data:raise ValueError('Merged roundtrip')
                kind='merged-BE16';extra={'roundtrip':'count + BE16 length + payload exact'}
            else:
                if data[:4+len(entries)*2]+b''.join(raw for _,raw in entries)!=data:raise ValueError('Indexed roundtrip')
                kind='indexed-offsets';extra={'roundtrip':'original table + payloads exact'}
            if not any(any('\uac00'<=c<='\ud7a3' for c in raw.decode('cp949')) for _,raw in entries):continue
        files.append({'resource':name,'format':kind,'count':len(entries),'sha256':sha(data),**extra})
        for i,(at,raw) in enumerate(entries):
            body=raw if kind=='merged-BE16' else raw[:-1];text=body.decode('cp949')
            if text.encode('cp949')!=body:raise ValueError('Codec roundtrip')
            catalog.append({'id':name+':'+str(i),'resource':name,'index':i,'offset_decrypted':at,'bytes':len(raw),'sha256':sha(raw),'ko':text,'en':'','review':'pending'})
    # A small offline seed, for review; it is not injected into the game yet.
    seeds={'{게임시작':'{Start Game','{환경설정':'{Options','{게임방법':'{How to Play','{게임종료':'{Quit Game',
        '매도우 힐 3':'Meadow Hill 3','뭐하는 거야, 티르!':'What are you doing, Tyr!',
        '다리가...':'My leg...', '이러다가 붙잡히겠어!':"They'll catch us at this rate!",
        '너.. 너라도 먼저가!!':'You... Go on without me!!','웃기지마!':"Don't be ridiculous!",
        '헛소리 하지마. 우리 얼굴을 다 봤는데, 신고라도 하면...':"Don't be stupid. They've seen our faces. If they report us..."}
    for item in catalog:
        if item['ko'] in seeds:item['en']=seeds[item['ko']];item['review']='draft; names need glossary'
    catalog_path=out/'catalog.ko-en.json'
    previous={r['id']:r for r in json.loads(catalog_path.read_text(encoding='utf-8'))} if catalog_path.exists() else {}
    for item in catalog:
        item['ptbr']=''
        old=previous.get(item['id'])
        if old and old['sha256']==item['sha256'] and old['ko']==item['ko']:
            for field in ('en','ptbr','review','translation_method'):
                if field in old:item[field]=old[field]
    catalog_path.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    report={'date':'2026-10-21','method':'offline original SaveDataInit/MX_desDecrypt/MX_desEncrypt; no network',
        'encoding':'CP949; strict decode/encode identity for extracted records','files':files,'records':len(catalog),
        'english_drafts':sum(bool(x['en']) for x in catalog),'unparsed_scenes':failures,
        'catalog_local':'scratch/localization/catalog.ko-en.json','limits':['indexed text + SC scene text only; items/quests/ELF strings not fully parsed','English drafts not integrated into renderer','all control markers and original script bytes preserved']}
    (ROOT/'reference/analysis/TEXT_INVENTORY.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('TEXT:',len(files),'resources,',len(catalog),'records,',len(failures),'unparsed scenes;',report['english_drafts'],'English drafts')

if __name__=='__main__':main()
