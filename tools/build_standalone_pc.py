#!/usr/bin/env python3
"""Builds a 100% self-contained standalone PC distribution for Heroes Lore 3 Recomp.

Packages the native runner into `dist/HeroesLore3_PC/` containing:
  * HeroesLore3.exe (compiled binary)
  * All necessary runtime DLLs (Unicorn, Capstone, SDL2, OpenGL, Python)
  * Bundled game assets and configuration
  * Portable saves directory
  * Complete documentation and controls manual
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

README_CONTENT = """========================================================================
Heroes Lore 3: Stigmata of Gaia Recomp - PC Native Standalone Edition
========================================================================

Esta é a versão nativa e independente para PC (Windows x64) de
Heroes Lore 3: Stigmata of Gaia.

Não requer Python ou nenhuma outra ferramenta instalada na sua máquina!
Todos os executáveis, bibliotecas nativas (.dll) e recursos do jogo
estão incluídos nesta pasta.

------------------------------------------------------------------------
COMO JOGAR
------------------------------------------------------------------------
Execute:
  HeroesLore3.exe

Para rodar em tela cheia:
  Pressione F11 durante o jogo, ou execute via linha de comando:
  HeroesLore3.exe --fullscreen

Para ajustar a escala da janela (ex.: 3x = 1440x960):
  HeroesLore3.exe --scale 3

------------------------------------------------------------------------
CONTROLES
------------------------------------------------------------------------
1. TECLADO:
   * Movimentação: Setas direcionais ou W, A, S, D
   * Falar com NPC / Interagir: Tecla E, Enter ou Barra de Espaço
   * Ataque / Confirmar Menu: Tecla J ou Barra de Espaço
   * Iniciar Jogo (START GAME na tela título): Enter ou Barra de Espaço
   * Habilidade 1: Tecla U ou 1
   * Habilidade 2: Tecla I ou 2
   * Habilidade 3: Tecla O ou 3
   * Usar Item / Poção: Tecla K ou Q
   * Menu / Pausa: Tecla Esc ou Tab
   * Status do Personagem: Tecla P
   * Volume: Teclas + / = (Aumentar), - (Diminuir), M (Mudo)
   * F11: Alternar Tela Cheia / Janela
   * F12: Capturar Screenshot

2. CONTROLE / GAMEPAD (Xbox, PlayStation, etc.):
   * Movimentação: D-Pad ou Analógico Esquerdo
   * Ataque / Confirmar (A / X): Botão A (Xbox) / X (PS)
   * Habilidade 1: Botão X (Xbox) / Quadrado (PS)
   * Habilidade 2: Botão Y (Xbox) / Triângulo (PS)
   * Habilidade 3: Botão RB / R1
   * Usar Poção / Item: Botão LB / L1
   * Menu / Pausa: Botão Start / Options ou Botão B / Círculo
   * Status: Botão Select / Share

3. MOUSE / TOUCH DIRETO:
   * O jogo suporta clique e arraste com o mouse em qualquer lugar da tela,
     com resposta 1:1 e precisão de toque em todos os botões e menus!

------------------------------------------------------------------------
SAVES E CONFIGURAÇÕES
------------------------------------------------------------------------
Os arquivos de progresso e saves são salvos localmente na pasta:
  data/internal/Documents/

Ajustes de volume são persistidos automaticamente em:
  config/audio.json

