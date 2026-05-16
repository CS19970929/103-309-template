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
BUILD_GUARD = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "Project_BuildGuard.h"
CONF_H = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "conf.h"
PROJECT_FEATURES_H = ROOT / "103 + 309" / "Project" / "Source" / "Project_Features.h"
PROJECT_PROTECTION_H = ROOT / "103 + 309" / "Project" / "Source" / "Project_Protection.h"
PROJECT_TYPES_H = ROOT / "103 + 309" / "Project" / "Source" / "Project_Types.h"
PLATFORM_PORT_H = ROOT / "103 + 309" / "Project" / "Source" / "Platform_Port.h"
BMS_MODEL_H = ROOT / "103 + 309" / "Project" / "Source" / "BmsModel.h"
PROJECT_APP_TASKS_H = ROOT / "103 + 309" / "Project" / "Source" / "Project_AppTasks.h"
BOARD_CONTROL_H = ROOT / "103 + 309" / "Project" / "Source" / "BoardControl.h"
ELOG_CFG_H = ROOT / "103 + 309" / "Project" / "Source" / "easylogger" / "inc" / "elog_cfg.h"
ADC_H = ROOT / "103 + 309" / "Project" / "Source" / "ADC.h"
PUBFUNC_C = ROOT / "103 + 309" / "Project" / "Source" / "PubFunc.c"
PUBFUNC_H = ROOT / "103 + 309" / "Project" / "Source" / "PubFunc.h"
SYSTEM_MONITOR_C = ROOT / "103 + 309" / "Project" / "Source" / "System_Monitor.c"
SYSTEM_MONITOR_H = ROOT / "103 + 309" / "Project" / "Source" / "System_Monitor.h"
CHARGER_LOAD_FUNC_C = ROOT / "103 + 309" / "Project" / "Source" / "ChargerLoadFunc.c"
CHARGER_LOAD_FUNC_H = ROOT / "103 + 309" / "Project" / "Source" / "ChargerLoadFunc.h"
IODRIVERS_C = ROOT / "103 + 309" / "Project" / "Source" / "IODrivers.c"
DATADEAL_C = ROOT / "103 + 309" / "Project" / "Source" / "DataDeal.c"
SH367309_FUNC_C = ROOT / "103 + 309" / "Project" / "Source" / "SH367309_Func.c"
SOC_C = ROOT / "103 + 309" / "Project" / "Source" / "SOC.c"
SOC_ENHANCE_C = ROOT / "103 + 309" / "Project" / "Source" / "SocEnhance.c"
RUNTIME_C = ROOT / "103 + 309" / "Project" / "Source" / "Runtime.c"
MAIN_C = ROOT / "103 + 309" / "Project" / "Source" / "main.c"
SCI_UPPER_C = ROOT / "103 + 309" / "Project" / "Source" / "Sci_Upper.c"
SCI_UPPER_H = ROOT / "103 + 309" / "Project" / "Source" / "Sci_Upper.h"
SLEEPDEAL_C = ROOT / "103 + 309" / "Project" / "Source" / "SleepDeal.c"
SLEEPDEAL_H = ROOT / "103 + 309" / "Project" / "Source" / "SleepDeal.h"
RTC_SLEEP_C = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep.c"
RTC_SLEEP_H = ROOT / "103 + 309" / "Project" / "Source" / "rtc_sleep.h"
CAN_HDX_C = ROOT / "103 + 309" / "Project" / "Source" / "Can_HDX.c"
CAN_HDX_H = ROOT / "103 + 309" / "Project" / "Source" / "Can_HDX.h"
CAN_FEIDAO_FRAMES_C = ROOT / "103 + 309" / "Project" / "Source" / "CanFeidaoFrames.c"
FLASH64K_APP_TEST_H = ROOT / "103 + 309" / "Project" / "Source" / "Flash64KAppTest.h"
FLASH64K_APP_TEST_C = ROOT / "103 + 309" / "Project" / "Source" / "Flash64KAppTest.c"
LEDBAR_C = ROOT / "103 + 309" / "Project" / "Source" / "LedBar.c"
LOGRECORD_C = ROOT / "103 + 309" / "Project" / "Source" / "LogRecord.c"
LOW_POWER_SLEEP_C = ROOT / "103 + 309" / "Project" / "Source" / "LowPowerSleep.c"
PRODUCTION_ID_C = ROOT / "103 + 309" / "Project" / "Source" / "ProductionID.c"
FACTORY_AGING_C = ROOT / "103 + 309" / "Project" / "Source" / "FactoryAging.c"
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
PORTABILITY_DOC = ROOT / "PORTABILITY_DECOUPLING_FOUNDATION_2026-05-16.md"
PROTECTION_DOC = ROOT / "PROTECTION_STRATEGY_CONFIG_2026-05-16.md"
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
    "PROJECT_CFG_PROTECTION_MODE": "0",
}
GUARD_REQUIRED_TOKENS = [
    "PROJECT_CFG_WDOG_ENABLE",
    "PROJECT_CFG_DEBUG_CODE_ENABLE",
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
    "PROJECT_CFG_PROTECTION_MODE",
    "PROJECT_CFG_FEATURE_AFE",
    "PROJECT_CFG_FEATURE_SOC",
    "PROJECT_CFG_FEATURE_ANALOG_ADC",
    "PROJECT_CFG_FEATURE_RS485",
    "PROJECT_CFG_FEATURE_CAN",
    "PROJECT_CFG_FEATURE_LEDBAR",
    "PROJECT_CFG_FEATURE_STORAGE",
    "PROJECT_CFG_FEATURE_LOG_RECORD",
    "PROJECT_CFG_FEATURE_PRODUCTION_ID",
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
    required = [
        PROJECT,
        PROJECT_CONFIG,
        BUILD_GUARD,
        CONF_H,
        PROJECT_FEATURES_H,
        PROJECT_PROTECTION_H,
        PROJECT_TYPES_H,
        PLATFORM_PORT_H,
        BMS_MODEL_H,
        PROJECT_APP_TASKS_H,
        BOARD_CONTROL_H,
        ELOG_CFG_H,
        GITIGNORE,
        PRE_COMMIT,
        PRE_PUSH,
    ]
    for path in required:
        if path.exists():
            reporter.ok("required file exists: {0}".format(path.relative_to(ROOT)))
        else:
            reporter.fail("required file missing: {0}".format(path.relative_to(ROOT)))


def check_utf8_text_files(reporter):
    invalid = []
    bom = []

    for path in iter_git_tracked_files():
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
        reporter.fail("tracked source/text files must be UTF-8: {0}".format("; ".join(invalid[:8])))
    else:
        reporter.ok("tracked source/text files are valid UTF-8")

    if bom:
        reporter.fail("tracked source/text files should use UTF-8 without BOM: {0}".format("; ".join(bom[:8])))
    else:
        reporter.ok("tracked source/text files do not contain UTF-8 BOM")


def check_project_config_wizard_encoding(reporter):
    if not PROJECT_CONFIG.exists():
        return

    try:
        text = PROJECT_CONFIG.read_bytes().decode("utf-8")
    except UnicodeDecodeError as exc:
        reporter.fail("Project_Config.h must be saved as UTF-8 for shared editing and Keil use: {0}".format(exc))
        return

    if PROJECT_CONFIG_WIZARD_MARKER in text:
        reporter.ok("Project_Config.h UTF-8 text is readable and keeps Keil Configuration Wizard marker")
    else:
        reporter.fail("Project_Config.h UTF-8 text marker is missing or unreadable for Keil Configuration Wizard")


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

    if release and debug:
        if release["output_name"] == debug["output_name"]:
            reporter.fail("FD_Release and FD_Debug share the same OutputName")
        else:
            reporter.ok("Keil targets use separate output names")


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


def check_portability_foundation(reporter):
    required_files = [
        PROJECT_CONFIG,
        PROJECT_FEATURES_H,
        PROJECT_PROTECTION_H,
        PROJECT_TYPES_H,
        PLATFORM_PORT_H,
        BMS_MODEL_H,
        PROJECT_APP_TASKS_H,
        RUNTIME_C,
        MAIN_C,
        DATADEAL_C,
        SH367309_FUNC_C,
        RTC_SLEEP_C,
        SOC_C,
        ADC_H,
        PUBFUNC_C,
        PUBFUNC_H,
        SYSTEM_MONITOR_C,
        SYSTEM_MONITOR_H,
        CHARGER_LOAD_FUNC_C,
        CHARGER_LOAD_FUNC_H,
        IODRIVERS_C,
        CAN_FEIDAO_FRAMES_C,
        FLASH64K_APP_TEST_H,
        FLASH64K_APP_TEST_C,
        LEDBAR_C,
        LOGRECORD_C,
        LOW_POWER_SLEEP_C,
        PRODUCTION_ID_C,
        FACTORY_AGING_C,
        BUILD_GUARD,
        PORTABILITY_DOC,
        PROTECTION_DOC,
    ]
    if any(not path.exists() for path in required_files):
        missing = [str(path.relative_to(ROOT)) for path in required_files if not path.exists()]
        reporter.fail("portability foundation files missing: {0}".format(",".join(missing)))
        return

    project_config = read_text(PROJECT_CONFIG)
    project_features = read_text(PROJECT_FEATURES_H)
    project_protection = read_text(PROJECT_PROTECTION_H)
    project_types = read_text(PROJECT_TYPES_H)
    platform_port = read_text(PLATFORM_PORT_H)
    bms_model = read_text(BMS_MODEL_H)
    project_app_tasks = read_text(PROJECT_APP_TASKS_H)
    board_control = read_text(BOARD_CONTROL_H)
    runtime_c = read_text(RUNTIME_C)
    main_c = read_text(MAIN_C)
    datadeal_c = read_text(DATADEAL_C)
    sh367309_func_c = read_text(SH367309_FUNC_C)
    rtc_sleep_c = read_text(RTC_SLEEP_C)
    soc_c = read_text(SOC_C)
    adc_c = read_text(ROOT / "103 + 309" / "Project" / "Source" / "ADC.c")
    adc_h = read_text(ADC_H)
    pubfunc_c = read_text(PUBFUNC_C)
    pubfunc_h = read_text(PUBFUNC_H)
    system_monitor_c = read_text(SYSTEM_MONITOR_C)
    system_monitor_h = read_text(SYSTEM_MONITOR_H)
    charger_load_func_c = read_text(CHARGER_LOAD_FUNC_C)
    charger_load_func_h = read_text(CHARGER_LOAD_FUNC_H)
    iodrivers_c = read_text(IODRIVERS_C)
    can_frames_c = read_text(CAN_FEIDAO_FRAMES_C)
    flash64k_app_test_h = read_text(FLASH64K_APP_TEST_H)
    flash64k_app_test_c = read_text(FLASH64K_APP_TEST_C)
    ledbar_c = read_text(LEDBAR_C)
    logrecord_c = read_text(LOGRECORD_C)
    low_power_sleep_c = read_text(LOW_POWER_SLEEP_C)
    production_id_c = read_text(PRODUCTION_ID_C)
    factory_aging_c = read_text(FACTORY_AGING_C)
    build_guard = read_text(BUILD_GUARD)
    doc = read_text(PORTABILITY_DOC)
    protection_doc = read_text(PROTECTION_DOC)

    feature_tokens = [
        "PROJECT_CFG_FEATURE_AFE",
        "PROJECT_CFG_PROTECTION_MODE",
        "PROJECT_CFG_FEATURE_SOC",
        "PROJECT_CFG_FEATURE_ANALOG_ADC",
        "PROJECT_CFG_FEATURE_RS485",
        "PROJECT_CFG_FEATURE_CAN",
        "PROJECT_CFG_FEATURE_LEDBAR",
        "PROJECT_CFG_FEATURE_STORAGE",
        "PROJECT_CFG_FEATURE_LOG_RECORD",
        "PROJECT_CFG_FEATURE_PRODUCTION_ID",
    ]
    if "模块裁剪开关" in project_config and all(token in project_config for token in feature_tokens):
        reporter.ok("Project_Config.h exposes module feature switches")
    else:
        reporter.fail("Project_Config.h should expose module feature switches")

    feature_map_tokens = [
        "PROJECT_FEATURE_AFE           PROJECT_CFG_FEATURE_AFE",
        "PROJECT_FEATURE_AFE_HARDWARE_PROTECTION PROJECT_PROTECTION_USES_AFE_HARDWARE",
        "PROJECT_FEATURE_SOFTWARE_PROTECTION PROJECT_PROTECTION_USES_MCU_SOFTWARE",
        "PROJECT_FEATURE_SOC           PROJECT_CFG_FEATURE_SOC",
        "PROJECT_FEATURE_ANALOG_ADC    PROJECT_CFG_FEATURE_ANALOG_ADC",
        "PROJECT_FEATURE_RS485         PROJECT_CFG_FEATURE_RS485",
        "PROJECT_FEATURE_CAN           PROJECT_CFG_FEATURE_CAN",
        "PROJECT_FEATURE_LEDBAR        PROJECT_CFG_FEATURE_LEDBAR",
        "PROJECT_FEATURE_STORAGE       PROJECT_CFG_FEATURE_STORAGE",
        "PROJECT_FEATURE_LOG_RECORD    PROJECT_CFG_FEATURE_LOG_RECORD",
        "PROJECT_FEATURE_PRODUCTION_ID PROJECT_CFG_FEATURE_PRODUCTION_ID",
    ]
    if all(token in project_features for token in feature_map_tokens):
        reporter.ok("Project_Features.h maps config switches to runtime feature gates")
    else:
        reporter.fail("Project_Features.h should map config switches to runtime feature gates")

    if (
        "PROJECT_PROTECTION_MODE_AFE_HARDWARE_ONLY" in project_protection
        and "PROJECT_PROTECTION_MODE_MCU_SOFTWARE" in project_protection
        and "PROJECT_PROTECTION_MODE_HYBRID" in project_protection
        and "PROJECT_PROTECTION_USES_AFE_HARDWARE 1" in project_protection
        and "PROJECT_PROTECTION_USES_MCU_SOFTWARE 1" in project_protection
    ):
        reporter.ok("Project_Protection.h defines one-switch hardware/software protection modes")
    else:
        reporter.fail("Project_Protection.h should define one-switch hardware/software protection modes")

    if (
        "BMS_INLINE" in project_types
        and "typedef uint8_t bms_u8;" in project_types
        and "DELAYB10MS_100MS" in project_types
        and "typedef enum _BOOL" in project_types
    ):
        reporter.ok("Project_Types.h defines portable aliases and shared legacy constants")
    else:
        reporter.fail("Project_Types.h should provide portable aliases, BMS_INLINE, and shared legacy constants")

    if (
        "PROJECT_PLATFORM_STM32F1_SPL" in platform_port
        and "Platform_FeedWatchdog" in platform_port
        and "Platform_LatchTaskFlags" in platform_port
        and "Platform_Get10msTick" in platform_port
    ):
        reporter.ok("Platform_Port.h exposes MCU platform wrappers")
    else:
        reporter.fail("Platform_Port.h should expose MCU platform wrappers")

    if (
        "BmsModel_CellInfoConst" in bms_model
        and "BmsModel_GetSocPercent" in bms_model
        and "BmsModel_SystemStatusConst" in bms_model
        and "BmsModel_GetPackVoltageMv" in bms_model
    ):
        reporter.ok("BmsModel.h exposes central runtime model accessors")
    else:
        reporter.fail("BmsModel.h should expose central runtime model accessors")

    if (
        "void App_AFEGet(void);" in project_app_tasks
        and "void App_Can(void);" in project_app_tasks
        and "void App_WarnCtrl(void);" in project_app_tasks
        and "void InitSci(void);" in project_app_tasks
        and "void InitSystemWakeUp(void);" in project_app_tasks
    ):
        reporter.ok("Project_AppTasks.h exposes runtime/init task prototypes without main.h")
    else:
        reporter.fail("Project_AppTasks.h should expose runtime/init task prototypes without main.h")

    if "void enter_fac_mode(bool on);" in board_control and "open_chg_close_dsg" in board_control:
        reporter.ok("BoardControl.h exposes board-level MOS/factory-mode controls")
    else:
        reporter.fail("BoardControl.h should expose board-level MOS/factory-mode controls")

    runtime_tokens = [
        '#include "Project_Features.h"',
        '#include "Platform_Port.h"',
        '#include "Project_AppTasks.h"',
        "#if PROJECT_FEATURE_AFE",
        "#if PROJECT_FEATURE_RS485",
        "#if PROJECT_FEATURE_RTC_LOW_POWER",
        "#if PROJECT_FEATURE_CAN",
        "#if PROJECT_FEATURE_LEDBAR",
        "#if PROJECT_FEATURE_SOFTWARE_PROTECTION",
        "#if PROJECT_FEATURE_STORAGE",
        "Platform_FeedWatchdog();",
        "App_WarnCtrl();",
    ]
    if all(token in runtime_c for token in runtime_tokens):
        reporter.ok("Runtime.c dispatch uses feature gates and platform wrappers")
    else:
        reporter.fail("Runtime.c should dispatch through feature gates and platform wrappers")

    if '#include "main.h"' not in runtime_c:
        reporter.ok("Runtime.c no longer depends on the main.h include umbrella")
    else:
        reporter.fail("Runtime.c should not include main.h")

    init_tokens = [
        "#if PROJECT_FEATURE_RTC_LOW_POWER",
        "#if PROJECT_FEATURE_RS485",
        "#if PROJECT_FEATURE_AFE",
        "#if PROJECT_FEATURE_CAN",
        "#if PROJECT_FEATURE_ANALOG_ADC",
        "#if PROJECT_FEATURE_SOC",
        "#if defined(wdog_enable) && PROJECT_FEATURE_WATCHDOG",
    ]
    if all(token in main_c for token in init_tokens):
        reporter.ok("main.c initialization path follows feature gates")
    else:
        reporter.fail("main.c initialization path should follow feature gates")

    if "#if PROJECT_FEATURE_SOC" in datadeal_c and "App_SOC();" in datadeal_c:
        reporter.ok("DataDeal.c gates AFE-triggered SOC execution")
    else:
        reporter.fail("DataDeal.c should gate AFE-triggered SOC execution")

    soc_forbidden_tokens = [
        '#include "main.h"',
        "g_stCellInfoReport",
        "OtherElement",
        "System_Func_StartUp",
        "g_u32AfeCurrentSampleSeq",
    ]
    if (
        '#include "BmsModel.h"' in soc_c
        and '#include "ADC.h"' in soc_c
        and all(token not in soc_c for token in soc_forbidden_tokens)
    ):
        reporter.ok("SOC.c uses explicit inputs and BmsModel instead of main.h/global model access")
    else:
        reporter.fail("SOC.c should use explicit inputs and BmsModel instead of main.h/global model access")

    if (
        '#include "BmsModel.h"' in can_frames_c
        and '#include "main.h"' not in can_frames_c
        and "g_stCellInfoReport" not in can_frames_c
        and "BmsModel_GetSocPercent" in can_frames_c
    ):
        reporter.ok("CAN Feidao frames read runtime data through BmsModel accessors without main.h")
    else:
        reporter.fail("CAN Feidao frames should read runtime data through BmsModel accessors without main.h")

    if '#include "Flash.h"' in flash64k_app_test_h and '#include "stm32f10x.h"' in flash64k_app_test_h:
        reporter.ok("Flash64KAppTest.h declares its own storage dependencies")
    else:
        reporter.fail("Flash64KAppTest.h should declare its own storage dependencies")

    if (
        '#include "main.h"' not in flash64k_app_test_c
        and '#include "Flash64KAppTest.h"' in flash64k_app_test_c
        and '#include "System_Init.h"' in flash64k_app_test_c
        and '#include "PubFunc.h"' in flash64k_app_test_c
    ):
        reporter.ok("Flash64KAppTest.c declares specific dependencies instead of main.h")
    else:
        reporter.fail("Flash64KAppTest.c should declare specific dependencies instead of main.h")

    if '#include "BmsModel.h"' in ledbar_c and "g_stCellInfoReport" not in ledbar_c and "SystemStatus.bits" not in ledbar_c:
        reporter.ok("LedBar.c reads SOC/fault/MOS status through BmsModel accessors")
    else:
        reporter.fail("LedBar.c should read SOC/fault/MOS status through BmsModel accessors")

    if (
        '#include "main.h"' not in ledbar_c
        and '#include "LedBar.h"' in ledbar_c
        and '#include "System_Init.h"' in ledbar_c
        and '#include "rtc_sleep.h"' in ledbar_c
    ):
        reporter.ok("LedBar.c declares display/runtime dependencies without main.h")
    else:
        reporter.fail("LedBar.c should declare display/runtime dependencies without main.h")

    if (
        '#include "main.h"' not in pubfunc_c
        and '#include "PubFunc.h"' in pubfunc_c
        and '#include "System_Monitor.h"' in pubfunc_c
        and '#include "conf.h"' in pubfunc_c
        and '#include "stm32f10x.h"' in pubfunc_h
    ):
        reporter.ok("PubFunc.c/h declare utility dependencies without main.h")
    else:
        reporter.fail("PubFunc.c/h should declare utility dependencies without main.h")

    if (
        '#include "main.h"' not in system_monitor_c
        and '#include "System_Monitor.h"' in system_monitor_c
        and '#include "EEPROM.h"' in system_monitor_c
        and "SYSTEM_MONITOR_STATUS_CLOSE" in system_monitor_c
        and '#include "stm32f10x.h"' in system_monitor_h
    ):
        reporter.ok("System_Monitor.c/h declare monitor/storage dependencies without main.h")
    else:
        reporter.fail("System_Monitor.c/h should declare monitor/storage dependencies without main.h")

    if (
        '#include "main.h"' not in charger_load_func_c
        and '#include "ChargerLoadFunc.h"' in charger_load_func_c
        and '#include "System_Init.h"' in charger_load_func_c
        and '#include "rtc_sleep.h"' in charger_load_func_c
        and '#include "stm32f10x.h"' in charger_load_func_h
    ):
        reporter.ok("ChargerLoadFunc.c/h declare charger/load dependencies without main.h")
    else:
        reporter.fail("ChargerLoadFunc.c/h should declare charger/load dependencies without main.h")

    if (
        '#include "main.h"' not in adc_c
        and '#include "ADC.h"' in adc_c
        and '#include "BmsModel.h"' in adc_c
        and "Platform_FeedWatchdog();" in adc_c
        and "Platform_Get10msTick()" in adc_c
        and '#include "Project_Types.h"' in adc_h
    ):
        reporter.ok("ADC.c uses platform/model accessors instead of main.h")
    else:
        reporter.fail("ADC.c should use platform/model accessors instead of main.h")

    if (
        "#define DELAYB10MS_5S" not in iodrivers_c
        and "#define UPDNLMT16" not in iodrivers_c
    ):
        reporter.ok("IODrivers.c reuses shared timing/limit macros from Project_Types.h")
    else:
        reporter.fail("IODrivers.c should not redefine shared timing/limit macros")

    if (
        '#include "Project_Protection.h"' in sh367309_func_c
        and "#if PROJECT_PROTECTION_USES_AFE_HARDWARE" in sh367309_func_c
        and "Fault_ChangeToMCU();" in sh367309_func_c
    ):
        reporter.ok("SH367309 hardware fault mirroring follows protection mode")
    else:
        reporter.fail("SH367309 hardware fault mirroring should be gated by protection mode")

    if (
        '#include "Project_Protection.h"' in rtc_sleep_c
        and "#if PROJECT_PROTECTION_USES_AFE_HARDWARE" in rtc_sleep_c
        and "Fault_ChangeToMCU();" in rtc_sleep_c
    ):
        reporter.ok("RTC wake fault mirroring follows protection mode")
    else:
        reporter.fail("RTC wake fault mirroring should be gated by protection mode")

    if (
        '#include "main.h"' not in logrecord_c
        and '#include "BmsModel.h"' in logrecord_c
        and "g_stCellInfoReport" not in logrecord_c
        and "SystemStatus.bits" not in logrecord_c
    ):
        reporter.ok("LogRecord.c reads fault/status data through BmsModel accessors without main.h")
    else:
        reporter.fail("LogRecord.c should read fault/status data through BmsModel accessors without main.h")

    if (
        '#include "main.h"' not in low_power_sleep_c
        and '#include "Can_HDX.h"' in low_power_sleep_c
        and '#include "LowPowerSleep.h"' in low_power_sleep_c
        and '#include "SocEnhance.h"' in low_power_sleep_c
    ):
        reporter.ok("LowPowerSleep.c declares sleep-save dependencies without main.h")
    else:
        reporter.fail("LowPowerSleep.c should declare sleep-save dependencies without main.h")

    if (
        '#include "main.h"' not in production_id_c
        and '#include "ProductionID.h"' in production_id_c
        and '#include "DataDeal.h"' in production_id_c
    ):
        reporter.ok("ProductionID.c declares production data dependencies without main.h")
    else:
        reporter.fail("ProductionID.c should declare production data dependencies without main.h")

    if (
        '#include "main.h"' not in factory_aging_c
        and '#include "BoardControl.h"' in factory_aging_c
        and '#include "Flash.h"' in factory_aging_c
        and '#include "System_Init.h"' in factory_aging_c
    ):
        reporter.ok("FactoryAging.c declares board/storage/tick dependencies without main.h")
    else:
        reporter.fail("FactoryAging.c should declare board/storage/tick dependencies without main.h")

    if "PROJECT_CFG_FEATURE_SOC && !PROJECT_CFG_FEATURE_AFE" in build_guard:
        reporter.ok("Project_BuildGuard.h blocks SOC enabled while AFE runtime is disabled")
    else:
        reporter.fail("Project_BuildGuard.h should block SOC enabled while AFE runtime is disabled")

    if (
        "STM32F0" in doc
        and "Project_Features.h" in doc
        and "Platform_Port.h" in doc
        and "BmsModel.h" in doc
        and "Project_AppTasks.h" in doc
        and "Project_Protection.h" in doc
        and "第二阶段" in doc
        and "PROJECT_CFG_PROTECTION_MODE" in protection_doc
        and "PROJECT_PROTECTION_MODE_AFE_HARDWARE_ONLY" in protection_doc
        and "PROJECT_PROTECTION_MODE_MCU_SOFTWARE" in protection_doc
    ):
        reporter.ok("portability documentation describes the first-stage decoupling boundary")
    else:
        reporter.fail("portability documentation should describe the current decoupling boundary")


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
        and "PROJECT_CFG_PROTECTION_MODE" in flow_doc
        and "App_WarnCtrl()" in flow_doc
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
    check_release_defaults(reporter)
    check_guard_includes(reporter)
    check_build_guard(reporter)
    check_gitignore(reporter)
    check_hooks(reporter)
    check_soc_parameter_side_effects(reporter)
    check_soc_current_and_typec_policy(reporter)
    check_low_power_cleanup(reporter)
    check_fault_snapshot_mapping(reporter)
    check_can_rtc_service_runtime(reporter)
    check_portability_foundation(reporter)
    check_runtime_docs(reporter)

    return reporter.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
