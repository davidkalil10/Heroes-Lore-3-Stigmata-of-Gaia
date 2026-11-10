"""Generate the MSVC import library for the shipped Unicorn DLL (not its static .lib)."""
import re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
dll=root/'scratch/analysis_deps/unicorn/lib/unicorn.dll'
out=subprocess.check_output(['dumpbin','/exports',str(dll)],text=True)
names=re.findall(r'^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(uc_\w+)\s*$',out,re.M)
if len(names)<40:raise ValueError('Unexpected Unicorn DLL export table')
definition=root/'build/unicorn.def'
definition.write_text('LIBRARY unicorn.dll\nEXPORTS\n'+'\n'.join(names)+'\n',encoding='ascii')
subprocess.run(['lib','/nologo','/machine:x64','/def:'+str(definition),'/out:'+str(root/'build/unicorn_dynamic.lib')],check=True)
