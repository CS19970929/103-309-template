#!/usr/bin/env python3
"""
Lightweight project consistency checks for the Keil firmware project.

The script intentionally avoids Keil/ARMCC dependencies. It is meant to catch
release/debug profile mistakes before commit, push, or manual release builds.
"""

from __future__ import print_function

import argparse
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "103 + 309" / "Project" / "Users" / "CommomSH367309_16series_103RCT6_C.uvprojx"
PROJECT_CONFIG = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "Project_Config.h"
PROJECT_CONFIG_ENCODING = "gbk"
BUILD_GUARD = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "Project_BuildGuard.h"
CONF_H = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "conf.h"
CONF_C = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "conf.c"
MAIN_C = ROOT / "103 + 309" / "Project" / "Source" / "main.c"
MAIN_H = ROOT / "103 + 309" / "Project" / "Source" / "main.h"
MOS_STARTUP_C = ROOT / "103 + 309" / "Project" / "Source" / "MosStartup.c"
MOS_STARTUP_H = ROOT / "103 + 309" / "Project" / "Source" / "MosStartup.h"
APP_INIT_C = ROOT / "103 + 309" / "Project" / "Source" / "AppInit.c"
APP_INIT_H = ROOT / "103 + 309" / "Project" / "Source" / "AppInit.h"
RELEASE_MAP = ROOT / "103 + 309" / "Project" / "Users" / "Listings" / "FD_Release.map"
ELOG_CFG_H = ROOT / "103 + 309" / "Project" / "Source" / "easylogger" / "inc" / "elog_cfg.h"
ADC_H = ROOT / "103 + 309" / "Project" / "Source" / "ADC.h"
DATADEAL_C = ROOT / "103 + 309" / "Project" / "Source" / "DataDeal.c"
SOC_C = ROOT / "103 + 309" / "Project" / "Source" / "SOC.c"
SOC_ENHANCE_C = ROOT / "103 + 309" / "Project" / "Source" / "SocEnhance.c"
SCI_UPPER_C = ROOT / "103 + 309" / "Project" / "Source" / "Sci_Upper.c"
SCI_UPPER_H = ROOT / "103 + 309" / "Project" / "Source" / "Sci_Upper.h"
SLEEPDEAL_C = ROOT / "103 + 309" / "Project" / "Source" / "SleepDeal.c"
SLEEPDEAL_H = ROOT / "103 + 309" / "Project" / "Source" / "SleepDeal.h"
RTC_C = ROOT / "103 + 309" / "Project" / "Source" / "RTC.c"
RTC_SLEEP_C = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep.c"
RTC_SLEEP_H = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep.h"
CAN_HDX_C = ROOT / "103 + 309" / "Project" / "Source" / "Can_HDX.c"
CAN_HDX_H = ROOT / "103 + 309" / "Project" / "Source" / "Can_HDX.h"
LEDBAR_C = ROOT / "103 + 309" / "Project" / "Source" / "LedBar.c"
LOGRECORD_C = ROOT / "103 + 309" / "Project" / "Source" / "LogRecord.c"
FAULT_SNAPSHOT_H = ROOT / "103 + 309" / "Project" / "Source" / "FaultSnapshot.h"
STM32F10X_IT_C = ROOT / "103 + 309" / "Project" / "STM32F10x_StdPeriph_Lib_V3.5.0" / "drivers" / "stm32f10x_it.c"
GITIGNORE = ROOT / ".gitignore"
PRE_COMMIT = ROOT / ".githooks" / "pre-commit"
PRE_PUSH = ROOT / ".githooks" / "pre-push"
PROJECT_CONFIG_WIZARD_MARKER = "\u9879\u76ee\u53ef\u89c6\u5316\u914d\u7f6e"
FLOW_DOC = ROOT / "\u9879\u76ee\u8fd0\u884c\u6d41\u7a0b\u4e0e\u65f6\u5e8f\u6e90\u7801\u68b3\u7406_2026-05-16.md"
COMM_ADDRESS_INDEX = ROOT / "COMMUNICATION_ADDRESS_INDEX.md"
CAN_RUNTIME_REFACTOR = ROOT / "CAN_RUNTIME_REFACTOR.md"
CAN_MODULE_SIMPLIFY = ROOT / "CAN_MODULE_SIMPLIFY_2026-05-15.md"
RTC_SLEEP_OPT_DOC = ROOT / "RTC_STANDBY_SLEEP_OPTIMIZATION_2026-05-22.md"
APP_ARCH_REFACTOR_DOC = ROOT / "PROJECT_ARCH_REFACTOR_2026-05-22.md"
HEAT_COOL_REMOVE_DOC = ROOT / "HEAT_COOL_IODRIVERS_REMOVAL_2026-05-22.md"
UNUSED_SYMBOL_CLEANUP_DOC = ROOT / "UNUSED_SYMBOL_CLEANUP_2026-05-22.md"
UTF8_TEXT_SUFFIXES = {
    ".c",
    ".h",
    ".s",
    ".S",
    ".md",
    ".py",
    ".ps1",
    ".uvprojx",
    ".uvoptx",
    ".sct",
    ".txt",
}


