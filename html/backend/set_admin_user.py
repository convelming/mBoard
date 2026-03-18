#!/usr/bin/env python3
"""Create or update admin user in data/admin_users.csv.

Usage:
  cd /Users/convel/Documents/mTabula/html
  python3 ./backend/set_admin_user.py --username admin
"""

from __future__ import annotations

import argparse
import csv
import getpass
import hashlib
import os
import re
import secrets
from datetime import datetime, timezone
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DATA_ROOT = PROJECT_ROOT / "data"
ADMIN_CSV = DATA_ROOT / "admin_users.csv"
FIELDS = ["username", "salt", "password_hash", "active", "created_at"]
USERNAME_PATTERN = re.compile(r"^[A-Za-z0-9_.-]{3,32}$")
PASSWORD_ITERATIONS = 200_000


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def hash_password(password: str, salt_hex: str, pepper: str) -> str:
    salt = bytes.fromhex(salt_hex)
    payload = (password + pepper).encode("utf-8")
    digest = hashlib.pbkdf2_hmac("sha256", payload, salt, PASSWORD_ITERATIONS)
    return digest.hex()


def read_rows() -> list[dict[str, str]]:
    if not ADMIN_CSV.exists():
        return []
    with ADMIN_CSV.open("r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        out = []
        for row in reader:
            username = (row.get("username") or "").strip()
            if not username:
                continue
            out.append({k: (row.get(k) or "").strip() for k in FIELDS})
        return out


def write_rows(rows: list[dict[str, str]]) -> None:
    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    with ADMIN_CSV.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow({k: row.get(k, "") for k in FIELDS})


def main() -> None:
    parser = argparse.ArgumentParser(description="Set admin user")
    parser.add_argument("--username", required=True)
    parser.add_argument("--inactive", action="store_true")
    args = parser.parse_args()

    username = args.username.strip()
    if not USERNAME_PATTERN.fullmatch(username):
        raise SystemExit("invalid username format")

    password = getpass.getpass("Password: ")
    confirm = getpass.getpass("Confirm password: ")
    if not password:
        raise SystemExit("password cannot be empty")
    if password != confirm:
        raise SystemExit("password mismatch")

    pepper = os.getenv("MTABULA_AUTH_PEPPER", "")
    salt_hex = secrets.token_hex(16)
    password_hash = hash_password(password, salt_hex, pepper)

    rows = read_rows()
    found = False
    for row in rows:
        if row.get("username") == username:
            row["salt"] = salt_hex
            row["password_hash"] = password_hash
            row["active"] = "0" if args.inactive else "1"
            row["created_at"] = now_iso()
            found = True
            break

    if not found:
        rows.append(
            {
                "username": username,
                "salt": salt_hex,
                "password_hash": password_hash,
                "active": "0" if args.inactive else "1",
                "created_at": now_iso(),
            }
        )

    write_rows(rows)
    print(f"saved user: {username}")


if __name__ == "__main__":
    main()
