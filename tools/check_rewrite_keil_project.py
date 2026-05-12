#!/usr/bin/env python3
"""Validate the Keil project wiring for the clean-room firmware rewrite."""

from __future__ import print_function

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "103 + 309" / "Project" / "Users" / "CommomSH367309_16series_103RCT6_C.uvprojx"
USERS_DIR = PROJECT.parent

EXPECTED_TARGETS = ("FD_Release", "FD_Debug")
EXPECTED_FILES = (
    r"..\..\..\firmware_rewrite\src\bms_app.c",
    r"..\..\..\firmware_rewrite\src\bms_comm.c",
    r"..\..\..\firmware_rewrite\src\bms_firmware.c",
    r"..\..\..\firmware_rewrite\src\bms_power_can.c",
    r"..\..\..\firmware_rewrite\src\bms_protection.c",
    r"..\..\..\firmware_rewrite\src\bms_soc.c",
    r"..\..\..\firmware_rewrite\src\bms_storage.c",
    r"..\..\..\firmware_rewrite\src\bms_ui_iap.c",
    r"..\..\..\firmware_rewrite\ports\stm32f1_spl\bms_main_stm32f1_spl.c",
    r"..\..\..\firmware_rewrite\ports\stm32f1_spl\bms_port_stm32f1_spl.c",
    r"..\..\..\firmware_rewrite\ports\stm32f1_spl\bms_board_stm32f1_spl.c",
    r"..\..\..\firmware_rewrite\ports\stm32f1_spl\bms_it_stm32f1_spl.c",
    r"..\..\..\firmware_rewrite\ports\stm32f1_spl\bms_system_stm32f1_spl.c",
)
RETIRED_TOKENS = (
    r"..\Source\\",
    "main.h",
    "stm32f10x_it.c",
    "system_stm32f10x.c",
    "SocEnhance.c",
    "Can_HDX.c",
    "LedBar.c",
    "rtc_sleep.c",
)


def rel_to_abs(path_text):
    return (USERS_DIR / Path(path_text.replace("\\", os.sep))).resolve()


def text_of(node, path, default=""):
    found = node.find(path)
    if found is None or found.text is None:
        return default
    return found.text


def parse_project(path):
    tree = ElementTree.parse(str(path))
    return tree.getroot()


def validate_project(path):
    errors = []
    root = parse_project(path)
    text = path.read_text(encoding="utf-8", errors="replace")

    for token in RETIRED_TOKENS:
        if token in text:
            errors.append("Keil project still references retired token: {0}".format(token))

    targets = {}
    for target in root.findall("./Targets/Target"):
        name = text_of(target, "TargetName")
        files = [text_of(file_node, "FilePath") for file_node in target.findall("./Groups/Group/Files/File")]
        cpu = text_of(target, "./TargetOption/TargetCommonOption/Cpu")
        defines = " ".join(node.text or "" for node in target.findall(".//Define"))
        text_addresses = [node.text or "" for node in target.findall(".//TextAddressRange")]
        targets[name] = {
            "files": files,
            "cpu": cpu,
            "defines": defines,
            "text_addresses": text_addresses,
        }

    for target_name in EXPECTED_TARGETS:
        info = targets.get(target_name)
        if info is None:
            errors.append("missing target: {0}".format(target_name))
            continue

        if "IROM(0x08004800,0x0001B800)" not in info["cpu"]:
            errors.append("{0}: IROM is not 0x08004800/0x0001B800".format(target_name))
        if "0x08004800" not in info["text_addresses"]:
            errors.append("{0}: TextAddressRange is not 0x08004800".format(target_name))
        if target_name == "FD_Release" and "PROJECT_CFG_BUILD_PROFILE=0" not in info["defines"]:
            errors.append("{0}: release build profile must be 0".format(target_name))

        for expected in EXPECTED_FILES:
            if expected not in info["files"]:
                errors.append("{0}: missing file {1}".format(target_name, expected))
                continue
            if not rel_to_abs(expected).exists():
                errors.append("{0}: referenced file does not exist: {1}".format(target_name, expected))

    return targets, errors


def find_uv4(explicit):
    if explicit:
        return explicit
    env = os.environ.get("UV4") or os.environ.get("UV4_EXE")
    if env:
        return env
    for candidate in (
        r"C:\Keil_v5\UV4\UV4.exe",
        r"C:\Keil\UV4\UV4.exe",
        r"C:\Keil_v5\UV4\UV4.com",
        r"C:\Keil\UV4\UV4.com",
    ):
        if Path(candidate).exists():
            return candidate
    return shutil.which("UV4.exe") or shutil.which("UV4.com")


def run_keil_build(project, target, uv4, log):
    cmd = [uv4, "-b", str(project), "-t", target, "-o", str(log)]
    print("Keil build command:")
    print("  " + " ".join(cmd))
    completed = subprocess.run(cmd)
    return completed.returncode


def main(argv):
    parser = argparse.ArgumentParser(description="Check clean-room rewrite Keil project wiring.")
    parser.add_argument("--project", default=str(PROJECT), help="Keil .uvprojx path.")
    parser.add_argument("--target", default="FD_Release", help="Target used when --build is set.")
    parser.add_argument("--uv4", default="", help="UV4.exe/UV4.com path.")
    parser.add_argument("--build", action="store_true", help="Run Keil command-line build after validation.")
    parser.add_argument("--log", default=str(ROOT / "build" / "keil_rewrite_build.log"), help="Keil build log path.")
    args = parser.parse_args(argv)

    project = Path(args.project).resolve()
    if not project.exists():
        print("Keil project missing: {0}".format(project))
        return 1

    targets, errors = validate_project(project)
    print("Keil project: {0}".format(project))
    print("Targets: {0}".format(", ".join(name for name in targets if name)))

    if errors:
        print("")
        print("Validation errors:")
        for error in errors:
            print("  - " + error)
        return 1

    print("Validation: OK")

    if not args.build:
        print("Build: skipped. Use --build on Windows with Keil MDK installed.")
        return 0

    uv4 = find_uv4(args.uv4)
    if not uv4:
        print("Build: blocked. UV4.exe/UV4.com not found.")
        return 2

    log = Path(args.log).resolve()
    log.parent.mkdir(parents=True, exist_ok=True)
    return run_keil_build(project, args.target, uv4, log)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
