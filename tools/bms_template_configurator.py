#!/usr/bin/env python3
"""
BMS template configurator.

This tool is intentionally conservative: it reads the checked-in profile matrix
and produces a dry-run generation report. Actual Keil project materialization
should only be enabled after the app baseline and selected port profile are both
fully validated.
"""

from __future__ import print_function

import argparse
import json
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROFILES_PATH = ROOT / "templates" / "bms" / "target_profiles.json"
APP_BASELINE = "103 + 309/Project/Source"
PROJECT_TEMPLATE_ROOT = ROOT / "103 + 309" / "Project"
DEFAULT_OUTPUT_ROOT = ROOT / "generated" / "bms_projects"

MCU_CONFIG_VALUES = {
    "stm32f103": "103",
    "stm32f030": "30",
}
BOARD_CONFIG_VALUES = {
    "FD_103_309": "103309",
    "A002_F030_BQ76940": "30240",
}
AFE_CONFIG_VALUES = {
    "sh367309": "1",
    "bq76940": "0",
}
PROTECTION_CONFIG_VALUES = {
    "afe_hardware_only": "0",
    "mcu_software": "1",
    "hybrid": "2",
}
ARTIFACT_SUFFIXES = {
    ".axf",
    ".bin",
    ".build_log.htm",
    ".crf",
    ".d",
    ".dep",
    ".hex",
    ".htm",
    ".lnp",
    ".lst",
    ".map",
    ".o",
    ".sct.dep",
}


def load_profiles():
    with PROFILES_PATH.open("r", encoding="utf-8") as f:
        data = json.load(f)
    profiles = data.get("profiles", {})
    if not isinstance(profiles, dict):
        raise ValueError("target_profiles.json missing object field: profiles")
    return data, profiles


def parse_int(value):
    if isinstance(value, int):
        return value
    return int(str(value), 0)


def fmt_hex(value, width=8):
    return "0x{0:0{1}X}".format(value, width)


def profile_flash_report(profile):
    flash = profile["flash"]
    flash_start = parse_int(flash["flash_start"])
    flash_size = parse_int(flash["flash_size"])
    app_start = parse_int(flash["app_start"])
    app_size = parse_int(flash["app_size"])
    storage_start = parse_int(flash["storage_start"])
    storage_size = parse_int(flash["storage_size"])
    return {
        "flash": "{0}..{1}".format(fmt_hex(flash_start), fmt_hex(flash_start + flash_size - 1)),
        "iap": fmt_hex(parse_int(flash["iap_start"])),
        "app": "{0}..{1}".format(fmt_hex(app_start), fmt_hex(app_start + app_size - 1)),
        "storage": "{0}..{1}".format(fmt_hex(storage_start), fmt_hex(storage_start + storage_size - 1)),
    }


def validate_profile(name, profile):
    errors = []
    required = [
        "template_role",
        "application_logic_source",
        "mcu_family",
        "mcu_driver",
        "afe_type",
        "board_profile",
        "protection_mode",
        "storage_backend",
        "keil_project",
        "keil_targets",
        "flash",
    ]
    for key in required:
        if key not in profile:
            errors.append("profile {0} missing {1}".format(name, key))

    if profile.get("application_logic_source") != "current_project":
        errors.append("profile {0} must use current_project application logic".format(name))
    if profile.get("storage_backend") != "internal_flash":
        errors.append("profile {0} must use internal_flash storage".format(name))

    if "flash" in profile:
        flash = profile["flash"]
        try:
            app_start = parse_int(flash["app_start"])
            app_size = parse_int(flash["app_size"])
            storage_start = parse_int(flash["storage_start"])
            storage_size = parse_int(flash["storage_size"])
            flash_start = parse_int(flash["flash_start"])
            flash_size = parse_int(flash["flash_size"])
            if not (flash_start <= app_start < app_start + app_size <= storage_start):
                errors.append("profile {0} app overlaps storage".format(name))
            if not (storage_start < storage_start + storage_size <= flash_start + flash_size):
                errors.append("profile {0} storage outside flash".format(name))
        except (KeyError, TypeError, ValueError) as exc:
            errors.append("profile {0} has invalid flash layout: {1}".format(name, exc))

    return errors


