#!/usr/bin/env python3
"""Build and run the from-scratch BMS rewrite host tests."""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REWRITE = ROOT / "firmware_rewrite"
BUILD = ROOT / "build" / "firmware_rewrite"


def compiler() -> str:
    env_cc = os.environ.get("CC")
    if env_cc:
        return env_cc
    for candidate in ("clang", "cc", "gcc"):
        if shutil.which(candidate):
            return candidate
    raise RuntimeError("no C compiler found; install clang/gcc or set CC")


def main() -> int:
    cc = compiler()
    BUILD.mkdir(parents=True, exist_ok=True)
    exe = BUILD / "test_rewrite_core"
    cmd = [
        cc,
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I",
        str(REWRITE / "include"),
        str(REWRITE / "src" / "bms_app.c"),
        str(REWRITE / "src" / "bms_comm.c"),
        str(REWRITE / "src" / "bms_power_can.c"),
        str(REWRITE / "src" / "bms_soc.c"),
        str(REWRITE / "src" / "bms_storage.c"),
        str(REWRITE / "tests" / "test_rewrite_core.c"),
        "-o",
        str(exe),
    ]
    print("Building firmware rewrite host tests with:", cc, flush=True)
    subprocess.run(cmd, check=True)
    subprocess.run([str(exe)], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
