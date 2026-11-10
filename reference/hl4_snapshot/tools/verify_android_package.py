"""Check the actual APK assets, ARM64 entry point and dependency closure."""
import argparse,hashlib,io,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scratch/analysis_deps'))
from elftools.elf.elffile import ELFFile
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('apk');p.add_argument('--abis',nargs='+',default=['arm64-v8a']);a=p.parse_args()
machines={'arm64-v8a':('EM_AARCH64',64),'armeabi-v7a':('EM_ARM',32),'x86_64':('EM_X86_64',64)}
system={'libc.so','libm.so','libdl.so','liblog.so','libandroid.so','libGLESv1_CM.so','libGLESv2.so','libEGL.so','libOpenSLES.so'}
with zipfile.ZipFile(a.apk) as z:
    for name in ['hl4.dat','hl4.lang']:
        assert z.read('assets/'+name)==(ROOT/'build/runtime'/name).read_bytes(),'Missing/stale runtime asset '+name
    for filename in ['hl4_icon.png','hl4_icon_adaptive.png']:
        names=[n for n in z.namelist() if n.startswith('res/drawable') and n.endswith('/'+filename)]
        assert len(names)==1,'Missing owner launcher icon '+filename
        actual_image=Image.open(io.BytesIO(z.read(names[0]))).convert('RGBA')
        expected_image=Image.open(ROOT/'build/android-res/drawable'/filename).convert('RGBA')
        assert actual_image.size==expected_image.size and actual_image.tobytes()==expected_image.tobytes(),'Stale launcher artwork '+filename
    adaptive=[n for n in z.namelist() if n.startswith('res/mipmap-anydpi-v26') and n.endswith('/hl4_launcher.xml')]
    assert len(adaptive)==1,'Missing adaptive launcher resource'
    axml=z.read(adaptive[0]);tag='adaptive-icon'
    assert tag.encode() in axml or tag.encode('utf-16le') in axml,'Not an adaptive icon'
    actual={n.split('/')[1] for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')}
    assert actual==set(a.abis),'APK ABI set: '+str(actual)
    for abi in a.abis:
        for name in ['libmain.so','libSDL2.so','libc++_shared.so']:assert 'lib/'+abi+'/'+name in z.namelist(),name
    for name in z.namelist():
        if not name.startswith('lib/') or not name.endswith('.so'):continue
        elf=ELFFile(io.BytesIO(z.read(name)))
        abi=name.split('/')[1]
        assert (elf['e_machine'],elf.elfclass)==machines[abi],name
        if name.endswith('/libmain.so'):
            symbols=elf.get_section_by_name('.dynsym').get_symbol_by_name('SDL_main')
            assert symbols and any(s['st_shndx']!='SHN_UNDEF' for s in symbols),'Missing SDL_main'
        for t in elf.get_section_by_name('.dynamic').iter_tags():
            if t.entry.d_tag=='DT_NEEDED':assert t.needed in system or 'lib/'+abi+'/'+t.needed in z.namelist(),'Unbundled dependency '+t.needed
print('PASS Android APK: exact assets, '+','.join(a.abis)+', SDL_main and dependency closure')
