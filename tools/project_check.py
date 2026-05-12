#!/usr/bin/env python3
"""
Project consistency checks for the clean-room BMS rewrite branch.

This script intentionally no longer validates the retired Keil application
layer under 103 + 309/Project/Source. The active implementation is
firmware_rewrite/.
"""

from __future__ import print_function

import argparse
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REWRITE = ROOT / "firmware_rewrite"
LEGACY_SOURCE = ROOT / "103 + 309" / "Project" / "Source"
KEIL_PROJECT = ROOT / "103 + 309" / "Project" / "Users" / "CommomSH367309_16series_103RCT6_C.uvprojx"

REQUIRED_FILES = [
    ROOT / "README.md",
    ROOT / "PROJECT_REWRITE_REQUIREMENTS_2026-05-12.md",
    ROOT / "FIRMWARE_REWRITE_REPLACEMENT_REPORT_2026-05-12.md",
    KEIL_PROJECT,
    REWRITE / "README.md",
    REWRITE / "CMakeLists.txt",
    REWRITE / "include" / "bms_app.h",
    REWRITE / "include" / "bms_firmware.h",
    REWRITE / "src" / "bms_app.c",
    REWRITE / "src" / "bms_comm.c",
    REWRITE / "src" / "bms_firmware.c",
    REWRITE / "src" / "bms_power_can.c",
    REWRITE / "src" / "bms_protection.c",
    REWRITE / "src" / "bms_soc.c",
    REWRITE / "src" / "bms_storage.c",
    REWRITE / "src" / "bms_ui_iap.c",
    REWRITE / "tests" / "test_rewrite_core.c",
    REWRITE / "ports" / "stm32f1_spl" / "README.md",
    REWRITE / "ports" / "stm32f1_spl" / "bms_main_stm32f1_spl.c",
    REWRITE / "ports" / "stm32f1_spl" / "bms_port_stm32f1_spl.c",
    REWRITE / "ports" / "stm32f1_spl" / "bms_port_stm32f1_spl.h",
    ROOT / "tools" / "run_rewrite_host_tests.py",
    ROOT / "tools" / "soc_flash_app_safe.ps1",
]

REQUIRED_HEADER_TOKENS = [
    "BMS_FLASH_IAP_START 0x08000000ul",
    "BMS_FLASH_APP_START 0x08004800ul",
    "BMS_FLASH_STORAGE_START 0x0801C000ul",
    "BMS_FLASH_SOC_SLOT_A 0x0801E000ul",
    "BMS_FLASH_SOC_SLOT_B 0x0801E800ul",
    "BMS_ADDR_SET_ONCE_SOC 0x1005u",
    "BMS_ADDR_SOC_TABLE_START 0x2200u",
    "BMS_ADDR_SOC_PARAM_START 0x2318u",
    "BMS_ADDR_IAP_CONNECT 0xFFFDu",
    "BMS_IAP_REQUEST_VALUE 0x00ABu",
    "BMS_RO_D000_WORDS 63u",
    "BMS_FAULT_CELL_OVP",
    "bms_platform_ops_t",
]

REQUIRED_SOC_TOKENS = [
    "BMS_SOC_FULL_FAST_MS 5000u",
    "BMS_SOC_FULL_NORMAL_MS 15000u",
    "BMS_SOC_SAG_HOLD_MS 30000u",
    "BMS_SOC_REST_MIN_MS 300000u",
    "BMS_SOC_REST_TARGET_MS 600000u",
    "BMS_SOC_LONG_REST_DOWN_MS 1800000u",
    "BMS_SOC_DISPLAY_NORMAL_MS 5000u",
    "BMS_SOC_DISPLAY_EMPTY_MS 200u",
    "deferred_ocv_target",
    "step_soc_down",
    "step_soc_up",
]

REQUIRED_CAN_TOKENS = [
    "BMS_CAN_NO_ACK_INACTIVE_LIMIT 6u",
    "rtc_idle_with_can_s",
    "rtc_idle_without_can_s",
    "pending_probe_frames = BMS_CAN_RTC_PROBE_FRAMES",
]

