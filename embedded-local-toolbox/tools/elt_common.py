#!/usr/bin/env python3
"""
Shared helpers for embedded-local-toolbox.

The toolbox intentionally stays local-only and standard-library first. Helpers
in this module avoid network access, never overwrite files unless requested, and
write reports as Markdown.
"""

from __future__ import annotations

import csv
import shutil
import subprocess
import json
import re
from pathlib import Path
from typing import Any, Dict, Iterable, List, Sequence


TOOLBOX_ROOT = Path(__file__).resolve().parents[1]


def parse_int(value: Any) -> int:
    if isinstance(value, int):
        return value
    text = str(value).strip()
    if not text:
        raise ValueError("empty integer")
    return int(text, 0)


def read_csv(path: str | Path) -> List[Dict[str, str]]:
    with Path(path).open("r", encoding="utf-8-sig", newline="") as fp:
        return [dict(row) for row in csv.DictReader(fp)]


def write_csv(path: str | Path, rows: Sequence[Dict[str, Any]], fieldnames: Sequence[str], force: bool = False) -> None:
    target = Path(path)
    if target.exists() and not force:
        raise FileExistsError(f"{target} already exists; use --force to overwrite")
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("w", encoding="utf-8", newline="") as fp:
        writer = csv.DictWriter(fp, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def read_json(path: str | Path) -> Any:
    with Path(path).open("r", encoding="utf-8") as fp:
        return json.load(fp)


def write_text_report(path: str | Path | None, text: str, force: bool = False) -> None:
    if path is None:
        print(text)
        return
    target = Path(path)
    if target.exists() and not force:
        raise FileExistsError(f"{target} already exists; use --force to overwrite")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")


def md_table(headers: Sequence[str], rows: Iterable[Sequence[Any]]) -> str:
    lines = []
    lines.append("| " + " | ".join(headers) + " |")
    lines.append("|" + "|".join(["---"] * len(headers)) + "|")
    for row in rows:
        lines.append("| " + " | ".join(_md_cell(item) for item in row) + " |")
    return "\n".join(lines) + "\n"


def _md_cell(value: Any) -> str:
    text = "" if value is None else str(value)
    return text.replace("\n", "<br>").replace("|", "\\|")


def rel(path: str | Path, root: str | Path) -> str:
    try:
        return str(Path(path).resolve().relative_to(Path(root).resolve()))
    except ValueError:
        return str(path)


def find_text_files(root: str | Path, suffixes: Sequence[str]) -> List[Path]:
    base = Path(root)
    result = []
    for path in base.rglob("*"):
        if path.is_file() and path.suffix.lower() in suffixes:
            result.append(path)
    return sorted(result)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*", "", text)
    return text


def auto_note(source: str) -> str:
    return f"> 自动生成，请勿手动修改。源数据：`{source}`。\n\n"


def format_hex(value: int, width: int = 4) -> str:
    return f"0x{value:0{width}X}"


def command_exists(name: str) -> str | None:
    return shutil.which(name)


def run_command(args: Sequence[str], timeout: float = 10.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, text=True, capture_output=True, timeout=timeout, check=False)


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def crc32_file(path: str | Path) -> int:
    import zlib

    crc = 0
    with Path(path).open("rb") as fp:
        for chunk in iter(lambda: fp.read(65536), b""):
            crc = zlib.crc32(chunk, crc)
    return crc & 0xFFFFFFFF


def file_sha256(path: str | Path) -> str:
    import hashlib

    digest = hashlib.sha256()
    with Path(path).open("rb") as fp:
        for chunk in iter(lambda: fp.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()
