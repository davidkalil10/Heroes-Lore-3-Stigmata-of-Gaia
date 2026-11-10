"""Corresponding sources for Android/Switch GPL host and engine artifacts."""
import argparse,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('platform',choices=['android','switch','all']);a=p.parse_args()
out=ROOT/'build'/('hl4-'+a.platform+'-corresponding-source.zip')
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    directories=['src','tools','platforms','android','docs','assets/locales','assets/branding','third_party/SDL2','third_party/licenses']
    directories += ['third_party/unicorn','third_party/unicorn-tci'] if a.platform=='all' else ['third_party/unicorn-tci' if a.platform=='switch' else 'third_party/unicorn']
    for directory in directories:
        base=ROOT/directory
        if not base.exists():continue
        for path in base.rglob('*'):
            if path.is_file() and not any(v in path.parts for v in ['.git','__pycache__','.gradle','.cxx','build']) and path.suffix not in ['.jks','.keystore','.apk']:z.write(path,path.relative_to(ROOT))
    for name in ['CMakeLists.txt','project.json','ci-inputs.json','.github/workflows/build.yml','third_party/NOTICE.txt']:z.write(ROOT/name,name)
print('Corresponding sources:',out)
