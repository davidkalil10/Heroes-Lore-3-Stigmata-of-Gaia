"""HL4 GitHub CI operations; obtain credentials from Git without logging them."""
import argparse,json,os,subprocess,urllib.request,urllib.error
from urllib.parse import urlparse
from pathlib import Path
REPO='davidkalil10/Heroes-Lore-4-Phantasmal-Mask-Recomp'

class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,req,fp,code,msg,headers,newurl):
        redirected=super().redirect_request(req,fp,code,msg,headers,newurl)
        if redirected is not None and urlparse(req.full_url).netloc!=urlparse(newurl).netloc:
            redirected.remove_header('Authorization')
        return redirected

def token():
    value=os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
    if value:return value
    env=dict(os.environ,GIT_TERMINAL_PROMPT='0',GCM_INTERACTIVE='never')
    r=subprocess.run(['git','credential','fill'],input='protocol=https\nhost=github.com\n\n',text=True,capture_output=True,env=env,check=True)
    fields=dict(line.split('=',1) for line in r.stdout.splitlines() if '=' in line)
    if not fields.get('password'):raise RuntimeError('GitHub credential unavailable')
    return fields['password']

def api(path,method='GET',body=None,binary=False,accept=None):
    url=path if path.startswith('https://') else 'https://api.github.com/repos/'+REPO+'/'+path
    if urlparse(url).netloc not in ('api.github.com','uploads.github.com'):
        raise ValueError('Credentials may only be sent to the GitHub API/upload hosts')
    data=body if isinstance(body,bytes) else json.dumps(body).encode() if body is not None else None
    headers={'Authorization':'Bearer '+token(),'Accept':'application/octet-stream' if binary else 'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28','User-Agent':'HL4-build-tools'}
    if accept:headers['Accept']=accept
    if data is not None:headers['Content-Type']='application/octet-stream' if isinstance(body,bytes) else 'application/json'
    with urllib.request.build_opener(SafeRedirect()).open(urllib.request.Request(url,data=data,headers=headers,method=method),timeout=120) as r:
        content=r.read()
    return content if binary else json.loads(content) if content else None

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('path');p.add_argument('--method',default='GET');p.add_argument('--body');p.add_argument('--output');a=p.parse_args()
    result=api(a.path,a.method,json.loads(a.body) if a.body else None,binary=bool(a.output))
    if a.output:Path(a.output).write_bytes(result);print('Downloaded',a.output)
    else:print(json.dumps(result,indent=2))
