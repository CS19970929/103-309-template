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
RELEASE_MAP = ROOT / "103 + 309" / "Project" / "Users" / "Listings" / "FD_Release.map"
ELOG_CFG_H = ROOT / "103 + 309" / "Project" / "Source" / "easylogger" / "inc" / "elog_cfg.h"
ADC_C = ROOT / "103 + 309" / "Project" / "Source" / "ADC.c"
ADC_H = ROOT / "103 + 309" / "Project" / "Source" / "ADC.h"
DATADEAL_C = ROOT / "103 + 309" / "Project" / "Source" / "DataDeal.c"
DATADEAL_H = ROOT / "103 + 309" / "Project" / "Source" / "DataDeal.h"
I2C_AFE1_C = ROOT / "103 + 309" / "Project" / "Source" / "I2C_AFE1.c"
SOC_C = ROOT / "103 + 309" / "Project" / "Source" / "SOC.c"
SOC_ENHANCE_C = ROOT / "103 + 309" / "Project" / "Source" / "SocEnhance.c"
SCI_UPPER_C = ROOT / "103 + 309" / "Project" / "Source" / "Sci_Upper.c"
SCI_UPPER_H = ROOT / "103 + 309" / "Project" / "Source" / "Sci_Upper.h"
SLEEPDEAL_C = ROOT / "103 + 309" / "Project" / "Source" / "SleepDeal.c"
SLEEPDEAL_H = ROOT / "103 + 309" / "Project" / "Source" / "SleepDeal.h"
RTC_C = ROOT / "103 + 309" / "Project" / "Source" / "RTC.c"
RTC_SLEEP_C = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep.c"
RTC_SLEEP_H = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep.h"
RTC_SLEEP_PORT_C = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep_port.c"
RTC_SLEEP_PORT_H = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep_port.h"
RTC_SLEEP_AFE_PORT_H = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep_afe_port.h"
RTC_SLEEP_AFE_SH367309_C = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep_afe_sh367309.c"
CAN_HDX_C = ROOT / "103 + 309" / "Project" / "Source" / "Can_HDX.c"
CAN_HDX_H = ROOT / "103 + 309" / "Project" / "Source" / "Can_HDX.h"
CAN_FEIDAO_FRAMES_C = ROOT / "103 + 309" / "Project" / "Source" / "CanFeidaoFrames.c"
CAN_FEIDAO_FRAMES_H = ROOT / "103 + 309" / "Project" / "Source" / "CanFeidaoFrames.h"
SYSTEM_DEBUG_C = ROOT / "103 + 309" / "Project" / "Source" / "SystemDebug.c"
SYSTEM_DEBUG_H = ROOT / "103 + 309" / "Project" / "Source" / "SystemDebug.h"
RUNTIME_C = ROOT / "103 + 309" / "Project" / "Source" / "Runtime.c"
FACTORY_AGING_C = ROOT / "103 + 309" / "Project" / "Source" / "FactoryAging.c"
FACTORY_AGING_H = ROOT / "103 + 309" / "Project" / "Source" / "FactoryAging.h"
FLASH_C = ROOT / "103 + 309" / "Project" / "Source" / "Flash.c"
FLASH_H = ROOT / "103 + 309" / "Project" / "Source" / "Flash.h"
UPGRADE_PARAM_POLICY_H = ROOT / "103 + 309" / "Project" / "Source" / "UpgradeParamPolicy.h"
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
CAN_RUNTIME_REFACTOR = ROOT / "docs" / "devlog" / "CAN_RUNTIME_REFACTOR.md"
CAN_MODULE_SIMPLIFY = ROOT / "docs" / "devlog" / "CAN_MODULE_SIMPLIFY_2026-05-15.md"
CAN_POWER_RTC_SIMPLIFY = ROOT / "docs" / "devlog" / "CAN_POWER_RTC_SIMPLIFY_2026-06-02.md"
COMM_TOOL_ARCH_DOC = ROOT / "docs" / "COMM_TOOL_CAN_IAP_ARCHITECTURE_2026-05-22.md"
COMM_TOOL_SERIAL_DOC = ROOT / "docs" / "COMM_TOOL_SERIAL_PROTOCOL.md"
BMS_CAN_SERVICE_DOC = ROOT / "docs" / "BMS_CAN_SERVICE_PROTOCOL.md"
BMS_CAN_AGING_SOC_DOC = ROOT / "docs" / "CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md"
COMM_TOOL_UPGRADE_UI_DOC = ROOT / "docs" / "COMM_TOOL_UPGRADE_UI_2026-05-23.md"
BMS_CAN_IAP_DOC = ROOT / "docs" / "BMS_CAN_IAP_PROTOCOL.md"
BMS_CAN_IAP_RELIABILITY_DOC = ROOT / "docs" / "BMS_CAN_IAP_RELIABILITY_STATUS_2026-05-22.md"
BMS_SERIAL_IAP_REFACTOR_DOC = ROOT / "docs" / "BMS_SERIAL_IAP_REFACTOR_2026-05-22.md"
COMM_TOOL_KEIL_DOC = ROOT / "docs" / "COMM_TOOL_F103RET6_KEIL_PORT_2026-05-23.md"
COMM_TOOL_UART_SELECT_DOC = ROOT / "docs" / "COMM_TOOL_UART_SELECT_2026-05-25.md"
COMM_TOOL_BMS_REVIEW_HTML = ROOT / "docs" / "COMM_TOOL_BMS_APP_REVIEW_LOGGING_2026-05-25.html"
COMM_TOOL_HOST = ROOT / "tools" / "comm_tool_host.py"
COMM_TOOL_HOST_START = ROOT / "tools" / "start_comm_tool_host.ps1"
COMM_TOOL_UPGRADE_UI = ROOT / "tools" / "comm_tool_upgrade_ui.py"
COMM_TOOL_UPGRADE_UI_BUILD = ROOT / "tools" / "build_comm_tool_upgrade_ui_exe.ps1"
CAN_BMS_HOST = ROOT / "tools" / "can_bms_host.py"
CAN_BMS_HOST_START = ROOT / "tools" / "start_can_bms_host.ps1"
COMM_TOOL_UART_SELECT_SCRIPT = ROOT / "tools" / "set_comm_tool_uart.ps1"
COMM_TOOL_SOURCE = ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "app"
COMM_TOOL_BSP_SOURCE = ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "bsp"
COMM_TOOL_KEIL_PROJECT = ROOT / "firmware" / "comm_tool_f103ret6" / "keil" / "COMM_TOOL_F103RET6.uvprojx"
COMM_TOOL_IAP_PROJECT = ROOT / "firmware" / "comm_tool_f103ret6" / "keil" / "COMM_TOOL_IAP.uvprojx"
RTC_SLEEP_OPT_DOC = ROOT / "RTC_STANDBY_SLEEP_OPTIMIZATION_2026-05-22.md"
RTC_SLEEP_PORT_REFACTOR_DOC = ROOT / "docs" / "RTC_SLEEP_PORT_REFACTOR_2026-05-25.md"
APP_ARCH_REFACTOR_DOC = ROOT / "PROJECT_ARCH_REFACTOR_2026-05-22.md"
REFACTOR_REQUIREMENTS_DOC = ROOT / "PROJECT_REFACTOR_REQUIREMENTS_2026-05-22.md"
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
RETAINED_FACTORY_TEST_TOKENS = [
    "test_Autocurrent_cycle",
]
RELEASE_FORBIDDEN_DEFINES = {
    "_DEBUG_",
    "_DEBUG_CODE",
    "PROJECT_CFG_DEBUG_WATCH_ENABLE",
    "PROJECT_CFG_DEBUG_MONITOR_ENABLE",
    "PROJECT_CFG_IRQ_DEBUG_ENABLE",
    "PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE",
    "FLASH64K_APP_QUICK_TEST_ENABLE",
    "FLASH64K_APP_USE_TEST_ENABLE",
    "ELOG_OUTPUT_ENABLE",
}
DEBUG_ONLY_SOURCE_FILES = {
    "../Source/DebugHooks.c",
    "../Source/DebugWatch.c",
    "../Source/SystemDebug.c",
    "../Source/IrqDebug.c",
}
STARTUP_DEFAULT_HANDLER_SOURCE_FILE = "../Source/StartupDefaultHandler.c"
RUNTIME_DEBUG_FORBIDDEN_TOKENS = [
    "SystemDebug_Event",
    "SystemDebug_ProfileRecord",
    "SystemDebug_GetCycleCount",
    "DBG_PROFILE_",
    "DBG_MODULE_",
    "DbgPrint_Summary",
]
RELEASE_SAFE_DEFAULTS = {
    "PROJECT_CFG_WDOG_ENABLE": "1",
    "PROJECT_CFG_HOST_WRITE_ENABLE": "1",
    "PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS": "259200",
}
FORBIDDEN_PROJECT_CONFIG_MACROS = {
    "PROJECT_CFG_FACTORY_AGING_ENABLE",
    "PROJECT_CFG_IAP_ENABLE",
    "PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE",
}
GUARD_REQUIRED_TOKENS = [
    "PROJECT_CFG_HOST_WRITE_ENABLE",
    "PROJECT_CFG_IAP_ENABLE",
    "PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE",
    "PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS",
    "PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV",
    "PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV",
    "PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV",
    "PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV",
    "PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS",
    "PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV",
    "PROJECT_CFG_SOC_REST_OCV_SECONDS",
    "PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS",
    "PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT",
    "PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA",
    "PROJECT_CFG_FACTORY_AGING_ENABLE",
    "PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS",
    "PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT",
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


def normalize_keil_path(path):
    return (path or "").replace("\\", "/").strip().lower()


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
        build_files = set()
        for file_item in target.findall("./Groups/Group/Files/File"):
            file_path = file_item.findtext("FilePath") or ""
            if not file_path:
                continue
            normalized = file_path.replace("\\", "/")
            files.add(normalized)
            include_in_build = file_item.findtext("./FileOption/CommonProperty/IncludeInBuild")
            if include_in_build != "0":
                build_files.add(normalized)
        targets[name] = {
            "defines": define_tokens(c_define_text),
            "output_name": output_name.strip(),
            "output_dir": output_dir.strip(),
            "files": files,
            "build_files": build_files,
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
    for path in [PROJECT, PROJECT_CONFIG, BUILD_GUARD, CONF_H, MAIN_C, MAIN_H, MOS_STARTUP_C, MOS_STARTUP_H, RUNTIME_C, ELOG_CFG_H, GITIGNORE, PRE_COMMIT, PRE_PUSH]:
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

        if normalize_keil_path(release["output_dir"]) != "./objects/":
            reporter.fail("FD_Release OutputDirectory should remain .\\Objects\\ for safe flash scripts, got {0}".format(release["output_dir"]))
        else:
            reporter.ok("FD_Release output directory remains Objects")

        if "../Source/conf/Project_BuildGuard.h" not in release["files"]:
            reporter.fail("FD_Release project tree does not include Project_BuildGuard.h")
        else:
            reporter.ok("FD_Release includes Project_BuildGuard.h")

        if "../Source/MosStartup.c" not in release["files"]:
            reporter.fail("FD_Release project tree does not include MosStartup.c")
        else:
            reporter.ok("FD_Release includes MosStartup.c")

        if "../Source/Runtime.c" not in release["files"]:
            reporter.fail("FD_Release project tree does not include Runtime.c")
        else:
            reporter.ok("FD_Release includes Runtime.c")

        if STARTUP_DEFAULT_HANDLER_SOURCE_FILE not in release["build_files"]:
            reporter.fail("FD_Release should build StartupDefaultHandler.c for startup default-vector no-op")
        else:
            reporter.ok("FD_Release builds StartupDefaultHandler.c default-vector no-op")

        debug_only_files = DEBUG_ONLY_SOURCE_FILES & release["build_files"]
        if debug_only_files:
            reporter.fail("FD_Release builds debug-only source files: {0}".format(",".join(sorted(debug_only_files))))
        else:
            reporter.ok("FD_Release excludes debug-only source files from build")

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

        monitor_value = find_define_value(debug["defines"], "PROJECT_CFG_DEBUG_MONITOR_ENABLE")
        if monitor_value != "1":
            reporter.fail("FD_Debug should define PROJECT_CFG_DEBUG_MONITOR_ENABLE=1 for g_dbg snapshot")
        else:
            reporter.ok("FD_Debug enables g_dbg snapshot under g_dbg_watch")

        irq_value = find_define_value(debug["defines"], "PROJECT_CFG_IRQ_DEBUG_ENABLE")
        if irq_value != "1":
            reporter.fail("FD_Debug should define PROJECT_CFG_IRQ_DEBUG_ENABLE=1 for IRQ counters")
        else:
            reporter.ok("FD_Debug enables IRQ debug counters")

        irq_event_value = find_define_value(debug["defines"], "PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE")
        if irq_event_value != "0":
            reporter.fail("FD_Debug should define PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE=0 to avoid high-rate IRQ ring noise")
        else:
            reporter.ok("FD_Debug keeps high-rate IRQ event ring disabled")

        if debug["output_name"] != "FD_Debug":
            reporter.fail("FD_Debug OutputName should be FD_Debug, got {0}".format(debug["output_name"]))
        else:
            reporter.ok("FD_Debug output name is isolated")

        if normalize_keil_path(debug["output_dir"]) != "./objects_debug/":
            reporter.fail("FD_Debug OutputDirectory should be .\\Objects_Debug\\ to avoid Release object reuse, got {0}".format(debug["output_dir"]))
        else:
            reporter.ok("FD_Debug output directory is separated from Release")

        if "../Source/conf/Project_BuildGuard.h" not in debug["files"]:
            reporter.fail("FD_Debug project tree does not include Project_BuildGuard.h")
        else:
            reporter.ok("FD_Debug includes Project_BuildGuard.h")

        if "../Source/MosStartup.c" not in debug["files"]:
            reporter.fail("FD_Debug project tree does not include MosStartup.c")
        else:
            reporter.ok("FD_Debug includes MosStartup.c")

        if "../Source/Runtime.c" not in debug["files"]:
            reporter.fail("FD_Debug project tree does not include Runtime.c")
        else:
            reporter.ok("FD_Debug includes Runtime.c")

        missing_debug_files = DEBUG_ONLY_SOURCE_FILES - debug["build_files"]
        if missing_debug_files:
            reporter.fail("FD_Debug missing built debug source files: {0}".format(",".join(sorted(missing_debug_files))))
        else:
            reporter.ok("FD_Debug includes debug hook/watch/system/IRQ source files")

    if release and debug:
        if release["output_name"] == debug["output_name"]:
            reporter.fail("FD_Release and FD_Debug share the same OutputName")
        else:
            reporter.ok("Keil targets use separate output names")
        if normalize_keil_path(release["output_dir"]) == normalize_keil_path(debug["output_dir"]):
            reporter.fail("FD_Release and FD_Debug share the same OutputDirectory")
        else:
            reporter.ok("Keil targets use separate output directories")


def check_runtime_debug_isolation(reporter):
    if not RUNTIME_C.exists():
        return

    runtime_c = read_text(RUNTIME_C)
    leaked = [token for token in RUNTIME_DEBUG_FORBIDDEN_TOKENS if token in runtime_c]
    if leaked:
        reporter.fail("Runtime.c should call DebugHooks instead of debug implementation tokens: {0}".format(",".join(leaked)))
    else:
        reporter.ok("Runtime.c keeps event/profile/debug-print implementation behind DebugHooks")

    if '#include "DebugHooks.h"' in runtime_c and "DebugHooks_RuntimeAfterFrontSection" in runtime_c:
        reporter.ok("Runtime.c routes debug touchpoints through DebugHooks")
    else:
        reporter.fail("Runtime.c should include DebugHooks.h and route runtime debug touchpoints through DebugHooks")


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

    datadeal_text = read_text(DATADEAL_C) if DATADEAL_C.exists() else ""
    missing_retained = [token for token in RETAINED_FACTORY_TEST_TOKENS if token not in datadeal_text]
    if missing_retained:
        reporter.fail("retained factory/test hooks are missing: {0}".format(",".join(missing_retained)))
    else:
        reporter.ok("retained factory/test hooks remain available")

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

    for name in sorted(FORBIDDEN_PROJECT_CONFIG_MACROS):
        if name in defines:
            reporter.fail("Project_Config.h must not expose required feature switch {0}".format(name))
        else:
            reporter.ok("Project_Config.h does not expose required feature switch {0}".format(name))


def check_required_board_features(reporter):
    required = [PROJECT_CONFIG, BUILD_GUARD, FLASH_C, UPGRADE_PARAM_POLICY_H, FACTORY_AGING_C]
    if any(not path.exists() for path in required):
        return

    project_config = read_text(PROJECT_CONFIG)
    build_guard = read_text(BUILD_GUARD)
    flash_c = read_text(FLASH_C)
    policy_h = read_text(UPGRADE_PARAM_POLICY_H)
    aging_c = read_text(FACTORY_AGING_C)

    if (
        "PROJECT_CFG_IAP_ENABLE" not in project_config
        and "#ifdef _IAP" not in flash_c
        and "u8FlashUpdateFlag" in flash_c
        and "MCU_RESET();" in flash_c
        and "IAP is a required board feature" in build_guard
    ):
        reporter.ok("IAP is always compiled and guarded against legacy disable macros")
    else:
        reporter.fail("IAP must be always compiled and must not be a Project_Config.h switch")

    if (
        "PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE" not in project_config
        and "#define UPGRADE_PARAM_POLICY_ENABLE        1" in policy_h
        and "#error \"Upgrade parameter policy is a required board feature and must not be disabled\"" in policy_h
        and "PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION" in policy_h
        and "PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD" in policy_h
        and "PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT" in policy_h
        and "Upgrade parameter policy is a required board feature" in build_guard
    ):
        reporter.ok("upgrade parameter policy mechanism is required while reset policy remains configurable")
    else:
        reporter.fail("upgrade parameter policy must always be present while keeping version/reset settings configurable")

    if (
        "PROJECT_CFG_FACTORY_AGING_ENABLE" not in project_config
        and "PROJECT_CFG_FACTORY_AGING_ENABLE" not in aging_c
        and "FactoryAging_StartByHost" in aging_c
        and "FactoryAging_Task" in aging_c
        and "Factory aging is a required board feature" in build_guard
    ):
        reporter.ok("factory aging is always compiled while duration remains configurable")
    else:
        reporter.fail("factory aging must always be present while keeping duration configurable")


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
    range_check_pos = text.find("/* --- range checks --- */")
    if include_guard_pos != -1 and range_check_pos != -1 and range_check_pos > include_guard_pos:
        reporter.ok("Project_BuildGuard.h range checks stay outside the include guard")
    else:
        reporter.fail("Project_BuildGuard.h range checks should stay outside the include guard")


def check_release_map(reporter):
    if not RELEASE_MAP.exists():
        reporter.warn("release map is missing; build FD_Release before final release checks")
        return

    defines = parse_header_defines(PROJECT_CONFIG) if PROJECT_CONFIG.exists() else {}
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

    removed_soc_table_symbols = [
        "SOC_Table_Set",
        "SOC_Table_Default",
        "SOC_Table_CanSet",
        "SocTable_LiFEPO2",
        "SocTable_LiFePO2",
    ]
    leaked = [symbol for symbol in removed_soc_table_symbols if symbol in text]
    if leaked:
        reporter.fail("removed runtime SOC table symbols still appear in FD_Release map: {0}".format(",".join(leaked)))
    else:
        reporter.ok("runtime SOC table symbols are removed from release map")

    chemistry = defines.get("PROJECT_CFG_BAT_CHEMISTRY")
    if chemistry == "0":
        fixed_table_leaks = ["SOC_Table_LiFePO"]
    elif chemistry == "1":
        fixed_table_leaks = ["SocTable_TernaryLi"]
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
        "Sci_RangeOverlaps(offset, count, 24, 4)" in body
        and "reload_soc = 1U;" in body
        and "SOC_RequestCapacityReset();" in body
    ):
        reporter.ok("SOC capacity parameter writes refresh SOC runtime state")
    else:
        reporter.fail("SOC capacity side effects must refresh SOC runtime state")


def check_sci_host_write_policy(reporter):
    if not SCI_UPPER_C.exists():
        return

    sci_c = read_text(SCI_UPPER_C)
    if (
        "#if PROJECT_CFG_HOST_WRITE_ENABLE" in sci_c
        and "Sci_SetWrError(s, RS485_ERROR_NO_PERMISSION);" in sci_c
        and "Sci_IsCalibPairStart" in sci_c
    ):
        reporter.ok("SCI host write entry keeps build-time permission switch and rejects disabled writes")
    else:
        reporter.fail("SCI 0x06/0x10 write entry should be controlled by PROJECT_CFG_HOST_WRITE_ENABLE")

    defines = parse_header_defines(PROJECT_CONFIG) if PROJECT_CONFIG.exists() else {}
    if defines.get("PROJECT_CFG_HOST_WRITE_ENABLE") == "1" and RELEASE_MAP.exists():
        release_map = read_text(RELEASE_MAP)
        write_symbols = [
            "Sci_WrReg_0x06_Reset_EventRecord",
            "Sci_WrReg_0x06_BMS_FunctionON",
            "Sci_WrRegs_0x10_Protect",
            "Sci_WrRegs_0x10_OtherElement",
        ]
        missing = [
            symbol for symbol in write_symbols
            if not re.search(r"^\s*{0}\s+0x[0-9a-fA-F]+\s+Thumb Code".format(symbol), release_map, re.MULTILINE)
        ]
        if missing:
            reporter.fail("PROJECT_CFG_HOST_WRITE_ENABLE=1 but FD_Release misses write handlers: {0}".format(",".join(missing)))
        else:
            reporter.ok("PROJECT_CFG_HOST_WRITE_ENABLE=1 keeps protected/other parameter write handlers in FD_Release")


def check_soc_current_and_typec_policy(reporter):
    required_files = [ADC_H, DATADEAL_C, SOC_C, SOC_ENHANCE_C]
    if any(not path.exists() for path in required_files):
        return

    adc_h = read_text(ADC_H)
    datadeal_c = read_text(DATADEAL_C)
    soc_c = read_text(SOC_C)
    soc_enhance_c = read_text(SOC_ENHANCE_C)

    legacy_typec_current_path = (
        "g_u16TypeCBatEquivCurrent_A10" in adc_h
        and "g_u16TypeCOutCurrent_mA" in soc_c
    )
    getter_typec_current_path = (
        "UINT16 ADC_GetTypeCOutCurrentMilliAmp(void);" in adc_h
        and "ADC_GetTypeCOutCurrentMilliAmp()" in soc_c
    )
    typec_current_not_added_directly = (
        "report_idsg + (UINT32)g_u16TypeCOutCurrent_A10" not in soc_c
        and "report_idsg + (UINT32)g_u16TypeCOutCurrent_mA" not in soc_c
        and "report_idsg + (UINT32)ADC_GetTypeCOutCurrentMilliAmp()" not in soc_c
    )

    if (
        "TYPEC_OUT_VOLTAGE_MV" in adc_h
        and "TYPEC_DCDC_EFFICIENCY_PERMILLE" in adc_h
        and "SOC_GetTypeCBatEquivCurrentA10" in soc_c
        and (legacy_typec_current_path or getter_typec_current_path)
        and typec_current_not_added_directly
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
    required_files = [
        SLEEPDEAL_C,
        SLEEPDEAL_H,
        RTC_SLEEP_C,
        RTC_SLEEP_H,
        RTC_SLEEP_PORT_C,
        RTC_SLEEP_PORT_H,
        RTC_SLEEP_AFE_PORT_H,
        RTC_SLEEP_AFE_SH367309_C,
        LEDBAR_C,
        LOGRECORD_C,
        SYSTEM_DEBUG_C,
    ]
    if any(not path.exists() for path in required_files):
        return

    sleepdeal_c = read_text(SLEEPDEAL_C)
    sleepdeal_h = read_text(SLEEPDEAL_H)
    rtc_sleep_c = read_text(RTC_SLEEP_C)
    rtc_sleep_h = read_text(RTC_SLEEP_H)
    rtc_sleep_port_c = read_text(RTC_SLEEP_PORT_C)
    rtc_sleep_port_h = read_text(RTC_SLEEP_PORT_H)
    rtc_sleep_afe_port_h = read_text(RTC_SLEEP_AFE_PORT_H)
    rtc_sleep_afe_sh367309_c = read_text(RTC_SLEEP_AFE_SH367309_C)
    project = read_text(PROJECT)
    ledbar_c = read_text(LEDBAR_C)
    logrecord_c = read_text(LOGRECORD_C)
    system_debug_c = read_text(SYSTEM_DEBUG_C)

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
        and "uint8_t sleep_mode;" in rtc_sleep_c
        and "sleep_mode = g_stLowPowerRtcStatus.mode;" in rtc_sleep_c
        and "low_power_log_and_commit_sleep(sleep_mode);" in rtc_sleep_c
        and "LowPower_IsToSleepPending" not in rtc_sleep_c
        and "LowPower_IsToSleepPending" not in rtc_sleep_h
        and "LowPower_ClearToSleepFlag" not in logrecord_c
        and "LowPower_IsToSleepPending" not in ledbar_c
        and "void SleepDeal_Continue(UINT8 sleep_mode)" in sleepdeal_c
        and "RtcSleep_PortCommitResetSleep(sleep_mode);" in rtc_sleep_c
        and "SleepDeal_Continue(sleep_mode);" in rtc_sleep_port_c
        and "low_power_log_and_commit_sleep(DEEP_MODE);" in ledbar_c
        and "SleepDeal_Continue((UINT8)DEEP_MODE);" not in ledbar_c
    ):
        reporter.ok("low power sleep commit uses local sleep_mode without readyToSleep cross-module state")
    else:
        reporter.fail("low power sleep commit should remove LowPower_IsToSleepPending/ClearToSleepFlag and use local sleep_mode")

    if (
        "LOW_POWER_RTC_BLOCK" not in rtc_sleep_c
        and "LOW_POWER_RTC_BLOCK" not in rtc_sleep_h
        and "s_u16IdleDelaySeconds" not in rtc_sleep_c
        and "s_u32RtcSleepElapsedSeconds" not in rtc_sleep_c
        and "s_u32RtcWakeCycles" not in rtc_sleep_c
        and "s_u32LastSleepSeconds" not in rtc_sleep_c
        and "LP_BLOCK_EXT_COMM" in rtc_sleep_h
        and "LP_BLOCK_AGING" in rtc_sleep_h
        and "g_stLowPowerRtcStatus.block = LP_GetBlockReason();" in rtc_sleep_c
        and "g_stLowPowerRtcStatus.idle = 0U;" in rtc_sleep_c
        and "g_stLowPowerRtcStatus.sleep += rtc_elapsed_seconds;" in rtc_sleep_c
        and "LP_GetBlockReason()" not in system_debug_c
        and "g_dbg.lp.block        = g_stLowPowerRtcStatus.block;" in system_debug_c
    ):
        reporter.ok("low power state uses g_stLowPowerRtcStatus and a single LP_BLOCK bitmask")
    else:
        reporter.fail("low power state should remove split counters and LOW_POWER_RTC_BLOCK mapping")

    forbidden_core_tokens = [
        "AFE_TYPE",
        "bq76xx_afe",
        "sh36xx",
        "MTPRead(",
        "UpdateVoltageFromBqMaximo(",
        "SH367309_",
        "GPIO_ReadInputDataBit(",
        "Sys_StopMode(",
        "Init_RTC(",
        "Can_RtcWakeService(",
        "SOC_ApplyRtcRelaxationCompensation(",
        "SleepDeal_Continue(",
    ]
    leaked_tokens = [token for token in forbidden_core_tokens if token in rtc_sleep_c]
    if leaked_tokens:
        reporter.fail("rtc_sleep.c still depends on low-level MCU/AFE tokens: {0}".format(",".join(leaked_tokens)))
    else:
        reporter.ok("rtc_sleep.c stays independent from low-level MCU/AFE drivers")

    if (
        '#include "rtc_sleep_afe_port.h"' in rtc_sleep_port_c
        and "RtcSleep_AfePortUpdateRtcData();" in rtc_sleep_port_c
        and "RtcSleep_AfePortHasCurrentWake(source);" in rtc_sleep_port_c
        and "RtcSleep_AfePortHasAfeWake(source);" in rtc_sleep_port_c
        and "RtcSleep_AfePortIsSleepBlocked" not in rtc_sleep_afe_port_h
        and "RtcSleep_AfePortIsSleepBlocked" not in rtc_sleep_afe_sh367309_c
        and "RtcSleep_PortIsAfeSleepBlocked" not in rtc_sleep_port_h
        and "RtcSleep_PortIsAfeSleepBlocked" not in rtc_sleep_port_c
        and "AFE_TYPE" not in rtc_sleep_port_c
        and "AFE_TYPE" not in rtc_sleep_afe_sh367309_c
        and "rtc_sleep_port.c" in project
        and "rtc_sleep_afe_sh367309.c" in project
    ):
        reporter.ok("RTC sleep port layer selects MCU/AFE adapters by file boundary instead of AFE_TYPE branches")
    else:
        reporter.fail("RTC sleep port layer should keep AFE details in a separate source file and avoid unused AFE sleep-block wrappers")


def check_global_state_phase1(reporter):
    required_files = [
        SYSTEM_DEBUG_C,
        RUNTIME_C,
        FLASH_C,
        SLEEPDEAL_C,
        SLEEPDEAL_H,
        RTC_C,
        RTC_SLEEP_C,
        RTC_SLEEP_PORT_C,
        SCI_UPPER_C,
        CONF_C,
    ]
    if any(not path.exists() for path in required_files):
        return

    system_debug_c = read_text(SYSTEM_DEBUG_C)
    runtime_c = read_text(RUNTIME_C)
    flash_c = read_text(FLASH_C)
    sleepdeal_c = read_text(SLEEPDEAL_C)
    sleepdeal_h = read_text(SLEEPDEAL_H)
    rtc_c = read_text(RTC_C)
    rtc_sleep_c = read_text(RTC_SLEEP_C)
    rtc_sleep_port_c = read_text(RTC_SLEEP_PORT_C)
    sci_upper_c = read_text(SCI_UPPER_C)
    conf_c = read_text(CONF_C)
    combined = "\n".join([
        system_debug_c,
        runtime_c,
        flash_c,
        sleepdeal_c,
        sleepdeal_h,
        rtc_c,
        rtc_sleep_c,
        rtc_sleep_port_c,
        sci_upper_c,
        conf_c,
    ])

    stale_tokens = [
        "s_dbg_events",
        "s_dbg_event_head",
        "s_dbg_event_count",
        "s_dbg_fault_snap",
        "s_dbg_fault_valid",
        "s_dbg_print_tick",
        "s_last_fault",
        "s_last_lp_mode",
        "s_u8StorageFlashBusy",
        "RTC_ExtComCnt",
        "s_u8BootFromSleepStartup",
        "s_u8BootFromSleepChargerWakeup",
        "TimeDisplay",
        "s_u32RtcLastWakeupPeriodSeconds",
        "s_u32RtcWakeupPeriodOverrideSeconds",
        "is_rtc_wakekup",
    ]
    found = [token for token in stale_tokens if token in combined]
    if found:
        reporter.fail("global state phase1 still has split state tokens: {0}".format(",".join(found)))
    else:
        reporter.ok("global state phase1 removed selected split state tokens")

    if (
        "static DBG_RUNTIME s_dbgRt;" in system_debug_c
        and "static APP_RUNTIME s_rt" in runtime_c
        and "static FLASH_RUNTIME s_flash;" in flash_c
        and "static SLEEP_RUNTIME s_sleep;" in sleepdeal_c
        and "static RTC_RUNTIME s_rtc" in rtc_c
    ):
        reporter.ok("global state phase1 uses module runtime structs")
    else:
        reporter.fail("global state phase1 should keep selected state in module runtime structs")

    if (
        "void SleepDeal_RecordExternalComm(void)" in sleepdeal_h
        and "UINT8 SleepDeal_GetExternalCommCounter(void)" in sleepdeal_h
        and "SleepDeal_RecordExternalComm();" in sci_upper_c
        and "SleepDeal_GetExternalCommCounter()" in rtc_sleep_port_c
        and "UINT8 RTC_IsStopWakeup(void)" in rtc_c
        and "void RTC_ClearStopWakeup(void)" in rtc_c
        and "RTC_ClearStopWakeup();" in rtc_sleep_c
        and "RTC_IsStopWakeup() != 0U" in conf_c
    ):
        reporter.ok("global state phase1 keeps SleepDeal/RTC state behind accessors")
    else:
        reporter.fail("global state phase1 should access SleepDeal/RTC state through module functions")


def check_adc_state_runtime(reporter):
    required_files = [
        ADC_C,
        ADC_H,
        DATADEAL_C,
        SOC_C,
        SYSTEM_DEBUG_C,
    ]
    if any(not path.exists() for path in required_files):
        return

    adc_c = read_text(ADC_C)
    adc_h = read_text(ADC_H)
    datadeal_c = read_text(DATADEAL_C)
    soc_c = read_text(SOC_C)
    system_debug_c = read_text(SYSTEM_DEBUG_C)
    combined = "\n".join([adc_c, adc_h, datadeal_c, soc_c, system_debug_c])

    stale_tokens = [
        "g_u16ADCValFilter",
        "g_i32ADCResult",
        "g_u32ADCValFilter2",
        "s_u32AnlogCalLast10msTick",
        "g_u16TypeCOutCurrent_mA",
        "g_u32Vbat_mV",
        "su8_ADcnt",
        "su8_ZeroCnt",
        "s8ADcnt",
    ]
    found = [token for token in stale_tokens if token in combined]
    if found:
        reporter.fail("ADC state still has split tokens: {0}".format(",".join(found)))
    else:
        reporter.ok("ADC state removed selected split variables")

    if (
        "static ADC_RUNTIME s_adc;" in adc_c
        and "__IO UINT16 raw[ADC_NUM];" in adc_c
        and "INT32 filt[ADC_NUM];" in adc_c
        and "INT32 result[ADC_NUM];" in adc_c
        and "UINT32 vbat;" in adc_c
        and "UINT16 typec;" in adc_c
    ):
        reporter.ok("ADC state uses s_adc runtime struct")
    else:
        reporter.fail("ADC state should keep raw/filter/result/vbat/typec in s_adc")

    if (
        "INT32 ADC_GetResult(UINT8 index);" in adc_h
        and "UINT16 ADC_GetRaw(UINT8 index);" in adc_h
        and "ADC_GetResult(ADC_TEMP_MOS1)" in datadeal_c
        and "ADC_GetVbatMilliVolt()" in soc_c
        and "ADC_GetRaw(ADC_VBC)" in system_debug_c
    ):
        reporter.ok("ADC state is read through accessors outside ADC.c")
    else:
        reporter.fail("ADC state should be accessed through ADC getters outside ADC.c")


def check_datadeal_runtime_state(reporter):
    required_files = [
        DATADEAL_C,
        DATADEAL_H,
        I2C_AFE1_C,
        SOC_C,
    ]
    if any(not path.exists() for path in required_files):
        return

    datadeal_c = read_text(DATADEAL_C)
    datadeal_h = read_text(DATADEAL_H)
    i2c_afe1_c = read_text(I2C_AFE1_C)
    soc_c = read_text(SOC_C)
    combined = "\n".join([datadeal_c, datadeal_h, i2c_afe1_c, soc_c])

    stale_tokens = [
        "g_u32AfeCurrentSampleSeq",
        "u8IICFaultcnt1",
        "u8IICFaultcnt2",
        "u8WakeCnt1",
        "u8WakeCnt2",
        "su16_Sleep_DelayT1",
        "su16_Sleep_DelayT2",
        "su16_Sleep_DelayT3",
        "s_afe_current",
    ]
    found = [token for token in stale_tokens if token in combined]
    if found:
        reporter.fail("DataDeal runtime state still has split tokens: {0}".format(",".join(found)))
    else:
        reporter.ok("DataDeal runtime state removed selected split variables")

    if (
        "static DATA_RUNTIME s_data" in datadeal_c
        and "AFE_CURRENT_RUNTIME cur;" in datadeal_c
        and "AFE_MONITOR_RUNTIME mon;" in datadeal_c
        and "UINT32 afeSeq;" in datadeal_c
        and "AFE_MONITOR_CH ch[2];" in datadeal_c
        and "UINT16 sleepDelay[3];" in datadeal_c
    ):
        reporter.ok("DataDeal runtime state uses s_data with current/monitor/sequence groups")
    else:
        reporter.fail("DataDeal runtime state should keep current/monitor/sequence in s_data")

    if (
        "UINT32 AfeCurrent_GetSeq(void);" in datadeal_h
        and "AfeCurrent_NextSeq();" in datadeal_c
        and "AfeCurrent_GetSeq() == 0U" in i2c_afe1_c
        and "u32AfeCurrentSeq = AfeCurrent_GetSeq();" in soc_c
    ):
        reporter.ok("AFE current sample sequence is read through accessor")
    else:
        reporter.fail("AFE current sample sequence should use AfeCurrent_GetSeq outside DataDeal.c")


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
    required_files = [
        CAN_HDX_C,
        CAN_HDX_H,
        CAN_FEIDAO_FRAMES_H,
        RTC_C,
        RTC_SLEEP_C,
        RTC_SLEEP_PORT_C,
        RTC_SLEEP_PORT_H,
        PROJECT_CONFIG,
        SYSTEM_DEBUG_C,
        SYSTEM_DEBUG_H,
    ]
    if any(not path.exists() for path in required_files):
        return

    can_c = read_text(CAN_HDX_C)
    can_h = read_text(CAN_HDX_H)
    frames_h = read_text(CAN_FEIDAO_FRAMES_H)
    rtc_c = read_text(RTC_C)
    rtc_sleep_c = read_text(RTC_SLEEP_C)
    rtc_sleep_port_c = read_text(RTC_SLEEP_PORT_C)
    rtc_sleep_port_h = read_text(RTC_SLEEP_PORT_H)
    project_config = read_text(PROJECT_CONFIG)
    system_debug_c = read_text(SYSTEM_DEBUG_C)
    system_debug_h = read_text(SYSTEM_DEBUG_H)

    combined = "\n".join([
        can_c,
        can_h,
        frames_h,
        rtc_c,
        rtc_sleep_c,
        rtc_sleep_port_c,
        rtc_sleep_port_h,
        project_config,
        system_debug_c,
        system_debug_h,
    ])
    removed_tokens = [
        "Can_RtcWakeService",
        "Can_GetIdleRtcPeriodSeconds",
        "Can_IsBusActive",
        "RtcSleep_PortRunCanRtcWakeService",
        "RtcSleep_PortGetCanRtcPeriodSeconds",
        "RtcSleep_PortIsCanBusActive",
        "PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS",
        "FEIDAO_CAN_RTC_SERVICE_TIMEOUT_TICKS",
        "FEIDAO_CAN_POWER_STABLE_TICKS",
        "s_runtime.rtc_service_active",
        "s_runtime.bus_off",
        "feidao_can_busoff_monitor",
        "PROJECT_CFG_CAN_BUS_ACTIVE_HOLD_SECONDS",
        "PROJECT_CFG_CAN_NO_ACK_BACKOFF_THRESHOLD",
        "PROJECT_CFG_CAN_PROBE_PERIOD_SECONDS",
        "FEIDAO_CAN_BUS_ACTIVE_HOLD_TICKS",
        "FEIDAO_CAN_NO_ACK_BACKOFF_THRESHOLD",
        "FEIDAO_CAN_PROBE_PERIOD_TICKS",
        "CAN_FEIDAO_RTC_PROBE_MSG_MASK",
        "s_runtime.bus_active",
        "s_runtime.no_ack_cnt",
        "s_runtime.probe_active",
        "last_probe_tick",
        "feidao_can_mark_bus_active",
        "feidao_can_handle_no_ack",
        "feidao_can_update_bus_active_timeout",
        "rtc_svc",
        "tx_ok_cnt",
        "tx_fail_cnt",
        "busoff_in_cnt",
        "busoff_out_cnt",
        "last_tx_id",
    ]
    stale_tokens = [token for token in removed_tokens if token in combined]
    snapshot_ok = (
        "void Can_GetDebugSnapshot(uint8_t *power_on" in can_h
        and "Can_GetDebugSnapshot(&g_dbg.can.power_on" in system_debug_c
        and "uint8_t  power_on;" in system_debug_h
        and "uint8_t  bus_off;" in system_debug_h
        and "uint8_t  tx_queue;" in system_debug_h
        and "uint16_t esr;" in system_debug_h
    )

    if (
        "FEIDAO_CAN_TX_QUEUE_SIZE" in can_c
        and "feidao_can_service_tx" in can_c
        and "CAN_ABOM = ENABLE" in can_c
        and "CAN_NART = ENABLE" in can_c
        and "CAN_FEIDAO_1000MS_MSG_MASK" in can_c
        and "CAN_FEIDAO_5000MS_MSG_MASK" in can_c
        and "Can_PrepareSleep" in can_c
        and "feidao_can_power_off();" in can_c
        and "RTC_WAKEUP_DEFAULT_SECONDS" in rtc_c
        and snapshot_ok
        and not stale_tokens
    ):
        reporter.ok("CAN runtime uses queued TX, fixed run-mode periodic schedule, no RTC CAN service, no active/probe/no-ack state, and ABOM bus-off recovery")
    else:
        if stale_tokens:
            reporter.fail("CAN RTC/bus-off simplification still contains stale tokens: {0}".format(",".join(stale_tokens)))
        elif not snapshot_ok:
            reporter.fail("CAN debug snapshot should only expose power_on, bus_off, tx_queue, and esr")
        else:
            reporter.fail("CAN runtime should keep queued TX, fixed run-mode periodic schedule, CMNT off before RTC sleep, remove RTC CAN service, remove active/probe/no-ack state, and keep ABOM enabled")


def check_can_aging_soc_service(reporter):
    required_files = [
        CAN_HDX_C,
        CAN_FEIDAO_FRAMES_C,
        FACTORY_AGING_C,
        FACTORY_AGING_H,
        FLASH_H,
        CAN_BMS_HOST,
        CAN_BMS_HOST_START,
        COMM_TOOL_HOST,
        COMM_TOOL_UPGRADE_UI,
        COMM_TOOL_UPGRADE_UI_BUILD,
        COMM_TOOL_SOURCE / "ct_protocol.h",
        COMM_TOOL_SOURCE / "ct_can_gateway.c",
        COMM_TOOL_SOURCE / "ct_can_gateway.h",
        COMM_TOOL_SOURCE / "ct_app.c",
        COMM_TOOL_SERIAL_DOC,
        BMS_CAN_SERVICE_DOC,
        BMS_CAN_AGING_SOC_DOC,
        COMM_TOOL_UPGRADE_UI_DOC,
    ]
    if any(not path.exists() for path in required_files):
        missing = [str(path.relative_to(ROOT)) for path in required_files if not path.exists()]
        reporter.fail("CAN aging/SOC service files missing: {0}".format(",".join(missing)))
        return

    can_c = read_text(CAN_HDX_C)
    frames_c = read_text(CAN_FEIDAO_FRAMES_C)
    aging_c = read_text(FACTORY_AGING_C)
    aging_h = read_text(FACTORY_AGING_H)
    flash_h = read_text(FLASH_H)
    host_py = read_text(CAN_BMS_HOST)
    host_ps1 = read_text(CAN_BMS_HOST_START)
    comm_host = read_text(COMM_TOOL_HOST)
    upgrade_ui = read_text(COMM_TOOL_UPGRADE_UI)
    build_ui = read_text(COMM_TOOL_UPGRADE_UI_BUILD)
    comm_protocol_h = read_text(COMM_TOOL_SOURCE / "ct_protocol.h")
    comm_can_c = read_text(COMM_TOOL_SOURCE / "ct_can_gateway.c")
    comm_can_h = read_text(COMM_TOOL_SOURCE / "ct_can_gateway.h")
    comm_app_c = read_text(COMM_TOOL_SOURCE / "ct_app.c")
    serial_doc = read_text(COMM_TOOL_SERIAL_DOC)
    service_doc = read_text(BMS_CAN_SERVICE_DOC)
    aging_doc = read_text(BMS_CAN_AGING_SOC_DOC)
    upgrade_doc = read_text(COMM_TOOL_UPGRADE_UI_DOC)

    if (
        "FEIDAO_CAN_APP_CMD_AGING_START" in can_c
        and "FEIDAO_CAN_APP_CMD_AGING_STOP" in can_c
        and "FEIDAO_CAN_APP_CMD_AGING_RESET_TIME" in can_c
        and "FEIDAO_CAN_APP_CMD_AGING_SET_HOURS" in can_c
        and "FactoryAging_StartByHost" in can_c
        and "FactoryAging_StopByHost" in can_c
        and "FactoryAging_ResetTimeByHost" in can_c
        and "FactoryAging_SetDurationHoursByHost" in can_c
        and "FEIDAO_CAN_APP_AGING_GUARD" in can_c
    ):
        reporter.ok("CAN App service exposes separate guarded aging start/stop/reset/set-hours commands")
    else:
        reporter.fail("CAN App service should expose separate guarded aging start/stop/reset/set-hours commands")

    if (
        "FactoryAging_GetState" in frames_c
        and "FactoryAging_GetRemainingSeconds" in frames_c
        and "CanFeidao_PutU16Be(data, 3U" in frames_c
        and "FLASH_FACTORY_AGING_STATE_STOPPED" in flash_h
        and "FactoryAging_StartByHost" in aging_h
        and "FactoryAging_StopByHost" in aging_h
        and "FactoryAging_ResetTimeByHost" in aging_h
        and "FactoryAging_SetDurationHoursByHost" in aging_h
        and "u16DurationHours" in flash_h
        and "FACTORY_AGING_PUBLIC_STATE_RUNNING" in aging_h
        and "FACTORY_AGING_STATE_STOPPED" in aging_c
    ):
        reporter.ok("factory aging module reports remaining time and supports host-controlled aging restart/finish")
    else:
        reporter.fail("factory aging module should report remaining time and support host-controlled aging restart/finish")

    if (
        "APP_SET_ONCE_SOC_ADDR = 0x1005" in host_py
        and "app-write-soc" in host_py
        and "cmd_app_write_soc" in host_py
        and "app-aging-start" in host_py
        and "app-aging-stop" in host_py
        and "app-aging-reset-time" in host_py
        and "app-aging-set-hours" in host_py
        and "app-write-soc" in host_ps1
        and "ConfirmWriteSoc" in host_ps1
        and "ConfirmAgingStart" in host_ps1
        and "ConfirmAgingStop" in host_ps1
        and "ConfirmAgingResetTime" in host_ps1
        and "ConfirmAgingSetHours" in host_ps1
    ):
        reporter.ok("CAN host exposes SOC write as a common standalone feature and aging actions separately")
    else:
        reporter.fail("CAN host should expose app-write-soc and separate aging action modes")

    if (
        "CMD_BMS_AGING_CTRL = 0x12" in comm_host
        and "CMD_BMS_AGING_STATUS = 0x13" in comm_host
        and "CMD_BMS_AGING_SET_HOURS = 0x14" in comm_host
        and "APP_SET_ONCE_SOC_ADDR = 0x1005" in comm_host
        and "cmd_bms_write_soc" in comm_host
        and "cmd_bms_aging" in comm_host
        and "cmd_bms_aging_status" in comm_host
        and "cmd_bms_aging_set_hours" in comm_host
        and "CT_CMD_BMS_AGING_CTRL" in comm_protocol_h
        and "CT_CMD_BMS_AGING_STATUS" in comm_protocol_h
        and "CT_CMD_BMS_AGING_SET_HOURS" in comm_protocol_h
        and "CtCan_AppAgingControl" in comm_can_c
        and "CtCan_AppSetAgingHours" in comm_can_c
        and "CtCan_ReadFactoryAgingBroadcast" in comm_can_c
        and "CT_CAN_APP_AGING_ACTION_RESET" in comm_can_h
        and "CT_CAN_APP_AGING_SET_HOURS" in comm_can_h
        and "CT_CAN_FEIDAO_FACTORY_TIME_ID" in comm_can_h
        and "handle_bms_aging_ctrl" in comm_app_c
        and "handle_bms_aging_status" in comm_app_c
        and "handle_bms_aging_set_hours" in comm_app_c
        and "BMS_V1.13.1 - CAN用户上位机" in upgrade_ui
        and "写SOC" in upgrade_ui
        and "开启老化模式" in upgrade_ui
        and "关闭老化模式" in upgrade_ui
        and "重置老化时间" in upgrade_ui
        and "读取老化时间" in upgrade_ui
        and "CMD_BMS_AGING_CTRL" in upgrade_ui
        and "CMD_BMS_AGING_STATUS" in upgrade_ui
        and "CMD_BMS_AGING_SET_HOURS" in upgrade_ui
        and "BMS_PRODUCT_INFO_ADDR = 0xC002" in upgrade_ui
        and "BMS_PRODUCT_INFO_WORDS" in upgrade_ui
        and "_decode_product_info_words" in upgrade_ui
        and "product_info_var" in upgrade_ui
        and "BMS 版本/序列号" in upgrade_ui
        and "BMS_CommTool_Upgrade_UI" in build_ui
        and "--windowed" in build_ui
        and "--onefile" in build_ui
        and "pyinstaller" in build_ui
    ):
        reporter.ok("comm tool upgrade UI keeps original EXE name and exposes SOC/aging/product-info common controls")
    else:
        reporter.fail("comm tool upgrade UI should keep BMS_CommTool_Upgrade_UI.exe and expose SOC/aging/product-info controls")

    if (
        "0x1005" in service_doc
        and "app-write-soc" in service_doc
        and "0x07 AGING_START" in service_doc
        and "0x08 AGING_STOP" in service_doc
        and "0x09 AGING_RESET_TIME" in service_doc
        and "0x0A AGING_SET_HOURS" in service_doc
        and "老化剩余分钟" in service_doc
        and "BMS_AGING_CTRL" in serial_doc
        and "BMS_AGING_STATUS" in serial_doc
        and "BMS_AGING_SET_HOURS" in serial_doc
        and "读取老化时间" in aging_doc
        and "0xC002" in service_doc
        and "BMS 版本/序列号" in upgrade_doc
        and "BMS_CommTool_Upgrade_UI.exe" in aging_doc
        and "build_comm_tool_upgrade_ui_exe.ps1 -Clean" in aging_doc
        and "其它功能 -> 常用功能 -> 写SOC" in aging_doc
        and "不要另起新的 exe 名称" in aging_doc
        and "0x14F80208" in aging_doc
    ):
        reporter.ok("CAN aging/SOC documentation records standalone host functions, product info display, aging time display, and broadcast layout")
    else:
        reporter.fail("CAN aging/SOC docs should record original EXE, SOC/aging controls, product info display, UI aging time display, ch8 remaining minutes, and EXE overwrite rule")


def check_rtc_stop_sleep_contract(reporter):
    required_files = [RTC_C, RTC_SLEEP_C, RTC_SLEEP_PORT_C, CONF_C, RTC_SLEEP_OPT_DOC, RTC_SLEEP_PORT_REFACTOR_DOC]
    if any(not path.exists() for path in required_files):
        missing = [str(path.relative_to(ROOT)) for path in required_files if not path.exists()]
        reporter.fail("RTC low-power contract files missing: {0}".format(",".join(missing)))
        return

    rtc_c = read_text(RTC_C)
    rtc_sleep_c = read_text(RTC_SLEEP_C)
    rtc_sleep_port_c = read_text(RTC_SLEEP_PORT_C)
    conf_c = read_text(CONF_C)
    doc = read_text(RTC_SLEEP_OPT_DOC)
    port_doc = read_text(RTC_SLEEP_PORT_REFACTOR_DOC)

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
        "RtcSleep_PortRestoreAfterStop();" in rtc_sleep_c
        and "void RtcSleep_PortRestoreAfterStop(void)" in rtc_sleep_port_c
        and "InitRunAfterStopWakeup();" in rtc_sleep_port_c
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
        and "rtc_sleep_port.c" in port_doc
        and "rtc_sleep_afe_sh367309.c" in port_doc
        and "rtc_sleep.c" in port_doc
        and "过放" in doc
    ):
        reporter.ok("RTC sleep documents describe state machine, wakeup contract, and port boundary")
    else:
        reporter.fail("RTC sleep documents should cover ALR/SEC, EXTI17, recovery, over-discharge priority, and port boundary")


