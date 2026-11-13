"""Display, aspect ratio, FPS target, and true-widescreen viewport geometry helpers for Heroes Lore 3."""

from __future__ import annotations

import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path

ASPECT_PRESET_RATIOS: dict[str, float] = {
    "original": 480.0 / 320.0,  # 1.50 (classic 3:2)
    "3:2": 480.0 / 320.0,
    "16:9": 16.0 / 9.0,
    "16:10": 16.0 / 10.0,
    "20:9": 20.0 / 9.0,
    "4:3": 4.0 / 3.0,
}

UI_ASPECT_CYCLE = ("auto", "16:9", "original")
VALID_FPS_TARGETS = (30, 60)
VALID_LANGUAGES = ("en", "pt-BR")
DEFAULT_ASPECT_MODE = "auto"
DEFAULT_FPS_TARGET = 30
DEFAULT_LANGUAGE = "en"

BASE_W = 480
BASE_H = 320
MAX_PATCH_DIM = 1020  # 255 * 4: fits natively in a single ARM rotated-8-bit immediate


def _resolve_root() -> Path:
    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        if (exe_dir / "project.json").exists():
            return exe_dir
        return exe_dir
    return Path(__file__).resolve().parents[2]


def default_display_config_path() -> Path:
    return _resolve_root() / "config" / "display.json"


def normalize_aspect_mode(mode: str | None) -> str:
    if not mode:
        return DEFAULT_ASPECT_MODE
    key = str(mode).strip().lower()
    if key in ("3:2", "classic", "orig", "original"):
        return "original"
    if key in ("auto", "fit", "fill"):
        return "auto"
    if key in ASPECT_PRESET_RATIOS:
        return key
    return DEFAULT_ASPECT_MODE


def normalize_fps(fps: int | str | None) -> int:
    if fps is None:
        return DEFAULT_FPS_TARGET
    try:
        val = int(fps)
    except (TypeError, ValueError):
        return DEFAULT_FPS_TARGET
    return 60 if val >= 45 else 30


def normalize_language(lang: str | None) -> str:
    if not lang:
        return DEFAULT_LANGUAGE
    key = str(lang).strip().lower().replace("_", "-")
    if key.startswith("pt") or "portugu" in key:
        return "pt-BR"
    if key.startswith("en") or "english" in key:
        return "en"
    return DEFAULT_LANGUAGE


