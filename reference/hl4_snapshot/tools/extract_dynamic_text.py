"""Extract verified length-prefixed HL4 equipment/enemy names, offline.

The ARM DES routines validate decrypt/encrypt roundtrips. Original records and
numeric payloads stay immutable; this only extends the separate language catalog.
"""
import contextlib,hashlib,io,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def records(blob):
    at=0;result=[]
    while at+2<=len(blob) and int.from_bytes(blob[at:at+2],'little'):
        n=int.from_bytes(blob[at:at+2],'little')
        if n<1 or at+2+n>len(blob):raise ValueError('Sized record bounds')
        result.append((at+2,blob[at+2:at+2+n]));at+=n+2
    padding=blob[at:]
    if len(padding)>7 or any(padding):raise ValueError('Unexpected DES padding')
    rebuilt=b''.join(len(b).to_bytes(2,'little')+b for _,b in result)+padding
    if rebuilt!=blob:raise ValueError('Record roundtrip')
    return result

def text_at(payload,offset):
    if offset>=len(payload):raise ValueError('Text length offset')
    n=payload[offset];raw=payload[offset+1:offset+1+n]
    if len(raw)!=n or b'\0' in raw:raise ValueError('Text bounds/NUL')
    text=raw.decode('cp949')
    if text.encode('cp949')!=raw:raise ValueError('Text roundtrip')
    return text,raw

def extract():
    import sys
    sys.path.insert(0,str(ROOT/'tools'))
    from trace_boot import Wipi,TEMP
    with contextlib.redirect_stdout(io.StringIO()):r=Wipi();r.call('SaveDataInit')
    added=[];files=[]
    for name,encrypted in sorted(r.resources.items()):
        item=re.fullmatch(r'res/ITM/DAT/_ITM_(\d\d)_DAT',name)
        gear=re.fullmatch(r'res/ITM/DAT/_ITM_\d_SD',name)
        enemy=re.fullmatch(r'res/E/_(E|B)SDAT_[012]',name)
        npc=name=='res/NPC/NAME_DAT'
        if not(item or gear or enemy or npc):continue
        if npc:plain=encrypted
        else:
            if len(encrypted)%8:raise ValueError('Encrypted block alignment')
            with contextlib.redirect_stdout(io.StringIO()):
                r.uc.mem_write(TEMP+0x10000,encrypted);r.call('MX_desDecrypt',(TEMP+0x10000,len(encrypted)))
                plain=bytes(r.uc.mem_read(TEMP+0x10000,len(encrypted)))
                r.call('MX_desEncrypt',(TEMP+0x10000,len(encrypted)))
            if bytes(r.uc.mem_read(TEMP+0x10000,len(encrypted)))!=encrypted:raise ValueError('DES roundtrip '+name)
        rows=records(plain);count=0
        for index,(at,payload) in enumerate(rows):
            offset=0 if gear or npc or enemy and enemy[1]=='B' else 1
            try:text,raw=text_at(payload,offset)
            except (ValueError,UnicodeError) as exc:raise ValueError((name,index,payload[:30].hex(),offset)) from exc
            if not re.search('[\uac00-\ud7a3]',text):continue
            added.append({'id':name+':'+str(index)+':name','resource':name,'index':index,
                          'offset_decrypted':at+offset+1,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),
                          'ko':text,'en':'','ptbr':'','review':'pending; equipment/enemy name'})
            count+=1
        files.append({'resource':name,'records':len(rows),'names':count,'sha256':hashlib.sha256(encrypted).hexdigest(),
                      'format':'LE16 payload length; byte text length at payload+'+str(offset),
                      'roundtrip':'container exact; DES decrypt/encrypt exact' if not npc else 'container exact'})
    path=ROOT/'scratch/localization/catalog.ko-en.json';old=json.loads(path.read_text(encoding='utf8'));byid={x['id']:x for x in old}
    for x in added:
        previous=byid.get(x['id'])
        if previous:
            if previous['sha256']!=x['sha256'] or previous['ko']!=x['ko']:raise ValueError('Changed name record')
        else:old.append(x)
    path.write_text(json.dumps(old,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    report={'result':'PASS','method':'Offline original ARM DES; strict record and CP949 roundtrips','names':len(added),'catalog_records':len(old),'files':files}
    (ROOT/'reference/analysis/DYNAMIC_TEXT_INVENTORY.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS dynamic text:',len(added),'names;',len(old),'catalog records')

if __name__=='__main__':extract()