def check_app_architecture(reporter):
    required_files = [MAIN_C, MAIN_H, MOS_STARTUP_C, MOS_STARTUP_H, RUNTIME_C, CONF_C, DATADEAL_C, APP_ARCH_REFACTOR_DOC, REFACTOR_REQUIREMENTS_DOC]
    if any(not path.exists() for path in required_files):
        missing = [str(path.relative_to(ROOT)) for path in required_files if not path.exists()]
        reporter.fail("app architecture files missing: {0}".format(",".join(missing)))
        return

    main_c = read_text(MAIN_C)
    main_h = read_text(MAIN_H)
    mos_c = read_text(MOS_STARTUP_C)
    mos_h = read_text(MOS_STARTUP_H)
    runtime_c = read_text(RUNTIME_C)
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
        '#include "Runtime.h"' in main_c
        and "Runtime_Boot();" in main_c
        and "Runtime_RunOnce();" in main_c
        and "void InitDevice(" not in main_c
        and "void InitVar(" not in main_c
        and "void InitSci(" not in main_c
        and "void App_Sci(" not in main_c
        and "void Runtime_Boot(void)" in runtime_c
        and "void Runtime_RunOnce(void)" in runtime_c
        and "UINT8 SeriesNum" in runtime_c
        and "InitUSART_CommonUpper();" in runtime_c
        and "App_CommonUpper();" in runtime_c
        and "Runtime_RunFrontTasks" not in runtime_c
        and "Runtime_RunIoAndPowerTasks" not in runtime_c
        and "Runtime_RunBackgroundTasks" not in runtime_c
        and "Runtime_RunNormalOnce" not in runtime_c
    ):
        reporter.ok("boot initialization is owned by Runtime while main.c stays as a thin entry point")
    else:
        reporter.fail("boot initialization should stay in Runtime.c and main.c should only call Runtime_Boot/Runtime_RunOnce")

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
        and "SeriesSelect_AFE1" not in runtime_c
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
        and "Runtime.c" in doc
        and "SeriesSelect_AFE1" in doc
        and "Conf_InitRunSharedIo" in doc
        and "FD_Release" in doc
        and "0x03/0x06/0x10" in doc
    ):
        reporter.ok("architecture refactor document records module boundary, size optimization, and follow-up split order")
    else:
        reporter.fail("architecture refactor document should cover MosStartup, Runtime, compact cell mapping, checks, and next module split order")

    requirements = read_text(REFACTOR_REQUIREMENTS_DOC)
    required_terms = [
        "不可破坏约束",
        "目标架构",
        "Code/ROM 优化方法",
        "当前未提交改动判定",
        "Fault.c/Fault.h",
        "Sci_Upper.c",
        "0x08004800",
        "Project_Config.h",
    ]
    missing_terms = [term for term in required_terms if term not in requirements]
    if missing_terms:
        reporter.fail("refactor requirements document is missing: {0}".format(",".join(missing_terms)))
    else:
        reporter.ok("refactor requirements document records architecture goals, safety constraints, and size policy")


