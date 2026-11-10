"""Assert a native CI run reached gameplay and produced real nonzero audio."""
import argparse,json,os
from pathlib import Path
from verify_standalone import imports
p=argparse.ArgumentParser();p.add_argument('captures');p.add_argument('--windows-exe');a=p.parse_args()
s=json.loads((Path(a.captures)/'stats.json').read_text())
assert s['frames']>100 and s['audio_nonzero']>0,'No rendered/audio gameplay evidence'
assert any(x['callback']=='MainGameFrame_KeyPressed' and x['x']>0 and x['y']>0 for x in s['states']),'Gameplay not reached'
if a.windows_exe:
    system=Path(os.environ['SystemRoot'])/'System32'
    for name in imports(Path(a.windows_exe)):
        assert name.lower().startswith(('api-ms-win-','ext-ms-win-')) or (system/name).is_file(),'Missing non-system DLL: '+name
print('PASS CI desktop: gameplay state, frames, audio'+('; no non-system DLL imports' if a.windows_exe else ''))
