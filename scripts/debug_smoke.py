#!/usr/bin/env python3
"""Run a minimal OpenOCD + GDB debug smoke test."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
APP_ADDRESS = 0x08004800


def tool_path(name: str) -> str:
    root = os.environ.get("ARM_GNU_TOOLCHAIN_PATH")
    search_path = os.environ.get("PATH", "")
    if root:
        search_path = os.pathsep.join([str(Path(root) / "bin"), root, search_path])
    path = shutil.which(name, path=search_path)
    if not path:
        raise FileNotFoundError(f"{name} not found")
    return path


def default_elf(config: str) -> Path:
    folder = "gcc-debug" if config == "Debug" else "gcc-release"
    return REPO_ROOT / "build" / folder / "firmware.elf"


def run_gdb(gdb: str, elf: Path, timeout: int) -> subprocess.CompletedProcess[str]:
    command = [
        gdb,
        "-q",
        "-batch",
        "-ex",
        "set pagination off",
        "-ex",
        "set confirm off",
        "-ex",
        "set remotetimeout 10",
        "-ex",
        "target extended-remote localhost:3333",
        "-ex",
        "monitor reset halt",
        "-ex",
        f"x/4wx 0x{APP_ADDRESS:08X}",
        "-ex",
        "break main",
        "-ex",
        "continue",
        "-ex",
        "info registers pc sp xpsr",
        "-ex",
        "bt",
        "-ex",
        "delete breakpoints",
        "-ex",
        "monitor reset run",
        "-ex",
        "detach",
        str(elf),
    ]
    print("+ " + " ".join(command))
    return subprocess.run(
        command,
        cwd=REPO_ROOT,
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", choices=["Debug", "Release"], default="Release")
    parser.add_argument("--elf", type=Path, help="Explicit ELF file path.")
    parser.add_argument("--interface", default="interface/stlink.cfg")
    parser.add_argument("--target", default="target/stm32f1x.cfg")
    parser.add_argument("--startup-delay", type=float, default=2.0)
    parser.add_argument("--timeout", type=int, default=30)
    args = parser.parse_args()

    elf = (args.elf or default_elf(args.config)).resolve()
    if not elf.exists():
        raise FileNotFoundError(f"ELF not found: {elf}")

    openocd = tool_path("openocd")
    gdb = tool_path("arm-none-eabi-gdb")
    openocd_command = [openocd, "-f", args.interface, "-f", args.target]
    print("+ " + " ".join(openocd_command))

    server = subprocess.Popen(
        openocd_command,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    time.sleep(args.startup_delay)

    try:
        if server.poll() is not None:
            stdout, stderr = server.communicate(timeout=2)
            print(stderr, file=sys.stderr)
            print(stdout)
            return server.returncode or 1

        result = run_gdb(gdb, elf, args.timeout)
        if result.stdout:
            print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")

        if "Breakpoint 1, main" not in result.stdout:
            print("error: main breakpoint was not hit", file=sys.stderr)
            return 1
        return result.returncode
    finally:
        if server.poll() is None:
            server.terminate()
            try:
                server.wait(timeout=2)
            except subprocess.TimeoutExpired:
                server.kill()
        stdout, stderr = server.communicate()
        if stderr:
            print("--- openocd stderr ---")
            print(stderr, end="")
        if stdout:
            print("--- openocd stdout ---")
            print(stdout, end="")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, subprocess.TimeoutExpired) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