COMMON_DEFINES = {"STM32F10X_MD", "USE_STDPERIPH_DRIVER"}
APP_FLASH_BASE = 0x08004800
RELEASE_ROM_WARN_BYTES = 96 * 1024
RELEASE_RAM_WARN_BYTES = 14 * 1024
REMOVED_LEGACY_SOURCE_FILES = [
    ROOT / "103 + 309" / "Project" / "Source" / "ChargerLoadFunc.c",
    ROOT / "103 + 309" / "Project" / "Source" / "ChargerLoadFunc.h",
    ROOT / "103 + 309" / "Project" / "Source" / "IO_Control.c",
    ROOT / "103 + 309" / "Project" / "Source" / "IO_Control.h",
    ROOT / "103 + 309" / "Project" / "Source" / "Heat_Cool.c",
    ROOT / "103 + 309" / "Project" / "Source" / "Heat_Cool.h",
    ROOT / "103 + 309" / "Project" / "Source" / "IODrivers.c",
    ROOT / "103 + 309" / "Project" / "Source" / "IODrivers.h",
]
REMOVED_LEGACY_PROJECT_PATHS = {
    "../Source/ChargerLoadFunc.c",
    "../Source/IO_Control.c",
    "../Source/Heat_Cool.c",
    "../Source/IODrivers.c",
}
REMOVED_LEGACY_TOKENS = [
    "ChargerLoadFunc",
    "IO_Control",
    "ChargerLoad_Func",
    "Init_ChargerLoad_Det",
    "App_ChargerLoad_Det",
    "App_DI1_Switch",
    "Heat_Cool",
    "HeatCool",
    "Heat_Cool_Element",
    "InitHeat_Cool",
    "App_Heat_Cool_Ctrl",
    "IODrivers",
    "__FUNC__HEAT__",
    "PROJECT_CFG_HEAT_ENABLE",
    "CHG_LOWTEMP_PARAM",
    "HEAT_OPEN_CURR",
    "MCUO_RELAY_HEAT",
    "MCUO_RELAY_COOL",
    "HEAT_OPEN",
    "COOL_OPEN",
    "LOW_POWER_RTC_BLOCK_HEAT",
    "ERROR_HEAT",
    "ERROR_COOL",
    "ERROR_REMOVE_HEAT",
    "ERROR_REMOVE_COOL",
    "ERROR_STATUS_HEAT",
    "ERROR_STATUS_COOL",
    "u8ErrFlag_Heat",
    "u8ErrFlag_Cool",
    "b1Status_Heat",
    "b1Status_Cool",
    "b1Status_HeatCloseIO",
    "b1OnOFF_Heat",
    "b1OnOFF_Cool",
    "b1StartUpFlag_Heat",
    "b1StartUpFlag_Cool",
    "SystemMonitorResetData_EEPROM",
    "RS485_CMD_ADDR_SWITCH_ON",
    "RS485_CMD_ADDR_SWITCH_OFF",
    "Sci_WrReg_0x06_SwitchON",
    "Sci_WrReg_0x06_SwitchOFF",
    "InitE2PROM_i2c",
    "App_E2promDeal",
    "EEPROM_test",
    "EEPROM_ADDR_PASS",
    "EEPROM_ADDR_SLEEP",
    "EEPROM_ADDR_FLASHUPDATE",
    "EEPROM_VALUE_SLEEP",
    "EEPROM_VALUE_FLASHUPDATE",
    "EEPROM_ADDR_SWITCH_ONOFF",
    "E2P_ADDR_E2POS_PROTECT",
    "E2P_ADDR_E2POS_RTC",
    "E2P_ADDR_E2POS_OTHER_ELEMENT1",
    "E2P_ADDR_E2POS_RESERVED_RW_PARAM",
    "E2P_ADDR_E2POS_ENHANCE_SOC",
    "E2P_ADDR_E2POS_SERIAL_NUM",
    "E2P_ADDR_E2POS_HAEDWARE_VER",
    "E2P_ADDR_E2POS_SOFTWARE_VER",
    "E2P_ADDR_START_OTHER_ELEMENT1",
    "E2P_ADDR_START_EVENT_RECORD",
    "E2P_ADDR_E2POS_EVENT_POINT",
    "E2P_ADDR_SH367309_VALUE",
    "DataLoad_CellVolt_Test",
    "test_Autocurrent_cycle",
    "gu8_DriverStartUpFlag",
    "aaaaaa1",
    "aaa11",
    "AFE_IDLE_Old",
    "AFE_GetData",
    "OddEven_Check",
    "Usart_9bitOddEvenData_Frame",
    "FlashTest",
    "IOstatus_TestMode",
    "InitWakeUp_TestMode",
    "IORecover_TestMode",
    "Sys_SleepOnExitMode",
    "TwiWrite_old",
    "TwiRead_old",
]
RELEASE_FORBIDDEN_DEFINES = {
    "_DEBUG_",
    "_DEBUG_CODE",
    "FLASH64K_APP_QUICK_TEST_ENABLE",
    "FLASH64K_APP_USE_TEST_ENABLE",
    "ELOG_OUTPUT_ENABLE",
}
RELEASE_SAFE_DEFAULTS = {
    "PROJECT_CFG_BUILD_PROFILE": "0",
    "PROJECT_CFG_WDOG_ENABLE": "1",
    "PROJECT_CFG_DEBUG_CODE_ENABLE": "0",
    "PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE": "0",
    "PROJECT_CFG_DEBUG_WATCH_ENABLE": "0",
    "PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE": "0",
    "PROJECT_CFG_FLASH64K_USE_TEST_ENABLE": "0",
    "PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE": "0",
    "PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST": "0",
    "PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON": "0",
    "PROJECT_CFG_SOC_TEST_MODE_ENABLE": "0",
    "PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY": "0",
    "PROJECT_CFG_FACTORY_AGING_ENABLE": "1",
    "PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS": "259200",
}
GUARD_REQUIRED_TOKENS = [
    "PROJECT_CFG_WDOG_ENABLE",
    "PROJECT_CFG_DEBUG_CODE_ENABLE",
    "PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE",
    "PROJECT_CFG_DEBUG_WATCH_ENABLE",
    "PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE",
    "PROJECT_CFG_FLASH64K_USE_TEST_ENABLE",
    "PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST",
    "PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON",
    "PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY",
    "PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS",
    "PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS",
    "PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT",
    "PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV",
    "PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV",
    "PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV",
    "PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV",
    "PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV",
    "PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV",
    "PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS",
    "PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV",
    "PROJECT_CFG_SOC_REST_OCV_SECONDS",
    "PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS",
    "PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS",
    "PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS",
    "PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT",
    "PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA",
    "PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV",
    "PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT",
    "PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT",
    "PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS",
    "PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS",
    "PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS",
    "PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV",
    "PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV",
    "PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT",
    "PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT",
    "PROJECT_CFG_FACTORY_AGING_ENABLE",
    "PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS",
    "_DEBUG_",
    "_DEBUG_CODE",
    "FLASH64K_APP_QUICK_TEST_ENABLE",
    "FLASH64K_APP_USE_TEST_ENABLE",
    "ELOG_OUTPUT_ENABLE",
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
    if path == PROJECT_CONFIG:
        return path.read_text(encoding=PROJECT_CONFIG_ENCODING, errors="replace")
    return path.read_text(encoding="utf-8", errors="replace")


def iter_git_tracked_files():
    try:
        output = subprocess.check_output(["git", "ls-files", "-z"], cwd=str(ROOT))
    except (OSError, subprocess.CalledProcessError):
        return []

    result = []
    for item in output.split(b"\0"):
        if not item:
            continue
        result.append(ROOT / item.decode("utf-8", errors="surrogateescape"))
    return result


def define_tokens(text):
    result = set()
    for item in re.split(r"[,;\s]+", text or ""):
        item = item.strip()
        if item:
            result.add(item)
    return result


def find_define_value(tokens, name):
    exact = name
    prefix = name + "="
    if exact in tokens:
        return ""
    for token in tokens:
        if token.startswith(prefix):
            return token[len(prefix):]
    return None


def parse_project_targets(project_path):
    tree = ET.parse(str(project_path))
    root = tree.getroot()
    targets = {}

    for target in root.findall("./Targets/Target"):
        name = (target.findtext("TargetName") or "").strip()
        if not name:
            continue
        c_define_text = target.findtext("./TargetOption/TargetArmAds/Cads/VariousControls/Define") or ""
        output_name = target.findtext("./TargetOption/TargetCommonOption/OutputName") or ""
        output_dir = target.findtext("./TargetOption/TargetCommonOption/OutputDirectory") or ""
        files = set()
        for file_path in target.findall("./Groups/Group/Files/File/FilePath"):
            if file_path.text:
                files.add(file_path.text.replace("\\", "/"))
        targets[name] = {
            "defines": define_tokens(c_define_text),
            "output_name": output_name.strip(),
            "output_dir": output_dir.strip(),
            "files": files,
        }

    return targets


def parse_header_defines(header_path):
    defines = {}
    define_re = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s*(.*?)(?:\s*/[/*].*)?$")

    for line in read_text(header_path).splitlines():
        match = define_re.match(line)
        if not match:
            continue
        name, value = match.groups()
        defines[name] = value.strip()

    return defines


def check_required_files(reporter):
    for path in [PROJECT, PROJECT_CONFIG, BUILD_GUARD, CONF_H, MAIN_C, MAIN_H, MOS_STARTUP_C, MOS_STARTUP_H, APP_INIT_C, APP_INIT_H, ELOG_CFG_H, GITIGNORE, PRE_COMMIT, PRE_PUSH]:
        if path.exists():
            reporter.ok("required file exists: {0}".format(path.relative_to(ROOT)))
        else:
            reporter.fail("required file missing: {0}".format(path.relative_to(ROOT)))


def check_utf8_text_files(reporter):
    invalid = []
    bom = []

    for path in iter_git_tracked_files():
        if not path.exists():
            continue
        if path == PROJECT_CONFIG:
            continue
        if path.suffix not in UTF8_TEXT_SUFFIXES:
            continue
        data = path.read_bytes()
        if data.startswith(b"\xef\xbb\xbf"):
            bom.append(str(path.relative_to(ROOT)))
            continue
        try:
            data.decode("utf-8")
        except UnicodeDecodeError as exc:
            invalid.append("{0}: {1}".format(path.relative_to(ROOT), exc))

    if invalid:
        reporter.fail("tracked source/text files except Project_Config.h must be UTF-8: {0}".format("; ".join(invalid[:8])))
    else:
        reporter.ok("tracked source/text files except Project_Config.h are valid UTF-8")

    if bom:
        reporter.fail("tracked source/text files should use UTF-8 without BOM: {0}".format("; ".join(bom[:8])))
    else:
        reporter.ok("tracked source/text files do not contain UTF-8 BOM")


def check_project_config_wizard_encoding(reporter):
    if not PROJECT_CONFIG.exists():
        return

    try:
        text = PROJECT_CONFIG.read_bytes().decode(PROJECT_CONFIG_ENCODING)
    except UnicodeDecodeError as exc:
        reporter.fail("Project_Config.h must be saved as GBK/ANSI for Keil Configuration Wizard: {0}".format(exc))
        return

    if PROJECT_CONFIG_WIZARD_MARKER in text:
        reporter.ok("Project_Config.h GBK/ANSI text is readable and keeps Keil Configuration Wizard marker")
    else:
        reporter.fail("Project_Config.h GBK/ANSI text marker is missing or unreadable for Keil Configuration Wizard")


def check_keil_targets(reporter):
    if not PROJECT.exists():
        return

    try:
        targets = parse_project_targets(PROJECT)
    except ET.ParseError as exc:
        reporter.fail("uvprojx XML parse failed: {0}".format(exc))
        return

    release = targets.get("FD_Release")
    debug = targets.get("FD_Debug")

    if release is None:
        reporter.fail("Keil target FD_Release is missing")
    else:
        reporter.ok("Keil target FD_Release found")
        missing = COMMON_DEFINES - release["defines"]
        if missing:
            reporter.fail("FD_Release missing common defines: {0}".format(",".join(sorted(missing))))
        else:
            reporter.ok("FD_Release common defines are present")

        forbidden = RELEASE_FORBIDDEN_DEFINES & release["defines"]
        profile_value = find_define_value(release["defines"], "PROJECT_CFG_BUILD_PROFILE")
        if profile_value not in (None, "0"):
            reporter.fail("FD_Release must not override PROJECT_CFG_BUILD_PROFILE to {0}".format(profile_value))
        else:
            reporter.ok("FD_Release build profile override is release-safe")

        if forbidden:
            reporter.fail("FD_Release contains forbidden defines: {0}".format(",".join(sorted(forbidden))))
        else:
            reporter.ok("FD_Release contains no forbidden debug/test defines")

        if release["output_name"] != "FD_Release":
            reporter.fail("FD_Release OutputName should be FD_Release, got {0}".format(release["output_name"]))
        else:
            reporter.ok("FD_Release output name is isolated")

        if "../Source/conf/Project_BuildGuard.h" not in release["files"]:
            reporter.fail("FD_Release project tree does not include Project_BuildGuard.h")
        else:
            reporter.ok("FD_Release includes Project_BuildGuard.h")

        if "../Source/MosStartup.c" not in release["files"]:
            reporter.fail("FD_Release project tree does not include MosStartup.c")
        else:
            reporter.ok("FD_Release includes MosStartup.c")

        if "../Source/AppInit.c" not in release["files"]:
            reporter.fail("FD_Release project tree does not include AppInit.c")
        else:
            reporter.ok("FD_Release includes AppInit.c")

    if debug is None:
        reporter.fail("Keil target FD_Debug is missing")
    else:
        reporter.ok("Keil target FD_Debug found")
        missing = COMMON_DEFINES - debug["defines"]
        if missing:
            reporter.fail("FD_Debug missing common defines: {0}".format(",".join(sorted(missing))))
        else:
            reporter.ok("FD_Debug common defines are present")

        profile_value = find_define_value(debug["defines"], "PROJECT_CFG_BUILD_PROFILE")
        if profile_value != "1":
            reporter.fail("FD_Debug should define PROJECT_CFG_BUILD_PROFILE=1")
        else:
            reporter.ok("FD_Debug selects debug build profile")

        if "_DEBUG_" not in debug["defines"]:
            reporter.fail("FD_Debug should define _DEBUG_")
        else:
            reporter.ok("FD_Debug defines _DEBUG_")

        watch_value = find_define_value(debug["defines"], "PROJECT_CFG_DEBUG_WATCH_ENABLE")
        if watch_value != "1":
            reporter.fail("FD_Debug should define PROJECT_CFG_DEBUG_WATCH_ENABLE=1 for Keil Watch")
        else:
            reporter.ok("FD_Debug enables Keil SOC Watch")

        if debug["output_name"] != "FD_Debug":
            reporter.fail("FD_Debug OutputName should be FD_Debug, got {0}".format(debug["output_name"]))
        else:
            reporter.ok("FD_Debug output name is isolated")

        if "../Source/conf/Project_BuildGuard.h" not in debug["files"]:
            reporter.fail("FD_Debug project tree does not include Project_BuildGuard.h")
        else:
            reporter.ok("FD_Debug includes Project_BuildGuard.h")

        if "../Source/MosStartup.c" not in debug["files"]:
            reporter.fail("FD_Debug project tree does not include MosStartup.c")
        else:
            reporter.ok("FD_Debug includes MosStartup.c")

        if "../Source/AppInit.c" not in debug["files"]:
            reporter.fail("FD_Debug project tree does not include AppInit.c")
        else:
            reporter.ok("FD_Debug includes AppInit.c")

    if release and debug:
        if release["output_name"] == debug["output_name"]:
            reporter.fail("FD_Release and FD_Debug share the same OutputName")
        else:
            reporter.ok("Keil targets use separate output names")


def check_removed_legacy_modules(reporter):
    existing = [str(path.relative_to(ROOT)) for path in REMOVED_LEGACY_SOURCE_FILES if path.exists()]
    if existing:
        reporter.fail("removed legacy modules should not exist: {0}".format("; ".join(existing)))
    else:
        reporter.ok("removed legacy modules are absent from source tree")

    if PROJECT.exists():
        try:
            targets = parse_project_targets(PROJECT)
        except ET.ParseError:
            targets = {}

        stale_targets = []
        for name, target in sorted(targets.items()):
            stale = REMOVED_LEGACY_PROJECT_PATHS & target["files"]
            if stale:
                stale_targets.append("{0}: {1}".format(name, ",".join(sorted(stale))))
        if stale_targets:
            reporter.fail("Keil project still references removed legacy modules: {0}".format("; ".join(stale_targets)))
        else:
            reporter.ok("Keil project has no removed legacy module entries")

    search_roots = [
        ROOT / "103 + 309" / "Project" / "Source",
        ROOT / "103 + 309" / "Project" / "STM32F10x_StdPeriph_Lib_V3.5.0" / "drivers",
    ]
    stale_refs = []
    removed_paths = set(REMOVED_LEGACY_SOURCE_FILES)
    for base in search_roots:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path in removed_paths or path.suffix not in (".c", ".h"):
                continue
            try:
                text = read_text(path)
            except OSError:
                continue
            for token in REMOVED_LEGACY_TOKENS:
                if token in text:
                    stale_refs.append("{0}: {1}".format(path.relative_to(ROOT), token))
                    break
    if stale_refs:
        reporter.fail("source still references removed legacy modules: {0}".format("; ".join(stale_refs[:8])))
    else:
        reporter.ok("source has no removed legacy module references")

    if HEAT_COOL_REMOVE_DOC.exists():
        doc = read_text(HEAT_COOL_REMOVE_DOC)
        if (
            "0x2320 ~ 0x2337" in doc
            and "reserved[24]" in doc
            and "0x1004" in doc
            and "project_check.py" in doc
        ):
            reporter.ok("Heat_Cool/IODrivers removal document covers protocol and Flash compatibility")
        else:
            reporter.fail("Heat_Cool/IODrivers removal document should cover protocol and Flash compatibility")
    else:
        reporter.fail("Heat_Cool/IODrivers removal document is missing")

    if UNUSED_SYMBOL_CLEANUP_DOC.exists():
        doc = read_text(UNUSED_SYMBOL_CLEANUP_DOC)
        if (
            "0x1100" in doc
            and "0x1101" in doc
            and "reserved" in doc
            and "System_Monitor" in doc
            and "TwiWrite_old" in doc
            and "project_check.py" in doc
        ):
            reporter.ok("unused symbol cleanup document covers protocol, reserved layout, and guard checks")
        else:
            reporter.fail("unused symbol cleanup document should cover protocol, reserved layout, and guard checks")
    else:
        reporter.fail("unused symbol cleanup document is missing")


def check_release_defaults(reporter):
    if not PROJECT_CONFIG.exists():
        return

    defines = parse_header_defines(PROJECT_CONFIG)
    for name, expected in sorted(RELEASE_SAFE_DEFAULTS.items()):
        actual = defines.get(name)
        if actual is None:
            reporter.fail("Project_Config.h missing {0}".format(name))
        elif actual != expected:
            reporter.fail("Project_Config.h {0} should default to {1}, got {2}".format(name, expected, actual))
        else:
            reporter.ok("Project_Config.h {0} default is {1}".format(name, expected))


def check_guard_includes(reporter):
    for path in [CONF_H, ELOG_CFG_H]:
        if not path.exists():
            continue
        text = read_text(path)
        if '#include "Project_BuildGuard.h"' in text:
            reporter.ok("{0} includes Project_BuildGuard.h".format(path.relative_to(ROOT)))
        else:
            reporter.fail("{0} must include Project_BuildGuard.h".format(path.relative_to(ROOT)))


def check_build_guard(reporter):
    if not BUILD_GUARD.exists():
        return

    text = read_text(BUILD_GUARD)
    for token in GUARD_REQUIRED_TOKENS:
        if token in text:
            reporter.ok("Project_BuildGuard.h checks {0}".format(token))
        else:
            reporter.fail("Project_BuildGuard.h does not check {0}".format(token))

    include_guard_pos = text.find("#endif")
    release_check_pos = text.find("#if (PROJECT_CFG_BUILD_PROFILE == PROJECT_BUILD_PROFILE_RELEASE)")
    if include_guard_pos != -1 and release_check_pos != -1 and release_check_pos > include_guard_pos:
        reporter.ok("Project_BuildGuard.h late macro checks are outside the include guard")
    else:
        reporter.fail("Project_BuildGuard.h late macro checks should stay outside the include guard")


def check_release_map(reporter):
    if not RELEASE_MAP.exists():
        reporter.warn("release map is missing; build FD_Release before final release checks")
        return

    text = read_text(RELEASE_MAP)
    load = re.search(r"Load Region LR_IROM1 \(Base: 0x([0-9a-fA-F]+),", text)
    exec_region = re.search(r"Execution Region ER_IROM1 \(Exec base: 0x([0-9a-fA-F]+),", text)
    if load and int(load.group(1), 16) == APP_FLASH_BASE:
        reporter.ok("FD_Release map Load Region base is 0x{0:08X}".format(APP_FLASH_BASE))
    else:
        reporter.fail("FD_Release map Load Region must start at 0x{0:08X}".format(APP_FLASH_BASE))

    if exec_region and int(exec_region.group(1), 16) == APP_FLASH_BASE:
        reporter.ok("FD_Release map Execution Region base is 0x{0:08X}".format(APP_FLASH_BASE))
    else:
        reporter.fail("FD_Release map Execution Region must start at 0x{0:08X}".format(APP_FLASH_BASE))

    rom = re.search(r"Total ROM Size \(Code \+ RO Data \+ RW Data\)\s+(\d+)", text)
    ram = re.search(r"Total RW\s+Size \(RW Data \+ ZI Data\)\s+(\d+)", text)
    if rom:
        rom_size = int(rom.group(1))
        if rom_size > RELEASE_ROM_WARN_BYTES:
            reporter.warn("FD_Release ROM size is {0} bytes, above warning line {1}".format(rom_size, RELEASE_ROM_WARN_BYTES))
        else:
            reporter.ok("FD_Release ROM size is under warning line: {0} bytes".format(rom_size))
    else:
        reporter.warn("FD_Release map Total ROM Size line was not found")

    if ram:
        ram_size = int(ram.group(1))
        if ram_size > RELEASE_RAM_WARN_BYTES:
            reporter.warn("FD_Release RAM size is {0} bytes, above warning line {1}".format(ram_size, RELEASE_RAM_WARN_BYTES))
        else:
            reporter.ok("FD_Release RAM size is under warning line: {0} bytes".format(ram_size))
    else:
        reporter.warn("FD_Release map Total RW Size line was not found")

    tracked_sources = [
        PROJECT_CONFIG,
        BUILD_GUARD,
        CONF_H,
        ROOT / "103 + 309" / "Project" / "Source" / "main.c",
        ROOT / "103 + 309" / "Project" / "Source" / "Flash.c",
        ROOT / "103 + 309" / "Project" / "Source" / "SOC.c",
        ROOT / "103 + 309" / "Project" / "Source" / "SocEnhance.c",
        ROOT / "103 + 309" / "Project" / "Source" / "SH367309_Func.c",
        ROOT / "103 + 309" / "Project" / "Source" / "SH367309_DataDeal.c",
    ]
    newest_source = max((path.stat().st_mtime for path in tracked_sources if path.exists()), default=0)
    if RELEASE_MAP.stat().st_mtime < newest_source:
        reporter.warn("FD_Release map is older than recently changed source/config files; rebuild before trusting size numbers")
    else:
        reporter.ok("FD_Release map timestamp is newer than checked source/config files")

    linked_printf = re.search(r"^\s*\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+\d+\s+printf", text, re.MULTILINE)
    if linked_printf:
        reporter.warn("FD_Release linked image still contains printf library members; keep disabled in production unless diagnostics require it")
    else:
        reporter.ok("FD_Release linked image does not contain printf library members")

    defines = parse_header_defines(PROJECT_CONFIG) if PROJECT_CONFIG.exists() else {}
    disabled_sci_symbols = {
        "PROJECT_CFG_SCI2_ROLE": [
            "g_stCurrentMsgPtr_SCI2",
            "g_stSciPort2",
            "gu16_CommuErrCnt_SCI2",
            "gu8_TxEnable_SCI2",
            "gu8_TxFinishFlag_SCI2",
        ],
        "PROJECT_CFG_SCI3_ROLE": [
            "g_stCurrentMsgPtr_SCI3",
            "g_stSciPort3",
            "gu16_CommuErrCnt_SCI3",
            "gu8_TxEnable_SCI3",
            "gu8_TxFinishFlag_SCI3",
        ],
    }
    for define_name, symbols in sorted(disabled_sci_symbols.items()):
        if defines.get(define_name) != "0":
            continue
        leaked = [symbol for symbol in symbols if symbol in text]
        if leaked:
            reporter.fail("{0}=0 but FD_Release map still contains: {1}".format(define_name, ",".join(leaked)))
        else:
            reporter.ok("{0}=0 removes unused SCI runtime symbols".format(define_name))

    if defines.get("PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE") == "0":
        runtime_soc_table_symbols = [
            "SOC_Table_Set",
            "SOC_Table_Default",
            "SOC_Table_CanSet",
        ]
        leaked = [symbol for symbol in runtime_soc_table_symbols if symbol in text]
        if leaked:
            reporter.fail("PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE=0 but FD_Release map still contains: {0}".format(",".join(leaked)))
        else:
            reporter.ok("PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE=0 removes runtime SOC table symbols")

        chemistry = defines.get("PROJECT_CFG_BAT_CHEMISTRY")
        if chemistry == "0":
            fixed_table_leaks = ["SOC_Table_LiFePO", "SocTable_LiFePO2"]
        elif chemistry == "1":
            fixed_table_leaks = ["SocTable_TernaryLi", "SocTable_LiFePO2"]
        else:
            fixed_table_leaks = []
        leaked = [symbol for symbol in fixed_table_leaks if symbol in text]
        if leaked:
            reporter.fail("fixed compile-time SOC table build still contains unused table symbols: {0}".format(",".join(leaked)))
        elif fixed_table_leaks:
            reporter.ok("fixed compile-time SOC table build keeps only the selected chemistry table")


def check_gitignore(reporter):
    if not GITIGNORE.exists():
        return

    text = read_text(GITIGNORE)
    required_patterns = [".DS_Store", "*.uvguix.*", "*.uvoptx", "*.dep", "Listings/"]
    for pattern in required_patterns:
        if pattern in text:
            reporter.ok(".gitignore contains {0}".format(pattern))
        else:
            reporter.warn(".gitignore should ignore {0}".format(pattern))


def check_hooks(reporter):
    for hook in [PRE_COMMIT, PRE_PUSH]:
        if not hook.exists():
            continue
        text = read_text(hook)
        if "tools/project_check.py" in text:
            reporter.ok("{0} runs tools/project_check.py".format(hook.relative_to(ROOT)))
        else:
            reporter.warn("{0} does not run tools/project_check.py".format(hook.relative_to(ROOT)))


def check_soc_parameter_side_effects(reporter):
    if not SCI_UPPER_C.exists():
        return

    text = read_text(SCI_UPPER_C)
    start = text.find("static void Sci_ApplyOtherElementSideEffects")
    end = text.find("\n}\n\nvoid Sci_Deal_WrRegs_0x10", start)
    if start == -1 or end == -1:
        reporter.fail("Sci_ApplyOtherElementSideEffects is missing or moved unexpectedly")
        return

    body = text[start:end]
    if (
        "Sci_RangeOverlaps(offset, count, 12, 1)" in body
        and "reload_soc = 1U;" in body
        and "Sci_RangeOverlaps(offset, count, 24, 4)" in body
        and "SOC_Enhance_Element.u16_RefreshData_Flag = 2;" in body
    ):
        reporter.ok("SOC table select and capacity parameter writes refresh SOC runtime state")
    else:
        reporter.fail("SOC table select/capacity side effects must refresh SOC runtime state")


def check_soc_current_and_typec_policy(reporter):
    required_files = [ADC_H, DATADEAL_C, SOC_C, SOC_ENHANCE_C]
    if any(not path.exists() for path in required_files):
        return

    adc_h = read_text(ADC_H)
    datadeal_c = read_text(DATADEAL_C)
    soc_c = read_text(SOC_C)
    soc_enhance_c = read_text(SOC_ENHANCE_C)

    if (
        "TYPEC_OUT_VOLTAGE_MV" in adc_h
        and "TYPEC_DCDC_EFFICIENCY_PERMILLE" in adc_h
        and "g_u16TypeCBatEquivCurrent_A10" in adc_h
        and "SOC_GetTypeCBatEquivCurrentA10" in soc_c
        and "g_u16TypeCOutCurrent_mA" in soc_c
        and "report_idsg + (UINT32)g_u16TypeCOutCurrent_A10" not in soc_c
    ):
        reporter.ok("Type-C SOC path uses battery-side equivalent current")
    else:
        reporter.fail("Type-C output current must be converted before entering SOC")

    if (
        "#define SOC_CURRENT_ACTIVE_A10       ((UINT16)2U)" in soc_enhance_c
        and "#define AFE_CURRENT_OUTPUT_DEADBAND_MA ((UINT32)200U)" in datadeal_c
        and "#define AFE_CURRENT_OUTPUT_DEADBAND_A10 ((UINT16)2U)" in datadeal_c
    ):
        reporter.ok("SOC active current threshold and AFE output deadband are 0.2A")
    else:
        reporter.fail("SOC active current threshold and AFE output deadband must stay at 0.2A")


def check_low_power_cleanup(reporter):
    required_files = [SLEEPDEAL_C, SLEEPDEAL_H, RTC_SLEEP_C, RTC_SLEEP_H, LEDBAR_C, LOGRECORD_C]
    if any(not path.exists() for path in required_files):
        return

    sleepdeal_c = read_text(SLEEPDEAL_C)
    sleepdeal_h = read_text(SLEEPDEAL_H)
    rtc_sleep_c = read_text(RTC_SLEEP_C)
    rtc_sleep_h = read_text(RTC_SLEEP_H)
    ledbar_c = read_text(LEDBAR_C)
    logrecord_c = read_text(LOGRECORD_C)

    removed_tokens = [
        "App_SleepDeal",
        "SleepDeal_SelectMode",
        "SleepDeal_Normal",
        "SleepDeal_Shift",
        "Sleep_Mode",
        "Sleep_Status",
        "SLEEP_HICCUP",
    ]
    combined = "\n".join([sleepdeal_c, sleepdeal_h, rtc_sleep_c, ledbar_c, logrecord_c])
    stale_tokens = [token for token in removed_tokens if token in combined]
    if stale_tokens:
        reporter.fail("low power cleanup still contains stale tokens: {0}".format(",".join(stale_tokens)))
    else:
        reporter.ok("legacy App_SleepDeal/Sleep_Mode state machine is removed from active source")

    if (
        "void LowPower_Request(enum _SLEEP_MODE mode)" in rtc_sleep_c
        and "UINT8 LowPower_IsToSleepPending(void)" in rtc_sleep_c
        and "LowPower_ClearToSleepFlag();" in logrecord_c
        and "LowPower_IsToSleepPending() != 0u" in ledbar_c
        and "void SleepDeal_Continue(UINT8 sleep_mode)" in sleepdeal_c
        and "SleepDeal_Continue(sleep_mode);" in rtc_sleep_c
        and "SleepDeal_Continue((UINT8)DEEP_MODE);" in ledbar_c
        and "UINT8 LowPower_IsToSleepPending(void);" in rtc_sleep_h
    ):
        reporter.ok("low power mode ownership is centralized in LowPower runtime APIs")
    else:
        reporter.fail("low power mode ownership should use LowPower_Request/IsToSleepPending and explicit SleepDeal_Continue(mode)")


def check_fault_snapshot_mapping(reporter):
    required_files = [FAULT_SNAPSHOT_H, STM32F10X_IT_C, SCI_UPPER_C, SCI_UPPER_H]
    if any(not path.exists() for path in required_files):
        return

    header = read_text(FAULT_SNAPSHOT_H)
    it_c = read_text(STM32F10X_IT_C)
    sci_c = read_text(SCI_UPPER_C)
    sci_h = read_text(SCI_UPPER_H)

    if (
        "FAULT_BKP_REASON_REG BKP_DR11" in header
        and "FAULT_BKP_REASON_INV_REG BKP_DR12" in header
        and "FAULT_REASON_HARD" in header
        and '#include "FaultSnapshot.h"' in it_c
        and "Fault_SaveReason" in it_c
        and "BKP_WriteBackupRegister(FAULT_BKP_REASON_REG" in it_c
    ):
        reporter.ok("fault handlers write shared BKP fault snapshot definitions")
    else:
        reporter.fail("fault handlers must use FaultSnapshot.h and write BKP fault reason/inverse")

    if (
        '#include "FaultSnapshot.h"' in sci_c
        and "BKP_ReadBackupRegister(FAULT_BKP_REASON_REG)" in sci_c
        and "BKP_ReadBackupRegister(FAULT_BKP_REASON_INV_REG)" in sci_c
        and "#define RS485_RO_BASE_WORDS" in sci_h
        and "((UINT16)98U)" in sci_h
        and "D200 reason, D201 inverse" in sci_h
    ):
        reporter.ok("RS485 0xD200 exposes fault snapshot and preserves 0xD300 SOC test offset")
    else:
        reporter.fail("RS485 0xD200 should expose BKP fault snapshot and base words should be 98")


def check_can_rtc_service_runtime(reporter):
    required_files = [CAN_HDX_C, CAN_HDX_H]
    if any(not path.exists() for path in required_files):
        return

    can_c = read_text(CAN_HDX_C)
    can_h = read_text(CAN_HDX_H)

    if (
        "rtc_service_active" in can_c
        and "last_rtc_wake_tx_acked" in can_c
        and "last_rtc_wake_timeout" in can_c
        and "s_u8FeidaoCanLastRtcWakeTxAcked = 1U" in can_c
        and "s_u8FeidaoCanLastRtcWakeTimeout = 1U" in can_c
        and "(0U == s_u8FeidaoCanRtcServiceActive)" in can_c
        and "u8RtcServiceActive" in can_h
        and "u8LastRtcWakeTxAcked" in can_h
        and "u8LastRtcWakeTimeout" in can_h
    ):
        reporter.ok("CAN RTC wake service has bounded window and exposes ack/timeout status")
    else:
        reporter.fail("CAN RTC wake service should expose ack/timeout status and avoid scheduling extra frames inside service window")


def check_rtc_stop_sleep_contract(reporter):
    required_files = [RTC_C, RTC_SLEEP_C, CONF_C, RTC_SLEEP_OPT_DOC]
    if any(not path.exists() for path in required_files):
        missing = [str(path.relative_to(ROOT)) for path in required_files if not path.exists()]
        reporter.fail("RTC low-power contract files missing: {0}".format(",".join(missing)))
        return

    rtc_c = read_text(RTC_C)
    rtc_sleep_c = read_text(RTC_SLEEP_C)
    conf_c = read_text(CONF_C)
    doc = read_text(RTC_SLEEP_OPT_DOC)

    if (
        "void RTC_WKTimeConfig(void)" in rtc_c
        and "RTC_DisableSecondInterrupt();" in rtc_c
        and "RTC_DisableAlarmInterrupt();" in rtc_c
        and "RTC_EnableAlarmAfterSeconds(wake_seconds);" in rtc_c
        and "NVIC_ClearPendingIRQ(RTCAlarm_IRQn);" in rtc_c
        and "NVIC_ClearPendingIRQ(RTC_IRQn);" in rtc_c
    ):
        reporter.ok("RTC STOP wakeup uses alarm-only path and clears RTC/EXTI/NVIC pending bits")
    else:
        reporter.fail("RTC STOP wakeup should disable SEC, arm ALR, and clear RTC/EXTI/NVIC pending bits")

    if (
        "static void rtc_sleep_restore_after_stop(void)" in rtc_sleep_c
        and "InitRunAfterStopWakeup();" in rtc_sleep_c
        and "void InitRtcWakeupCheck(void)" in conf_c
        and "InitRunAfterStopWakeup();" in conf_c
        and "sys_time.wakeup_rtc = is_rtc_wakekup ? true : false;" in conf_c
        and "InitWakeUp_Base();" not in conf_c[conf_c.find("void InitRunAfterStopWakeup"):conf_c.find("void Init(void)")]
    ):
        reporter.ok("STOP wakeup recovery is unified through InitRunAfterStopWakeup")
    else:
        reporter.fail("STOP wakeup recovery should use one full InitRunAfterStopWakeup path")

    if (
        "RTC_IT_ALR" in doc
        and "RTC_IT_SEC" in doc
        and "EXTI17" in doc
        and "InitRunAfterStopWakeup" in doc
        and "过放" in doc
    ):
        reporter.ok("RTC sleep optimization document describes state machine and wakeup contract")
    else:
        reporter.fail("RTC sleep optimization document should cover ALR/SEC, EXTI17, recovery, and over-discharge priority")


def check_app_architecture(reporter):
    required_files = [MAIN_C, MAIN_H, MOS_STARTUP_C, MOS_STARTUP_H, APP_INIT_C, APP_INIT_H, CONF_C, DATADEAL_C, APP_ARCH_REFACTOR_DOC]
    if any(not path.exists() for path in required_files):
        missing = [str(path.relative_to(ROOT)) for path in required_files if not path.exists()]
        reporter.fail("app architecture files missing: {0}".format(",".join(missing)))
        return

    main_c = read_text(MAIN_C)
    main_h = read_text(MAIN_H)
    mos_c = read_text(MOS_STARTUP_C)
    mos_h = read_text(MOS_STARTUP_H)
    app_init_c = read_text(APP_INIT_C)
    app_init_h = read_text(APP_INIT_H)
    conf_c = read_text(CONF_C)
    datadeal_c = read_text(DATADEAL_C)

    if (
        '#include "MosStartup.h"' in main_h
        and "MosStartup_WriteMosState" in mos_c
        and "void MosStartup_ApplyInitialState(void)" in mos_c
        and "void MosStartup_ApplyInitialState(void)" not in main_c
        and "void open_chg_close_dsg(void)" not in main_c
        and "#define open_chg_close_dsg()" in mos_h
        and "#define enter_fac_mode(on)" in mos_h
    ):
        reporter.ok("MOS startup and factory-mode switching are isolated in MosStartup module")
    else:
        reporter.fail("MOS startup control should stay out of main.c and be owned by MosStartup.c/.h")

    if (
        '#include "AppInit.h"' in main_c
        and "AppInit_Boot();" in main_c
        and "Runtime_RunOnce();" in main_c
        and "void InitDevice(" not in main_c
        and "void InitVar(" not in main_c
        and "void InitSci(" not in main_c
        and "void App_Sci(" not in main_c
        and "static void AppInit_InitDevice(void)" in app_init_c
        and "static void AppInit_InitRuntimeState(void)" in app_init_c
        and "void AppInit_Boot(void)" in app_init_c
        and "UINT8 SeriesNum" in app_init_c
        and "#define AppInit_InitSci()" in app_init_h
        and "#define AppInit_ServiceSci()" in app_init_h
    ):
        reporter.ok("boot initialization is isolated in AppInit while main.c stays as a thin entry point")
    else:
        reporter.fail("boot initialization should stay in AppInit.c/.h and main.c should only call AppInit_Boot/Runtime_RunOnce")

    if (
        "static void Conf_InitRunSharedIo(void)" in conf_c
        and conf_c.count("Conf_InitRunSharedIo();") == 2
        and "static void Conf_InitMainPowerRails" in conf_c
        and "static void Conf_PrepareStopEntry(void)" in conf_c
        and "static void Conf_InitAllPortsAnalog(void)" in conf_c
        and "Conf_InitMainPowerRails(Bit_RESET" in conf_c
    ):
        reporter.ok("GPIO run/STOP setup reuses shared conf.c helpers")
    else:
        reporter.fail("conf.c GPIO setup should keep duplicated run/STOP sequences in shared helpers")

    if (
        "SeriesSelect_AFE1" not in main_h
        and "SeriesSelect_AFE1" not in app_init_c
        and "SeriesSelect_AFE1" not in datadeal_c
        and "UINT8 series_num = SeriesNum;" in datadeal_c
        and "series_num < 5U" in datadeal_c
        and "series_num == 13U" in datadeal_c
        and "afe_index = 9U;" in datadeal_c
    ):
        reporter.ok("cell voltage series mapping is compact and preserves runtime series selection")
    else:
        reporter.fail("SeriesSelect_AFE1 should remain removed while DataLoad_CellVolt preserves special series mapping")

    if RELEASE_MAP.exists():
        release_map = read_text(RELEASE_MAP)
        if "SeriesSelect_AFE1" in release_map:
            reporter.fail("FD_Release map still contains SeriesSelect_AFE1 const table")
        else:
            reporter.ok("FD_Release map no longer links SeriesSelect_AFE1 const table")

    doc = read_text(APP_ARCH_REFACTOR_DOC)
    if (
        "MosStartup.c" in doc
        and "AppInit.c" in doc
        and "SeriesSelect_AFE1" in doc
        and "Conf_InitRunSharedIo" in doc
        and "FD_Release" in doc
        and "0x03/0x06/0x10" in doc
    ):
        reporter.ok("architecture refactor document records module boundary, size optimization, and follow-up split order")
    else:
        reporter.fail("architecture refactor document should cover MosStartup, AppInit, compact cell mapping, checks, and next module split order")


def check_runtime_docs(reporter):
    docs = [FLOW_DOC, COMM_ADDRESS_INDEX, CAN_RUNTIME_REFACTOR, CAN_MODULE_SIMPLIFY]
    if any(not path.exists() for path in docs):
        missing = [str(path.relative_to(ROOT)) for path in docs if not path.exists()]
        reporter.fail("runtime documentation missing: {0}".format(",".join(missing)))
        return

    flow_doc = read_text(FLOW_DOC)
    comm_doc = read_text(COMM_ADDRESS_INDEX)
    can_runtime = read_text(CAN_RUNTIME_REFACTOR)
    can_simplify = read_text(CAN_MODULE_SIMPLIFY)

    if (
        "LowPower_Request()" in flow_doc
        and "LowPower_IsToSleepPending()" in flow_doc
        and "SleepDeal_Continue(mode)" in flow_doc
        and "D200" in comm_doc
        and "fault reason" in comm_doc
        and "D201" in comm_doc
    ):
        reporter.ok("runtime and communication docs describe current low-power and D200 mapping")
    else:
        reporter.fail("runtime/communication docs should describe LowPower APIs and D200 fault snapshot")

    if (
        "CanFeidaoFrames" in can_runtime
        and "Can_RtcWakeService" in can_runtime
        and "Can_GetIdleRtcPeriodSeconds" in can_runtime
        and "rtc_service_active" in can_runtime
        and "last_rtc_wake_tx_acked" in can_runtime
        and "CanFeidaoFrames.c/.h" in can_simplify
    ):
        reporter.ok("CAN docs describe current runtime/frame/low-power boundaries")
    else:
        reporter.fail("CAN docs should describe current runtime/frame/low-power boundaries")


def main(argv):
    parser = argparse.ArgumentParser(description="Check Keil project release/debug configuration.")
    parser.add_argument("-q", "--quiet", action="store_true", help="Only print warnings, errors, and summary.")
    args = parser.parse_args(argv)

    reporter = Reporter(verbose=not args.quiet)
    print("Project check: {0}".format(ROOT))

    check_required_files(reporter)
    check_utf8_text_files(reporter)
    check_project_config_wizard_encoding(reporter)
    check_keil_targets(reporter)
    check_removed_legacy_modules(reporter)
    check_release_defaults(reporter)
    check_guard_includes(reporter)
    check_build_guard(reporter)
    check_release_map(reporter)
    check_gitignore(reporter)
    check_hooks(reporter)
    check_soc_parameter_side_effects(reporter)
    check_soc_current_and_typec_policy(reporter)
    check_low_power_cleanup(reporter)
    check_fault_snapshot_mapping(reporter)
    check_can_rtc_service_runtime(reporter)
    check_rtc_stop_sleep_contract(reporter)
    check_app_architecture(reporter)
    check_runtime_docs(reporter)

    return reporter.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
