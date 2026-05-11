#!/usr/bin/env python3
"""Build and run host tests for the hardware-independent upgrader MCU core."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "upgrader_mcu" / "core"
TEST = ROOT / "upgrader_mcu" / "tests" / "upgrader_core_test.c"
BUILD = ROOT / "build" / "upgrader_mcu_tests"


def compiler() -> str:
    for candidate in ("clang", "cc", "gcc"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("no C compiler found; install clang/gcc or set CC")


def main() -> int:
    BUILD.mkdir(parents=True, exist_ok=True)
    exe = BUILD / ("upgrader_core_test.exe" if sys.platform.startswith("win") else "upgrader_core_test")
    cc = compiler()
    sources = [
        TEST,
        CORE / "upg_core.c",
        CORE / "upg_serial.c",
        CORE / "upg_feidao.c",
        CORE / "upg_params.c",
        CORE / "upg_crc16.c",
        CORE / "upg_utils.c",
    ]
    cmd = [
        cc,
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I" + str(CORE),
        *[str(src) for src in sources],
        "-o",
        str(exe),
    ]
    print("Building upgrader MCU core tests with:", cc, flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)
    print("Running", exe, flush=True)
    subprocess.run([str(exe)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
