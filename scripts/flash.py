#!/usr/bin/env python3
"""Safe firmware flashing wrapper for J-Link, ST-LINK, and OpenOCD."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Optional


REPO_ROOT = Path(__file__).resolve().parents[1]
APP_ADDRESS = 0x08004800


def find_tool(*names: str) -> Optional[str]:
    for name in names:
        path = shutil.which(name)
        if path:
            return path
    return None


def default_file(config: str, suffix: str) -> Path:
    folder = "gcc-debug" if config == "Debug" else "gcc-release"
    return REPO_ROOT / "build" / folder / f"firmware.{suffix}"


def validate_address(path: Path, address: int) -> None:
    if address == 0x08000000:
        raise ValueError("refuse to flash App image at 0x08000000; this would overwrite IAP")
    if path.suffix.lower() == ".bin" and address != APP_ADDRESS:
        raise ValueError(f"refuse to flash raw bin at 0x{address:08X}; expected 0x{APP_ADDRESS:08X}")


def print_or_run(command: list[str], do_flash: bool) -> None:
    print("+ " + " ".join(command))
    if do_flash:
        subprocess.run(command, cwd=REPO_ROOT, check=True)
    else:
        print("dry-run only; pass --flash to execute")


def openocd_path(path: Path) -> str:
    return "{" + path.as_posix() + "}"


def flash_stlink(args: argparse.Namespace, image: Path) -> None:
    tool = find_tool("STM32_Programmer_CLI") or "STM32_Programmer_CLI"
    if args.flash and not Path(tool).exists() and find_tool("STM32_Programmer_CLI") is None:
        raise FileNotFoundError("STM32_Programmer_CLI not found")
    command = [tool, "-c", f"port={args.port}", "-w", str(image)]
    if image.suffix.lower() == ".bin":
        command.append(f"0x{args.address:08X}")
    command.extend(["-v", "-rst"])
    print_or_run(command, args.flash)


def flash_openocd(args: argparse.Namespace, image: Path) -> None:
    tool = find_tool("openocd") or "openocd"
    if args.flash and not Path(tool).exists() and find_tool("openocd") is None:
        raise FileNotFoundError("openocd not found")
    program = f"program {openocd_path(image)}"
    if image.suffix.lower() == ".bin":
        program += f" 0x{args.address:08X}"
    program += " verify reset exit"
    command = [tool, "-f", args.interface, "-f", args.target, "-c", program]
    print_or_run(command, args.flash)


def flash_jlink(args: argparse.Namespace, image: Path) -> None:
    tool = find_tool("JLinkExe", "JLink", "JLink.exe") or "JLinkExe"
    if args.flash and not Path(tool).exists() and find_tool("JLinkExe", "JLink", "JLink.exe") is None:
        raise FileNotFoundError("JLinkExe/JLink not found")
    with tempfile.NamedTemporaryFile("w", suffix=".jlink", delete=False) as script:
        script_path = Path(script.name)
        script.write(f"device {args.device}\n")
        script.write("si SWD\n")
        script.write(f"speed {args.speed}\n")
        script.write("r\n")
        if image.suffix.lower() == ".bin":
            script.write(f"loadfile {image} 0x{args.address:08X}\n")
            script.write(f"verifybin {image} 0x{args.address:08X}\n")
        else:
            script.write(f"loadfile {image}\n")
        script.write("r\n")
        script.write("g\n")
        script.write("exit\n")
    try:
        command = [tool, "-CommanderScript", str(script_path)]
        print_or_run(command, args.flash)
    finally:
        if args.flash:
            script_path.unlink(missing_ok=True)
        else:
            print(f"J-Link command file kept for review: {script_path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--method", choices=["jlink", "stlink", "openocd"], required=True)
    parser.add_argument("--config", choices=["Debug", "Release"], default="Debug")
    parser.add_argument("--file", type=Path, help="Firmware file. Defaults to firmware.bin.")
    parser.add_argument("--address", type=lambda value: int(value, 0), default=APP_ADDRESS)
    parser.add_argument("--flash", action="store_true", help="Actually run the flashing command. Default is dry-run.")
    parser.add_argument("--port", default="SWD", help="STM32_Programmer_CLI port.")
    parser.add_argument("--device", default="STM32F103C8", help="J-Link device name. TODO: confirm physical MCU.")
    parser.add_argument("--speed", default="4000", help="J-Link SWD speed.")
    parser.add_argument("--interface", default="interface/stlink.cfg", help="OpenOCD interface config.")
    parser.add_argument("--target", default="target/stm32f1x.cfg", help="OpenOCD target config.")
    args = parser.parse_args()

    image = (args.file or default_file(args.config, "bin")).resolve()
    if not image.exists():
        raise FileNotFoundError(f"firmware image not found: {image}")

    validate_address(image, args.address)
    print(f"App flash address: 0x{args.address:08X}")
    print("Full-chip erase is intentionally not implemented by default.")

    if args.method == "stlink":
        flash_stlink(args, image)
    elif args.method == "openocd":
        flash_openocd(args, image)
    else:
        flash_jlink(args, image)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}")
        raise SystemExit(1) from exc
    except subprocess.CalledProcessError as exc:
        print(f"error: command failed with exit code {exc.returncode}")
        raise SystemExit(exc.returncode) from exc
