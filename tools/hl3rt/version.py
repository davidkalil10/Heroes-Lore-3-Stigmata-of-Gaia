"""Canonical port version, attribution, and release constants for Heroes Lore 3 Recomp."""

from __future__ import annotations

import sys

PORT_VERSION = "1.0.4"
ORIGINAL_GAME_VERSION = "1.0.1"
PORT_AUTHOR = "David Kalil Braga"
PORT_YEAR = "2026"
GITHUB_REPOSITORY = "davidkalil10/Heroes-Lore-3-Stigmata-of-Gaia"

APP_DISPLAY_NAME = "Heroes Lore 3: Stigmata of Gaia Recomp"
ANDROID_PACKAGE_ID = "com.davidbraga.heroeslore3"
SWITCH_TITLE_ID = "0100F3A300030000"

RELEASES_API_URL = f"https://api.github.com/repos/{GITHUB_REPOSITORY}/releases/latest"
RELEASES_HTML_URL = f"https://github.com/{GITHUB_REPOSITORY}/releases"
DOWNLOAD_URL_PREFIX = f"https://github.com/{GITHUB_REPOSITORY}/releases/download/"

ASSET_WINDOWS_X64 = "heroes_lore_3_windows_x64.zip"
ASSET_LINUX_X86_64 = "heroes_lore_3_linux_x86_64.AppImage"
ASSET_ANDROID_APK = "heroes_lore_3_android_universal.apk"
ASSET_SWITCH_NRO = "heroes_lore_3.nro"


def get_port_tag(version: str = PORT_VERSION) -> str:
    clean = version.strip()
    if clean.lower().startswith("v"):
        clean = clean[1:]
    return f"v{clean}"


def get_target_asset_name(platform_id: str | None = None) -> str:
    plat = (platform_id or sys.platform).lower()
    if plat.startswith("win"):
        return ASSET_WINDOWS_X64
    if plat.startswith("linux"):
        return ASSET_LINUX_X86_64
    if "android" in plat:
        return ASSET_ANDROID_APK
    if "switch" in plat or "horizon" in plat:
        return ASSET_SWITCH_NRO
    return ASSET_WINDOWS_X64


def about_credit_prefix(lang: str = "en", version: str = PORT_VERSION) -> str:
    """Text prepended to About slot 6632 so port author and version sit cleanly below 1.0.1."""
    if lang == "pt-BR":
        return f"Port por {PORT_AUTHOR} ({PORT_YEAR}) - v{version}||"
    return f"Port by {PORT_AUTHOR} ({PORT_YEAR}) - v{version}||"
