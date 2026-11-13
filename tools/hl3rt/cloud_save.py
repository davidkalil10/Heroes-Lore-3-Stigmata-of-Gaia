"""Google Drive Cloud Save subsystem for Heroes Lore 3: Stigmata of Gaia.

Implements RFC 8628 OAuth 2.0 Device Flow, Drive v3 single-file structured JSON
backup inside a dedicated game folder ("Heroes Lore III - Stigmata of Gaia" /
"heroes_lore_3_save.json"), SHA-256 verification, root auto-migration, optimistic
conflict checks, and atomic staging + local rollback under save-backups/.
"""

from __future__ import annotations

import base64
import datetime as _dt
import hashlib
import json
import os
import shutil
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

CLOUD_APP_TAG = "heroes_lore_3"
CLOUD_FORMAT_VERSION = 1
CLOUD_FOLDER_NAME = "Heroes Lore III - Stigmata of Gaia"
CLOUD_FILE_NAME = "heroes_lore_3_save.json"
ANDROID_PACKAGE_ID = "com.davidbraga.heroeslore3"
SWITCH_TITLE_ID = "0100F3A300030000"

DRIVE_SCOPE = "https://www.googleapis.com/auth/drive.file"
OAUTH_DEVICE_URL = "https://oauth2.googleapis.com/device/code"
OAUTH_TOKEN_URL = "https://oauth2.googleapis.com/token"
DRIVE_FILES_URL = "https://www.googleapis.com/drive/v3/files"
DRIVE_UPLOAD_URL = "https://www.googleapis.com/upload/drive/v3/files"

MAX_BACKUP_FILES = 64
MAX_BACKUP_BYTES = 16 * 1024 * 1024
MAX_JSON_BYTES = 32 * 1024 * 1024
DEFAULT_HTTP_TIMEOUT = 20.0

_RESERVED_WIN_NAMES = frozenset({
    "CON", "PRN", "AUX", "NUL",
    *(f"COM{i}" for i in range(1, 10)),
    *(f"LPT{i}" for i in range(1, 10)),
})


def _xor_decode(data: tuple[int, ...], key: int = 0x5A) -> str:
    return bytes(b ^ key for b in data).decode("ascii")


def get_google_client_id() -> str:
    enc = (
        108, 104, 108, 99, 104, 98, 111, 110, 98, 107, 111, 107, 119, 107, 104, 98,
        59, 42, 50, 54, 59, 52, 46, 110, 104, 111, 54, 61, 109, 47, 57, 108, 55,
        106, 43, 53, 44, 48, 47, 108, 52, 63, 60, 41, 61, 116, 59, 42, 42, 41,
        116, 61, 53, 53, 61, 54, 63, 47, 41, 63, 40, 57, 53, 52, 46, 63, 52,
        46, 116, 57, 53, 55,
    )
    return _xor_decode(enc)


def get_google_client_secret() -> str:
    enc = (
        29, 21, 25, 9, 10, 2, 119, 45, 107, 28, 106, 56, 21, 48, 106, 99, 109,
        107, 44, 11, 98, 119, 119, 61, 23, 24, 48, 43, 106, 8, 25, 109, 104,
        45, 10,
    )
    return _xor_decode(enc)


def _resolve_root() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[2]


def default_data_root() -> Path:
    return _resolve_root() / "data"


def default_auth_path() -> Path:
    return _resolve_root() / "config" / "cloud_auth.json"


def platform_name() -> str:
    if sys.platform.startswith("win"):
        return "Windows"
    if sys.platform == "darwin":
        return "macOS"
    if sys.platform.startswith("linux"):
        return "Linux"
    return sys.platform.capitalize()


def format_iso_local(iso_str: str | None) -> str:
    if not iso_str:
        return ""
    raw = str(iso_str).strip()
    if not raw:
        return ""
    try:
        norm = raw[:-1] + "+00:00" if raw.endswith("Z") else raw
        dt_obj = _dt.datetime.fromisoformat(norm)
        if dt_obj.tzinfo is None:
            dt_obj = dt_obj.replace(tzinfo=_dt.timezone.utc)
        local_dt = dt_obj.astimezone()
        return local_dt.strftime("%Y-%m-%d %H:%M")
    except Exception:
        return raw[:16].replace("T", " ")