def check_runtime_docs(reporter):
    docs = [FLOW_DOC, COMM_ADDRESS_INDEX, CAN_RUNTIME_REFACTOR, CAN_MODULE_SIMPLIFY, CAN_POWER_RTC_SIMPLIFY]
    if any(not path.exists() for path in docs):
        missing = [str(path.relative_to(ROOT)) for path in docs if not path.exists()]
        reporter.fail("runtime documentation missing: {0}".format(",".join(missing)))
        return

    flow_doc = read_text(FLOW_DOC)
    comm_doc = read_text(COMM_ADDRESS_INDEX)
    can_runtime = read_text(CAN_RUNTIME_REFACTOR)
    can_simplify = read_text(CAN_MODULE_SIMPLIFY)
    can_power_rtc = read_text(CAN_POWER_RTC_SIMPLIFY)

    if (
        "LowPower_Request()" in flow_doc
        and "sleep_mode" in flow_doc
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
        and "RTC 周期 CAN 服务已删除" in can_runtime
        and "ABOM" in can_runtime
        and "CanFeidaoFrames.c/.h" in can_simplify
        and "Can_PrepareSleep()" in can_power_rtc
        and "Can_RtcWakeService()" in can_power_rtc
        and "已删除" in can_power_rtc
    ):
        reporter.ok("CAN docs describe current runtime/frame/low-power boundaries")
    else:
        reporter.fail("CAN docs should describe current runtime/frame/low-power boundaries")


