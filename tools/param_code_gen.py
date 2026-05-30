#!/usr/bin/env python3
"""
Generate C parameter metadata and default tables from CSV/JSON.
"""

from __future__ import print_function

import argparse
import sys
from pathlib import Path

from param_table_common import DATA_TYPES, ACCESS_VALUES, SAVE_POLICY_VALUES
from param_table_common import ParamTableError, c_string, enum_name, format_addr
from param_table_common import load_param_table, validate_params


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "data" / "param_tables" / "example_bms_params.csv"
DEFAULT_OUT_DIR = ROOT / "generated"
AUTO_COMMENT = "/* 自动生成，请勿手动修改。源数据: {source} */\n\n"


def main():
    parser = argparse.ArgumentParser(description="从 BMS 参数表生成 C 文件")
    parser.add_argument(
        "input",
        nargs="?",
        default=str(DEFAULT_INPUT),
        help="参数表路径，默认 data/param_tables/example_bms_params.csv",
    )
    parser.add_argument("--out-dir", default=str(DEFAULT_OUT_DIR), help="输出目录，默认 generated")
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
    _write_text(out_dir / "param_table.h", render_header(params, source))
    _write_text(out_dir / "param_default.c", render_defaults(params, source))
    _write_text(out_dir / "modbus_param_map.c", render_modbus_map(params, source))

    print("已生成:")
    print("  {0}".format(out_dir / "param_table.h"))
    print("  {0}".format(out_dir / "param_default.c"))
    print("  {0}".format(out_dir / "modbus_param_map.c"))
    return 0


def render_header(params, source):
    lines = []
    lines.append(AUTO_COMMENT.format(source=source))
    lines.append("#ifndef GENERATED_PARAM_TABLE_H\n")
    lines.append("#define GENERATED_PARAM_TABLE_H\n\n")
    lines.append("#include <stdint.h>\n\n")

    lines.append("typedef enum {\n")
    for index, row in enumerate(params):
        lines.append("    {0} = {1},\n".format(enum_name("PARAM_ID", row["c_name"]), index))
    lines.append("    PARAM_ID_COUNT = {0}\n".format(len(params)))
    lines.append("} param_id_t;\n\n")

    lines.append("typedef enum {\n")
    for index, data_type in enumerate(sorted(DATA_TYPES)):
        comma = "," if index < len(DATA_TYPES) - 1 else ""
        lines.append("    {0} = {1}{2}\n".format(enum_name("PARAM_TYPE", data_type), index, comma))
    lines.append("} param_data_type_t;\n\n")

    lines.append("typedef enum {\n")
    for index, access in enumerate(sorted(ACCESS_VALUES)):
        comma = "," if index < len(ACCESS_VALUES) - 1 else ""
        lines.append("    {0} = {1}{2}\n".format(enum_name("PARAM_ACCESS", access), index, comma))
    lines.append("} param_access_t;\n\n")

    lines.append("typedef enum {\n")
    save_values = sorted(SAVE_POLICY_VALUES)
    for index, save_policy in enumerate(save_values):
        comma = "," if index < len(save_values) - 1 else ""
        lines.append("    {0} = {1}{2}\n".format(enum_name("PARAM_SAVE", save_policy), index, comma))
    lines.append("} param_save_policy_t;\n\n")

    lines.append("typedef struct {\n")
    lines.append("    param_id_t id;\n")
    lines.append("    uint16_t modbus_addr;\n")
    lines.append("    uint8_t reg_count;\n")
    lines.append("    param_data_type_t data_type;\n")
    lines.append("    int64_t min_value;\n")
    lines.append("    int64_t max_value;\n")
    lines.append("    int64_t default_value;\n")
    lines.append("    param_access_t access;\n")
    lines.append("    param_save_policy_t save_policy;\n")
    lines.append("    const char *group;\n")
    lines.append("    const char *name;\n")
    lines.append("    const char *c_name;\n")
    lines.append("    const char *scale;\n")
    lines.append("    const char *unit;\n")
    lines.append("    const char *description;\n")
    lines.append("} param_meta_t;\n\n")

    lines.append("typedef struct {\n")
    lines.append("    uint16_t modbus_addr;\n")
    lines.append("    param_id_t id;\n")
    lines.append("    uint8_t reg_count;\n")
    lines.append("} param_modbus_map_t;\n\n")

    lines.append("extern const param_meta_t g_param_table[PARAM_ID_COUNT];\n")
    lines.append("extern const int64_t g_param_defaults[PARAM_ID_COUNT];\n")
    lines.append("extern const param_modbus_map_t g_modbus_param_map[PARAM_ID_COUNT];\n")
    lines.append("extern const uint16_t g_modbus_param_map_count;\n\n")
    lines.append("#endif /* GENERATED_PARAM_TABLE_H */\n")
    return "".join(lines)


def render_defaults(params, source):
    lines = []
    lines.append(AUTO_COMMENT.format(source=source))
    lines.append('#include "param_table.h"\n\n')
    lines.append("const param_meta_t g_param_table[PARAM_ID_COUNT] = {\n")
    for row in params:
        lines.append("    {\n")
        lines.append("        {0},\n".format(enum_name("PARAM_ID", row["c_name"])))
        lines.append("        {0}U,\n".format(format_addr(row["modbus_addr_value"])))
        lines.append("        {0}U,\n".format(row["reg_count"]))
        lines.append("        {0},\n".format(enum_name("PARAM_TYPE", row["data_type"])))
        lines.append("        {0}LL,\n".format(row["min_value"]))
        lines.append("        {0}LL,\n".format(row["max_value"]))
        lines.append("        {0}LL,\n".format(row["default_value"]))
        lines.append("        {0},\n".format(enum_name("PARAM_ACCESS", row["access"])))
        lines.append("        {0},\n".format(enum_name("PARAM_SAVE", row["save_policy"])))
        lines.append("        {0},\n".format(c_string(row["group"])))
        lines.append("        {0},\n".format(c_string(row["name"])))
        lines.append("        {0},\n".format(c_string(row["c_name"])))
        lines.append("        {0},\n".format(c_string(row["scale"])))
        lines.append("        {0},\n".format(c_string(row["unit"])))
        lines.append("        {0}\n".format(c_string(row["description"])))
        lines.append("    },\n")
    lines.append("};\n\n")
    lines.append("const int64_t g_param_defaults[PARAM_ID_COUNT] = {\n")
    for row in params:
        lines.append("    {0}LL, /* {1} */\n".format(row["default_value"], enum_name("PARAM_ID", row["c_name"])))
    lines.append("};\n")
    return "".join(lines)


def render_modbus_map(params, source):
    lines = []
    lines.append(AUTO_COMMENT.format(source=source))
    lines.append('#include "param_table.h"\n\n')
    lines.append("const param_modbus_map_t g_modbus_param_map[PARAM_ID_COUNT] = {\n")
    for row in params:
        lines.append(
            "    {{ {0}U, {1}, {2}U }},\n".format(
                format_addr(row["modbus_addr_value"]),
                enum_name("PARAM_ID", row["c_name"]),
                row["reg_count"],
            )
        )
    lines.append("};\n\n")
    lines.append("const uint16_t g_modbus_param_map_count = (uint16_t)PARAM_ID_COUNT;\n")
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