REQUIRED_TEST_TOKENS = [
    "test_full_reaches_100",
    "test_low_voltage_reaches_0",
    "test_rest_ocv_no_jump_over_one",
    "test_sag_hold_blocks_wide_low_table",
    "test_can_idle_strategy",
    "test_storage_dual_slot",
    "test_protection_and_outputs",
    "test_ui_key_and_mcu_wake",
    "test_iap_and_rtc_outputs",
    "test_firmware_run_once_entry",
]

REQUIRED_FIRMWARE_TOKENS = [
    "bms_firmware_run_once",
    "bms_app_process_sample",
    "bms_app_apply_outputs",
]

REQUIRED_PORT_TOKENS = [
    "BMS_WEAK",
    "bms_stm32f1_board_read_sample",
    "bms_stm32f1_platform_ops",
    "read_sample",
    "enter_rtc_stop",
    "request_iap_reset",
    "bms_stm32f1_wait_tick",
]

REQUIRED_KEIL_TOKENS = [
    "IROM(0x08004800,0x0001B800)",
    "<TextAddressRange>0x08004800</TextAddressRange>",
    "PROJECT_CFG_BUILD_PROFILE=0",
    "firmware_rewrite\\src\\bms_app.c",
    "firmware_rewrite\\src\\bms_soc.c",
    "firmware_rewrite\\ports\\stm32f1_spl\\bms_main_stm32f1_spl.c",
]

REQUIRED_PROTECTION_TOKENS = [
    "BMS_FAULT_CELL_OVP",
    "BMS_FAULT_CELL_UVP",
    "charge_mos_on",
    "discharge_mos_on",
]

REQUIRED_UI_TOKENS = [
    "BMS_UI_DISPLAY_HOLD_MS 5000u",
    "BMS_UI_LONG_PRESS_MS 3000u",
    "bms_iap_request",
]


class Reporter(object):
    def __init__(self, verbose):
        self.verbose = verbose
        self.errors = []
        self.warnings = []
        self.ok_count = 0

    def ok(self, message):
        self.ok_count += 1
        if self.verbose:
            print("[OK]   " + message)

    def warn(self, message):
        self.warnings.append(message)
        print("[WARN] " + message)

    def fail(self, message):
        self.errors.append(message)
        print("[FAIL] " + message)

    def summary(self):
        print("")
        print("Project check summary:")
        print("  OK:       {0}".format(self.ok_count))
        print("  Warnings: {0}".format(len(self.warnings)))
        print("  Errors:   {0}".format(len(self.errors)))
        return 1 if self.errors else 0


def read_text(path):
    return path.read_text(encoding="utf-8", errors="replace")


def check_required_files(reporter):
    for path in REQUIRED_FILES:
        if path.exists():
            reporter.ok("required file exists: {0}".format(path.relative_to(ROOT)))
        else:
            reporter.fail("required file missing: {0}".format(path.relative_to(ROOT)))


def check_tokens(reporter, path, tokens):
    if not path.exists():
        return
    text = read_text(path)
    for token in tokens:
        if token in text:
            reporter.ok("{0} contains {1}".format(path.relative_to(ROOT), token))
        else:
            reporter.fail("{0} missing {1}".format(path.relative_to(ROOT), token))


def check_legacy_source_retired(reporter):
    if not LEGACY_SOURCE.exists():
        reporter.ok("legacy application Source directory has been removed")
        return

    legacy_code = []
    for suffix in ("*.c", "*.h"):
        legacy_code.extend(LEGACY_SOURCE.rglob(suffix))

    legacy_code = [path for path in legacy_code if path.name != "README.md"]
    if legacy_code:
        reporter.fail("legacy application Source still contains C/H files: {0}".format(len(legacy_code)))
    else:
        reporter.ok("legacy application Source contains no retired C/H code")


def check_safe_flash_script(reporter):
    path = ROOT / "tools" / "soc_flash_app_safe.ps1"
    if not path.exists():
        return
    text = read_text(path)
    if "0x08004800" in text and "0x08000000" in text:
        reporter.ok("safe flash script preserves App/IAP address checks")
    else:
        reporter.fail("safe flash script must mention 0x08004800 and 0x08000000")
    if "dry" in text.lower():
        reporter.ok("safe flash script keeps dry-run behavior")
    else:
        reporter.warn("safe flash script should keep dry-run behavior visible")


