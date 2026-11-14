"""Automated S09/S10 tests: baseline extraction, extended PT-BR fonts, full chunks, and live switch."""

from __future__ import annotations

import hashlib
import json
import struct
import subprocess
import sys
import unittest
from pathlib import Path

import pygame

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from extract_texts import PTBR_DIACRITICS, decode_t_file, parse_hdr, parse_ingame_txt_2, parse_rff_and_pvr
from hl3rt.display import DEFAULT_LANGUAGE
from hl3rt.i18n import get_active_language, set_active_language
from hl3rt.machine import Bundle
from run_pc import PCRunner
from validate_ptbr_chunk import validate_chunk_file


EXPECTED_PTBR_FONTS = ("font_mainmenu", "font_menu_medium", "font_menu_small")


def _read_tu_chunk1_slot(m, slot: int) -> str:
    tu = m.rd32(0x000d8220)
    buf1 = m.rd32(m.rd32(tu + 0x20) + 4)
    offs1 = m.rd32(m.rd32(tu + 0x24) + 4)
    o0, o1 = struct.unpack("<2I", m.rdbytes(offs1 + slot * 4, 8))
    return bytes(m.rdbytes(buf1 + o0, o1 - o0)).decode("utf-16-be", errors="strict")


def _snapshot_data_hashes() -> dict[str, str]:
    data_dir = ROOT / "data"
    if not data_dir.exists():
        return {}
    return {
        str(p.relative_to(data_dir)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(data_dir.rglob("*"))
        if p.is_file()
    }


class S09S10LocalizationTests(unittest.TestCase):
    def test_s09_baseline_extract_check_and_counts(self):
        proc = subprocess.run(
            [sys.executable, str(ROOT / "tools" / "extract_texts.py"), "--check"],
            cwd=str(ROOT),
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn("[S09 CHECK PASS]", proc.stdout)

        cat = json.loads((ROOT / "reference" / "analysis" / "text_catalog_en.json").read_text(encoding="utf-8"))
        self.assertEqual(cat["summary"]["total_slots"], 6733)
        self.assertEqual(cat["summary"]["non_empty_slots"], 6648)
        self.assertEqual(cat["summary"]["anomalies_count"], 2)
        self.assertEqual(cat["text_header"]["languages"][0]["display_name"], "English")
        self.assertEqual(cat["entries"][6056]["section"], "quests_and_objectives")
        self.assertEqual(cat["entries"][6057]["section"], "skills_and_passives")

    def test_s10_extended_fonts_have_all_ptbr_diacritics_and_valid_bounds(self):
        for font_name in EXPECTED_PTBR_FONTS:
            rff = (ROOT / "assets" / "fonts" / f"{font_name}.rff").read_bytes()
            pvr = (ROOT / "assets" / "fonts" / f"{font_name}.pvr").read_bytes()
            metrics = parse_rff_and_pvr(rff, pvr)
            cps = [g["codepoint"] for g in metrics["rff"]["glyphs"]]
            self.assertEqual(cps, sorted(cps), f"{font_name} codepoints must stay sorted for bsearch")
            self.assertEqual(len(set(cps)), len(cps), f"{font_name} must not contain duplicate codepoints")
            self.assertEqual(metrics["rff"]["ptbr_missing"], [], f"{font_name} missing PT-BR chars")
            self.assertLessEqual(metrics["pvr"]["max_used_x"], metrics["pvr"]["width"])
            self.assertLessEqual(metrics["pvr"]["max_used_y"], metrics["pvr"]["height"])

    def test_s10_all_chunks_valid_and_compiled_binaries_complete(self):
        chunk_files = sorted((ROOT / "localization" / "chunks").glob("*.json"))
        self.assertEqual(len(chunk_files), 12)
        merged: dict[int, str] = {}
        for cf in chunk_files:
            ok, errs = validate_chunk_file(cf)
            self.assertTrue(ok, f"{cf.name} failed validation: {errs[:5]}")
            data = json.loads(cf.read_text(encoding="utf-8"))
            for k, v in data.get("translations", data).items():
                merged[int(k)] = v

        self.assertEqual(set(merged.keys()), set(range(6733)))

        t_pt = (ROOT / "assets" / "texts" / "t_pt-br").read_bytes()
        hdr_pt = (ROOT / "assets" / "texts" / "hdr_pt-br").read_bytes()
        ig2_pt = (ROOT / "assets" / "texts" / "InGame_txt_2_pt-br").read_bytes()

        slots = decode_t_file(t_pt, 6733)
        self.assertEqual(len(slots), 6733)
        self.assertEqual(slots[6218], "Iniciar Jogo ")
        self.assertEqual(slots[6220], "Ajuda e Sobre")
        self.assertEqual(slots[6225], "Configurações ")
        self.assertEqual(slots[6732], merged[6732])

        hdr_meta = parse_hdr(hdr_pt)
        expected_blob = len(t_pt) - 6734 * 4
        self.assertEqual(hdr_meta["languages"][0]["blob_bytes"], expected_blob)

        ig2_meta = parse_ingame_txt_2(ig2_pt)
        self.assertEqual([e["text"] for e in ig2_meta["entries"]], ["Mochila cheia.", "Sim"])

    def test_s10_bundle_override_and_live_runtime_switch_preserve_saves(self):
        cfg_path = ROOT / "config" / "display.json"
        orig_cfg = cfg_path.read_text(encoding="utf-8") if cfg_path.exists() else None
        orig_lang = get_active_language()
        try:
            set_active_language("en")
            bundle = Bundle()
            self.assertEqual(bundle.read("newres/fonts/font_mainmenu.rff"), (ROOT / "assets" / "fonts" / "font_mainmenu.rff").read_bytes())
            self.assertNotEqual(bundle.read("newres/texts/t_en"), (ROOT / "assets" / "texts" / "t_pt-br").read_bytes())

            set_active_language("pt-BR")
            self.assertEqual(bundle.read("newres/texts/t_en"), (ROOT / "assets" / "texts" / "t_pt-br").read_bytes())
            self.assertEqual(bundle.read("newres/texts/hdr"), (ROOT / "assets" / "texts" / "hdr_pt-br").read_bytes())
            self.assertEqual(bundle.read("dat/InGame_txt_2"), (ROOT / "assets" / "texts" / "InGame_txt_2_pt-br").read_bytes())

            set_active_language("en")
            runner = PCRunner(scale=1, headless_frames=0)
            runner.set_language("en", notify=False)
            for _ in range(45):
                runner.step_frame()

            saves_before = _snapshot_data_hashes()
            m = runner.rt.m
            self.assertEqual(_read_tu_chunk1_slot(m, 6218), "Start Game ")
            self.assertEqual(_read_tu_chunk1_slot(m, 6225), "Environment Settings ")

            runner.set_language("pt-BR", notify=True)
            for _ in range(10):
                runner.step_frame()
            self.assertEqual(get_active_language(), "pt-BR")
            self.assertEqual(_read_tu_chunk1_slot(m, 6218), "Iniciar Jogo ")
            self.assertEqual(_read_tu_chunk1_slot(m, 6225), "Configurações ")
            self.assertEqual(_snapshot_data_hashes(), saves_before)

            runner.set_language("en", notify=True)
            for _ in range(10):
                runner.step_frame()
            self.assertEqual(get_active_language(), "en")
            self.assertEqual(_read_tu_chunk1_slot(m, 6218), "Start Game ")
            self.assertEqual(_snapshot_data_hashes(), saves_before)

            # Verify F7 hotkey toggles EN -> PT-BR -> EN live in real event loop
            pygame.event.post(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_F7, mod=0, unicode=""))
            for _ in range(5):
                runner.step_frame()
            self.assertEqual(runner.language, "pt-BR")
            self.assertEqual(get_active_language(), "pt-BR")
            self.assertEqual(_read_tu_chunk1_slot(m, 6218), "Iniciar Jogo ")

            pygame.event.post(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_F7, mod=0, unicode=""))
            for _ in range(5):
                runner.step_frame()
            self.assertEqual(runner.language, "en")
            self.assertEqual(get_active_language(), "en")
            self.assertEqual(_read_tu_chunk1_slot(m, 6218), "Start Game ")
            self.assertEqual(_snapshot_data_hashes(), saves_before)
        finally:
            set_active_language(orig_lang or DEFAULT_LANGUAGE)
            if orig_cfg is not None:
                cfg_path.write_text(orig_cfg, encoding="utf-8")
            pygame.quit()

    def test_s10_speaker_tags_preserved_in_packed_t_ptbr(self):
        cat = json.loads((ROOT / "reference" / "analysis" / "text_catalog_en.json").read_text(encoding="utf-8"))
        pt_slots = decode_t_file((ROOT / "assets" / "texts" / "t_pt-br").read_bytes(), 6733)
        self.assertEqual(len(cat["entries"]), 6733)
        self.assertEqual(len(pt_slots), 6733)

        tag_chars = set("#|&{}^")
        for idx, (entry, pt) in enumerate(zip(cat["entries"], pt_slots)):
            en = entry["en"]
            if "^[" in en:
                self.assertIn("^[", pt, f"slot {idx} missing speaker opening '^[' tag")
                self.assertIn("]#", pt, f"slot {idx} missing speaker closing ']#' reset tag")
                if "]# " in en and not pt.endswith("]#"):
                    self.assertIn("]# ", pt, f"slot {idx} missing separating space after ']#'")
            en_tags = [c for c in en if c in tag_chars]
            pt_tags = [c for c in pt if c in tag_chars]
            if idx not in (5623, 6220):
                self.assertEqual(pt_tags, en_tags, f"slot {idx} color tag sequence mismatch: {en!r} vs {pt!r}")

        self.assertEqual(cat["entries"][1798]["en"], "^[Masked Warrior]# Out of my way, Red Witch! ")
        self.assertEqual(pt_slots[1798], "^[Guerreiro Mascarado]# Saia da frente, Bruxa Vermelha! ")

    def test_s10_refresh_guest_banner_live_guards_and_clamps_page(self):
        class _FakeMem:
            def __init__(self):
                self.mem32: dict[int, int] = {}
                self.mem16: dict[int, int] = {}
                self.mem8: dict[int, int] = {}
                self.calls: list[tuple[int, tuple[int, ...], int | None]] = []
                self.on_call = None

            def rd32(self, addr: int) -> int:
                return self.mem32.get(addr, 0)

            def rd16(self, addr: int) -> int:
                return self.mem16.get(addr, 0)

            def rds16(self, addr: int) -> int:
                v = self.rd16(addr)
                return v - 0x10000 if (v & 0x8000) else v

            def rd8(self, addr: int) -> int:
                return self.mem8.get(addr, 0) & 0xFF

            def rds8(self, addr: int) -> int:
                v = self.rd8(addr)
                return v - 0x100 if (v & 0x80) else v

            def wr8(self, addr: int, val: int) -> None:
                self.mem8[addr] = val & 0xFF

            def call(self, addr: int, args=(), sp=None) -> int:
                self.calls.append((addr, tuple(args), sp))
                if self.on_call:
                    return self.on_call(addr, args, sp)
                return 0

        dummy = object.__new__(PCRunner)
        app = 0x100000
        gw = 0x000D8270

        def _base_mem(app_state: int = 2, gw_ready: int = 0x100, banner_tid: int = 1799) -> _FakeMem:
            fm = _FakeMem()
            fm.mem32[0x00039D30] = gw
            fm.mem32[app + 0x78] = app_state
            fm.mem32[gw + 0x40] = gw_ready
            fm.mem32[app + 0x1C] = 0x200000
            fm.mem32[app + 0x20] = 0x200100
            fm.mem32[app + 0x28] = 0x200200
            fm.mem16[gw + 0x611A] = banner_tid & 0xFFFF
            return fm

        # Title screen (app_state != 2/8) must skip even if zero-init banner_tid == 0
        fm = _base_mem(app_state=1, banner_tid=0)
        dummy._refresh_guest_banner_live(fm, app, 0x7FFF0000)
        self.assertEqual(fm.calls, [])

        # Uninitialized GameWorld static struct (gw + 0x40 <= 0x20) must skip
        fm = _base_mem(app_state=2, gw_ready=0, banner_tid=0)
        dummy._refresh_guest_banner_live(fm, app, 0x7FFF0000)
        self.assertEqual(fm.calls, [])

        # Missing wrap buffer ([app + 0x28] == 0) must skip
        fm = _base_mem(app_state=2, banner_tid=1799)
        fm.mem32[app + 0x28] = 0
        dummy._refresh_guest_banner_live(fm, app, 0x7FFF0000)
        self.assertEqual(fm.calls, [])

        # Negative banner_tid (-1) must skip
        fm = _base_mem(app_state=2, banner_tid=-1)
        dummy._refresh_guest_banner_live(fm, app, 0x7FFF0000)
        self.assertEqual(fm.calls, [])

        # Active banner re-wraps via 0x0002787c and clamps shrunk page offset (6 -> 3 when new_lines=4)
        fm = _base_mem(app_state=8, banner_tid=1799)
        fm.wr8(gw + 0x611D, 6)

        def _simulate_rewrap(_addr, _args, _sp):
            fm.wr8(gw + 0x611C, 4)
            fm.wr8(gw + 0x611D, 0)
            return 0

        fm.on_call = _simulate_rewrap
        dummy._refresh_guest_banner_live(fm, app, 0x7FFF0000)
        self.assertEqual(fm.calls, [(0x0002787C, (1799, 0, 0), 0x7FFF0000)])
        self.assertEqual(fm.rd8(gw + 0x611D), 3)

        # Page offset within new line count is preserved directly
        fm = _base_mem(app_state=2, banner_tid=1799)
        fm.wr8(gw + 0x611D, 3)
        fm.on_call = lambda _a, _args, _sp: (fm.wr8(gw + 0x611C, 5), fm.wr8(gw + 0x611D, 0), 0)[-1]
        dummy._refresh_guest_banner_live(fm, app, 0x7FFF0000)
        self.assertEqual(fm.rd8(gw + 0x611D), 3)

    def test_s10_live_language_switch_rewraps_active_dialogue_banner(self):
        cfg_path = ROOT / "config" / "display.json"
        orig_cfg = cfg_path.read_text(encoding="utf-8") if cfg_path.exists() else None
        orig_lang = get_active_language()
        saves_before = _snapshot_data_hashes()
        try:
            set_active_language("en")
            runner = PCRunner(scale=1, headless_frames=0)
            m = runner.rt.m
            host_sp = runner.rt.host_sp
            for _ in range(20):
                runner.step_frame()

            for _ in range(10):
                if runner.get_engine_state()["app_state"] == 2:
                    break
                runner.handle_confirm()
                for _ in range(20):
                    runner.step_frame()
            self.assertEqual(runner.get_engine_state()["app_state"], 2)

            for _ in range(90):
                runner.step_frame()
                if runner.get_engine_state()["active_popup_id"] and not runner.pending_tap_ups:
                    runner.handle_confirm()

            gw = m.rd32(0x00039D30)
            self.assertGreater(m.rd32(gw + 0x40), 0x20)

            def _read_wrap_layout():
                app = m.rd32(0x000D7DD4)
                wrap_ptr = m.rd32(app + 0x28)
                arr0 = m.rd32(wrap_ptr)
                n_lines = m.rds16(arr0)
                offs = [m.rds16(arr0 + (i + 1) * 2) for i in range(n_lines + 1)]
                tag_ptr = m.rd32(wrap_ptr + 4)
                tcnt = m.rds16(tag_ptr)
                tags = [m.rds16(tag_ptr + (k + 1) * 2) for k in range(tcnt + 2)]
                return n_lines, offs, tags

            # Open speaker-tagged dialogue banner tid=1799 in EN
            m.wr8(gw + 0x609C, 22)
            m.wr8(gw + 0x6116, 0)
            m.wr8(gw + 0x6119, 0xFF)
            m.call(0x0002787C, (1799, 0, 0), sp=host_sp)
            self.assertEqual(m.rds16(gw + 0x611A), 1799)
            self.assertEqual(_read_wrap_layout(), (1, [0, 45], [0, 17, 45, 0]))

            # Press F7 to switch live to PT-BR; active banner must re-wrap immediately
            pygame.event.post(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_F7, mod=0, unicode=""))
            for _ in range(5):
                m.wr8(gw + 0x609C, 22)
                runner.step_frame()
            self.assertEqual(runner.language, "pt-BR")
            self.assertEqual(get_active_language(), "pt-BR")
            self.assertEqual(m.rds16(gw + 0x611A), 1799)
            self.assertEqual(_read_wrap_layout(), (1, [0, 56], [0, 22, 56, 0]))

            # Press F7 again to switch live back to EN; wrap/tags must return cleanly
            pygame.event.post(pygame.event.Event(pygame.KEYDOWN, key=pygame.K_F7, mod=0, unicode=""))
            for _ in range(5):
                m.wr8(gw + 0x609C, 22)
                runner.step_frame()
            self.assertEqual(runner.language, "en")
            self.assertEqual(get_active_language(), "en")
            self.assertEqual(_read_wrap_layout(), (1, [0, 45], [0, 17, 45, 0]))
            self.assertEqual(_snapshot_data_hashes(), saves_before)
        finally:
            set_active_language(orig_lang or DEFAULT_LANGUAGE)
            if orig_cfg is not None:
                cfg_path.write_text(orig_cfg, encoding="utf-8")
            elif cfg_path.exists():
                cfg_path.unlink()
            pygame.quit()


if __name__ == "__main__":
    unittest.main()