def profile_config_values(profile):
    return {
        "PROJECT_CFG_MCU_FAMILY": MCU_CONFIG_VALUES[profile["mcu_family"]],
        "PROJECT_CFG_BOARD_PROFILE": BOARD_CONFIG_VALUES[profile["board_profile"]],
        "PROJECT_CFG_AFE_TYPE": AFE_CONFIG_VALUES[profile["afe_type"]],
        "PROJECT_CFG_PROTECTION_MODE": PROTECTION_CONFIG_VALUES[profile["protection_mode"]],
    }


def rewrite_define(text, name, value):
    prefix = "#define {0} ".format(name)
    lines = []
    replaced = False
    for line in text.splitlines():
        stripped = line.lstrip()
        if stripped.startswith(prefix):
            indent = line[:len(line) - len(stripped)]
            lines.append("{0}{1}{2}".format(indent, prefix, value))
            replaced = True
        else:
            lines.append(line)
    if not replaced:
        raise ValueError("missing define in Project_Config.h: {0}".format(name))
    return "\n".join(lines) + "\n"


def apply_profile_to_project_config(project_dir, profile):
    config_path = project_dir / "Source" / "conf" / "Project_Config.h"
    text = config_path.read_text(encoding="utf-8")
    for name, value in profile_config_values(profile).items():
        text = rewrite_define(text, name, value)
    config_path.write_text(text, encoding="utf-8")


def should_ignore_project_entry(directory, name):
    if name in (".DS_Store", "DebugConfig", "Listings"):
        return True
    if name.endswith(".uvguix.cs") or ".uvguix." in name:
        return True

    suffix = Path(name).suffix.lower()
    if suffix == ".md" and Path(directory).resolve() == PROJECT_TEMPLATE_ROOT:
        return True
    if suffix in ARTIFACT_SUFFIXES:
        return True

    directory_text = str(directory).replace("\\", "/")
    if "/Objects" in directory_text and suffix != ".sct":
        return True

    return False


def project_copy_ignore(directory, names):
    return [name for name in names if should_ignore_project_entry(directory, name)]


