#!/usr/bin/env python3
"""Remove generated CMake build directories."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
BUILD_ROOT = REPO_ROOT / "build"


def safe_rmtree(path: Path) -> None:
    resolved = path.resolve()
    build_root = BUILD_ROOT.resolve()
    if build_root not in resolved.parents and resolved != build_root:
        raise RuntimeError(f"refuse to delete outside build root: {resolved}")
    if resolved.exists():
        print(f"remove {resolved}")
        shutil.rmtree(resolved)
    else:
        print(f"skip missing {resolved}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", choices=["Debug", "Release", "all"], default="all")
    args = parser.parse_args()

    if args.config in ("Debug", "all"):
        safe_rmtree(BUILD_ROOT / "gcc-debug")
    if args.config in ("Release", "all"):
        safe_rmtree(BUILD_ROOT / "gcc-release")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
