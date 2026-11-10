#!/usr/bin/env python3
"""Package only runtime dependencies, notices and corresponding host/engine source."""
import argparse,hashlib,json,re,shutil,zipfile
from pathlib import Path
from verify_standalone import verify

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'dist/HeroesLore4-Windows'

def main():
    global DEST
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--name',default='HeroesLore4-Windows-English-review');args=parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_-]+',args.name):raise ValueError('Package name must be a simple folder name')
    DEST=ROOT/'dist'/args.name
    DEST.mkdir(parents=True,exist_ok=True)
    runtime_names=('heroes_lore_4.exe','hl4.dat','hl4.lang','hl4-icon.bmp','SDL2.dll','unicorn.dll','vcruntime140.dll')
    for name in runtime_names:
        shutil.copy2(ROOT/'dist/windows'/name,DEST/name)
    shutil.copy2(ROOT/'docs/WINDOWS_README.txt',DEST/'LEIA-ME.txt')
    shutil.copytree(ROOT/'third_party/licenses',DEST/'licenses',dirs_exist_ok=True)
    shutil.copy2(ROOT/'third_party/NOTICE.txt',DEST/'licenses/NOTICE.txt')
    # Corresponding source stays separate from the playable portable ZIP.
    for name in ('unicorn-2.1.4-source.zip','hl4-host-source.zip'):
        (DEST/'licenses'/name).unlink(missing_ok=True)
    with zipfile.ZipFile(ROOT/'dist/hl4-corresponding-source.zip','w',zipfile.ZIP_DEFLATED) as z:
        z.write(ROOT/'scratch/unicorn-2.1.4-source.zip','unicorn-2.1.4-source.zip')
        for pattern in ('src/**/*','assets/locales/*.tsv','assets/branding/*.json','project.json','ci-inputs.json','tools/*.py','tools/*.cmd','tools/requirements*.txt','tests/*.py','tests/*.txt','docs/WINDOWS_RUNTIME.md','docs/POINTER_INPUT.md','docs/TEXT_LOCALIZATION.md','docs/LOCALIZATION_RUNTIME.md','third_party/NOTICE.txt'):
            for p in ROOT.glob(pattern):
                if p.is_file() and '__pycache__' not in p.parts:
                    z.write(p,p.relative_to(ROOT).as_posix())
    verify(DEST)
    # A user may already have saves/logs in this directory. Preserve them on disk
    # and include only explicitly produced runtime/license files in the archive.
    files=[DEST/n for n in (*runtime_names,'LEIA-ME.txt')]
    license_names=[p.relative_to(ROOT/'third_party/licenses') for p in (ROOT/'third_party/licenses').rglob('*') if p.is_file()]
    files += [DEST/'licenses'/n for n in license_names]
    files += [DEST/'licenses/NOTICE.txt']
    manifest={p.relative_to(DEST).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}
    (DEST/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    output=ROOT/'dist'/(args.name+'.zip')
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted([*files,DEST/'SHA256.json']):
            z.write(p,p.relative_to(DEST.parent).as_posix())
    print('PACKAGED',output,output.stat().st_size,'bytes; DLL dependencies verified')

if __name__=='__main__':main()
