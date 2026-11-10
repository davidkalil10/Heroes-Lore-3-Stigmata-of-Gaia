"""Encode the owner-provided artwork for platform packaging, offline."""
import argparse,hashlib,io,json,sys,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scratch/analysis_deps'))
from PIL import Image

def prepare(output,windows_resource=None):
    config=json.loads((ROOT/'assets/branding/icon.json').read_text())
    raw=(ROOT/config['source']).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=config['sha256']:raise ValueError('Owner icon checksum')
    icon=Image.open(io.BytesIO(raw)).convert('RGB')
    if icon.size!=tuple(config['size']) or icon.width!=icon.height:raise ValueError('Owner icon dimensions')
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    # Packaging conversion only; source artwork remains byte-for-byte intact.
    icon.resize((1024,1024),Image.Resampling.LANCZOS).save(output/'hl4-icon.png')
    icon.resize((256,256),Image.Resampling.LANCZOS).save(output/'hl4-icon.bmp')
    icon.resize((256,256),Image.Resampling.LANCZOS).save(output/'hl4-icon.jpg',quality=95)
    icon.convert('RGBA').save(output/'hl4-icon.ico',format='ICO',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
    if windows_resource:
        rc=Path(windows_resource);rc.parent.mkdir(parents=True,exist_ok=True)
        rc.write_text('1 ICON "'+(output/'hl4-icon.ico').resolve().as_posix()+'"\n')
    android=ROOT/'build/android-res/drawable';android.mkdir(parents=True,exist_ok=True)
    icon.resize((512,512),Image.Resampling.LANCZOS).save(android/'hl4_icon.png')
    # Adaptive layers have a 108dp canvas with a central 72dp masked viewport.
    # Put the supplied art in that viewport; fill overscan with opaque dark
    # pixels so launcher animations never expose a white/transparent plate.
    layer=Image.new('RGB',(1296,1296),(35,40,43))
    layer.paste(icon.resize((864,864),Image.Resampling.LANCZOS),(216,216))
    layer.save(android/'hl4_icon_adaptive.png')
    fallback=ROOT/'build/android-res/mipmap';fallback.mkdir(parents=True,exist_ok=True)
    icon.resize((512,512),Image.Resampling.LANCZOS).save(fallback/'hl4_launcher.png')
    adaptive=ROOT/'build/android-res/mipmap-anydpi-v26';adaptive.mkdir(parents=True,exist_ok=True)
    (adaptive/'hl4_launcher.xml').write_text('''<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@color/hl4_icon_background" />
    <foreground android:drawable="@drawable/hl4_icon_adaptive" />
</adaptive-icon>
''')
    values=ROOT/'build/android-res/values';values.mkdir(parents=True,exist_ok=True)
    (values/'hl4_icon.xml').write_text('<resources><color name="hl4_icon_background">#23282b</color></resources>\n')
    print('Owner HL4 icon:',icon.size,'source SHA256',config['sha256'])

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',default=ROOT/'build/runtime');p.add_argument('--windows-resource');a=p.parse_args();prepare(a.output,a.windows_resource)
