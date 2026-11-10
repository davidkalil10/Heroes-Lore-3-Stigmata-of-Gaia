# -*- mode: python ; coding: utf-8 -*-
from PyInstaller.utils.hooks import collect_data_files
from PyInstaller.utils.hooks import collect_dynamic_libs
from PyInstaller.utils.hooks import collect_submodules
from PyInstaller.utils.hooks import collect_all

datas = [('C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools', 'tools'), ('C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/project.json', '.'), ('C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/config', 'config')]
binaries = []
hiddenimports = ['OpenGL.platform.win32', 'OpenGL.arrays.ctypesarrays', 'OpenGL.arrays.strings', 'OpenGL.arrays.numbers', 'PIL.Image']
datas += collect_data_files('OpenGL')
binaries += collect_dynamic_libs('unicorn')
binaries += collect_dynamic_libs('capstone')
hiddenimports += collect_submodules('unicorn')
hiddenimports += collect_submodules('capstone')
tmp_ret = collect_all('pygame')
datas += tmp_ret[0]; binaries += tmp_ret[1]; hiddenimports += tmp_ret[2]


a = Analysis(
    ['C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools/run_pc.py'],
    pathex=['C:/Users/david/Desktop/Projetos_Flutter/heroes_lore_3_recomp/tools'],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='HeroesLore3',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='HeroesLore3',
)
