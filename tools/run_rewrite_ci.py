#!/usr/bin/env python3
"""Run local CI gates for the clean-room firmware rewrite."""

from __future__ import print_function

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run_step(name, cmd):
    print("")
    print("==> {0}".format(name))
    print("    {0}".format(" ".join(cmd)))
    completed = subprocess.run(cmd, cwd=str(ROOT))
    if completed.returncode != 0:
        print("FAILED: {0}".format(name))
        return completed.returncode
    return 0


def main():
    steps = [
        ("git diff whitespace check", ["git", "diff", "--check"]),
        ("project consistency check", [sys.executable, "tools/project_check.py", "--quiet"]),
        ("Keil project wiring check", [sys.executable, "tools/check_rewrite_keil_project.py"]),
        ("ARM GCC compile gate", [sys.executable, "tools/build_rewrite_arm_gcc.py"]),
        ("rewrite host tests", [sys.executable, "tools/run_rewrite_host_tests.py"]),
        ("CMake configure", ["cmake", "-S", "firmware_rewrite", "-B", "build/firmware_rewrite_cmake"]),
        ("CMake build", ["cmake", "--build", "build/firmware_rewrite_cmake"]),
        ("CTest", ["ctest", "--test-dir", "build/firmware_rewrite_cmake", "--output-on-failure"]),
    ]

    for name, cmd in steps:
        code = run_step(name, cmd)
        if code != 0:
            return code

    print("")
    print("Rewrite CI gates passed.")
    print("Keil compile is not part of local CI on non-Windows hosts; run tools\\build_rewrite_keil.ps1 -Build on Windows.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
