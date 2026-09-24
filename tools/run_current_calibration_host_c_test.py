#!/usr/bin/env python3
"""Build and run host tests for the production current K/B math."""

import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "host_tests"
SOURCE_DIR = ROOT / "103 + 309" / "Project" / "Source"


def compiler() -> str:
    env_cc = os.environ.get("CC")
    if env_cc:
        return env_cc
    for candidate in ("clang", "cc", "gcc"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("no C compiler found; install clang/gcc or set CC")


def main() -> int:
    cc = compiler()
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    exe = BUILD_DIR / "current_calibration_host_c_test"
    cmd = [
        cc,
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-I" + str(SOURCE_DIR),
        str(ROOT / "tools" / "current_calibration_host_c_test.c"),
        "-o",
        str(exe),
    ]
    print("Building current calibration host C test with:", cc, flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)
    print("Running", exe, flush=True)
    subprocess.run([str(exe)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