def check_comm_tool_can_iap_contract(reporter):
    docs = [
        COMM_TOOL_ARCH_DOC,
        COMM_TOOL_SERIAL_DOC,
        BMS_CAN_SERVICE_DOC,
        BMS_CAN_IAP_DOC,
        BMS_CAN_IAP_RELIABILITY_DOC,
        COMM_TOOL_KEIL_DOC,
        COMM_TOOL_UART_SELECT_DOC,
        COMM_TOOL_BMS_REVIEW_HTML,
    ]
    source_files = [
        COMM_TOOL_HOST,
        COMM_TOOL_HOST_START,
        COMM_TOOL_UART_SELECT_SCRIPT,
        COMM_TOOL_SOURCE / "ct_config.h",
        COMM_TOOL_SOURCE / "ct_protocol.c",
        COMM_TOOL_SOURCE / "ct_debug_log.c",
        COMM_TOOL_SOURCE / "ct_debug_log.h",
        COMM_TOOL_SOURCE / "ct_flash_store.c",
        COMM_TOOL_SOURCE / "ct_can_gateway.c",
        COMM_TOOL_SOURCE / "ct_upgrade_manager.c",
        COMM_TOOL_SOURCE / "ct_boot_control.c",
        COMM_TOOL_SOURCE / "ct_self_iap.c",
        COMM_TOOL_SOURCE / "ct_app.c",
        ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "iap" / "ct_iap.c",
        ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "iap" / "ct_iap_main.c",
        COMM_TOOL_BSP_SOURCE / "board.c",
        COMM_TOOL_BSP_SOURCE / "board_uart.c",
        COMM_TOOL_BSP_SOURCE / "board_can.c",
        COMM_TOOL_KEIL_PROJECT,
        COMM_TOOL_IAP_PROJECT,
        ELOG_CFG_H,
        PROJECT_CONFIG,
        BUILD_GUARD,
    ]
    required = docs + source_files
    if any(not path.exists() for path in required):
        missing = [str(path.relative_to(ROOT)) for path in required if not path.exists()]
        reporter.fail("comm tool CAN-IAP files missing: {0}".format(",".join(missing)))
        return

    arch_doc = read_text(COMM_TOOL_ARCH_DOC)
    serial_doc = read_text(COMM_TOOL_SERIAL_DOC)
    service_doc = read_text(BMS_CAN_SERVICE_DOC)
    iap_doc = read_text(BMS_CAN_IAP_DOC)
    iap_reliability_doc = read_text(BMS_CAN_IAP_RELIABILITY_DOC)
    keil_doc = read_text(COMM_TOOL_KEIL_DOC)
    uart_select_doc = read_text(COMM_TOOL_UART_SELECT_DOC)
    review_html = read_text(COMM_TOOL_BMS_REVIEW_HTML)
    host_py = read_text(COMM_TOOL_HOST)
    start_ps1 = read_text(COMM_TOOL_HOST_START)
    uart_select_ps1 = read_text(COMM_TOOL_UART_SELECT_SCRIPT)
    config_h = read_text(COMM_TOOL_SOURCE / "ct_config.h")
    protocol_c = read_text(COMM_TOOL_SOURCE / "ct_protocol.c")
    debug_log_c = read_text(COMM_TOOL_SOURCE / "ct_debug_log.c")
    debug_log_h = read_text(COMM_TOOL_SOURCE / "ct_debug_log.h")
    flash_c = read_text(COMM_TOOL_SOURCE / "ct_flash_store.c")
    can_c = read_text(COMM_TOOL_SOURCE / "ct_can_gateway.c")
    upgrade_c = read_text(COMM_TOOL_SOURCE / "ct_upgrade_manager.c")
    app_c = read_text(COMM_TOOL_SOURCE / "ct_app.c")
    iap_c = read_text(ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "iap" / "ct_iap.c")
    board_c = read_text(COMM_TOOL_BSP_SOURCE / "board.c")
    board_uart_c = read_text(COMM_TOOL_BSP_SOURCE / "board_uart.c")
    board_can_c = read_text(COMM_TOOL_BSP_SOURCE / "board_can.c")
    comm_tool_uvprojx = read_text(COMM_TOOL_KEIL_PROJECT)
    comm_tool_iap_uvprojx = read_text(COMM_TOOL_IAP_PROJECT)
    elog_cfg_h = read_text(ELOG_CFG_H)
    project_config_h = read_text(PROJECT_CONFIG)
    build_guard_h = read_text(BUILD_GUARD)
    bms_can_c = read_text(CAN_HDX_C)

    if (
        "PC 上位机 <UART> comm tool" in arch_doc
        and "0x08004800" in arch_doc
        and "0x08008000" in arch_doc
        and "0x08018000" in arch_doc
        and "FW_BEGIN" in serial_doc
        and "FW_DATA" in serial_doc
        and "ConfirmAppAddress 0x08004800" in serial_doc
        and "ConfirmAppAddress 0x08008000" in serial_doc
        and "GET_STATUS" in service_doc
        and "ENTER_IAP" in service_doc
        and "0x14F8F000" in iap_doc
        and "0x14000000" in iap_doc
        and "COMMIT" in iap_doc
        and "0x0801F800" in iap_reliability_doc
        and "VTOR" in iap_reliability_doc
    ):
        reporter.ok("comm tool architecture and protocol documents record UART/CAN/IAP rules")
    else:
        reporter.fail("comm tool docs should record UART chain, 0x08004800, cache area, commands, and CAN-IAP commit protocol")

    if (
        "CMD_FW_BEGIN" in host_py
        and "CMD_FW_DATA" in host_py
        and "CMD_UPGRADE" in host_py
        and "BMS_APP_BASE_ADDR = 0x08004800" in host_py
        and "COMM_TOOL_APP_BASE_ADDR = 0x08008000" in host_py
        and "confirm_app_address" in host_py
        and "AppAddress" in start_ps1
        and "pyserial" in host_py
        and "comm_tool_host.py" in start_ps1
        and "ConfirmAppAddress" in start_ps1
    ):
        reporter.ok("PC comm tool host enforces serial protocol and App address confirmation")
    else:
        reporter.fail("PC comm tool host should support firmware download, upgrade commands, pyserial, and 0x08004800 confirmation")

    if (
        "CT_SELF_APP_BASE               0x08008000u" in config_h
        and "CT_FW_CACHE_BASE               0x08018000u" in config_h
        and "CtProtocol_Encode" in protocol_c
        and "CtDebugLog_Record" in protocol_c
        and "CtFlash_Begin" in flash_c
        and "CT_FW_CACHE_BASE" in flash_c
        and "CtCan_IapSendCommit" in can_c
        and "CtCan_IapWaitAck" in can_c
        and "CtUpgrade_Start" in upgrade_c
        and "CtUpgrade_Task" in upgrade_c
        and "frames_this_block" in upgrade_c
        and "frame_count = s_ctx.seq" in upgrade_c
        and "CT_CMD_FW_BEGIN" in app_c
        and "CT_CMD_UPGRADE" in app_c
        and "CtSelfIap_PollCan" in app_c
    ):
        reporter.ok("comm tool firmware source contains protocol, flash cache, CAN gateway, and upgrade manager")
    else:
        reporter.fail("comm tool firmware source should contain protocol parser, flash cache, CAN-IAP commit, ACK wait, and command dispatch")

    if (
        "CT_BUILD_PROFILE_RELEASE" in config_h
        and "CT_DEBUG_LOG_ENABLE" in config_h
        and "CT_DEBUG_LOG_CAPACITY" in config_h
        and "CMD_DEBUG_LOG" in host_py
        and "debug-log" in host_py
        and "CT_CMD_DEBUG_LOG" in app_c
        and "handle_debug_log" in app_c
        and "CtDebugLog_EncodeLatest" in app_c
        and "CtDebugLog_Record" in debug_log_h
        and "CT_DEBUG_LOG_ENABLE" in debug_log_c
        and "CT_LOG_EVT_CMD_RX" in debug_log_h
        and "ct_debug_log.c" in comm_tool_uvprojx
        and "ct_debug_log.h" in comm_tool_uvprojx
        and "串口" in review_html
        and "环形结构化日志" in review_html
        and "Release" in review_html
        and "Debug" in review_html
    ):
        reporter.ok("comm tool debug logging is structured, protocol-readable, and release gated")
    else:
        reporter.fail("comm tool debug logging should use CT_CMD_DEBUG_LOG ring records and stay disabled in Release")

    if "PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE" in project_config_h:
        reporter.fail("PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE should not be a current app config macro")
    else:
        reporter.ok("BMS App serial log macro is removed from current app config")

    if (
        "COMM_TOOL_F103RET6.uvprojx" in keil_doc
        and "COMM_TOOL_IAP.uvprojx" in keil_doc
        and "CT_COMM_UART_PORT" in keil_doc
        and "USART1" in keil_doc
        and "PB6" in keil_doc
        and "PB7" in keil_doc
        and "USART3" in keil_doc
        and "PC10" in keil_doc
        and "PC11" in keil_doc
        and "PA11" in keil_doc
        and "PA12" in keil_doc
        and "PB15" in keil_doc
        and "PC12" in keil_doc
        and "PC13" in keil_doc
        and "PD2" in keil_doc
        and "0x08008000" in keil_doc
        and "0x08018000" in keil_doc
        and "STM32F103RE" in comm_tool_uvprojx
        and "COMM_TOOL_Release" in comm_tool_uvprojx
        and "IROM(0x08008000,0x10000)" in comm_tool_uvprojx
        and "ct_app.c" in comm_tool_uvprojx
        and "board_uart.c" in comm_tool_uvprojx
        and "COMM_TOOL_IAP" in comm_tool_iap_uvprojx
        and "IROM(0x08000000,0x8000)" in comm_tool_iap_uvprojx
        and "ct_iap.c" in comm_tool_iap_uvprojx
        and "ct_iap_main.c" in comm_tool_iap_uvprojx
        and "CT_COMM_UART_PORT              CT_COMM_UART_PORT_USART1" in config_h
        and "CT_COMM_UART_PORT_USART3" in config_h
        and "BOARD_UART_INSTANCE" in board_uart_c
        and "BOARD_UART_IRQHandler" in board_uart_c
        and "GPIO_Remap_USART1" in board_uart_c
        and "GPIO_PartialRemap_USART3" in board_uart_c
        and "USART1_IRQHandler" in board_uart_c
        and "USART3" in board_uart_c
        and "IAP_SERIAL_USART" in iap_c
        and "IAP_SERIAL_IRQHandler" in iap_c
        and "GPIO_Remap_USART1" in iap_c
        and "GPIO_PartialRemap_USART3" in iap_c
        and "-Port USART1" in uart_select_doc
        and "-Port USART3" in uart_select_doc
        and "ValidateSet('USART1', 'USART3')" in uart_select_ps1
        and "USB_LP_CAN1_RX0_IRQHandler" in board_can_c
        and "CAN_FilterScale_32bit" in board_can_c
        and "GPIO_Pin_11" in board_can_c
        and "GPIO_Pin_12" in board_can_c
        and "BOARD_DEBUG_LED_PIN" in board_c
        and "BOARD_CAN_POWER_PIN" in board_c
        and "BOARD_PWSV_CTRL_PIN" in board_c
        and "BOARD_PWSV_STB_PIN" in board_c
        and "SysTick_Config" in board_c
    ):
        reporter.ok("comm tool Keil/BSP contract records RET6 UART selection, CAN, power, LED, and cache boundary")
    else:
        reporter.fail("comm tool Keil/BSP should fix RET6 App/IAP projects, UART selection, CAN PA11/PA12, power pins, PB15 LED, and 0x08018000 cache boundary")

    if (
        "FEIDAO_CAN_APP_CMD_GET_STATUS" in bms_can_c
        and "FEIDAO_CAN_APP_CMD_ENTER_IAP" in bms_can_c
        and "FEIDAO_CAN_APP_CMD_READ_BLOCK" in bms_can_c
        and "feidao_can_service_tx" in bms_can_c
        and "AppUpgrade_RequestIap() == 0U" in bms_can_c
        and "s_app.enter_iap_delay_ticks" in bms_can_c
    ):
        reporter.ok("BMS App exposes queued CAN service for status, block read, and entering IAP")
    else:
        reporter.fail("BMS App should expose queued CAN GET_STATUS/READ_BLOCK/ENTER_IAP service")


