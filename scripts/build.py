#!/usr/bin/env python3
"""Configure and build the firmware through CMake presets."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def with_toolchain_path() -> dict[str, str]:
    env = os.environ.copy()
    root = env.get("ARM_GNU_TOOLCHAIN_PATH")
    if root:
        bin_dir = str(Path(root) / "bin")
        env["PATH"] = os.pathsep.join([bin_dir, root, env.get("PATH", "")])
    return env


def run(command: list[str]) -> None:
    print("+ " + " ".join(command))
    subprocess.run(command, cwd=REPO_ROOT, env=with_toolchain_path(), check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        choices=["Debug", "Release"],
        default="Debug",
        help="CMake build type to use.",
    )
    parser.add_argument("--clean-first", action="store_true", help="Pass --clean-first to cmake --build.")
    parser.add_argument("--configure-only", action="store_true", help="Only run cmake configure preset.")
    parser.add_argument("--verbose", action="store_true", help="Pass --verbose to cmake --build.")
    args = parser.parse_args()

    preset = "gcc-debug" if args.config == "Debug" else "gcc-release"
    run(["cmake", "--preset", preset])
    if args.configure_only:
        return 0

    command = ["cmake", "--build", "--preset", preset]
    if args.clean_first:
        command.append("--clean-first")
    if args.verbose:
        command.append("--verbose")
    run(command)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        raise SystemExit(exc.returncode) from exc
