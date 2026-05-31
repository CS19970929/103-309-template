#!/usr/bin/env python3
"""Check cross-platform GCC/CMake firmware build environment."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional


REQUIRED_TOOLS = [
    "cmake",
    "ninja",
    "arm-none-eabi-gcc",
    "arm-none-eabi-objcopy",
    "arm-none-eabi-size",
    "arm-none-eabi-gdb",
]

OPTIONAL_TOOLS = [
    "JLinkGDBServer",
    "JLinkGDBServerCL",
    "STM32_Programmer_CLI",
    "openocd",
]


def candidate_path_dirs() -> list[str]:
    dirs: list[str] = []
    root = os.environ.get("ARM_GNU_TOOLCHAIN_PATH")
    if root:
        dirs.append(root)
        dirs.append(str(Path(root) / "bin"))
    return dirs


def which_tool(name: str) -> Optional[str]:
    path = shutil.which(name)
    if path:
        return path
    for directory in candidate_path_dirs():
        candidate = shutil.which(name, path=directory)
        if candidate:
            return candidate
    return None


def version_line(command: str) -> str:
    try:
        result = subprocess.run(
            [command, "--version"],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
    except Exception as exc:  # noqa: BLE001
        return f"version check failed: {exc}"
    output = (result.stdout or result.stderr).strip().splitlines()
    return output[0] if output else f"exit={result.returncode}"


def print_tool(name: str, required: bool) -> bool:
    path = which_tool(name)
    label = "required" if required else "optional"
    if not path:
        print(f"[MISS] {label:8} {name}")
        return False
    print(f"[ OK ] {label:8} {name}: {path}")
    print(f"       {version_line(path)}")
    return True


def main() -> int:
    print(f"Python: {sys.version.split()[0]} ({sys.executable})")
    if os.environ.get("ARM_GNU_TOOLCHAIN_PATH"):
        print(f"ARM_GNU_TOOLCHAIN_PATH: {os.environ['ARM_GNU_TOOLCHAIN_PATH']}")
    else:
        print("ARM_GNU_TOOLCHAIN_PATH: <not set>")
    print()

    ok = True
    for tool in REQUIRED_TOOLS:
        ok = print_tool(tool, required=True) and ok
    print()
    for tool in OPTIONAL_TOOLS:
        print_tool(tool, required=False)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