def format_epoch_local(ts: float) -> str:
    if ts <= 0:
        return ""
    try:
        local_dt = _dt.datetime.fromtimestamp(ts).astimezone()
        return local_dt.strftime("%Y-%m-%d %H:%M")
    except Exception:
        return ""


def is_safe_save_path(name: str) -> bool:
    if not isinstance(name, str) or not name or len(name) >= 240:
        return False
    if "\\" in name or name.startswith("/") or name.endswith("/"):
        return False
    parts = name.split("/")
    if len(parts) < 2 or parts[0] not in ("internal", "external"):
        return False
    for part in parts[1:]:
        if not part or part.startswith(".") or part.endswith(".") or part.endswith(" "):
            return False
        for ch in part:
            o = ord(ch)
            if o < 32 or o > 126 or ch in '\\:*?"<>|':
                return False
        stem = part.split(".", 1)[0].upper()
        if stem in _RESERVED_WIN_NAMES:
            return False
    return True


def collect_local_saves(data_root: Path) -> dict[str, bytes]:
    root = Path(data_root)
    files: dict[str, bytes] = {}
    total = 0
    for group in ("internal", "external"):
        base_dir = root / group
        if base_dir.is_symlink():
            raise ValueError(f"Save directory is a symlink: {group}")
        if not base_dir.exists() or not base_dir.is_dir():
            continue
        for cur_root, dirnames, filenames in os.walk(base_dir):
            cur_path = Path(cur_root)
            # Filter hidden directories and block symlinks
            kept_dirs = []
            for d in sorted(dirnames):
                if d.startswith("."):
                    continue
                dp = cur_path / d
                if dp.is_symlink():
                    raise ValueError(f"Save symlinks are unsupported: {dp.name}")
                kept_dirs.append(d)
            dirnames[:] = kept_dirs
            for fn in sorted(filenames):
                if fn.startswith("."):
                    continue
                fp = cur_path / fn
                if fp.is_symlink():
                    raise ValueError(f"Save symlinks are unsupported: {fn}")
                if not fp.is_file():
                    continue
                rel = fp.relative_to(root).as_posix()
                if not is_safe_save_path(rel):
                    raise ValueError(f"Unsupported save filename: {rel}")
                data = fp.read_bytes()
                total += len(data)
                if len(files) >= MAX_BACKUP_FILES or total > MAX_BACKUP_BYTES:
                    raise ValueError("Save backup exceeds maximum limits")
                files[rel] = data
    return files


@dataclass(frozen=True)
class LocalSaveInfo:
    exists: bool
    file_count: int
    total_bytes: int
    slot_count: int
    has_autosave: bool
    updated_local: str
    platforms_summary: str


def inspect_local_saves(data_root: Path) -> LocalSaveInfo:
    root = Path(data_root)
    count = 0
    total = 0
    latest_mtime = 0.0
    slots: set[str] = set()
    has_autosave = False

    for group in ("internal", "external"):
        base_dir = root / group
        if not base_dir.exists() or not base_dir.is_dir() or base_dir.is_symlink():
            continue
        for cur_root, dirnames, filenames in os.walk(base_dir):
            dirnames[:] = [d for d in dirnames if not d.startswith(".") and not (Path(cur_root) / d).is_symlink()]
            for fn in filenames:
                if fn.startswith("."):
                    continue
                fp = Path(cur_root) / fn
                if fp.is_symlink() or not fp.is_file():
                    continue
                rel = fp.relative_to(root).as_posix()
                if not is_safe_save_path(rel):
                    continue
                st = fp.stat()
                count += 1
                total += st.st_size
                if st.st_mtime > latest_mtime:
                    latest_mtime = st.st_mtime
                if fn.startswith("Hero3SlotSave_10_") or fn.startswith("Hero3GameSave_10_"):
                    idx = fn.rsplit("_", 1)[-1]
                    slots.add(idx)
                elif fn.startswith("HL3_Autosave"):
                    has_autosave = True

    return LocalSaveInfo(
        exists=(count > 0),
        file_count=count,
        total_bytes=total,
        slot_count=len(slots),
        has_autosave=has_autosave,
        updated_local=format_epoch_local(latest_mtime),
        platforms_summary=platform_name(),
    )


