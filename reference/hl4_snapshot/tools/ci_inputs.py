"""Publish/fetch owner-provided private CI inputs, never commit APKs or keys."""
import argparse,hashlib,json,os,subprocess,sys,zipfile
from pathlib import Path
from github_ci import api,REPO
ROOT=Path(__file__).resolve().parents[1]
TAG='ci-inputs';NAME='hl4-ci-inputs.zip'
APK=json.loads((ROOT/'project.json').read_text())['original_apk']['path']
FILES=[APK,'scratch/localization/catalog.ko-en.json','scratch/branding/app-icon.jpg']

def publish():
    # Deliberately refuse to upload original assets to a public repository.
    info=api('https://api.github.com/repos/'+REPO)
    if not info['private']:raise RuntimeError('CI inputs require a private repository')
    out=ROOT/'scratch'/NAME
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
        for name in FILES:z.write(ROOT/name,name)
    releases=api('releases');release=next((r for r in releases if r['tag_name']==TAG),None)
    if release is None:release=api('releases','POST',{'tag_name':TAG,'name':'Private HL4 build inputs','draft':False,'prerelease':True,'body':'Owner-authorized original APK and localization draft. No signing keys. CI inputs in a private repository; not a playable release.'})
    digest=hashlib.sha256(out.read_bytes()).hexdigest()
    asset_name='hl4-ci-inputs-'+digest[:16]+'.zip'
    # Keep old immutable assets: previous commits pin their IDs for rebuilds.
    asset=next((v for v in release['assets'] if v['name']==asset_name),None)
    if asset is None:asset=api(release['upload_url'].split('{')[0]+'?name='+asset_name,'POST',out.read_bytes())
    manifest={'asset_id':asset['id'],'sha256':digest,'files':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in FILES}}
    (ROOT/'ci-inputs.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Private inputs uploaded:',asset['id'],out.stat().st_size,'bytes')

def fetch():
    manifest=json.loads((ROOT/'ci-inputs.json').read_text())
    raw=api('releases/assets/'+str(manifest['asset_id']),binary=True)
    if hashlib.sha256(raw).hexdigest()!=manifest['sha256']:raise ValueError('CI input archive checksum')
    import io
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        if set(z.namelist())!=set(manifest['files']):raise ValueError('CI input allowlist')
        for name,expected in manifest['files'].items():
            data=z.read(name)
            if hashlib.sha256(data).hexdigest()!=expected:raise ValueError('CI input file checksum: '+name)
            path=(ROOT/name).resolve()
            if not path.is_relative_to(ROOT):raise ValueError('CI input traversal')
            if path.exists() and path.read_bytes()!=data:raise ValueError('Refusing to overwrite different local input: '+name)
            path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    print('Private CI inputs verified and extracted')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('mode',choices=['publish','fetch']);a=p.parse_args()
    publish() if a.mode=='publish' else fetch()