def command_generate(args):
    _data, profiles = load_profiles()
    profile = profiles.get(args.profile)
    if profile is None:
        print("unknown profile: {0}".format(args.profile), file=sys.stderr)
        return 2

    errors = validate_profile(args.profile, profile)
    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 2

    if profile.get("template_role") != "canonical_application_baseline":
        print(
            "profile {0} is a port reference, not a materialized application baseline; use dry-run until its port is finalized".format(
                args.profile
            ),
            file=sys.stderr,
        )
        return 3

    output_root = Path(args.output) if args.output else DEFAULT_OUTPUT_ROOT
    if not output_root.is_absolute():
        output_root = ROOT / output_root
    project_root = output_root / args.name
    project_dir = project_root / "Project"

    if project_root.exists():
        if not args.force:
            print("output already exists: {0}".format(project_root), file=sys.stderr)
            return 2
        shutil.rmtree(str(project_root))

    project_root.mkdir(parents=True, exist_ok=True)
    shutil.copytree(str(PROJECT_TEMPLATE_ROOT), str(project_dir), ignore=project_copy_ignore)
    apply_profile_to_project_config(project_dir, profile)

    report = build_dry_run_report(args.profile, args.name)
    (project_root / "GENERATION_REPORT.md").write_text(report + "\n", encoding="utf-8")
    (project_root / "target_profile.json").write_text(json.dumps(profile, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(str(project_root))
    return 0


def command_list(_args):
    _data, profiles = load_profiles()
    for name in sorted(profiles):
        profile = profiles[name]
        print("{0}: {1} + {2}, role={3}, protection={4}".format(
            name,
            profile.get("mcu_family", "?"),
            profile.get("afe_type", "?"),
            profile.get("template_role", "?"),
            profile.get("protection_mode", "?"),
        ))
    return 0


def command_show(args):
    _data, profiles = load_profiles()
    profile = profiles.get(args.profile)
    if profile is None:
        print("unknown profile: {0}".format(args.profile), file=sys.stderr)
        return 2
    print(json.dumps(profile, indent=2, ensure_ascii=False))
    return 0


def build_dry_run_report(profile_name, project_name):
    _data, profiles = load_profiles()
    profile = profiles.get(profile_name)
    if profile is None:
        raise ValueError("unknown profile: {0}".format(profile_name))

    errors = validate_profile(profile_name, profile)
    flash = profile_flash_report(profile)

    lines = []
    lines.append("# BMS 模板配置 dry-run 报告")
    lines.append("")
    lines.append("| 项目 | 值 |")
    lines.append("| --- | --- |")
    lines.append("| project_name | `{0}` |".format(project_name))
    lines.append("| profile | `{0}` |".format(profile_name))
    lines.append("| mcu | `{0}` |".format(profile["mcu_family"]))
    lines.append("| afe | `{0}` |".format(profile["afe_type"]))
    lines.append("| template_role | `{0}` |".format(profile["template_role"]))
    lines.append("| application_logic_source | `{0}` |".format(profile["application_logic_source"]))
    lines.append("| app_baseline | `{0}` |".format(APP_BASELINE))
    lines.append("| protection_mode | `{0}` |".format(profile["protection_mode"]))
    lines.append("| storage_backend | `{0}` |".format(profile["storage_backend"]))
    lines.append("| keil_project_reference | `{0}` |".format(profile["keil_project"]))
    lines.append("")
    lines.append("## Flash 地址")
    lines.append("")
    lines.append("| 区域 | 地址 |")
    lines.append("| --- | --- |")
    lines.append("| Flash | `{0}` |".format(flash["flash"]))
    lines.append("| IAP | `{0}` |".format(flash["iap"]))
    lines.append("| App | `{0}` |".format(flash["app"]))
    lines.append("| Storage | `{0}` |".format(flash["storage"]))
    lines.append("")
    lines.append("## 生成策略")
    lines.append("")
    lines.append("- 应用层从当前项目基线生成，不复制旧 A002 应用层。")
    lines.append("- MCU/AFE 只按 profile 替换 port 层、配置头、Keil Target 和 scatter。")
    lines.append("- 存储固定为 MCU 内部 Flash，不生成外部 EEPROM 后端。")
    lines.append("- 生成前必须再次运行 `python3 tools/project_check.py --quiet`。")
    lines.append("")
    if errors:
        lines.append("## 阻塞问题")
        lines.append("")
        for error in errors:
            lines.append("- {0}".format(error))
    else:
        lines.append("## 校验结论")
        lines.append("")
        lines.append("- profile 基础字段和 Flash 区间检查通过。")
    lines.append("")
    return "\n".join(lines)


def command_dry_run(args):
    report = build_dry_run_report(args.profile, args.name)
    if args.output:
        out = Path(args.output)
        if not out.is_absolute():
            out = ROOT / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(report + "\n", encoding="utf-8")
        print(str(out))
    else:
        print(report)
    return 0


def build_arg_parser():
    parser = argparse.ArgumentParser(description="BMS template configurator")
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("list", help="list available profiles").set_defaults(func=command_list)

    show = sub.add_parser("show", help="show one profile as JSON")
    show.add_argument("--profile", required=True)
    show.set_defaults(func=command_show)

    dry = sub.add_parser("dry-run", help="write or print a generation dry-run report")
    dry.add_argument("--profile", required=True)
    dry.add_argument("--name", required=True)
    dry.add_argument("--output")
    dry.set_defaults(func=command_dry_run)

    generate = sub.add_parser("generate", help="materialize a checked project from a canonical profile")
    generate.add_argument("--profile", required=True)
    generate.add_argument("--name", required=True)
    generate.add_argument("--output")
    generate.add_argument("--force", action="store_true")
    generate.set_defaults(func=command_generate)

    return parser


def main(argv=None):
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    if not hasattr(args, "func"):
        parser.print_help()
        return 2
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