def build_save_summary(info: LocalSaveInfo) -> str:
    kb = info.total_bytes / 1024.0
    if info.slot_count > 0:
        return f"{platform_name()} | {info.slot_count} slot(s) | {info.file_count} files ({kb:.1f} KB)"
    return f"{platform_name()} | {info.file_count} files ({kb:.1f} KB)"


def pack_saves(files: dict[str, bytes], platform: str | None = None, summary: str | None = None) -> str:
    if not files:
        raise ValueError("No local saves to upload")
    if len(files) > MAX_BACKUP_FILES:
        raise ValueError("Too many save files")

    total = 0
    seen_lower: set[str] = set()
    encoded_files: dict[str, dict[str, str]] = {}
    for rel_path in sorted(files):
        if not is_safe_save_path(rel_path):
            raise ValueError(f"Invalid save path: {rel_path}")
        folded = rel_path.lower()
        if folded in seen_lower:
            raise ValueError(f"Colliding save filename: {rel_path}")
        seen_lower.add(folded)
        raw = files[rel_path]
        if not isinstance(raw, (bytes, bytearray)):
            raise ValueError("Save file payload must be bytes")
        raw_bytes = bytes(raw)
        total += len(raw_bytes)
        if total > MAX_BACKUP_BYTES:
            raise ValueError("Save backup too large")
        encoded_files[rel_path] = {
            "data": base64.b64encode(raw_bytes).decode("ascii"),
            "sha256": hashlib.sha256(raw_bytes).hexdigest(),
        }

    plat = platform or platform_name()
    now_iso = _dt.datetime.now(_dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    payload = {
        "app": CLOUD_APP_TAG,
        "version": CLOUD_FORMAT_VERSION,
        "package_id": ANDROID_PACKAGE_ID,
        "switch_title_id": SWITCH_TITLE_ID,
        "platform": plat,
        "timestamp": now_iso,
        "summary": summary or f"{plat} | {len(encoded_files)} files ({total / 1024.0:.1f} KB)",
        "files": encoded_files,
    }
    return json.dumps(payload, separators=(",", ":"))


def unpack_saves(text: str) -> tuple[dict[str, bytes], dict]:
    if not isinstance(text, str) or len(text.encode("utf-8", errors="ignore")) > MAX_JSON_BYTES:
        raise ValueError("Backup too large or invalid encoding")
    try:
        doc = json.loads(text)
    except Exception as exc:
        raise ValueError(f"Invalid backup JSON: {exc}") from exc

    if not isinstance(doc, dict):
        raise ValueError("Invalid backup format")
    if doc.get("app") != CLOUD_APP_TAG or doc.get("version") != CLOUD_FORMAT_VERSION:
        raise ValueError("Backup belongs to a different game or format")
    files_obj = doc.get("files")
    if not isinstance(files_obj, dict) or not files_obj:
        raise ValueError("Empty or invalid backup files table")
    if len(files_obj) > MAX_BACKUP_FILES:
        raise ValueError("Backup contains too many files")

    out: dict[str, bytes] = {}
    seen_lower: set[str] = set()
    total = 0
    for rel_path, entry in files_obj.items():
        if not is_safe_save_path(rel_path):
            raise ValueError(f"Unsafe backup path: {rel_path}")
        folded = rel_path.lower()
        if folded in seen_lower:
            raise ValueError(f"Backup has colliding filenames: {rel_path}")
        seen_lower.add(folded)
        if not isinstance(entry, dict):
            raise ValueError(f"Invalid file entry: {rel_path}")
        b64_text = entry.get("data")
        expected_sha = entry.get("sha256")
        if not isinstance(b64_text, str) or not isinstance(expected_sha, str):
            raise ValueError(f"Missing file data or checksum: {rel_path}")
        try:
            raw = base64.b64decode(b64_text.encode("ascii"), validate=True)
        except Exception as exc:
            raise ValueError(f"Invalid base64 payload: {rel_path}") from exc
        if base64.b64encode(raw).decode("ascii") != b64_text:
            raise ValueError(f"Noncanonical base64 payload: {rel_path}")
        if hashlib.sha256(raw).hexdigest().lower() != expected_sha.lower():
            raise ValueError(f"Backup checksum mismatch: {rel_path}")
        total += len(raw)
        if total > MAX_BACKUP_BYTES:
            raise ValueError("Backup exceeds size limit")
        out[rel_path] = raw

    meta = {
        "platform": str(doc.get("platform", "")),
        "timestamp": str(doc.get("timestamp", "")),
        "summary": str(doc.get("summary", "")),
    }
    return out, meta


def restore_saves(data_root: Path, files: dict[str, bytes]) -> Path:
    # Re-validate package end-to-end before touching disk
    checked_files, _ = unpack_saves(pack_saves(files, platform_name()))
    base = Path(data_root)
    base.mkdir(parents=True, exist_ok=True)
    for part in ("internal", "external", "save-backups"):
        p = base / part
        if p.is_symlink() or (p.exists() and not p.is_dir()):
            raise ValueError(f"Unsafe local save directory: {part}")

    stamp = str(time.time_ns())
    stage = base / f"restore-{stamp}"
    backup = base / "save-backups" / stamp
    for part in ("internal", "external"):
        (stage / part).mkdir(parents=True, exist_ok=True)

    try:
        for rel_path, content in checked_files.items():
            target = stage / Path(rel_path)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)

        backup.mkdir(parents=True, exist_ok=True)
        old_moved: list[str] = []
        new_moved: list[str] = []
        try:
            for part in ("internal", "external"):
                live_dir = base / part
                if live_dir.exists():
                    os.replace(live_dir, backup / part)
                    old_moved.append(part)
                os.replace(stage / part, live_dir)
                new_moved.append(part)
        except Exception:
            for part in reversed(new_moved):
                if (base / part).exists():
                    os.replace(base / part, stage / part)
            for part in reversed(old_moved):
                if (backup / part).exists():
                    os.replace(backup / part, base / part)
            raise
    finally:
        if stage.exists():
            shutil.rmtree(stage, ignore_errors=True)

    return backup


