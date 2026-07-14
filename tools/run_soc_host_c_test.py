#!/usr/bin/env python3
"""Build and run host tests against the real MCU SOC C sources."""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "host_tests"
SOURCE_DIR = ROOT / "103 + 309" / "Project" / "Source"
STM32_LIB = ROOT / "103 + 309" / "Project" / "STM32F10x_StdPeriph_Lib_V3.5.0"
CONF_DIR = SOURCE_DIR / "conf"


def compiler() -> str:
    env_cc = os.environ.get("CC")
    if env_cc:
        return env_cc
    for candidate in ("clang", "cc", "gcc"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("no C compiler found; install clang/gcc or set CC")


def configured_int(name: str, default: int) -> int:
    text = (CONF_DIR / "Project_Config.h").read_text(encoding="utf-8", errors="ignore")
    match = re.search(r"^\s*#define\s+{0}\s+(\d+)\b".format(re.escape(name)),
                      text,
                      re.MULTILINE)
    return int(match.group(1)) if match else default


def conf_override_dir(board_self_consumption_ma: int, reserve_capacity_ah10: int) -> Path:
    target = BUILD_DIR / f"conf_board_self_{board_self_consumption_ma}_reserve_{reserve_capacity_ah10}"
    target.mkdir(parents=True, exist_ok=True)
    for name in ("conf.h", "Project_Config.h", "Project_BuildGuard.h"):
        text = (CONF_DIR / name).read_text(encoding="utf-8", errors="ignore")
        if name == "Project_Config.h":
            text = re.sub(
                r"(^\s*#define\s+PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA\s+)\d+(\b)",
                rf"\g<1>{board_self_consumption_ma}\2",
                text,
                flags=re.MULTILINE,
            )
            text = re.sub(
                r"(^\s*#define\s+PROJECT_CFG_SOC_RESERVE_CAPACITY_AH10\s+)\d+(\b)",
                rf"\g<1>{reserve_capacity_ah10}\2",
                text,
                flags=re.MULTILINE,
            )
        (target / name).write_text(text, encoding="utf-8")
    return target


def build_and_run(cc: str, exe: Path, extra_defines=None, board_self_consumption_ma=None,
                  reserve_capacity_ah10=None) -> None:
    extra_defines = extra_defines or []
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    override_conf = (conf_override_dir(board_self_consumption_ma, reserve_capacity_ah10)
                     if board_self_consumption_ma is not None and reserve_capacity_ah10 is not None
                     else None)

    include_dirs = [
        SOURCE_DIR,
    ]
    if override_conf is not None:
        include_dirs.append(override_conf)
    include_dirs.extend([
        SOURCE_DIR / "conf",
        SOURCE_DIR / "easylogger" / "inc",
        ROOT / "103 + 309" / "Project" / "Users",
        STM32_LIB / "drivers",
        STM32_LIB / "inc",
        ROOT / "103 + 309" / "Libraries" / "CMSIS" / "CM3" / "CoreSupport",
        ROOT / "103 + 309" / "Libraries" / "CMSIS" / "CM3" / "DeviceSupport" / "ST" / "STM32F10x",
        ROOT / "103 + 309" / "Libraries" / "STM32F10x_StdPeriph_Driver" / "inc",
    ])

    cmd = [
        cc,
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-ffunction-sections",
        "-fdata-sections",
        "-DSTM32F10X_MD",
        "-DUSE_STDPERIPH_DRIVER",
        *extra_defines,
        *(("-I" + str(path)) for path in include_dirs),
        str(ROOT / "tools" / "soc_host_c_test.c"),
        str(SOURCE_DIR / "SOC.c"),
        str(SOURCE_DIR / "SocEnhance.c"),
        "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
        "-o",
        str(exe),
    ]

    label = ("current config" if board_self_consumption_ma is None
             else f"board_self={board_self_consumption_ma}mA reserve={reserve_capacity_ah10 / 10:.1f}Ah")
    print("Building SOC host C test with:", cc, exe.name, label, flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)
    print("Running", exe, flush=True)
    subprocess.run([str(exe)], cwd=ROOT, check=True)


def main() -> int:
    cc = compiler()
    current_self = configured_int("PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA", 30)
    current_reserve = configured_int("PROJECT_CFG_SOC_RESERVE_CAPACITY_AH10", 0)
    variants = []
    for item in (
        (current_self, current_reserve),
        (0, current_reserve),
        (30, current_reserve),
        (1000, current_reserve),
        (current_self, 0),
        (current_self, 270),
        (current_self, 300),
    ):
        if item not in variants:
            variants.append(item)

    for board_self, reserve in variants:
        suffix = f"board_self_{board_self}_reserve_{reserve}"
        build_and_run(
            cc,
            BUILD_DIR / f"soc_host_c_test_{suffix}",
            board_self_consumption_ma=board_self,
            reserve_capacity_ah10=reserve,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
