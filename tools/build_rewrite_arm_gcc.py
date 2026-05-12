#!/usr/bin/env python3
"""Compile clean-room rewrite C files with arm-none-eabi-gcc without linking."""

from __future__ import print_function

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPL = ROOT / "103 + 309" / "Project" / "STM32F10x_StdPeriph_Lib_V3.5.0"

SOURCES = [
    "firmware_rewrite/src/bms_app.c",
    "firmware_rewrite/src/bms_comm.c",
    "firmware_rewrite/src/bms_firmware.c",
    "firmware_rewrite/src/bms_power_can.c",
    "firmware_rewrite/src/bms_protection.c",
    "firmware_rewrite/src/bms_soc.c",
    "firmware_rewrite/src/bms_storage.c",
    "firmware_rewrite/src/bms_ui_iap.c",
    "firmware_rewrite/ports/stm32f1_spl/bms_main_stm32f1_spl.c",
    "firmware_rewrite/ports/stm32f1_spl/bms_port_stm32f1_spl.c",
    "firmware_rewrite/ports/stm32f1_spl/bms_board_stm32f1_spl.c",
    "firmware_rewrite/ports/stm32f1_spl/bms_it_stm32f1_spl.c",
    "firmware_rewrite/ports/stm32f1_spl/bms_system_stm32f1_spl.c",
]

COMPAT_HEADERS = {
    "stdint.h": """
#ifndef STDINT_H
#define STDINT_H
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef unsigned int uintptr_t;
typedef int intptr_t;
#endif
""",
    "stddef.h": """
#ifndef STDDEF_H
#define STDDEF_H
typedef unsigned int size_t;
#define offsetof(type, member) __builtin_offsetof(type, member)
#define NULL ((void *)0)
#endif
""",
    "stdbool.h": """
#ifndef STDBOOL_H
#define STDBOOL_H
#define bool _Bool
#define true 1
#define false 0
#endif
""",
    "string.h": """
#ifndef STRING_H
#define STRING_H
#include <stddef.h>
void *memset(void *dest, int value, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
int memcmp(const void *lhs, const void *rhs, size_t len);
#endif
""",
    "stdlib.h": """
#ifndef STDLIB_H
#define STDLIB_H
static inline int abs(int value) { return value < 0 ? -value : value; }
#endif
""",
}


def write_compat_headers(directory):
    for name, content in COMPAT_HEADERS.items():
        (directory / name).write_text(content.strip() + "\n", encoding="ascii")


def compile_source(gcc, compat_dir, out_dir, source):
    source_path = ROOT / source
    object_path = out_dir / (source_path.stem + ".o")
    cmd = [
        gcc,
        "-mcpu=cortex-m3",
        "-mthumb",
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-DSTM32F10X_MD",
        "-DUSE_STDPERIPH_DRIVER",
        "-I" + str(compat_dir),
        "-I" + str(ROOT / "firmware_rewrite" / "include"),
        "-I" + str(ROOT / "firmware_rewrite" / "ports" / "stm32f1_spl"),
        "-I" + str(SPL / "drivers"),
        "-I" + str(SPL / "inc"),
        "-c",
        str(source_path),
        "-o",
        str(object_path),
    ]
    print("compile {0}".format(source))
    return subprocess.run(cmd, cwd=str(ROOT)).returncode


def main():
    gcc = shutil.which("arm-none-eabi-gcc")
    if not gcc:
        print("arm-none-eabi-gcc not found")
        return 2

    with tempfile.TemporaryDirectory(prefix="bms-arm-gcc-") as tmp:
        tmp_path = Path(tmp)
        compat_dir = tmp_path / "compat"
        out_dir = tmp_path / "obj"
        compat_dir.mkdir()
        out_dir.mkdir()
        write_compat_headers(compat_dir)

        for source in SOURCES:
            code = compile_source(gcc, compat_dir, out_dir, source)
            if code != 0:
                return code

    print("ARM GCC compile gate passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