def check_readme_index(reporter):
    text = read_text(ROOT / "README.md") if (ROOT / "README.md").exists() else ""
    if (
        "PROJECT_REWRITE_REQUIREMENTS_2026-05-12.md" in text
        and "FIRMWARE_REWRITE_REPLACEMENT_REPORT_2026-05-12.md" in text
        and "firmware_rewrite" in text
    ):
        reporter.ok("README indexes the clean-room rewrite")
    else:
        reporter.fail("README must index the clean-room rewrite docs")


def check_no_legacy_truth_source(reporter):
    req = ROOT / "PROJECT_REWRITE_REQUIREMENTS_2026-05-12.md"
    if not req.exists():
        return
    text = read_text(req)
    patterns = [
        "不以旧 `main.c`",
        "firmware_rewrite/",
        "已删除 `103 + 309/Project/Source` 下旧应用层 C/H 源码",
    ]
    for pattern in patterns:
        if pattern in text:
            reporter.ok("rewrite requirements document states clean-room boundary")
        else:
            reporter.fail("rewrite requirements document missing clean-room boundary: {0}".format(pattern))


def check_script_uses_warnings(reporter):
    script = ROOT / "tools" / "run_rewrite_host_tests.py"
    if not script.exists():
        return
    text = read_text(script)
    required_flags = ["-std=c99", "-Wall", "-Wextra", "-Werror"]
    for flag in required_flags:
        if flag in text:
            reporter.ok("rewrite host build uses {0}".format(flag))
        else:
            reporter.fail("rewrite host build missing {0}".format(flag))


def check_keil_project(reporter):
    if not KEIL_PROJECT.exists():
        return
    text = read_text(KEIL_PROJECT)
    for token in REQUIRED_KEIL_TOKENS:
        if token in text:
            reporter.ok("Keil project contains {0}".format(token))
        else:
            reporter.fail("Keil project missing {0}".format(token))

    legacy_patterns = [
        r"\.\.\\Source\\",
        "SocEnhance.c",
        "Can_HDX.c",
        "LedBar.c",
        "rtc_sleep.c",
    ]
    for pattern in legacy_patterns:
        if re.search(pattern, text):
            reporter.fail("Keil project still references retired application path/token: {0}".format(pattern))
        else:
            reporter.ok("Keil project has no retired application token: {0}".format(pattern))


def main(argv):
    parser = argparse.ArgumentParser(description="Check clean-room BMS rewrite consistency.")
    parser.add_argument("-q", "--quiet", action="store_true", help="Only print warnings, errors, and summary.")
    args = parser.parse_args(argv)

    reporter = Reporter(verbose=not args.quiet)
    print("Project check: {0}".format(ROOT))

    check_required_files(reporter)
    check_tokens(reporter, REWRITE / "include" / "bms_app.h", REQUIRED_HEADER_TOKENS)
    check_tokens(reporter, REWRITE / "src" / "bms_soc.c", REQUIRED_SOC_TOKENS)
    check_tokens(reporter, REWRITE / "src" / "bms_firmware.c", REQUIRED_FIRMWARE_TOKENS)
    check_tokens(reporter, REWRITE / "src" / "bms_power_can.c", REQUIRED_CAN_TOKENS)
    check_tokens(reporter, REWRITE / "src" / "bms_protection.c", REQUIRED_PROTECTION_TOKENS)
    check_tokens(reporter, REWRITE / "src" / "bms_ui_iap.c", REQUIRED_UI_TOKENS)
    check_tokens(reporter, REWRITE / "ports" / "stm32f1_spl" / "bms_port_stm32f1_spl.c", REQUIRED_PORT_TOKENS)
    check_tokens(reporter, REWRITE / "tests" / "test_rewrite_core.c", REQUIRED_TEST_TOKENS)
    check_legacy_source_retired(reporter)
    check_safe_flash_script(reporter)
    check_readme_index(reporter)
    check_no_legacy_truth_source(reporter)
    check_script_uses_warnings(reporter)
    check_keil_project(reporter)

    return reporter.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
