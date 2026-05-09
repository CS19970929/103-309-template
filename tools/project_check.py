#!/usr/bin/env python3
"""
Lightweight project consistency checks for the Keil firmware project.

The script intentionally avoids Keil/ARMCC dependencies. It is meant to catch
release/debug profile mistakes before commit, push, or manual release builds.
"""

from __future__ import print_function

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "103 + 309" / "Project" / "Users" / "CommomSH367309_16series_103RCT6_C.uvprojx"
PROJECT_CONFIG = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "Project_Config.h"
BUILD_GUARD = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "Project_BuildGuard.h"
CONF_H = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "conf.h"
ELOG_CFG_H = ROOT / "103 + 309" / "Project" / "Source" / "easylogger" / "inc" / "elog_cfg.h"
GITIGNORE = ROOT / ".gitignore"
PRE_COMMIT = ROOT / ".githooks" / "pre-commit"
PRE_PUSH = ROOT / ".githooks" / "pre-push"
PROJECT_CONFIG_WIZARD_MARKER = "\u9879\u76ee\u53ef\u89c6\u5316\u914d\u7f6e"


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
    "PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE": "0",
    "PROJECT_CFG_FLASH64K_USE_TEST_ENABLE": "0",
    "PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE": "0",
    "PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST": "0",
    "PROJECT_CFG_SOC_TEST_MODE_ENABLE": "0",
    "PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY": "0",
}
GUARD_REQUIRED_TOKENS = [
    "PROJECT_CFG_WDOG_ENABLE",
    "PROJECT_CFG_DEBUG_CODE_ENABLE",
    "PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE",
    "PROJECT_CFG_FLASH64K_USE_TEST_ENABLE",
    "PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST",
    "PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY",
    "PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS",
    "PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS",
    "PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT",
    "PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV",
    "PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV",
    "PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV",
    "PROJECT_CFG_SOC_ONLINE_OCV_GUARD_ENABLE",
    "PROJECT_CFG_SOC_ONLINE_OCV_CORRECTION_SECONDS",
    "PROJECT_CFG_SOC_ONLINE_OCV_MIN_DELTA_PERCENT",
    "PROJECT_CFG_SOC_ONLINE_OCV_CURRENT_DIVIDER",
    "PROJECT_CFG_SOC_ONLINE_OCV_HEAVY_DSG_CURRENT_DIVIDER",
    "PROJECT_CFG_SOC_ONLINE_OCV_HEAVY_DSG_HOLDOFF_SECONDS",
    "PROJECT_CFG_SOC_ONLINE_OCV_STABLE_SECONDS",
    "PROJECT_CFG_SOC_ONLINE_OCV_STABLE_WINDOW_MV",
    "PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV",
    "PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV",
    "PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV",
    "PROJECT_CFG_SOC_EMPTY_FAST_MV",
    "PROJECT_CFG_SOC_EMPTY_FORCE_MV",
    "PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS",
    "PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV",
    "PROJECT_CFG_SOC_REST_OCV_SECONDS",
    "PROJECT_CFG_SOC_LOW_GUARD_MV",
    "PROJECT_CFG_SOC_LOW_GUARD_CRITICAL_MV",
    "PROJECT_CFG_SOC_LOW_GUARD_MARGIN_PERCENT",
    "PROJECT_CFG_SOC_LOW_GUARD_CRIT_MARGIN_PERCENT",
    "PROJECT_CFG_SOC_LOW_GUARD_SECONDS",
    "PROJECT_CFG_SOC_LOW_GUARD_CURRENT_DIVIDER",
    "PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT",
    "PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT",
    "PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT",
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
    for path in [PROJECT, PROJECT_CONFIG, BUILD_GUARD, CONF_H, ELOG_CFG_H, GITIGNORE, PRE_COMMIT, PRE_PUSH]:
        if path.exists():
            reporter.ok("required file exists: {0}".format(path.relative_to(ROOT)))
        else:
            reporter.fail("required file missing: {0}".format(path.relative_to(ROOT)))


def check_project_config_wizard_encoding(reporter):
    if not PROJECT_CONFIG.exists():
        return

    try:
        text = PROJECT_CONFIG.read_bytes().decode("gbk")
    except UnicodeDecodeError as exc:
        reporter.fail("Project_Config.h must be saved as GBK/ANSI for Keil Configuration Wizard: {0}".format(exc))
        return

    if PROJECT_CONFIG_WIZARD_MARKER in text:
        reporter.ok("Project_Config.h GBK/ANSI text is readable for Keil Configuration Wizard")
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


def main(argv):
    parser = argparse.ArgumentParser(description="Check Keil project release/debug configuration.")
    parser.add_argument("-q", "--quiet", action="store_true", help="Only print warnings, errors, and summary.")
    args = parser.parse_args(argv)

    reporter = Reporter(verbose=not args.quiet)
    print("Project check: {0}".format(ROOT))

    check_required_files(reporter)
    check_project_config_wizard_encoding(reporter)
    check_keil_targets(reporter)
    check_release_defaults(reporter)
    check_guard_includes(reporter)
    check_build_guard(reporter)
    check_gitignore(reporter)
    check_hooks(reporter)

    return reporter.summary()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