def ceil_mult4(val: float, minimum: int, maximum: int = MAX_PATCH_DIM) -> int:
    c = int(math.ceil(val - 1e-6))
    c = max(minimum, min(maximum, c))
    m = ((c + 3) // 4) * 4
    return min(maximum, m)


@dataclass(frozen=True)
class ViewportLayout:
    aspect_mode: str
    world_active: bool
    win_w: int
    win_h: int
    ortho_w: float
    ortho_h: float
    patch_w: int
    patch_h: int
    viewport_rect: tuple[int, int, int, int]
    true_widescreen: bool
    ui_dx: int
    ui_dy: int
    hud_dx_center: int
    hud_dx_right: int
    hud_dy_bottom: int


def compute_viewport_layout(
    aspect_mode: str,
    win_w: int,
    win_h: int,
    world_active: bool = True,
) -> ViewportLayout:
    """Compute zero-stretch OpenGL viewport + guest logical expansion dimensions.

    When `world_active` is False (static 480x320 Title / Character / Class screens),
    layout always aspect-fits a centered 480x320 canvas inside `(win_w, win_h)`.
    When `world_active` is True:
      * `"original"` (`"3:2"`): classic 480x320 aspect-fitted (letterboxed/pillarboxed).
      * `"16:9"` (and `"16:10"`, `"20:9"`): expands horizontal world coordinates (~568.89x320)
        fitted to the target aspect without stretching sprites, and draws extra scenery.
      * `"4:3"`: expands vertical world coordinates (480x360) without stretching.
      * `"auto"`: fills the entire `(win_w, win_h)` canvas seamlessly with True Widescreen
        (`>= 480x320`) matching current window/monitor proportions cleanly (capped at 1020px).
    """
    w = max(1, int(win_w))
    h = max(1, int(win_h))
    mode = normalize_aspect_mode(aspect_mode)
    win_aspect = float(w) / float(h)

    if not world_active or mode == "original":
        target_aspect = ASPECT_PRESET_RATIOS["original"]
        ortho_w, ortho_h = float(BASE_W), float(BASE_H)
        patch_w, patch_h = BASE_W, BASE_H
        vx, vy, vw, vh = _fit_aspect_rect(w, h, target_aspect)
    elif mode == "auto":
        max_aspect = float(MAX_PATCH_DIM) / float(BASE_H)
        min_aspect = float(BASE_W) / float(MAX_PATCH_DIM)
        clamped_aspect = max(min_aspect, min(max_aspect, win_aspect))
        if clamped_aspect >= 1.5:
            ortho_w = max(float(BASE_W), float(BASE_H) * clamped_aspect)
            ortho_h = float(BASE_H)
        else:
            ortho_w = float(BASE_W)
            ortho_h = max(float(BASE_H), float(BASE_W) / clamped_aspect)
        ortho_w = min(float(MAX_PATCH_DIM), ortho_w)
        ortho_h = min(float(MAX_PATCH_DIM), ortho_h)
        patch_w = ceil_mult4(ortho_w, BASE_W)
        patch_h = ceil_mult4(ortho_h, BASE_H)
        if abs(win_aspect - clamped_aspect) < 1e-4:
            vx, vy, vw, vh = 0, 0, w, h
        else:
            vx, vy, vw, vh = _fit_aspect_rect(w, h, clamped_aspect)
    else:
        target_aspect = ASPECT_PRESET_RATIOS[mode]
        if target_aspect >= 1.5:
            ortho_w = max(float(BASE_W), float(BASE_H) * target_aspect)
            ortho_h = float(BASE_H)
        else:
            ortho_w = float(BASE_W)
            ortho_h = max(float(BASE_H), float(BASE_W) / target_aspect)
        ortho_w = min(float(MAX_PATCH_DIM), ortho_w)
        ortho_h = min(float(MAX_PATCH_DIM), ortho_h)
        patch_w = ceil_mult4(ortho_w, BASE_W)
        patch_h = ceil_mult4(ortho_h, BASE_H)
        vx, vy, vw, vh = _fit_aspect_rect(w, h, target_aspect)

    widescreen = (patch_w > BASE_W) or (patch_h > BASE_H)
    extra_w = max(0.0, ortho_w - float(BASE_W))
    extra_h = max(0.0, ortho_h - float(BASE_H))
    dx_half = int(round(extra_w * 0.5)) if world_active else 0
    dy_half = int(round(extra_h * 0.5)) if world_active else 0
    return ViewportLayout(
        aspect_mode=mode,
        world_active=world_active,
        win_w=w,
        win_h=h,
        ortho_w=ortho_w,
        ortho_h=ortho_h,
        patch_w=patch_w,
        patch_h=patch_h,
        viewport_rect=(vx, vy, vw, vh),
        true_widescreen=widescreen,
        ui_dx=dx_half,
        ui_dy=dy_half,
        hud_dx_center=int(round(extra_w * 0.5)),
        hud_dx_right=int(round(extra_w)),
        hud_dy_bottom=int(round(extra_h)),
    )


def _fit_aspect_rect(win_w: int, win_h: int, target_aspect: float) -> tuple[int, int, int, int]:
    win_aspect = float(win_w) / float(win_h)
    if abs(win_aspect - target_aspect) < 0.002:
        return 0, 0, win_w, win_h
    if win_aspect > target_aspect:
        vh = win_h
        vw = max(1, int(round(vh * target_aspect)))
        vx = (win_w - vw) // 2
        vy = 0
    else:
        vw = win_w
        vh = max(1, int(round(vw / target_aspect)))
        vx = 0
        vy = (win_h - vh) // 2
    return vx, vy, vw, vh


def load_display_config(path: Path | None = None) -> dict:
    cfg_path = path or default_display_config_path()
    out = {
        "aspect_mode": DEFAULT_ASPECT_MODE,
        "target_fps": DEFAULT_FPS_TARGET,
        "language": DEFAULT_LANGUAGE,
    }
    try:
        if cfg_path.exists():
            data = json.loads(cfg_path.read_text(encoding="utf-8"))
            if isinstance(data, dict):
                if "aspect_mode" in data:
                    out["aspect_mode"] = normalize_aspect_mode(data["aspect_mode"])
                if "target_fps" in data:
                    out["target_fps"] = normalize_fps(data["target_fps"])
                if "language" in data:
                    out["language"] = normalize_language(data["language"])
    except Exception:
        pass
    return out


def save_display_config(cfg: dict, path: Path | None = None) -> Path:
    cfg_path = path or default_display_config_path()
    payload = {
        "aspect_mode": normalize_aspect_mode(cfg.get("aspect_mode")),
        "target_fps": normalize_fps(cfg.get("target_fps")),
        "language": normalize_language(cfg.get("language")),
    }
    try:
        cfg_path.parent.mkdir(parents=True, exist_ok=True)
        cfg_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    except Exception:
        pass
    return cfg_path

