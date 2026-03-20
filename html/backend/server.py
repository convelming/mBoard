#!/usr/bin/env python3
"""mTabula Python backend

- Product ID whitelist validation via data/products.csv
- CSV-based persistence under data/products/<product_id>/
- Static file serving for current project
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import secrets
from datetime import datetime, timezone
from http.cookies import SimpleCookie
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Dict, List, Optional
from urllib.parse import parse_qs, quote, urlparse

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DATA_ROOT = PROJECT_ROOT / "data"
WHITELIST_CSV = DATA_ROOT / "products.csv"
ADMIN_USERS_CSV = DATA_ROOT / "admin_users.csv"
PRODUCTS_ROOT = DATA_ROOT / "products"

PRODUCT_ID_PATTERN = re.compile(r"^[A-Za-z0-9_-]{3,64}$")
FULL_PRODUCT_ID_PATTERN = re.compile(r"^MB[A-Z0-9]{15}$")

GAME_FIELDS = [
    "id",
    "timestamp",
    "board_id",
    "type",
    "source",
    "move",
    "fen",
    "state",
    "detail_json",
]

CONFIG_FIELDS = [
    "id",
    "timestamp",
    "board_id",
    "scale_x",
    "scale_y",
    "offset_x",
    "offset_y",
    "magnet_level",
    "raw_json",
]

COMMAND_FIELDS = ["id", "timestamp", "board_id", "command", "payload_json"]

SENSOR_FIELDS = [
    "id",
    "timestamp",
    "board_id",
    "type",
    "from",
    "to",
    "move",
    "fen",
    "state",
    "raw_json",
]

PRODUCT_DEFAULT_FIELDS = [
    "product_id",
    "model",
    "active",
    "tier_code",
    "tier_name",
    "generation",
    "origin_code",
    "game_code",
    "sensor_code",
    "chip_code",
    "sw_code",
    "efuse_device_id",
    "esp_device_id",
    "esp_mac",
    "chip_model",
    "fw_version",
    "yy",
    "mm",
    "reserved",
    "serial",
    "created_at",
]

TIER_NAME_MAP = {
    "B": "basic",
    "P": "pro",
    "L": "lite",
}

ALLOWED_TIER_CODES = set(TIER_NAME_MAP.keys())
ALLOWED_GAME_CODES = {"C", "V"}
ALLOWED_SENSOR_CODES = {"N", "H"}
ALLOWED_CHIP_CODES = {"E", "A", "S"}

ADMIN_USER_FIELDS = ["username", "salt", "password_hash", "active", "created_at"]
USERNAME_PATTERN = re.compile(r"^[A-Za-z0-9_.-]{3,32}$")
PASSWORD_ITERATIONS = 200_000
SESSION_COOKIE_NAME = "mt_pid_session"
SESSION_TTL_SECONDS = 8 * 60 * 60
SESSIONS: Dict[str, Dict[str, Any]] = {}


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def ensure_data_layout() -> None:
    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    PRODUCTS_ROOT.mkdir(parents=True, exist_ok=True)

    if not WHITELIST_CSV.exists():
        with WHITELIST_CSV.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=["product_id", "model", "active"])
            writer.writeheader()
            writer.writerow(
                {
                    "product_id": "MBP1SCNEA26030001",
                    "model": "mBoard-pro-demo",
                    "active": "1",
                }
            )

    if not ADMIN_USERS_CSV.exists():
        with ADMIN_USERS_CSV.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=ADMIN_USER_FIELDS)
            writer.writeheader()
            # Default admin account: username=admin password=admin
            writer.writerow(
                {
                    "username": "admin",
                    "salt": "58dd1f445f934e8a69576143b035e568",
                    "password_hash": "8ac8fc8a1d4aa1ba44fad0b7bba0540eb404d4c5952622b794efaabaf17db77e",
                    "active": "1",
                    "created_at": now_iso(),
                }
            )


def is_active_flag(value: Any) -> bool:
    text = str(value or "").strip().lower()
    return text not in {"0", "false", "no", "off"}


def normalize_username(value: Any) -> str:
    username = str(value or "").strip()
    if not USERNAME_PATTERN.fullmatch(username):
        raise APIError(HTTPStatus.BAD_REQUEST, "invalid username format")
    return username


def auth_pepper() -> str:
    # Optional server-side secret not stored in CSV.
    return os.getenv("MTABULA_AUTH_PEPPER", "")


def password_hash_hex(password: str, salt_hex: str) -> str:
    try:
        salt = bytes.fromhex(salt_hex)
    except ValueError as exc:
        raise APIError(HTTPStatus.INTERNAL_SERVER_ERROR, "invalid auth salt format") from exc

    payload = (password + auth_pepper()).encode("utf-8")
    digest = hashlib.pbkdf2_hmac("sha256", payload, salt, PASSWORD_ITERATIONS)
    return digest.hex()


def read_admin_users() -> List[Dict[str, str]]:
    if not ADMIN_USERS_CSV.exists():
        return []
    with ADMIN_USERS_CSV.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        rows = []
        for row in reader:
            username = (row.get("username") or "").strip()
            if not username:
                continue
            rows.append(
                {
                    "username": username,
                    "salt": (row.get("salt") or "").strip(),
                    "password_hash": (row.get("password_hash") or "").strip(),
                    "active": (row.get("active") or "1").strip() or "1",
                    "created_at": (row.get("created_at") or "").strip(),
                }
            )
        return rows


def find_admin_user(username: str) -> Optional[Dict[str, str]]:
    for row in read_admin_users():
        if row.get("username") == username:
            return row
    return None


def verify_admin_password(username: str, password: str) -> bool:
    user = find_admin_user(username)
    if not user:
        return False
    if not is_active_flag(user.get("active", "1")):
        return False
    salt_hex = user.get("salt") or ""
    expected = user.get("password_hash") or ""
    if not salt_hex or not expected:
        return False
    actual = password_hash_hex(password, salt_hex)
    return secrets.compare_digest(actual, expected)


def now_ts() -> int:
    return int(datetime.now(timezone.utc).timestamp())


def cleanup_sessions() -> None:
    current = now_ts()
    expired = [token for token, data in SESSIONS.items() if int(data.get("expires_at", 0)) <= current]
    for token in expired:
        SESSIONS.pop(token, None)


def create_session(username: str) -> str:
    cleanup_sessions()
    token = secrets.token_urlsafe(32)
    SESSIONS[token] = {"username": username, "expires_at": now_ts() + SESSION_TTL_SECONDS}
    return token


def revoke_session(token: str) -> None:
    if token:
        SESSIONS.pop(token, None)


def session_username(token: str) -> Optional[str]:
    if not token:
        return None
    cleanup_sessions()
    data = SESSIONS.get(token)
    if not data:
        return None
    if int(data.get("expires_at", 0)) <= now_ts():
        SESSIONS.pop(token, None)
        return None
    return str(data.get("username") or "")


def sanitize_product_id(value: str) -> Optional[str]:
    product_id = (value or "").strip().upper()
    if not product_id:
        return None
    if not PRODUCT_ID_PATTERN.fullmatch(product_id):
        return None
    return product_id


def normalize_code(value: Any, *, name: str, allowed: Optional[set[str]] = None) -> str:
    code = str(value or "").strip().upper()
    if not re.fullmatch(r"[A-Z0-9]", code):
        raise APIError(HTTPStatus.BAD_REQUEST, f"{name} must be 1 char [A-Z0-9]")
    if allowed is not None and code not in allowed:
        allowed_text = ",".join(sorted(allowed))
        raise APIError(HTTPStatus.BAD_REQUEST, f"{name} must be one of [{allowed_text}]")
    return code


def normalize_two_digits(value: Any, *, name: str, min_val: int = 0, max_val: int = 99) -> str:
    raw = str(value or "").strip()
    if raw.isdigit() and len(raw) == 1:
        raw = "0" + raw
    if not re.fullmatch(r"\d{2}", raw):
        raise APIError(HTTPStatus.BAD_REQUEST, f"{name} must be 2 digits")
    number = int(raw)
    if number < min_val or number > max_val:
        raise APIError(HTTPStatus.BAD_REQUEST, f"{name} out of range")
    return raw


def normalize_optional_text(value: Any, *, max_len: int = 64) -> str:
    text = str(value or "").strip()
    if len(text) > max_len:
        raise APIError(HTTPStatus.BAD_REQUEST, f"text too long (max {max_len})")
    return text


def normalize_optional_device_id(value: Any) -> str:
    text = str(value or "").strip().upper()
    if not text:
        return ""
    if not re.fullmatch(r"[A-Z0-9_-]{6,64}", text):
        raise APIError(HTTPStatus.BAD_REQUEST, "invalid espDeviceId format")
    return text


def normalize_optional_mac(value: Any) -> str:
    raw = str(value or "").strip().upper()
    if not raw:
        return ""
    cleaned = re.sub(r"[^A-F0-9]", "", raw)
    if len(cleaned) != 12:
        raise APIError(HTTPStatus.BAD_REQUEST, "invalid espMac format")
    return ":".join(cleaned[i : i + 2] for i in range(0, 12, 2))


def parse_yy_mm(body: Dict[str, Any]) -> tuple[str, str]:
    production_month = str(body.get("productionMonth") or "").strip()
    yy = str(body.get("yy") or "").strip()
    mm = str(body.get("mm") or "").strip()

    if production_month:
        match = re.fullmatch(r"(20\d{2})-(\d{2})", production_month)
        if not match:
            raise APIError(HTTPStatus.BAD_REQUEST, "productionMonth must be YYYY-MM")
        yy = match.group(1)[2:]
        mm = match.group(2)

    yy = normalize_two_digits(yy, name="yy", min_val=0, max_val=99)
    mm = normalize_two_digits(mm, name="mm", min_val=1, max_val=12)
    return yy, mm


def make_product_id_from_body(body: Dict[str, Any]) -> Dict[str, str]:
    tier_code = normalize_code(body.get("tierCode"), name="tierCode", allowed=ALLOWED_TIER_CODES)
    generation = normalize_code(body.get("generation"), name="generation")
    origin_code = normalize_code(body.get("originCode"), name="originCode")
    game_code = normalize_code(body.get("gameCode"), name="gameCode", allowed=ALLOWED_GAME_CODES)
    sensor_code = normalize_code(
        body.get("sensorCode"), name="sensorCode", allowed=ALLOWED_SENSOR_CODES
    )
    chip_code = normalize_code(body.get("chipCode"), name="chipCode", allowed=ALLOWED_CHIP_CODES)
    sw_code = normalize_code(body.get("swCode"), name="swCode")
    yy, mm = parse_yy_mm(body)
    reserved = normalize_two_digits(body.get("reserved") or "00", name="reserved")
    serial = normalize_two_digits(body.get("serial"), name="serial")

    product_id = (
        f"MB{tier_code}{generation}{origin_code}{game_code}"
        f"{sensor_code}{chip_code}{sw_code}{yy}{mm}{reserved}{serial}"
    )
    if not FULL_PRODUCT_ID_PATTERN.fullmatch(product_id):
        raise APIError(HTTPStatus.BAD_REQUEST, "generated productId is invalid")

    provided = sanitize_product_id(str(body.get("productId") or ""))
    if provided and provided != product_id:
        raise APIError(
            HTTPStatus.BAD_REQUEST,
            "provided productId does not match naming fields",
        )

    active_flag = bool(body.get("active", True))
    model_text = str(body.get("model") or f"mBoard-{TIER_NAME_MAP[tier_code]}").strip()
    efuse_device_id = normalize_optional_device_id(
        body.get("efuseDeviceId") or body.get("espDeviceId")
    )
    esp_device_id = normalize_optional_device_id(body.get("espDeviceId") or efuse_device_id)
    esp_mac = normalize_optional_mac(body.get("espMac"))
    chip_model = normalize_optional_text(body.get("chipModel"), max_len=64)
    fw_version = normalize_optional_text(body.get("fwVersion"), max_len=32)

    return {
        "product_id": product_id,
        "model": model_text,
        "active": "1" if active_flag else "0",
        "tier_code": tier_code,
        "tier_name": TIER_NAME_MAP[tier_code],
        "generation": generation,
        "origin_code": origin_code,
        "game_code": game_code,
        "sensor_code": sensor_code,
        "chip_code": chip_code,
        "sw_code": sw_code,
        "efuse_device_id": efuse_device_id,
        "esp_device_id": esp_device_id,
        "esp_mac": esp_mac,
        "chip_model": chip_model,
        "fw_version": fw_version,
        "yy": yy,
        "mm": mm,
        "reserved": reserved,
        "serial": serial,
        "created_at": now_iso(),
    }


def whitelist_rows() -> List[Dict[str, str]]:
    if not WHITELIST_CSV.exists():
        return []

    with WHITELIST_CSV.open("r", newline="", encoding="utf-8") as f:
        sample = f.read(2048)
        f.seek(0)

        has_header = "product_id" in sample.lower()
        if has_header:
            reader = csv.DictReader(f)
            rows = []
            for row in reader:
                pid = (row.get("product_id") or "").strip()
                if not pid:
                    continue
                rows.append(
                    {
                        "product_id": pid,
                        "model": (row.get("model") or "").strip(),
                        "active": (row.get("active") or "1").strip() or "1",
                    }
                )
            return rows

        # Fallback: one product_id per line without header
        f.seek(0)
        rows = []
        for line in f:
            pid = line.strip().split(",")[0]
            if pid:
                rows.append({"product_id": pid, "model": "", "active": "1"})
        return rows


def read_product_table() -> tuple[List[str], List[Dict[str, str]]]:
    if not WHITELIST_CSV.exists():
        return PRODUCT_DEFAULT_FIELDS[:], []

    with WHITELIST_CSV.open("r", newline="", encoding="utf-8") as f:
        sample = f.read(2048)
        f.seek(0)
        has_header = "product_id" in sample.lower()

        if has_header:
            reader = csv.DictReader(f)
            headers = [h for h in (reader.fieldnames or []) if h]
            if not headers:
                headers = PRODUCT_DEFAULT_FIELDS[:]
            rows = []
            for row in reader:
                normalized = {}
                for key, value in row.items():
                    if not key:
                        continue
                    normalized[key] = (value or "").strip()
                if normalized.get("product_id"):
                    rows.append(normalized)
            return headers, rows

        # Fallback: file without header, one id per line
        rows = []
        for line in f:
            pid = line.strip().split(",")[0].strip()
            if not pid:
                continue
            rows.append(
                {
                    "product_id": pid,
                    "model": "",
                    "active": "1",
                }
            )
        return ["product_id", "model", "active"], rows


def write_product_table(headers: List[str], rows: List[Dict[str, str]]) -> None:
    WHITELIST_CSV.parent.mkdir(parents=True, exist_ok=True)
    with WHITELIST_CSV.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=headers)
        writer.writeheader()
        for row in rows:
            normalized = {h: row.get(h, "") for h in headers}
            writer.writerow(normalized)


def append_product_to_whitelist(row: Dict[str, str]) -> None:
    headers, rows = read_product_table()
    product_id = row["product_id"]

    if any((r.get("product_id") or "").strip() == product_id for r in rows):
        raise APIError(HTTPStatus.CONFLICT, "productId already exists")

    merged_headers: List[str] = []
    for key in PRODUCT_DEFAULT_FIELDS + headers + list(row.keys()):
        if key and key not in merged_headers:
            merged_headers.append(key)

    rows.append(row)
    write_product_table(merged_headers, rows)


def board_access_info(host: str, product_id: str, proto: str = "http") -> Dict[str, str]:
    # QR entry should start from scan/provision flow:
    # 1) verify product id
    # 2) check ESP32 online status
    # 3) if offline -> BLE Wi-Fi provisioning
    # 4) if online -> jump to board/game page
    path = f"/scan/public/scan.html?id={quote(product_id)}"
    access_url = f"{proto}://{host}{path}"
    qr_url = (
        "https://api.qrserver.com/v1/create-qr-code/?size=320x320&data="
        + quote(access_url, safe="")
    )
    return {"accessPath": path, "accessUrl": access_url, "qrUrl": qr_url}


def is_product_allowed(product_id: str) -> bool:
    for row in whitelist_rows():
        if row["product_id"] != product_id:
            continue
        return is_active_flag(row.get("active", "1"))
    return False


def product_workspace(product_id: str) -> Path:
    return PRODUCTS_ROOT / product_id


def ensure_csv_file(path: Path, fieldnames: List[str]) -> None:
    if path.exists():
        return
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()


def ensure_product_workspace(product_id: str) -> Path:
    workspace = product_workspace(product_id)
    workspace.mkdir(parents=True, exist_ok=True)

    ensure_csv_file(workspace / "games.csv", GAME_FIELDS)
    ensure_csv_file(workspace / "configs.csv", CONFIG_FIELDS)
    ensure_csv_file(workspace / "commands.csv", COMMAND_FIELDS)
    ensure_csv_file(workspace / "sensor_moves.csv", SENSOR_FIELDS)

    meta_path = workspace / "meta.json"
    if not meta_path.exists():
        meta_path.write_text(
            json.dumps(
                {
                    "product_id": product_id,
                    "created_at": now_iso(),
                    "updated_at": now_iso(),
                },
                ensure_ascii=False,
                indent=2,
            ),
            encoding="utf-8",
        )
    return workspace


def update_meta_updated_at(workspace: Path) -> None:
    meta_path = workspace / "meta.json"
    try:
        if meta_path.exists():
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
        else:
            meta = {}
        meta["updated_at"] = now_iso()
        meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")
    except Exception:
        # Keep API robust; metadata update failure should not break writes.
        pass


def board_online_status(workspace: Path, timeout_seconds: int = 90) -> Dict[str, Any]:
    meta_path = workspace / "meta.json"
    if not meta_path.exists():
        return {"online": False, "last_seen": ""}

    try:
        meta = json.loads(meta_path.read_text(encoding="utf-8"))
    except Exception:
        return {"online": False, "last_seen": ""}

    heartbeat_at = str(meta.get("esp_heartbeat_at") or "").strip()
    if not heartbeat_at:
        return {"online": False, "last_seen": ""}

    try:
        dt = datetime.fromisoformat(heartbeat_at.replace("Z", "+00:00"))
        now_dt = datetime.now(timezone.utc)
        online = (now_dt - dt).total_seconds() <= timeout_seconds
        return {"online": bool(online), "last_seen": heartbeat_at}
    except Exception:
        return {"online": False, "last_seen": heartbeat_at}


def update_board_heartbeat(workspace: Path, payload: Dict[str, Any]) -> Dict[str, Any]:
    meta_path = workspace / "meta.json"
    try:
        if meta_path.exists():
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
        else:
            meta = {}
    except Exception:
        meta = {}

    meta["product_id"] = workspace.name
    meta["esp_heartbeat_at"] = now_iso()
    if payload.get("deviceId"):
        meta["esp_device_id"] = str(payload.get("deviceId"))
    if payload.get("fwVersion"):
        meta["fw_version"] = str(payload.get("fwVersion"))
    if payload.get("ip"):
        meta["ip"] = str(payload.get("ip"))
    meta["updated_at"] = now_iso()
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")
    return meta


def next_row_id(path: Path) -> int:
    if not path.exists():
        return 1

    last_id = 0
    with path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                value = int(row.get("id") or 0)
                if value > last_id:
                    last_id = value
            except ValueError:
                continue
    return last_id + 1


def append_csv(path: Path, fieldnames: List[str], row: Dict[str, Any]) -> int:
    ensure_csv_file(path, fieldnames)
    row_id = next_row_id(path)
    payload: Dict[str, Any] = {k: "" for k in fieldnames}
    payload.update(row)
    payload["id"] = row_id

    with path.open("a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writerow(payload)
    return row_id


def read_csv(path: Path) -> List[Dict[str, str]]:
    if not path.exists():
        return []
    with path.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        return [dict(row) for row in reader]


class APIError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status
        self.message = message


class MTabulaHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(PROJECT_ROOT), **kwargs)

    def end_headers(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS")
        super().end_headers()

    def do_OPTIONS(self) -> None:
        self.send_response(HTTPStatus.NO_CONTENT)
        self.end_headers()

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path

        if path == "/pid.html":
            if not self.current_auth_username():
                self.redirect("/pid-login.html")
                return

        if path == "/pid-login.html":
            if self.current_auth_username():
                self.redirect("/pid.html")
                return

        if self.path.startswith("/api/"):
            self.handle_api("GET")
            return
        super().do_GET()

    def do_HEAD(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path
        if path == "/pid.html" and not self.current_auth_username():
            self.send_response(HTTPStatus.FOUND)
            self.send_header("Location", "/pid-login.html")
            self.end_headers()
            return
        if path == "/pid-login.html" and self.current_auth_username():
            self.send_response(HTTPStatus.FOUND)
            self.send_header("Location", "/pid.html")
            self.end_headers()
            return
        super().do_HEAD()

    def do_POST(self) -> None:
        if self.path.startswith("/api/"):
            self.handle_api("POST")
            return
        self.send_error(HTTPStatus.NOT_FOUND, "Not Found")

    def read_json_body(self) -> Dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0") or "0")
        if length <= 0:
            return {}
        raw = self.rfile.read(length)
        if not raw:
            return {}
        try:
            return json.loads(raw.decode("utf-8"))
        except json.JSONDecodeError as exc:
            raise APIError(HTTPStatus.BAD_REQUEST, f"invalid JSON: {exc}") from exc

    def write_json(
        self,
        status: int,
        payload: Dict[str, Any],
        extra_headers: Optional[Dict[str, str]] = None,
    ) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        if extra_headers:
            for key, value in extra_headers.items():
                self.send_header(key, value)
        self.end_headers()
        self.wfile.write(body)

    def redirect(self, location: str) -> None:
        self.send_response(HTTPStatus.FOUND)
        self.send_header("Location", location)
        self.end_headers()

    def request_proto(self) -> str:
        raw = (self.headers.get("X-Forwarded-Proto") or "http").split(",")[0].strip()
        return raw or "http"

    def request_host(self) -> str:
        host = (self.headers.get("Host") or "").strip()
        if host:
            return host
        return f"{self.server.server_address[0]}:{self.server.server_address[1]}"

    def raw_cookie_token(self) -> str:
        raw = self.headers.get("Cookie") or ""
        if not raw:
            return ""
        cookie = SimpleCookie()
        try:
            cookie.load(raw)
        except Exception:
            return ""
        morsel = cookie.get(SESSION_COOKIE_NAME)
        if not morsel:
            return ""
        return morsel.value or ""

    def current_auth_username(self) -> Optional[str]:
        return session_username(self.raw_cookie_token())

    def require_admin_auth(self) -> str:
        username = self.current_auth_username()
        if not username:
            raise APIError(HTTPStatus.UNAUTHORIZED, "login required")
        return username

    def require_product(self, product_id: str, create_workspace: bool = False) -> Path:
        pid = sanitize_product_id(product_id)
        if not pid:
            raise APIError(HTTPStatus.BAD_REQUEST, "invalid productId format")
        if not is_product_allowed(pid):
            raise APIError(HTTPStatus.FORBIDDEN, "productId not authorized")
        if create_workspace:
            return ensure_product_workspace(pid)
        return product_workspace(pid)

    def handle_api(self, method: str) -> None:
        parsed = urlparse(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)

        try:
            if method == "GET" and path == "/api/health":
                self.write_json(HTTPStatus.OK, {"ok": True, "service": "mTabula-python-backend"})
                return

            if method == "GET" and path == "/api/auth/me":
                username = self.current_auth_username()
                self.write_json(
                    HTTPStatus.OK,
                    {
                        "ok": True,
                        "authenticated": bool(username),
                        "username": username or "",
                    },
                )
                return

            if method == "POST" and path == "/api/auth/login":
                body = self.read_json_body()
                username = normalize_username(body.get("username"))
                password = str(body.get("password") or "")
                if not password:
                    raise APIError(HTTPStatus.BAD_REQUEST, "password is required")
                if not verify_admin_password(username, password):
                    raise APIError(HTTPStatus.UNAUTHORIZED, "invalid username or password")

                token = create_session(username)
                cookie = (
                    f"{SESSION_COOKIE_NAME}={token}; Path=/; HttpOnly; SameSite=Lax; "
                    f"Max-Age={SESSION_TTL_SECONDS}"
                )
                if self.request_proto() == "https":
                    cookie += "; Secure"

                self.write_json(
                    HTTPStatus.OK,
                    {"ok": True, "username": username},
                    extra_headers={"Set-Cookie": cookie},
                )
                return

            if method == "POST" and path == "/api/auth/logout":
                revoke_session(self.raw_cookie_token())
                cookie = f"{SESSION_COOKIE_NAME}=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0"
                if self.request_proto() == "https":
                    cookie += "; Secure"
                self.write_json(
                    HTTPStatus.OK,
                    {"ok": True},
                    extra_headers={"Set-Cookie": cookie},
                )
                return

            if method == "GET" and path == "/api/admin/products":
                self.require_admin_auth()
                headers, rows = read_product_table()
                self.write_json(HTTPStatus.OK, {"ok": True, "headers": headers, "rows": rows})
                return

            if method == "POST" and path == "/api/admin/products":
                self.require_admin_auth()
                body = self.read_json_body()
                product_row = make_product_id_from_body(body)
                append_product_to_whitelist(product_row)
                workspace = ensure_product_workspace(product_row["product_id"])
                update_meta_updated_at(workspace)
                access = board_access_info(
                    host=self.request_host(),
                    proto=self.request_proto(),
                    product_id=product_row["product_id"],
                )
                self.write_json(
                    HTTPStatus.OK,
                    {
                        "ok": True,
                        "productId": product_row["product_id"],
                        "workspace": str(workspace.relative_to(PROJECT_ROOT)),
                        "product": product_row,
                        **access,
                    },
                )
                return

            if method == "POST" and path == "/api/scan/verify":
                body = self.read_json_body()
                product_id = sanitize_product_id(str(body.get("productId") or ""))
                if not product_id:
                    raise APIError(HTTPStatus.BAD_REQUEST, "productId is required")
                if not is_product_allowed(product_id):
                    raise APIError(HTTPStatus.FORBIDDEN, "productId not authorized")
                workspace = ensure_product_workspace(product_id)
                update_meta_updated_at(workspace)
                access = board_access_info(
                    host=self.request_host(),
                    proto=self.request_proto(),
                    product_id=product_id,
                )
                self.write_json(
                    HTTPStatus.OK,
                    {
                        "ok": True,
                        "productId": product_id,
                        "workspace": str(workspace.relative_to(PROJECT_ROOT)),
                        **access,
                    },
                )
                return

            # /api/boards/<product_id>/...
            board_parts = path.split("/")
            if len(board_parts) >= 5 and board_parts[1] == "api" and board_parts[2] == "boards":
                product_id = board_parts[3]
                action = "/".join(board_parts[4:])

                if method == "GET" and action == "status":
                    workspace = self.require_product(product_id, create_workspace=True)
                    online_info = board_online_status(workspace)
                    self.write_json(
                        HTTPStatus.OK,
                        {
                            "ok": True,
                            "productId": product_id,
                            "online": online_info["online"],
                            "lastSeen": online_info["last_seen"],
                            "verified": True,
                            "now": now_iso(),
                        },
                    )
                    return

                if method == "POST" and action == "heartbeat":
                    workspace = self.require_product(product_id, create_workspace=True)
                    body = self.read_json_body()
                    meta = update_board_heartbeat(workspace, body)
                    self.write_json(
                        HTTPStatus.OK,
                        {
                            "ok": True,
                            "productId": product_id,
                            "online": True,
                            "lastSeen": meta.get("esp_heartbeat_at", ""),
                        },
                    )
                    return

                if method == "POST" and action == "moves":
                    workspace = self.require_product(product_id, create_workspace=True)
                    body = self.read_json_body()
                    row_id = append_csv(
                        workspace / "games.csv",
                        GAME_FIELDS,
                        {
                            "timestamp": body.get("ts") or now_iso(),
                            "board_id": product_id,
                            "type": body.get("type") or "chess",
                            "source": body.get("source") or "web",
                            "move": body.get("move") or "",
                            "fen": body.get("fen") or "",
                            "state": body.get("state") or "",
                            "detail_json": json.dumps(body.get("detail") or {}, ensure_ascii=False),
                        },
                    )
                    update_meta_updated_at(workspace)
                    self.write_json(HTTPStatus.OK, {"ok": True, "id": row_id})
                    return

                if method == "POST" and action == "config":
                    workspace = self.require_product(product_id, create_workspace=True)
                    body = self.read_json_body()
                    row_id = append_csv(
                        workspace / "configs.csv",
                        CONFIG_FIELDS,
                        {
                            "timestamp": body.get("ts") or now_iso(),
                            "board_id": product_id,
                            "scale_x": body.get("scaleX") or "",
                            "scale_y": body.get("scaleY") or "",
                            "offset_x": body.get("offsetX") or "",
                            "offset_y": body.get("offsetY") or "",
                            "magnet_level": body.get("magnetLevel") or "",
                            "raw_json": json.dumps(body, ensure_ascii=False),
                        },
                    )
                    update_meta_updated_at(workspace)
                    self.write_json(HTTPStatus.OK, {"ok": True, "id": row_id})
                    return

                if method == "POST" and action == "command/home":
                    workspace = self.require_product(product_id, create_workspace=True)
                    body = self.read_json_body()
                    row_id = append_csv(
                        workspace / "commands.csv",
                        COMMAND_FIELDS,
                        {
                            "timestamp": now_iso(),
                            "board_id": product_id,
                            "command": "home",
                            "payload_json": json.dumps(body or {}, ensure_ascii=False),
                        },
                    )
                    update_meta_updated_at(workspace)
                    self.write_json(HTTPStatus.OK, {"ok": True, "id": row_id})
                    return

                if method == "GET" and action == "sensor-moves":
                    self.require_product(product_id, create_workspace=True)
                    workspace = ensure_product_workspace(product_id)
                    after = int((query.get("after") or ["0"])[0] or "0")
                    rows = read_csv(workspace / "sensor_moves.csv")
                    moves = []
                    for row in rows:
                        try:
                            row_id = int(row.get("id") or 0)
                        except ValueError:
                            continue
                        if row_id <= after:
                            continue
                        moves.append(
                            {
                                "id": row_id,
                                "type": row.get("type") or "chess",
                                "from": row.get("from") or "",
                                "to": row.get("to") or "",
                                "move": row.get("move") or "",
                                "fen": row.get("fen") or "",
                                "state": row.get("state") or "",
                                "ts": row.get("timestamp") or "",
                            }
                        )
                    self.write_json(HTTPStatus.OK, {"ok": True, "moves": moves})
                    return

                if method == "POST" and action == "sensor-moves":
                    workspace = self.require_product(product_id, create_workspace=True)
                    body = self.read_json_body()
                    row_id = append_csv(
                        workspace / "sensor_moves.csv",
                        SENSOR_FIELDS,
                        {
                            "timestamp": body.get("ts") or now_iso(),
                            "board_id": product_id,
                            "type": body.get("type") or "chess",
                            "from": body.get("from") or "",
                            "to": body.get("to") or "",
                            "move": body.get("move") or "",
                            "fen": body.get("fen") or "",
                            "state": body.get("state") or "",
                            "raw_json": json.dumps(body, ensure_ascii=False),
                        },
                    )
                    update_meta_updated_at(workspace)
                    self.write_json(HTTPStatus.OK, {"ok": True, "id": row_id})
                    return

            if method == "GET" and path == "/api/boards/history":
                board_raw = (query.get("boardId") or [""])[0] or ""
                board_filter = sanitize_product_id(board_raw) if board_raw else None
                if board_raw and not board_filter:
                    raise APIError(HTTPStatus.BAD_REQUEST, "invalid boardId format")
                rows: List[Dict[str, Any]] = []

                if board_filter:
                    self.require_product(board_filter, create_workspace=True)
                    workspaces = [ensure_product_workspace(board_filter)]
                else:
                    workspaces = [p for p in PRODUCTS_ROOT.glob("*") if p.is_dir()]

                for workspace in workspaces:
                    for row in read_csv(workspace / "games.csv"):
                        rows.append(
                            {
                                "id": row.get("id"),
                                "ts": row.get("timestamp") or "",
                                "boardId": row.get("board_id") or workspace.name,
                                "type": row.get("type") or "",
                                "source": row.get("source") or "",
                                "move": row.get("move") or "",
                                "fen": row.get("fen") or "",
                                "state": row.get("state") or "",
                            }
                        )

                rows.sort(key=lambda x: str(x.get("ts") or ""), reverse=True)
                self.write_json(HTTPStatus.OK, {"ok": True, "rows": rows[:600]})
                return

            raise APIError(HTTPStatus.NOT_FOUND, "endpoint not found")

        except APIError as exc:
            self.write_json(exc.status, {"ok": False, "error": exc.message})
        except Exception as exc:  # noqa: BLE001
            self.write_json(
                HTTPStatus.INTERNAL_SERVER_ERROR,
                {"ok": False, "error": f"internal error: {exc}"},
            )


def run_server(host: str, port: int) -> None:
    ensure_data_layout()
    server = ThreadingHTTPServer((host, port), MTabulaHandler)
    print(f"mTabula Python backend running: http://{host}:{port}")
    print(f"Project root: {PROJECT_ROOT}")
    print(f"Whitelist CSV: {WHITELIST_CSV}")
    print(f"Admin users CSV: {ADMIN_USERS_CSV}")
    server.serve_forever()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="mTabula Python backend")
    parser.add_argument("--host", default="0.0.0.0", help="bind host")
    parser.add_argument("--port", type=int, default=8866, help="bind port")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    run_server(args.host, args.port)
