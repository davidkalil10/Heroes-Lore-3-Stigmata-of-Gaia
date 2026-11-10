"""Translate locally on the PC; this program has no network client.

NLLB model files and CTranslate2 dependencies live under ignored scratch/.
Machine drafts remain explicitly marked for editorial review.
"""
import ctypes,hashlib,json,os,re,shutil,sys,time,unicodedata
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CATALOG=ROOT/'scratch/localization/catalog.ko-en.json'
CONTROL=re.compile(r'(%[-+ #0.\d]*[sdiuxXfcp]|%\d*|[\n\r;{}|$\'"`^<&])')
HANGUL=re.compile(r'[\u1100-\u11ff\u3130-\u318f\uac00-\ud7af]')

def pieces(s):return CONTROL.split(s)
def structural(s):return [x for i,x in enumerate(pieces(s)) if i%2]
def clean_fragment(s):
    s=s.strip()
    replacements={"don't":"do not","Don't":"Do not","can't":"cannot","Can't":"Cannot","won't":"will not","I'm":"I am","it's":"it is","It's":"It is","that's":"that is","That's":"That is","they're":"they are","we're":"we are","you're":"you are","I'll":"I will","we'll":"we will","you'll":"you will","didn't":"did not","isn't":"is not","doesn't":"does not","there's":"there is"}
    for a,b in replacements.items():s=s.replace(a,b)
    # Generated control characters must not become guest color/format commands.
    s=s.translate(str.maketrans({'&':' and ',"'":'', '"':'','{':'','}':'','|':' ','$':'','`':'','^':'','<':'',';':','}))
    return s

def main():
    original=CATALOG.read_bytes();items=json.loads(original)
    backup=CATALOG.with_name('catalog.before-translation.json')
    if not backup.exists():backup.write_bytes(original)
    sys.path.insert(0,str(ROOT/'scratch/translation_deps'))
    bins=[str(p.resolve()) for p in (ROOT/'scratch/translation_gpu').rglob('bin')]
    os.environ['PATH']=os.pathsep.join(bins+[os.environ['PATH']]);handles=[os.add_dll_directory(p) for p in bins]
    dlls=[ctypes.WinDLL(str(next((ROOT/'scratch/translation_gpu').rglob(n)).resolve())) for n in ['cublasLt64_12.dll','cublas64_12.dll','cudart64_12.dll']]
    import ctranslate2
    from tokenizers import Tokenizer
    model=ROOT/'scratch/translation_models/nllb';tokenizer=Tokenizer.from_file(str(model/'tokenizer.json'))
    translator=ctranslate2.Translator(str(model),device='cuda',compute_type='int8_float16',inter_threads=1,intra_threads=2)
    segments=list(dict.fromkeys(part.strip() for item in items for i,part in enumerate(pieces(item['ko'])) if i%2==0 and part.strip()))
    cache_path=ROOT/'scratch/localization/translation-cache.json'
    cache=json.loads(cache_path.read_text(encoding='utf-8')) if cache_path.exists() else {'en':{},'ptbr':{}}
    start=time.monotonic()
    for lang,target in [('en','eng_Latn'),('ptbr','por_Latn')]:
        missing=[x for x in segments if x not in cache[lang] and (HANGUL.search(x) or lang=='ptbr' and re.search('[A-Za-z]',x))]
        for offset in range(0,len(missing),32):
            batch=missing[offset:offset+32]
            sources=[(['kor_Hang'] if HANGUL.search(x) else ['eng_Latn'])+tokenizer.encode(x,add_special_tokens=False).tokens+['</s>'] for x in batch]
            results=translator.translate_batch(sources,target_prefix=[[target]]*len(batch),beam_size=2,max_decoding_length=256,repetition_penalty=1.1,max_batch_size=1024,batch_type='tokens')
            for text,result in zip(batch,results):
                out=clean_fragment(tokenizer.decode([tokenizer.token_to_id(t) for t in result.hypotheses[0]],skip_special_tokens=True))
                if not out:out=text if not HANGUL.search(text) else '...'
                cache[lang][text]=out
            if offset%128==0:
                cache_path.write_text(json.dumps(cache,ensure_ascii=False),encoding='utf-8')
                print(lang,offset+len(batch),'/',len(missing),'elapsed',round(time.monotonic()-start),flush=True)
        cache_path.write_text(json.dumps(cache,ensure_ascii=False),encoding='utf-8')
    for item in items:
        added_translation=False
        for lang in ['en','ptbr']:
            if item.get(lang):continue
            added_translation=True
            out=[]
            for i,part in enumerate(pieces(item['ko'])):
                if i%2 or not part.strip():out.append(part);continue
                leading=part[:len(part)-len(part.lstrip())];trailing=part[len(part.rstrip()):]
                out.append(leading+cache[lang].get(part.strip(),part.strip())+trailing)
            item[lang]=''.join(out)
        if added_translation:item['translation_method']='offline NLLB-600M, direct ko-en/ko-pt; draft pending editorial review'
    # Preserve source and every non-translation field, including stable IDs.
    old=json.loads(original)
    for a,b in zip(old,items):
        assert all(b[k]==v for k,v in a.items() if k not in ('en','ptbr','translation_method'))
        if not a.get('ptbr'):assert structural(a['ko'])==structural(b['ptbr'])
        if not a.get('en'):assert structural(a['ko'])==structural(b['en'])
    temp=CATALOG.with_suffix('.tmp');temp.write_text(json.dumps(items,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');temp.replace(CATALOG)
    print('DONE',len(items),'records; EN',sum(bool(x['en']) for x in items),'PTBR',sum(bool(x['ptbr']) for x in items),flush=True)

if __name__=='__main__':main()
