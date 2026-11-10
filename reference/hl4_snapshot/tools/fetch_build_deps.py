"""Fetch build sources; never copies identifiers or runtime hooks from HL5."""
import argparse,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEPS={
 'unicorn':('https://github.com/unicorn-engine/unicorn.git','8028ec436f2d9376525352dd38ed9ed6b9f6be10'),
 'unicorn-tci':('https://github.com/1rhino2/unicorn-tci.git','e3f075a62742913bb734a55a474aec65c9e5745e'),
 'SDL2':('https://github.com/libsdl-org/SDL.git','release-2.30.12'),
}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--switch',action='store_true');p.add_argument('--android',action='store_true');p.add_argument('--tci',action='store_true');a=p.parse_args()
 names=['unicorn-tci' if a.switch or a.tci else 'unicorn']+([] if a.switch else ['SDL2'])
 for name in names:
  url,ref=DEPS[name];folder=ROOT/'third_party'/name
  if not folder.exists():
   folder.mkdir(parents=True);subprocess.run(['git','init',str(folder)],check=True)
   subprocess.run(['git','-C',str(folder),'remote','add','origin',url],check=True)
  subprocess.run(['git','-C',str(folder),'fetch','--depth','1','origin',ref],check=True)
  subprocess.run(['git','-C',str(folder),'checkout','--detach','FETCH_HEAD'],check=True)
  if a.switch:
   subprocess.run(['python3',str(ROOT/'tools/patch_unicorn_switch.py'),str(folder/'unicorn-engine-sys-tci')],check=True)
