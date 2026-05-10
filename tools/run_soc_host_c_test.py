#!/usr/bin/env python3
"""Build and run host tests against the real MCU SOC C sources."""

import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "host_tests"
SOURCE_DIR = ROOT / "103 + 309" / "Project" / "Source"
STM32_LIB = ROOT / "103 + 309" / "Project" / "STM32F10x_StdPeriph_Lib_V3.5.0"


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
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    exe = BUILD_DIR / "soc_host_c_test"
    cc = compiler()

    cmd = [
        cc,
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-ffunction-sections",
        "-fdata-sections",
        "-DSTM32F10X_HD",
        "-DUSE_STDPERIPH_DRIVER",
        "-I" + str(SOURCE_DIR),
        "-I" + str(SOURCE_DIR / "conf"),
        "-I" + str(SOURCE_DIR / "easylogger" / "inc"),
        "-I" + str(ROOT / "103 + 309" / "Project" / "Users"),
        "-I" + str(STM32_LIB / "drivers"),
        "-I" + str(STM32_LIB / "inc"),
        "-I" + str(ROOT / "103 + 309" / "Libraries" / "CMSIS" / "CM3" / "CoreSupport"),
        "-I" + str(ROOT / "103 + 309" / "Libraries" / "CMSIS" / "CM3" / "DeviceSupport" / "ST" / "STM32F10x"),
        "-I" + str(ROOT / "103 + 309" / "Libraries" / "STM32F10x_StdPeriph_Driver" / "inc"),
        str(ROOT / "tools" / "soc_host_c_test.c"),
        str(SOURCE_DIR / "SOC.c"),
        str(SOURCE_DIR / "SocEnhance.c"),
        str(SOURCE_DIR / "PubFunc.c"),
        "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
        "-o",
        str(exe),
    ]

    print("Building SOC host C test with:", cc, flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)
    print("Running", exe, flush=True)
    subprocess.run([str(exe)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
