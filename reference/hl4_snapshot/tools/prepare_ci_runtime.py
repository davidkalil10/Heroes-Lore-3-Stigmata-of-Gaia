"""Create game data from pinned private CI inputs, not from the Git tree."""
from pathlib import Path
import shutil
from pack_runtime import pack
from compile_locale import compile_pack
from prepare_icons import prepare
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/runtime';out.mkdir(parents=True,exist_ok=True)
pack(out/'hl4.dat')
compile_pack(ROOT/'scratch/localization/catalog.ko-en.json',out/'hl4.lang')
prepare(out)
shutil.copytree(ROOT/'third_party/licenses',out/'licenses',dirs_exist_ok=True)
shutil.copy2(ROOT/'third_party/NOTICE.txt',out/'licenses/NOTICE.txt')
