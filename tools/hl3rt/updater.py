"""GitHub Releases OTA updater subsystem for Heroes Lore 3: Stigmata of Gaia Recomp."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Callable

from .version import (
    DOWNLOAD_URL_PREFIX,
    GITHUB_REPOSITORY,
    ORIGINAL_GAME_VERSION,
    PORT_VERSION,
    RELEASES_API_URL,
    RELEASES_HTML_URL,
    get_target_asset_name,
)

DEFAULT_STARTUP_UPDATE_CHECK = True
MAX_UPDATE_JSON_BYTES = 1 * 1024 * 1024
MAX_UPDATE_PACKAGE_BYTES = 256 * 1024 * 1024
DEFAULT_HTTP_TIMEOUT = 15.0
STARTUP_CHECK_ATTEMPTS = 3

SEMVER_RE = re.compile(r"^[vV]?(\d+)\.(\d+)\.(\d+)$")
SHA256_HEX_RE = re.compile(r"\b([0-9a-fA-F]{64})\b")


def _resolve_root() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[2]


def default_services_config_path() -> Path:
    return _resolve_root() / "config" / "services.json"


def default_updates_dir() -> Path:
    return _resolve_root() / "data" / "updates"


def load_services_prefs(path: Path | None = None) -> dict:
    cfg = path or default_services_config_path()
    prefs = {"check_updates_at_startup": DEFAULT_STARTUP_UPDATE_CHECK}
    if not cfg.exists():
        return prefs
    try:
        raw = json.loads(cfg.read_text(encoding="utf-8"))
        if isinstance(raw, dict) and "check_updates_at_startup" in raw:
            prefs["check_updates_at_startup"] = bool(raw["check_updates_at_startup"])
    except Exception:
        pass
    return prefs


def save_services_prefs(prefs: dict, path: Path | None = None) -> Path:
    cfg = path or default_services_config_path()
    cfg.parent.mkdir(parents=True, exist_ok=True)
    clean = {
        "check_updates_at_startup": bool(
            prefs.get("check_updates_at_startup", DEFAULT_STARTUP_UPDATE_CHECK)
        )
    }
    tmp = cfg.with_suffix(".tmp")
    tmp.write_text(json.dumps(clean, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")
    os.replace(tmp, cfg)
    return cfg


def parse_semver(version_or_tag: str | None) -> tuple[int, int, int] | None:
    if not version_or_tag:
        return None
    m = SEMVER_RE.match(str(version_or_tag).strip())
    if not m:
        return None
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def is_newer_version(remote_tag: str | None, local_version: str = PORT_VERSION) -> bool:
    remote_v = parse_semver(remote_tag)
    local_v = parse_semver(local_version)
    if remote_v is None or local_v is None:
        return False
    return remote_v > local_v


def is_valid_release_download_url(url: str | None, repo: str = GITHUB_REPOSITORY) -> bool:
    if not url or not isinstance(url, str):
        return False
    expected = f"https://github.com/{repo}/releases/download/"
    return url.startswith(expected) and len(url) > len(expected)


def _extract_sha256_from_asset_or_body(asset: dict, body: str, asset_name: str) -> str:
    digest = str(asset.get("digest") or "").strip()
    if digest.lower().startswith("sha256:"):
        candidate = digest.split(":", 1)[1].strip()
        if len(candidate) == 64 and all(c in "0123456789abcdefABCDEF" for c in candidate):
            return candidate.lower()
    for line in (body or "").splitlines():
        if asset_name in line or "sha256" in line.lower():
            m = SHA256_HEX_RE.search(line)
            if m:
                return m.group(1).lower()
    return ""


@dataclass
class UpdateReleaseInfo:
    tag_name: str
    version: str
    release_name: str
    body: str
    html_url: str
    published_at: str
    asset_name: str
    asset_url: str
    asset_size: int
    asset_sha256: str = ""


class UpdatePhase:
    IDLE = "idle"
    CHECKING = "checking"
    UP_TO_DATE = "up_to_date"
    UPDATE_AVAILABLE = "update_available"
    NO_RELEASE_YET = "no_release_yet"
    ERROR = "error"
    DOWNLOADING = "downloading"
    READY_TO_INSTALL = "ready_to_install"
    INSTALLING = "installing"
    MANUAL_STAGED = "manual_staged"


def parse_latest_release_payload(
    payload: dict,
    *,
    target_asset: str | None = None,
    repo: str = GITHUB_REPOSITORY,
) -> tuple[UpdateReleaseInfo | None, str]:
    """Validate GitHub /releases/latest JSON payload cleanly and securely."""
    if not isinstance(payload, dict):
        return None, "invalid_payload"
    if payload.get("draft") or payload.get("prerelease"):
        return None, UpdatePhase.NO_RELEASE_YET

    tag_name = str(payload.get("tag_name") or "").strip()
    sv = parse_semver(tag_name)
    if sv is None:
        return None, "invalid_version"
    version_str = f"{sv[0]}.{sv[1]}.{sv[2]}"

    wanted_asset = target_asset or get_target_asset_name()
    assets = payload.get("assets") or []
    matched_asset: dict | None = None
    for item in assets:
        if isinstance(item, dict) and str(item.get("name") or "") == wanted_asset:
            matched_asset = item
            break

    asset_url = ""
    asset_size = 0
    asset_sha256 = ""
    if matched_asset is not None:
        raw_url = str(matched_asset.get("browser_download_url") or "").strip()
        raw_size = int(matched_asset.get("size") or 0)
        if not is_valid_release_download_url(raw_url, repo=repo):
            return None, "untrusted_url"
        if raw_size < 0 or raw_size > MAX_UPDATE_PACKAGE_BYTES:
            return None, "invalid_size"
        asset_url = raw_url
        asset_size = raw_size
        asset_sha256 = _extract_sha256_from_asset_or_body(
            matched_asset, str(payload.get("body") or ""), wanted_asset
        )

    html_url = str(payload.get("html_url") or f"https://github.com/{repo}/releases").strip()
    if not html_url.startswith(f"https://github.com/{repo}"):
        html_url = f"https://github.com/{repo}/releases"

    info = UpdateReleaseInfo(
        tag_name=tag_name,
        version=version_str,
        release_name=str(payload.get("name") or tag_name).strip(),
        body=str(payload.get("body") or "").strip(),
        html_url=html_url,
        published_at=str(payload.get("published_at") or "").strip(),
        asset_name=wanted_asset,
        asset_url=asset_url,
        asset_size=asset_size,
        asset_sha256=asset_sha256,
    )
    return info, "ok"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _safe_extract_zip(zip_path: Path, dest_dir: Path) -> Path:
    """Safely extract update ZIP without path traversal and return root containing exe."""
    if dest_dir.exists():
        shutil.rmtree(dest_dir, ignore_errors=True)
    dest_dir.mkdir(parents=True, exist_ok=True)
    base_resolved = dest_dir.resolve()

    with zipfile.ZipFile(zip_path, "r") as zf:
        for member in zf.infolist():
            norm = member.filename.replace("\\", "/")
            if not norm or norm.startswith("/") or ":" in norm:
                raise ValueError(f"Unsafe ZIP entry path: {member.filename}")
            parts = PurePosixPath(norm).parts
            if any(part == ".." for part in parts):
                raise ValueError(f"Path traversal blocked in ZIP entry: {member.filename}")
            target = (dest_dir / Path(*parts)).resolve()
            if target != base_resolved and base_resolved not in target.parents:
                raise ValueError(f"ZIP entry escapes staging dir: {member.filename}")
            if member.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with zf.open(member, "r") as src, target.open("wb") as dst:
                    shutil.copyfileobj(src, dst)

    if (dest_dir / "HeroesLore3.exe").exists():
        return dest_dir
    for child in dest_dir.iterdir():
        if child.is_dir() and (child / "HeroesLore3.exe").exists():
            return child
    return dest_dir


def build_windows_updater_script(
    *,
    target_pid: int,
    staged_root: Path,
    install_root: Path,
    rollback_root: Path,
    exe_name: str = "HeroesLore3.exe",
    log_path: Path | None = None,
) -> str:
    """Generate a safe atomic PowerShell self-updater script preserving config/ and data/."""
    staged_s = str(staged_root).replace("'", "''")
    install_s = str(install_root).replace("'", "''")
    rollback_s = str(rollback_root).replace("'", "''")
    exe_s = str(install_root / exe_name).replace("'", "''")
    log_s = str(log_path or (install_root / "data" / "updates" / "last_update.log")).replace("'", "''")
    return f"""$ErrorActionPreference = 'Stop'
