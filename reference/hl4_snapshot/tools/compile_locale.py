"""Compile separate canonical/English/PTBR dictionaries; never changes APK/guest resources."""
import argparse,hashlib,json,re,struct,unicodedata
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def notification_text(s, canonical='', language='en'):
    # Original quest notifications indent the second line with black dots.
    # That Korean-specific padding consumes the translated line's width.
    # Keep the explicit line break and white colour tag, without fake spacing.
    match=re.search(r';\$\.{2,}\{', s)
    if not match:return s
    statuses={'퀘스트 입수!':('Quest received!','Missão recebida!'),
              '퀘스트 완료!':('Quest complete!','Missão concluída!')}
    original_status=canonical.rsplit('{',1)[-1]
    if original_status in statuses:
        # Translate these two exact original statuses consistently, including
        # drafts which incorrectly said "Get in the quests!".
        return s[:match.start()]+';{'+statuses[original_status][language=='ptbr']
    return s[:match.start()]+';{'+s[match.end():]

def english_font_text(s):
    for old,new in [('’',"'"),('‘',"'"),('“','"'),('”','"'),('—','-'),('–','-'),('…','...')]:s=s.replace(old,new)
    contractions={"don't":"do not","Don't":"Do not","they've":"they have","They've":"They have","they'll":"they will","They'll":"They will","can't":"cannot","I'm":"I am","it's":"it is","It's":"It is"}
    for old,new in contractions.items():s=s.replace(old,new)
    return unicodedata.normalize('NFKD',s).encode('ascii','ignore').decode('ascii')

def compile_pack(catalog,destination):
    records=json.loads(Path(catalog).read_text(encoding='utf-8'));unique={}
    for r in records:
        assert r.get('en') and r.get('ptbr'),'Untranslated record: '+r['id']
        ko=r['ko'].encode('cp949');en=english_font_text(notification_text(r['en'],r['ko'])).encode('ascii');pt=notification_text(r['ptbr'],r['ko'],'ptbr').encode('utf-8')
        assert b'\0' not in ko+en+pt and ko and en and pt
        unique.setdefault(ko,(en,pt))
    for line in (ROOT/'assets/locales/runtime_extra.tsv').read_text(encoding='utf-8').splitlines():
        if not line or line.startswith('#'):continue
        ko,en,pt=line.split('\t')
        unique[ko.encode('cp949')]=(english_font_text(en).encode('ascii'),pt.encode('utf-8'))
    out=bytearray(b'HL4LANG1'+struct.pack('<I',len(unique)))
    for ko,(en,pt) in sorted(unique.items()):out+=struct.pack('<III',len(ko),len(en),len(pt))+ko+en+pt
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(out)
    print('LANGUAGE PACK:',len(records),'records,',len(unique),'canonical strings,',len(out),'bytes, SHA256',hashlib.sha256(out).hexdigest())

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--catalog',default=ROOT/'scratch/localization/catalog.ko-en.json');p.add_argument('--output',default=ROOT/'dist/windows/hl4.lang');a=p.parse_args();compile_pack(a.catalog,a.output)