@dataclass(frozen=True)
class HttpResponse:
    status_code: int
    body: str


HttpCallable = Callable[[str, str, dict[str, str], str, float], HttpResponse]


def default_http_execute(
    method: str,
    url: str,
    headers: dict[str, str] | None = None,
    body: str = "",
    timeout: float = DEFAULT_HTTP_TIMEOUT,
) -> HttpResponse:
    hdrs = dict(headers or {})
    data = body.encode("utf-8") if body else None
    req = urllib.request.Request(url, data=data, headers=hdrs, method=method.upper())
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
            return HttpResponse(int(getattr(resp, "status", 200)), raw)
    except urllib.error.HTTPError as err:
        raw = err.read().decode("utf-8", errors="replace") if err.fp else ""
        return HttpResponse(int(err.code), raw)
    except urllib.error.URLError as err:
        raise ConnectionError(f"Network error: {err.reason}") from err


@dataclass(frozen=True)
class CloudStatus:
    connected: bool = False
    busy: bool = False
    authorizing: bool = False
    backup_exists: bool = False
    backup_date: str = ""
    backup_date_local: str = ""
    backup_summary: str = ""
    backup_platform: str = ""
    user_code: str = ""
    verification_url: str = ""
    message_key: str = "cloud.msg.ready_disconnected"
    message_detail: str = ""
    restore_ready: bool = False
    last_op_ok: bool = True


