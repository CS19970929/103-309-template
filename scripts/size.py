#!/usr/bin/env python3
"""Print arm-none-eabi-size for a built firmware ELF."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def which_size() -> str:
    root = os.environ.get("ARM_GNU_TOOLCHAIN_PATH")
    search_path = None
    if root:
        search_path = os.pathsep.join([str(Path(root) / "bin"), root, os.environ.get("PATH", "")])
    tool = shutil.which("arm-none-eabi-size", path=search_path)
    if not tool:
        raise FileNotFoundError("arm-none-eabi-size not found")
    return tool


def default_elf(config: str) -> Path:
    folder = "gcc-debug" if config == "Debug" else "gcc-release"
    return REPO_ROOT / "build" / folder / "firmware.elf"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", choices=["Debug", "Release"], default="Debug")
    parser.add_argument("--elf", type=Path, help="Explicit ELF file path.")
    args = parser.parse_args()

    elf = (args.elf or default_elf(args.config)).resolve()
    if not elf.exists():
        raise FileNotFoundError(f"ELF not found: {elf}")

    subprocess.run([which_size(), str(elf)], cwd=REPO_ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
