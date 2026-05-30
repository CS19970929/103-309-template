#!/usr/bin/env python3
"""
Generate Markdown parameter documents from CSV/JSON.
"""

from __future__ import print_function

import argparse
import sys
from pathlib import Path

from param_table_common import ParamTableError, format_addr, load_param_table
from param_table_common import physical_value, validate_params


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "data" / "param_tables" / "example_bms_params.csv"
DEFAULT_OUT_DIR = ROOT / "docs" / "generated"
AUTO_NOTE = "> 自动生成，请勿手动修改。源数据：`{source}`。\n\n"


def main():
    parser = argparse.ArgumentParser(description="从 BMS 参数表生成 Markdown 文档")
    parser.add_argument(
        "input",
        nargs="?",
        default=str(DEFAULT_INPUT),
        help="参数表路径，默认 data/param_tables/example_bms_params.csv",
    )
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR), help="输出目录，默认 docs/generated")
    args = parser.parse_args()

    try:
        rows = load_param_table(args.input)
        params, errors = validate_params(rows)
    except ParamTableError as exc:
        print("ERROR: {0}".format(exc), file=sys.stderr)
        return 2

    if errors:
        print("参数表存在错误，已停止生成。请先运行 tools/param_table_check.py。", file=sys.stderr)
        for error in errors:
            print("ERROR row {row} field {field}: {message}".format(**error), file=sys.stderr)
        return 1

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    source = _source_label(args.input)
    _write_text(out_dir / "param_table.md", render_param_table(params, source))
    _write_text(out_dir / "modbus_register_map.md", render_modbus_map(params, source))

    print("已生成:")
    print("  {0}".format(out_dir / "param_table.md"))
    print("  {0}".format(out_dir / "modbus_register_map.md"))
    return 0


def render_param_table(params, source):
    lines = []
    lines.append("# 参数表\n\n")
    lines.append(AUTO_NOTE.format(source=source))
    lines.append("文档状态：自动生成\n")
    lines.append("维护方式：修改 CSV/JSON 后重新运行 `tools/param_table_check.py` 与生成脚本。\n\n")
    lines.append("## 字段约定\n\n")
    lines.append("- `min`、`max`、`default` 为固件侧原始整数值。\n")
    lines.append("- 实际物理值 = 原始整数值 x `scale`，单位见 `unit`。\n")
    lines.append("- `u32`、`s32` 参数占用 2 个连续 Modbus register。\n\n")
    lines.append("## 参数明细\n\n")
    lines.append("| Group | Name | C name | Type | Scale | Unit | Min | Max | Default | Access | Save policy | Description |\n")
    lines.append("|---|---|---|---|---|---|---:|---:|---:|---|---|---|\n")
    for row in params:
        lines.append(
            "| {group} | {name} | `{c_name}` | `{data_type}` | {scale} | {unit} | {min_value} | {max_value} | {default_value} | `{access}` | `{save_policy}` | {description} |\n".format(
                **row
            )
        )
    lines.append("\n## 默认值物理量视图\n\n")
    lines.append("| C name | Default raw | Default physical | Unit |\n")
    lines.append("|---|---:|---:|---|\n")
    for row in params:
        lines.append(
            "| `{0}` | {1} | {2} | {3} |\n".format(
                row["c_name"], row["default_value"], physical_value(row, "default"), row["unit"]
            )
        )
    return "".join(lines)


def render_modbus_map(params, source):
    lines = []
    lines.append("# Modbus 参数寄存器映射\n\n")
    lines.append(AUTO_NOTE.format(source=source))
    lines.append("文档状态：自动生成\n")
    lines.append("地址规则：每个参数的起始地址来自 `modbus_addr`，`u32/s32` 占用 2 个连续 register。\n\n")
    lines.append("| Address | Register count | C name | Name | Type | Access | Save policy | Description |\n")
    lines.append("|---:|---:|---|---|---|---|---|---|\n")
    for row in params:
        lines.append(
            "| `{0}` | {1} | `{2}` | {3} | `{4}` | `{5}` | `{6}` | {7} |\n".format(
                format_addr(row["modbus_addr_value"]),
                row["reg_count"],
                row["c_name"],
                row["name"],
                row["data_type"],
                row["access"],
                row["save_policy"],
                row["description"],
            )
        )
    return "".join(lines)


def _source_label(path):
    try:
        return str(Path(path).resolve().relative_to(ROOT))
    except ValueError:
        return str(Path(path).resolve())


def _write_text(path, text):
    path.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
