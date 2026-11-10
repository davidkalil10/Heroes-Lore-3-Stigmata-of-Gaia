"""Package exact pinned APK bytes in a simple local archive; never modifies input."""
import hashlib,io,json,struct,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def pack(destination):
    p=json.loads((ROOT/'project.json').read_text(encoding='utf-8'))
    apk=(ROOT/p['original_apk']['path']).read_bytes()
    if hashlib.sha256(apk).hexdigest()!=p['original_apk']['sha256']:raise ValueError('APK SHA-256 mismatch')
    with zipfile.ZipFile(io.BytesIO(apk)) as z:
        elf=z.read('lib/armeabi/libbinary.so');jar=z.read('res/raw/jarfile.mp3')
    with zipfile.ZipFile(io.BytesIO(jar)) as z:
        entries={'@elf':elf,**{n:z.read(n) for n in z.namelist() if not n.endswith('/')}}
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
    with destination.open('wb') as f:
        f.write(b'HL4DATA1'+struct.pack('<I',len(entries)))
        for name,blob in sorted(entries.items()):
            name=name.encode('utf-8');f.write(struct.pack('<II',len(name),len(blob))+name+hashlib.sha256(blob).digest()+blob)
    print('PACKED',destination,len(entries),'entries',destination.stat().st_size,'bytes')

if __name__=='__main__':
    import sys
    pack(sys.argv[1] if len(sys.argv)>1 else ROOT/'dist/windows/hl4.dat')