def check_serial_iap_refactor_contract(reporter):
    required = [
        BMS_SERIAL_IAP_REFACTOR_DOC,
        FLASH_C,
        FLASH_H,
        SCI_UPPER_C,
        CAN_HDX_C,
    ]
    if any(not path.exists() for path in required):
        missing = [str(path.relative_to(ROOT)) for path in required if not path.exists()]
        reporter.fail("serial IAP refactor files missing: {0}".format(",".join(missing)))
        return

    doc = read_text(BMS_SERIAL_IAP_REFACTOR_DOC)
    flash_c = read_text(FLASH_C)
    flash_h = read_text(FLASH_H)
    sci_upper_c = read_text(SCI_UPPER_C)
    can_hdx_c = read_text(CAN_HDX_C)

    if (
        "0xFFFD" in doc
        and "0xFFFE" in doc
        and "0xFFFF" in doc
        and "0x0801F800" in doc
        and "AppUpgrade_RequestIap" in flash_h
        and "APP_UPGRADE_MAILBOX_ADDR" in flash_c
        and "AppUpgrade_IsIapRequested()" in flash_c
        and "AppUpgrade_RequestIap() == 0U" in sci_upper_c
        and "AppUpgrade_RequestIap() == 0U" in can_hdx_c
    ):
        reporter.ok("serial IAP refactor keeps protocol entry and verifies App-to-IAP mailbox writes")
    else:
        reporter.fail("serial IAP refactor should document 0xFFFD/0xFFFE/0xFFFF and use AppUpgrade_RequestIap mailbox readback")


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
    check_runtime_debug_isolation(reporter)
    check_removed_legacy_modules(reporter)
    check_release_defaults(reporter)
    check_required_board_features(reporter)
    check_guard_includes(reporter)
    check_build_guard(reporter)
    check_release_map(reporter)
    check_gitignore(reporter)
    check_hooks(reporter)
    check_soc_parameter_side_effects(reporter)
    check_sci_host_write_policy(reporter)
    check_soc_current_and_typec_policy(reporter)
    check_low_power_cleanup(reporter)
    check_global_state_phase1(reporter)
    check_adc_state_runtime(reporter)
    check_datadeal_runtime_state(reporter)
    check_fault_snapshot_mapping(reporter)
    check_can_rtc_service_runtime(reporter)
    check_can_aging_soc_service(reporter)
    check_rtc_stop_sleep_contract(reporter)
    check_app_architecture(reporter)
    check_runtime_docs(reporter)
    check_comm_tool_can_iap_contract(reporter)
    check_serial_iap_refactor_contract(reporter)

    return reporter.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
