"""Package each desktop target with runtime assets; sources are a separate artifact."""
import argparse,hashlib,json,os,shutil,subprocess,tarfile,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('platform',choices=['windows','linux']);p.add_argument('--exe',required=True);a=p.parse_args()
dest=ROOT/'build/package';dest.mkdir(parents=True,exist_ok=True)
shutil.copy2(a.exe,dest/Path(a.exe).name)
for name in ['hl4.dat','hl4.lang','hl4-icon.bmp']:shutil.copy2(ROOT/'build/runtime'/name,dest/name)
if a.platform=='windows':
    instructions='Heroes Lore 4 - Windows x64\nExtract the entire ZIP and run heroes_lore_4.exe. SDL and Unicorn are linked statically; no extra DLL download is needed.\nSaves: saves/ beside the executable. Future local test builds reuse the same folder.\n'
else:
    instructions='Heroes Lore 4 - Linux x64\nRun the AppImage (chmod +x first) or extract the tar.gz and run HeroesLore4/AppRun.\nSaves: ~/.local/share/DavidKalilBraga/HeroesLore4/saves (or the XDG data directory).\n'
(dest/'README.txt').write_text(instructions+'English is the default; --language ko selects original Korean. Translation and gameplay validation remain incomplete.\nThe package does not contain personal saves. Licenses are in licenses/. Corresponding sources are a separate GitHub Actions artifact.\n',encoding='utf8')
shutil.copytree(ROOT/'third_party/licenses',dest/'licenses',dirs_exist_ok=True)
shutil.copy2(ROOT/'third_party/NOTICE.txt',dest/'licenses/NOTICE.txt')
files={path.relative_to(dest).as_posix():hashlib.sha256(path.read_bytes()).hexdigest() for path in dest.rglob('*') if path.is_file()}
(dest/'SHA256.json').write_text(json.dumps(files,indent=2)+'\n')
if a.platform=='windows':
    with zipfile.ZipFile(ROOT/'build/heroes_lore_4_windows_x64.zip','w',zipfile.ZIP_DEFLATED) as z:
        for path in dest.rglob('*'):
            if path.is_file():z.write(path,path.relative_to(dest))
else:
    app=ROOT/'AppDir';bindir=app/'usr/bin';bindir.mkdir(parents=True,exist_ok=True)
    shutil.copytree(dest,bindir,dirs_exist_ok=True)
    (app/'heroes_lore_4.desktop').write_text('[Desktop Entry]\nType=Application\nName=Heroes Lore 4\nExec=heroes_lore_4\nIcon=heroes_lore_4\nCategories=Game;\n')
    shutil.copy2(ROOT/'build/runtime/hl4-icon.png',app/'heroes_lore_4.png')
    (app/'AppRun').write_text('#!/bin/sh\nHERE="$(dirname "$(readlink -f "$0")")"\nexport LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\nexec "$HERE/usr/bin/heroes_lore_4" "$@"\n');(app/'AppRun').chmod(0o755)
    libs=app/'usr/lib';libs.mkdir(exist_ok=True)
    output=subprocess.check_output(['ldd',str(bindir/'heroes_lore_4')],text=True)
    for line in output.splitlines():
        fields=line.split()
        if '=>' not in fields:continue
        name=fields[0];source=Path(fields[2])
        if name.startswith(('libc.','libm.','libpthread.','librt.','libdl.')):continue
        if not source.is_file():raise ValueError('Missing runtime library '+line)
        shutil.copy2(source,libs/name)
    with tarfile.open(ROOT/'build/heroes_lore_4_linux_x64.tar.gz','w:gz') as tar:tar.add(app,arcname='HeroesLore4')
print('Packaged',a.platform)