class CloudSaveManager:
    """Thread-safe Google Drive cloud save manager for Heroes Lore 3."""

    def __init__(
        self,
        data_root: Path | None = None,
        auth_path: Path | None = None,
        http_fn: HttpCallable | None = None,
        clock_fn: Callable[[], float] | None = None,
    ):
        self.data_root = Path(data_root) if data_root is not None else default_data_root()
        self.auth_path = Path(auth_path) if auth_path is not None else default_auth_path()
        self._http = http_fn or default_http_execute
        self._clock = clock_fn or time.time

        self._lock = threading.Lock()
        self._cancel_event = threading.Event()
        self._worker: threading.Thread | None = None

        self._access_token: str = ""
        self._refresh_token: str = ""
        self._token_expiry: int = 0
        self._folder_id: str = ""
        self._file_id: str = ""
        self._file_version: str = ""
        self._pending_restore: dict[str, bytes] | None = None

        self._status = CloudStatus()
        self._load_tokens()

    # ------------------------------------------------------------------ Tokens
    def _load_tokens(self):
        try:
            if self.auth_path.exists():
                doc = json.loads(self.auth_path.read_text(encoding="utf-8"))
                rtok = str(doc.get("refresh_token", ""))
                atok = str(doc.get("access_token", ""))
                exp = int(doc.get("expires_at", 0))
                if rtok:
                    self._refresh_token = rtok
                    self._access_token = atok
                    self._token_expiry = exp
                    self._status = CloudStatus(
                        connected=True,
                        message_key="cloud.msg.ready_connected",
                    )
        except Exception:
            pass

    def _save_tokens(self):
        self.auth_path.parent.mkdir(parents=True, exist_ok=True)
        tmp = self.auth_path.with_suffix(".json.tmp")
        payload = {
            "app": CLOUD_APP_TAG,
            "refresh_token": self._refresh_token,
            "access_token": self._access_token,
            "expires_at": int(self._token_expiry),
        }
        tmp.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        try:
            if os.name != "nt":
                os.chmod(tmp, 0o600)
        except Exception:
            pass
        os.replace(tmp, self.auth_path)

    # ------------------------------------------------------------------ State
    def get_status(self) -> CloudStatus:
        with self._lock:
            return self._status

    def _update_status(self, **changes):
        with self._lock:
            cur = self._status
            data = {f.name: getattr(cur, f.name) for f in cur.__dataclass_fields__.values()}
            data.update(changes)
            self._status = CloudStatus(**data)

    def get_local_info(self) -> LocalSaveInfo:
        return inspect_local_saves(self.data_root)

    def _launch(self, target: Callable[[], None]):
        with self._lock:
            if self._status.busy:
                return False
            self._cancel_event.clear()
            cur = self._status
            data = {f.name: getattr(cur, f.name) for f in cur.__dataclass_fields__.values()}
            data["busy"] = True
            self._status = CloudStatus(**data)

        def _runner():
            try:
                target()
            except Exception as exc:
                self._update_status(
                    busy=False,
                    authorizing=False,
                    last_op_ok=False,
                    message_key="",
                    message_detail=str(exc),
                )
            else:
                self._update_status(busy=False)

        t = threading.Thread(target=_runner, name="HL3CloudSaveWorker", daemon=True)
        self._worker = t
        t.start()
        return True

    def wait_idle(self, timeout: float = 5.0):
        w = self._worker
        if w is not None and w.is_alive():
            w.join(timeout=timeout)

    # ------------------------------------------------------------------ HTTP / Drive
    @staticmethod
    def _parse_json_resp(resp: HttpResponse) -> dict:
        try:
            doc = json.loads(resp.body) if resp.body else {}
        except Exception as exc:
            raise RuntimeError(f"Server returned HTTP {resp.status_code}") from exc
        if 200 <= resp.status_code < 300:
            return doc if isinstance(doc, dict) else {}
        err_msg = ""
        if isinstance(doc, dict):
            err_obj = doc.get("error")
            if isinstance(err_obj, dict):
                err_msg = str(err_obj.get("message", ""))
            elif isinstance(err_obj, str):
                err_msg = str(doc.get("error_description") or err_obj)
        raise RuntimeError(err_msg or f"HTTP {resp.status_code}")

    def _auth_headers(self, json_content: bool = False) -> dict[str, str]:
        hdrs = {"Authorization": f"Bearer {self._access_token}"}
        if json_content:
            hdrs["Content-Type"] = "application/json; charset=UTF-8"
        return hdrs

    def _ensure_token(self):
        if not self._refresh_token:
            raise RuntimeError("Connect Google Drive first")
        now = int(self._clock())
        if self._access_token and self._token_expiry > now + 60:
            return
        body = urllib.parse.urlencode({
            "client_id": get_google_client_id(),
            "client_secret": get_google_client_secret(),
            "refresh_token": self._refresh_token,
            "grant_type": "refresh_token",
        })
        resp = self._http(
            "POST",
            OAUTH_TOKEN_URL,
            {"Content-Type": "application/x-www-form-urlencoded"},
            body,
            DEFAULT_HTTP_TIMEOUT,
        )
        j = self._parse_json_resp(resp)
        self._access_token = str(j["access_token"])
        self._token_expiry = int(self._clock()) + int(j.get("expires_in", 3600))
        self._save_tokens()

    def _find_folder(self, create: bool) -> str:
        if self._folder_id:
            return self._folder_id
        escaped = CLOUD_FOLDER_NAME.replace("'", "\\'")
        q = f"name='{escaped}' and mimeType='application/vnd.google-apps.folder' and trashed=false"
        url = f"{DRIVE_FILES_URL}?q={urllib.parse.quote(q)}&fields=files(id)&pageSize=1"
        j = self._parse_json_resp(self._http("GET", url, self._auth_headers(), "", DEFAULT_HTTP_TIMEOUT))
        files = j.get("files") or []
        if files:
            self._folder_id = str(files[0]["id"])
            return self._folder_id
        if create:
            body = json.dumps({
                "name": CLOUD_FOLDER_NAME,
                "mimeType": "application/vnd.google-apps.folder",
            })
            j2 = self._parse_json_resp(
                self._http("POST", DRIVE_FILES_URL, self._auth_headers(True), body, DEFAULT_HTTP_TIMEOUT)
            )
            self._folder_id = str(j2["id"])
        return self._folder_id

    def _query_backup_sync(self) -> dict | None:
        self._ensure_token()
        folder = self._find_folder(False)
        self._file_id = ""
        self._file_version = ""
        frec = None
        escaped_file = CLOUD_FILE_NAME.replace("'", "\\'")

        if folder:
            q = f"name='{escaped_file}' and '{folder}' in parents and trashed=false"
            url = (
                f"{DRIVE_FILES_URL}?q={urllib.parse.quote(q)}"
                "&fields=files(id,modifiedTime,description,version,parents)&orderBy=modifiedTime%20desc&pageSize=1"
            )
            j = self._parse_json_resp(self._http("GET", url, self._auth_headers(), "", DEFAULT_HTTP_TIMEOUT))
            files = j.get("files") or []
            if files:
                frec = files[0]

        # Auto-migrate an orphaned root backup file into the dedicated folder if needed
        if frec is None:
            q_any = f"name='{escaped_file}' and trashed=false"
            url_any = (
                f"{DRIVE_FILES_URL}?q={urllib.parse.quote(q_any)}"
                "&fields=files(id,modifiedTime,description,version,parents)&orderBy=modifiedTime%20desc&pageSize=1"
            )
            j_any = self._parse_json_resp(self._http("GET", url_any, self._auth_headers(), "", DEFAULT_HTTP_TIMEOUT))
            files_any = j_any.get("files") or []
            if files_any:
                frec = files_any[0]
                if not folder:
                    folder = self._find_folder(True)
                old_parents = [str(p) for p in (frec.get("parents") or [])]
                if folder and folder not in old_parents:
                    rm_param = f"&removeParents={urllib.parse.quote(','.join(old_parents))}" if old_parents else ""
                    mig_url = f"{DRIVE_FILES_URL}/{urllib.parse.quote(str(frec['id']))}?addParents={urllib.parse.quote(folder)}{rm_param}"
                    try:
                        self._parse_json_resp(self._http("PATCH", mig_url, self._auth_headers(True), "{}", DEFAULT_HTTP_TIMEOUT))
                    except Exception:
                        pass

        if frec is not None:
            self._file_id = str(frec["id"])
            self._file_version = str(frec.get("version", ""))
            mod = str(frec.get("modifiedTime", ""))
            desc = str(frec.get("description", ""))
            plat = desc.split("|", 1)[0].strip() if "|" in desc else ""
            self._update_status(
                backup_exists=True,
                backup_date=mod,
                backup_date_local=format_iso_local(mod),
                backup_summary=desc,
                backup_platform=plat,
            )
            return frec

        self._update_status(
            backup_exists=False,
            backup_date="",
            backup_date_local="",
            backup_summary="",
            backup_platform="",
        )
        return None

    # ------------------------------------------------------------------ Public Ops
    def start_login(self):
        def _op():
            self._update_status(
                authorizing=True,
                user_code="",
                verification_url="",
                last_op_ok=True,
                message_key="cloud.msg.requesting_code",
                message_detail="",
            )
            form_hdr = {"Content-Type": "application/x-www-form-urlencoded"}
            dev_body = urllib.parse.urlencode({
                "client_id": get_google_client_id(),
                "scope": DRIVE_SCOPE,
            })
            j = self._parse_json_resp(
                self._http("POST", OAUTH_DEVICE_URL, form_hdr, dev_body, DEFAULT_HTTP_TIMEOUT)
            )
            device_code = str(j["device_code"])
            user_code = str(j["user_code"])
            verify_url = str(j.get("verification_url", "https://www.google.com/device"))
            interval = max(1, int(j.get("interval", 5)))
            expires_in = max(30, int(j.get("expires_in", 1800)))
            deadline = self._clock() + float(expires_in)

            self._update_status(
                authorizing=True,
                user_code=user_code,
                verification_url=verify_url,
                message_key="cloud.msg.open_code",
                message_detail="",
            )

            while not self._cancel_event.is_set() and self._clock() < deadline:
                if self._cancel_event.wait(timeout=float(interval)):
                    break
                tok_body = urllib.parse.urlencode({
                    "client_id": get_google_client_id(),
                    "client_secret": get_google_client_secret(),
                    "device_code": device_code,
                    "grant_type": "urn:ietf:params:oauth:grant-type:device_code",
                })
                resp = self._http("POST", OAUTH_TOKEN_URL, form_hdr, tok_body, DEFAULT_HTTP_TIMEOUT)
                try:
                    res = json.loads(resp.body) if resp.body else {}
                except Exception:
                    res = {}

                if resp.status_code == 200 and "access_token" in res and not self._cancel_event.is_set():
                    self._access_token = str(res["access_token"])
                    self._refresh_token = str(res.get("refresh_token", self._refresh_token))
                    self._token_expiry = int(self._clock()) + int(res.get("expires_in", 3600))
                    self._folder_id = ""
                    self._save_tokens()
                    self._update_status(
                        connected=True,
                        authorizing=False,
                        user_code="",
                        verification_url="",
                        last_op_ok=True,
                        message_key="cloud.msg.connected",
                        message_detail="",
                    )
                    self._query_backup_sync()
                    return

                err = str(res.get("error", ""))
                if err == "authorization_pending":
                    continue
                if err == "slow_down":
                    interval += 5
                    continue
                raise RuntimeError(res.get("error_description") or err or "Google authorization denied or expired")

            self._update_status(
                authorizing=False,
                user_code="",
                verification_url="",
                last_op_ok=False,
                message_key="cloud.msg.auth_cancelled",
                message_detail="",
            )

        return self._launch(_op)

    def cancel_login(self):
        self._cancel_event.set()

    def refresh_backup(self):
        def _op():
            self._update_status(last_op_ok=True, message_key="cloud.msg.checking", message_detail="")
            rec = self._query_backup_sync()
            self._update_status(
                last_op_ok=True,
                message_key="cloud.msg.found" if rec else "cloud.msg.not_found",
                message_detail="",
            )

        return self._launch(_op)

    def upload_backup(self, flush_before_read: Callable[[], None] | None = None):
        if flush_before_read is not None:
            flush_before_read()
        files = collect_local_saves(self.data_root)
        if not files:
            self._update_status(last_op_ok=False, message_key="cloud.msg.no_local_save", message_detail="")
            return False

        local_info = self.get_local_info()
        summary = build_save_summary(local_info)
        package = pack_saves(files, platform_name(), summary=summary)
        expected_id = self._file_id
        expected_ver = self._file_version

        def _op():
            self._update_status(last_op_ok=True, message_key="cloud.msg.uploading", message_detail="")
            self._query_backup_sync()
            if expected_id and (self._file_id != expected_id or self._file_version != expected_ver):
                self._update_status(
                    last_op_ok=False,
                    message_key="cloud.msg.conflict_changed",
                    message_detail="",
                )
                return
            folder = self._find_folder(True)
            if self._file_id:
                up_url = f"{DRIVE_UPLOAD_URL}/{urllib.parse.quote(self._file_id)}?uploadType=media"
                self._parse_json_resp(
                    self._http("PATCH", up_url, self._auth_headers(True), package, DEFAULT_HTTP_TIMEOUT)
                )
                meta_url = f"{DRIVE_FILES_URL}/{urllib.parse.quote(self._file_id)}"
                self._parse_json_resp(
                    self._http("PATCH", meta_url, self._auth_headers(True), json.dumps({"description": summary}), DEFAULT_HTTP_TIMEOUT)
                )
            else:
                boundary = "HL3_Backup_2026_boundary"
                meta = json.dumps({"name": CLOUD_FILE_NAME, "parents": [folder], "description": summary})
                body = (
                    f"--{boundary}\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n{meta}\r\n"
                    f"--{boundary}\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n{package}\r\n"
                    f"--{boundary}--\r\n"
                )
                hdrs = self._auth_headers(False)
                hdrs["Content-Type"] = f"multipart/related; boundary={boundary}"
                self._parse_json_resp(
                    self._http("POST", f"{DRIVE_UPLOAD_URL}?uploadType=multipart", hdrs, body, DEFAULT_HTTP_TIMEOUT)
                )
            self._query_backup_sync()
            self._update_status(last_op_ok=True, message_key="cloud.msg.uploaded", message_detail="")

        return self._launch(_op)

    def restore_backup(self):
        expected_id = self._file_id
        expected_ver = self._file_version

        def _op():
            self._update_status(last_op_ok=True, message_key="cloud.msg.downloading", message_detail="")
            self._query_backup_sync()
            if not self._file_id:
                raise RuntimeError("No cloud backup found")
            if expected_id and (self._file_id != expected_id or self._file_version != expected_ver):
                self._update_status(
                    last_op_ok=False,
                    message_key="cloud.msg.conflict_changed",
                    message_detail="",
                )
                return
            dl_url = f"{DRIVE_FILES_URL}/{urllib.parse.quote(self._file_id)}?alt=media"
            resp = self._http("GET", dl_url, self._auth_headers(), "", DEFAULT_HTTP_TIMEOUT)
            if resp.status_code != 200:
                raise RuntimeError(f"Could not download backup (HTTP {resp.status_code})")
            files, _ = unpack_saves(resp.body)
            with self._lock:
                self._pending_restore = files
            self._update_status(
                restore_ready=True,
                last_op_ok=True,
                message_key="cloud.msg.restored_reloading",
                message_detail="",
            )

        return self._launch(_op)

    def consume_pending_restore(self) -> Path | None:
        with self._lock:
            if self._status.busy or not self._status.restore_ready or self._pending_restore is None:
                return None
            files = self._pending_restore
            self._pending_restore = None
        try:
            backup_path = restore_saves(self.data_root, files)
            self._update_status(restore_ready=False, last_op_ok=True)
            return backup_path
        except Exception as exc:
            self._update_status(
                restore_ready=False,
                last_op_ok=False,
                message_key="",
                message_detail=str(exc),
            )
            return None

    def disconnect(self):
        if self.get_status().busy:
            self.cancel_login()
            self.wait_idle(timeout=2.0)
        try:
            if self.auth_path.exists():
                self.auth_path.unlink()
        except Exception:
            pass
        self._access_token = ""
        self._refresh_token = ""
        self._token_expiry = 0
        self._folder_id = ""
        self._file_id = ""
        self._file_version = ""
        self._pending_restore = None
        self._update_status(
            connected=False,
            authorizing=False,
            backup_exists=False,
            backup_date="",
            backup_date_local="",
            backup_summary="",
            backup_platform="",
            user_code="",
            verification_url="",
            restore_ready=False,
            last_op_ok=True,
            message_key="cloud.msg.disconnected",
            message_detail="",
        )