Bom jogo!
========================================================================
"""


def strip_cfg(exe_path: Path):
    """Strips IMAGE_DLLCHARACTERISTICS_GUARD_CF (0x4000) from the PE Optional Header.

    PyInstaller's default Windows x64 bootloader enables Control Flow Guard (CFG).
    Unicorn Engine (which runs QEMU's TCG JIT to emulate ARMv6) uses internal setjmp/longjmp
    for CPU exits and dispatch loops. Under CFG, MSVC's longjmp validates that the target
    is registered in the CFG table. Since JIT targets are not registered in MSVC CFG,
    VCRUNTIME140.dll triggers __fastfail (0xc0000409).
    Disabling the CFG bit allows Unicorn to execute seamlessly without security termination.
    """
    import pefile

    pe = pefile.PE(str(exe_path))
    if pe.OPTIONAL_HEADER.DllCharacteristics & 0x4000:
        print("[Build] Disabling Control Flow Guard (CFG) in executable for Unicorn JIT compatibility...")
        pe.OPTIONAL_HEADER.DllCharacteristics &= ~0x4000
        data = pe.write()
        pe.close()
        exe_path.write_bytes(data)
        print("[Build] CFG bit cleared successfully!")
    else:
        pe.close()


def build():
    print("=" * 70)
    print("Building Heroes Lore 3 Standalone PC Distribution")
    print("=" * 70)

    dist_dir = ROOT / "dist"
    build_dir = ROOT / "build"
    output_dir = dist_dir / "HeroesLore3_PC"

    # PyInstaller command
    cmd = [
        sys.executable,
        "-m",
        "PyInstaller",
        "--name",
        "HeroesLore3",
        "--onedir",
        "--console",
        "--clean",
        "-y",
        "--paths",
        str(ROOT / "tools"),
        "--collect-binaries",
        "unicorn",
        "--collect-submodules",
        "unicorn",
        "--collect-binaries",
        "capstone",
        "--collect-submodules",
        "capstone",
        "--collect-all",
        "pygame",
        "--collect-data",
        "OpenGL",
        "--hidden-import",
        "OpenGL.platform.win32",
        "--hidden-import",
        "OpenGL.arrays.ctypesarrays",
        "--hidden-import",
        "OpenGL.arrays.strings",
        "--hidden-import",
        "OpenGL.arrays.numbers",
        "--hidden-import",
        "PIL.Image",
        "--add-data",
        f"{ROOT / 'tools'};tools",
        "--add-data",
        f"{ROOT / 'project.json'};.",
        "--add-data",
        f"{ROOT / 'config'};config",
        str(ROOT / "tools" / "run_pc.py"),
    ]

    print(f"[Build] Running PyInstaller...")
    subprocess.check_call(cmd, cwd=str(ROOT))

    # PyInstaller output directory is dist/HeroesLore3
    pyinstaller_out = dist_dir / "HeroesLore3"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    pyinstaller_out.rename(output_dir)

    # Disable Control Flow Guard (CFG) in HeroesLore3.exe for Unicorn JIT compatibility
    strip_cfg(output_dir / "HeroesLore3.exe")

    print(f"[Build] Copying game assets into standalone package...")

    # Copy pristine original APK containing assets/game.ipa
    apk_name = "Port_Heroes_Lore_III_Stigmata_Of_Gaia_iOS_Version_1_0_1_apkports.apk"
    shutil.copy2(ROOT / apk_name, output_dir / apk_name)

    # Copy project.json
    shutil.copy2(ROOT / "project.json", output_dir / "project.json")

    # Copy config directory
    out_config = output_dir / "config"
    out_config.mkdir(parents=True, exist_ok=True)
    if (ROOT / "config" / "audio.json").exists():
        shutil.copy2(ROOT / "config" / "audio.json", out_config / "audio.json")

    # Copy data directory (for Documents / saves)
    out_docs = output_dir / "data" / "internal" / "Documents"
    out_docs.mkdir(parents=True, exist_ok=True)
    src_docs = ROOT / "data" / "internal" / "Documents"
    if src_docs.exists():
        for item in src_docs.iterdir():
            if item.is_file():
                shutil.copy2(item, out_docs / item.name)

    # Copy tools directory
    shutil.copytree(ROOT / "tools", output_dir / "tools", dirs_exist_ok=True)

    # Write README.txt
    (output_dir / "README.txt").write_text(README_CONTENT, encoding="utf-8")

    print("\n" + "=" * 70)
    print("Standalone PC Distribution Built Successfully!")
    print(f"Location: {output_dir}")
    print(f"Executable: {output_dir / 'HeroesLore3.exe'}")
    print("=" * 70)


if __name__ == "__main__":
    build()

