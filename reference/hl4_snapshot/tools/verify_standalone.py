"""Check the recursive Windows PE import graph, then run with a minimal PATH."""
import argparse,json,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SYSTEM={'kernel32.dll','user32.dll','gdi32.dll','advapi32.dll','shell32.dll','ole32.dll',
        'oleaut32.dll','setupapi.dll','winmm.dll','imm32.dll','version.dll','hid.dll',
        'ws2_32.dll','bcrypt.dll','ntdll.dll','msvcrt.dll','ucrtbase.dll'}

def imports(path):
    b=path.read_bytes()
    def word(fmt,at):return struct.unpack_from(fmt,b,at)
    pe=word('<I',0x3c)[0]
    if b[pe:pe+4]!=b'PE\0\0':raise ValueError('Not PE: '+str(path))
    machine,sections=word('<HH',pe+4);optional=pe+24
    if machine!=0x8664 or word('<H',optional)[0]!=0x20b:raise ValueError('Expected x64 PE32+')
    table=optional+word('<H',pe+20)[0]
    spans=[word('<IIII',table+40*i+8) for i in range(sections)]
    def offset(rva):
        for size,va,raw_size,raw in spans:
            if va<=rva<va+max(size,raw_size):return raw+rva-va
        raise ValueError('Unmapped RVA')
    rva,size=word('<II',optional+112+8)
    if not rva:return []
    at=offset(rva);names=[]
    while any(word('<5I',at)):
        name=offset(word('<I',at+12)[0]);end=b.index(0,name)
        names.append(b[name:end].decode('ascii'));at+=20
    return names

def verify(folder):
    folder=Path(folder).resolve();files={p.name.lower():p for p in folder.iterdir() if p.is_file()}
    for required in ['heroes_lore_4.exe','sdl2.dll','unicorn.dll','vcruntime140.dll','hl4.dat','hl4.lang']:
        if required not in files:raise ValueError('Missing bundled file: '+required)
    graph={};pending=[files['heroes_lore_4.exe']];visited=set()
    while pending:
        path=pending.pop()
        if path.name.lower() in visited:continue
        visited.add(path.name.lower());graph[path.name]=imports(path)
        for name in graph[path.name]:
            lower=name.lower()
            if lower in files:pending.append(files[lower])
            elif lower not in SYSTEM and not lower.startswith(('api-ms-win-','ext-ms-win-')):
                raise ValueError('Unbundled non-system dependency: '+name)
    return graph

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder',nargs='?',default=ROOT/'dist/windows')
    parser.add_argument('--run',action='store_true')
    args=parser.parse_args();folder=Path(args.folder).resolve();graph=verify(folder)
    if args.run:
        cwd=ROOT/'scratch/standalone-empty-cwd';cwd.mkdir(parents=True,exist_ok=True)
        env=dict(os.environ);env['PATH']=str(Path(os.environ['SystemRoot'])/'System32')
        env['SDL_VIDEODRIVER']='dummy';env['SDL_AUDIODRIVER']='dummy'
        result=subprocess.run([str(folder/'heroes_lore_4.exe'),'--verify-core','--save-dir',str(ROOT/'scratch/standalone-core-saves')],cwd=cwd,env=env,capture_output=True,text=True,timeout=60)
        (ROOT/'scratch/standalone-core.txt').write_text(result.stdout+result.stderr,encoding='utf-8')
        if result.returncode or 'PASS native core' not in result.stdout:raise ValueError('Standalone core failed: '+str(result.returncode)+' '+result.stderr)
    print(json.dumps(graph,indent=2));print('PASS: recursive non-system DLLs bundled'+('; native core ran with System32-only PATH and separate working directory' if args.run else ''))

if __name__=='__main__':main()