$TargetPid = {int(target_pid)}
$StagedRoot = '{staged_s}'
$InstallRoot = '{install_s}'
$RollbackRoot = '{rollback_s}'
$ExePath = '{exe_s}'
$LogPath = '{log_s}'

function Write-Log([string]$Msg) {{
    $line = ('[' + (Get-Date -Format 's') + '] ' + $Msg)
    Add-Content -Path $LogPath -Value $line -Encoding UTF8 -ErrorAction SilentlyContinue
}}

New-Item -ItemType Directory -Force -Path (Split-Path $LogPath) | Out-Null
Write-Log "Waiting for PID $TargetPid to exit..."
for ($i = 0; $i -lt 240; $i++) {{
    if (-not (Get-Process -Id $TargetPid -ErrorAction SilentlyContinue)) {{ break }}
    Start-Sleep -Milliseconds 250
}}
Start-Sleep -Milliseconds 350

try {{
    New-Item -ItemType Directory -Force -Path $RollbackRoot | Out-Null
    $Preserved = @('config', 'data')
    Get-ChildItem -Force $InstallRoot | ForEach-Object {{
        if ($Preserved -contains $_.Name) {{ return }}
        $dest = Join-Path $RollbackRoot $_.Name
        Move-Item -Force $_.FullName $dest
    }}
    Get-ChildItem -Force $StagedRoot | ForEach-Object {{
        if ($Preserved -contains $_.Name) {{ return }}
        $dest = Join-Path $InstallRoot $_.Name
        Copy-Item -Recurse -Force $_.FullName $dest
    }}
    foreach ($dirName in $Preserved) {{
        $srcDir = Join-Path $StagedRoot $dirName
        $dstDir = Join-Path $InstallRoot $dirName
        if (Test-Path $srcDir) {{
            if (-not (Test-Path $dstDir)) {{
                Copy-Item -Recurse -Force $srcDir $dstDir
            }}
        }}
    }}
    Write-Log "Update applied cleanly; launching $ExePath"
    Remove-Item -Recurse -Force $RollbackRoot -ErrorAction SilentlyContinue
    if (Test-Path $ExePath) {{
        Start-Process -FilePath $ExePath -WorkingDirectory $InstallRoot | Out-Null
    }}
}} catch {{
    Write-Log ("Update failed; rolling back: " + $_.Exception.Message)
    Get-ChildItem -Force $RollbackRoot -ErrorAction SilentlyContinue | ForEach-Object {{
        $dest = Join-Path $InstallRoot $_.Name
        if (Test-Path $dest) {{ Remove-Item -Recurse -Force $dest -ErrorAction SilentlyContinue }}
        Move-Item -Force $_.FullName $dest -ErrorAction SilentlyContinue
    }}
    if (Test-Path $ExePath) {{
        Start-Process -FilePath $ExePath -WorkingDirectory $InstallRoot | Out-Null
    }}
}}
"""


@dataclass
class UpdaterState:
    phase: str = UpdatePhase.IDLE
    local_version: str = PORT_VERSION
    original_version: str = ORIGINAL_GAME_VERSION
    repo: str = GITHUB_REPOSITORY
    startup_check: bool = DEFAULT_STARTUP_UPDATE_CHECK
    release: UpdateReleaseInfo | None = None
    error_detail: str = ""
    downloaded_bytes: int = 0
    total_bytes: int = 0
    downloaded_path: Path | None = None
    staged_root: Path | None = None
    pending_notice: bool = False
    last_checked_at: str = ""


class UpdateManager:
    """Thread-safe background GitHub Releases checker, downloader, and staged installer."""

    def __init__(
        self,
        *,
        local_version: str = PORT_VERSION,
        repo: str = GITHUB_REPOSITORY,
        config_path: Path | None = None,
        updates_dir: Path | None = None,
        install_root: Path | None = None,
        is_standalone: bool | None = None,
        http_open: Callable[..., object] | None = None,
        target_asset: str | None = None,
        request_quit_cb: Callable[[], None] | None = None,
    ):
        self.repo = repo
        self.api_url = f"https://api.github.com/repos/{self.repo}/releases/latest"
        self.releases_url = f"https://github.com/{self.repo}/releases"
        self.config_path = config_path or default_services_config_path()
        self.updates_dir = updates_dir or default_updates_dir()
        self.install_root = install_root or _resolve_root()
        if is_standalone is None:
            self.is_standalone = bool(
                getattr(sys, "frozen", False) and (self.install_root / "HeroesLore3.exe").exists()
            )
        else:
            self.is_standalone = bool(is_standalone)
        self._http_open = http_open or urllib.request.urlopen
        self._target_asset = target_asset or get_target_asset_name()
        self._request_quit_cb = request_quit_cb

        prefs = load_services_prefs(self.config_path)
        self._lock = threading.Lock()
        self._stop_evt = threading.Event()
        self._worker: threading.Thread | None = None
        self.state = UpdaterState(
            local_version=local_version,
            repo=self.repo,
            startup_check=bool(prefs.get("check_updates_at_startup", DEFAULT_STARTUP_UPDATE_CHECK)),
        )

    def snapshot(self) -> UpdaterState:
        with self._lock:
            return UpdaterState(
                phase=self.state.phase,
                local_version=self.state.local_version,
                original_version=self.state.original_version,
                repo=self.state.repo,
                startup_check=self.state.startup_check,
                release=self.state.release,
                error_detail=self.state.error_detail,
                downloaded_bytes=self.state.downloaded_bytes,
                total_bytes=self.state.total_bytes,
                downloaded_path=self.state.downloaded_path,
                staged_root=self.state.staged_root,
                pending_notice=self.state.pending_notice,
                last_checked_at=self.state.last_checked_at,
            )

    def get_startup_check(self) -> bool:
        with self._lock:
            return self.state.startup_check

    def set_startup_check(self, enabled: bool) -> bool:
        val = bool(enabled)
        save_services_prefs({"check_updates_at_startup": val}, self.config_path)
        with self._lock:
            self.state.startup_check = val
        return val

    def toggle_startup_check(self) -> bool:
        return self.set_startup_check(not self.get_startup_check())

    def consume_pending_notice(self) -> UpdateReleaseInfo | None:
        with self._lock:
            if self.state.pending_notice and self.state.release is not None:
                self.state.pending_notice = False
                return self.state.release
            return None

    def maybe_start_startup_check(self) -> bool:
        if not self.get_startup_check():
            return False
        return self.start_check(startup=True)

    def shutdown(self):
        self._stop_evt.set()

    def _is_busy(self) -> bool:
        return self._worker is not None and self._worker.is_alive()

    def start_check(self, *, startup: bool = False) -> bool:
        with self._lock:
            if self._is_busy():
                return False
            self.state.phase = UpdatePhase.CHECKING
            self.state.error_detail = ""
            if not startup:
                self.state.pending_notice = False
            self._worker = threading.Thread(
                target=self._run_check,
                kwargs={"startup": startup},
                name="HL3-UpdateCheck",
                daemon=True,
            )
            self._worker.start()
            return True

    def _fetch_latest_sync(self) -> tuple[str, UpdateReleaseInfo | None, str]:
        req = urllib.request.Request(
            self.api_url,
            headers={
                "Accept": "application/vnd.github+json",
                "User-Agent": f"HeroesLore3Recomp/{self.state.local_version}",
            },
        )
        try:
            with self._http_open(req, timeout=DEFAULT_HTTP_TIMEOUT) as resp:
                status = int(getattr(resp, "status", 200) or 200)
                if status == 404:
                    return UpdatePhase.NO_RELEASE_YET, None, ""
                raw = resp.read(MAX_UPDATE_JSON_BYTES + 1)
                if len(raw) > MAX_UPDATE_JSON_BYTES:
                    return UpdatePhase.ERROR, None, "Response payload exceeds safety limit"
                payload = json.loads(raw.decode("utf-8"))
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                return UpdatePhase.NO_RELEASE_YET, None, ""
            return UpdatePhase.ERROR, None, f"HTTP {exc.code}"
        except Exception as exc:
            return UpdatePhase.ERROR, None, str(exc)

        info, reason = parse_latest_release_payload(
            payload, target_asset=self._target_asset, repo=self.repo
        )
        if reason == UpdatePhase.NO_RELEASE_YET:
            return UpdatePhase.NO_RELEASE_YET, None, ""
        if info is None:
            return UpdatePhase.ERROR, None, reason
        if is_newer_version(info.tag_name, self.state.local_version):
            return UpdatePhase.UPDATE_AVAILABLE, info, ""
        return UpdatePhase.UP_TO_DATE, info, ""

    def _run_check(self, *, startup: bool):
        attempts = STARTUP_CHECK_ATTEMPTS if startup else 1
        phase = UpdatePhase.ERROR
        info: UpdateReleaseInfo | None = None
        detail = ""

        for idx in range(attempts):
            if self._stop_evt.is_set():
                return
            phase, info, detail = self._fetch_latest_sync()
            if phase != UpdatePhase.ERROR:
                break
            if idx + 1 < attempts and self._stop_evt.wait(0.8 * (idx + 1)):
                return

        now_str = time.strftime("%Y-%m-%d %H:%M:%S")
        with self._lock:
            self.state.last_checked_at = now_str
            # Silent offline tolerance on background boot check
            if startup and phase == UpdatePhase.ERROR:
                self.state.phase = UpdatePhase.IDLE
                self.state.error_detail = ""
                return
            self.state.phase = phase
            self.state.release = info
            self.state.error_detail = detail
            if startup and phase == UpdatePhase.UPDATE_AVAILABLE:
                self.state.pending_notice = True

    def start_download(self) -> bool:
        with self._lock:
            if self._is_busy():
                return False
            rel = self.state.release
            if rel is None or not is_valid_release_download_url(rel.asset_url, repo=self.repo):
                self.state.phase = UpdatePhase.ERROR
                self.state.error_detail = "No valid download asset found"
                return False
            self.state.phase = UpdatePhase.DOWNLOADING
            self.state.downloaded_bytes = 0
            self.state.total_bytes = int(rel.asset_size or 0)
            self.state.error_detail = ""
            self._worker = threading.Thread(
                target=self._run_download,
                args=(rel,),
                name="HL3-UpdateDownload",
                daemon=True,
            )
            self._worker.start()
            return True

    def _run_download(self, rel: UpdateReleaseInfo):
        self.updates_dir.mkdir(parents=True, exist_ok=True)
        part_path = self.updates_dir / f"{rel.asset_name}.download"
        final_path = self.updates_dir / rel.asset_name
        req = urllib.request.Request(
            rel.asset_url,
            headers={"User-Agent": f"HeroesLore3Recomp/{self.state.local_version}"},
        )
        hasher = hashlib.sha256()
        written = 0
        try:
            with self._http_open(req, timeout=DEFAULT_HTTP_TIMEOUT * 2) as resp:
                resp_url = getattr(resp, "geturl", lambda: "")()
                if resp_url and not resp_url.startswith("https://"):
                    raise ValueError(f"Non-HTTPS redirect blocked: {resp_url}")
                hdr_len = 0
                if hasattr(resp, "headers") and resp.headers is not None:
                    raw_cl = resp.headers.get("Content-Length")
                    if raw_cl and str(raw_cl).isdigit():
                        hdr_len = int(raw_cl)
                expected_len = int(rel.asset_size or hdr_len or 0)
                if expected_len > MAX_UPDATE_PACKAGE_BYTES:
                    raise ValueError("Download exceeds size limit")
                with self._lock:
                    self.state.total_bytes = expected_len

                with part_path.open("wb") as fh:
                    while True:
                        if self._stop_evt.is_set():
                            raise InterruptedError("Cancelled on shutdown")
                        chunk = resp.read(65536)
                        if not chunk:
                            break
                        written += len(chunk)
                        if written > MAX_UPDATE_PACKAGE_BYTES:
                            raise ValueError("Download stream exceeds safety limit")
                        fh.write(chunk)
                        hasher.update(chunk)
                        with self._lock:
                            self.state.downloaded_bytes = written

            if written <= 0:
                raise ValueError("Downloaded empty file")
            if rel.asset_size > 0 and written != rel.asset_size:
                raise ValueError(f"Size mismatch: expected {rel.asset_size}, got {written}")
            actual_sha = hasher.hexdigest().lower()
            if rel.asset_sha256 and actual_sha != rel.asset_sha256.lower():
                raise ValueError("SHA-256 verification failed")
            if rel.asset_name.lower().endswith(".zip"):
                with part_path.open("rb") as fh:
                    if fh.read(2) != b"PK":
                        raise ValueError("Downloaded file is not a valid ZIP archive")

            os.replace(part_path, final_path)
            with self._lock:
                self.state.downloaded_path = final_path
                self.state.downloaded_bytes = written
                self.state.total_bytes = written
                self.state.phase = UpdatePhase.READY_TO_INSTALL
        except Exception as exc:
            try:
                part_path.unlink(missing_ok=True)
            except Exception:
                pass
            with self._lock:
                self.state.phase = UpdatePhase.ERROR
                self.state.error_detail = str(exc)

    def install_ready_update(self) -> tuple[bool, str]:
        with self._lock:
            dl_path = self.state.downloaded_path
            rel = self.state.release
        if dl_path is None or not dl_path.exists():
            with self._lock:
                self.state.phase = UpdatePhase.ERROR
                self.state.error_detail = "Update package not found"
            return False, "Update package not found"

        try:
            if dl_path.suffix.lower() == ".zip":
                staged_dir = self.updates_dir / "staged"
                staged_root = _safe_extract_zip(dl_path, staged_dir)
                with self._lock:
                    self.state.staged_root = staged_root

                if sys.platform == "win32" and self.is_standalone:
                    stamp = time.strftime("%Y%m%d_%H%M%S")
                    rollback_dir = self.updates_dir / f"rollback_{stamp}"
                    script_path = self.updates_dir / "update_windows.ps1"
                    script_text = build_windows_updater_script(
                        target_pid=os.getpid(),
                        staged_root=staged_root,
                        install_root=self.install_root,
                        rollback_root=rollback_dir,
                        log_path=self.updates_dir / "last_update.log",
                    )
                    script_path.write_text(script_text, encoding="utf-8")
                    creation_flags = getattr(subprocess, "CREATE_NO_WINDOW", 0) | getattr(
                        subprocess, "DETACHED_PROCESS", 0
                    )
                    subprocess.Popen(
                        [
                            "powershell",
                            "-NoProfile",
                            "-ExecutionPolicy",
                            "Bypass",
                            "-WindowStyle",
                            "Hidden",
                            "-File",
                            str(script_path),
                        ],
                        cwd=str(self.updates_dir),
                        creationflags=creation_flags,
                    )
                    with self._lock:
                        self.state.phase = UpdatePhase.INSTALLING
                    if self._request_quit_cb is not None:
                        self._request_quit_cb()
                    return True, str(staged_root)

                with self._lock:
                    self.state.phase = UpdatePhase.MANUAL_STAGED
                return True, str(staged_root)

            with self._lock:
                self.state.phase = UpdatePhase.MANUAL_STAGED
            return True, str(dl_path)
        except Exception as exc:
            with self._lock:
                self.state.phase = UpdatePhase.ERROR
                self.state.error_detail = str(exc)
            return False, str(exc)

