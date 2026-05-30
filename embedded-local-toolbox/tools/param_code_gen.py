#!/usr/bin/env python3
"""Generate minimal C parameter metadata from CSV."""

from __future__ import annotations

import argparse
from pathlib import Path

from elt_common import parse_int, read_csv, write_text_report
from param_table_check import TYPES, check_rows


def main() -> int:
    parser = argparse.ArgumentParser(description="从参数表 CSV 生成 C 头文件、默认值和 Modbus 映射")
    parser.add_argument("csv", nargs="?", default="data/examples/param_table.csv", help="参数表 CSV")
    parser.add_argument("--out-dir", default="generated", help="输出目录")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有生成文件")
    args = parser.parse_args()
    rows = read_csv(args.csv)
    errors = check_rows(rows)
    if errors:
        raise SystemExit("参数表检查失败，请先运行 param_table_check.py")
    out_dir = Path(args.out_dir)
    write_text_report(out_dir / "param_table.h", render_h(rows, args.csv), args.force)
    write_text_report(out_dir / "param_default.c", render_default_c(rows, args.csv), args.force)
    write_text_report(out_dir / "modbus_param_map.c", render_map_c(rows, args.csv), args.force)
    print(f"已生成到 {out_dir}")
    return 0


def param_id(c_name: str) -> str:
    return "PARAM_ID_" + c_name.upper()


def render_h(rows: list[dict], source: str) -> str:
    out = [f"/* 自动生成，请勿手动修改。源数据: {source} */\n\n#ifndef ELT_PARAM_TABLE_H\n#define ELT_PARAM_TABLE_H\n\n#include <stdint.h>\n\n"]
    out.append("typedef enum {\n")
    for idx, row in enumerate(rows):
        out.append(f"    {param_id(row['c_name'])} = {idx},\n")
    out.append(f"    PARAM_ID_COUNT = {len(rows)}\n}} param_id_t;\n\n")
    out.append("typedef struct {\n    param_id_t id;\n    uint16_t modbus_addr;\n    uint8_t reg_count;\n    int32_t min_value;\n    int32_t max_value;\n    int32_t default_value;\n    const char *data_type;\n    const char *access;\n    const char *save_policy;\n    const char *name;\n} param_meta_t;\n\n")
    out.append("extern const param_meta_t g_param_table[PARAM_ID_COUNT];\nextern const int32_t g_param_defaults[PARAM_ID_COUNT];\n\n#endif\n")
    return "".join(out)


def render_default_c(rows: list[dict], source: str) -> str:
    out = [f"/* 自动生成，请勿手动修改。源数据: {source} */\n\n#include \"param_table.h\"\n\nconst param_meta_t g_param_table[PARAM_ID_COUNT] = {{\n"]
    for row in rows:
        out.append(f"    {{ {param_id(row['c_name'])}, 0x{parse_int(row['modbus_addr']):04X}U, {TYPES[row['data_type'].lower()]}U, {parse_int(row['min'])}, {parse_int(row['max'])}, {parse_int(row['default'])}, \"{row['data_type']}\", \"{row['access']}\", \"{row['save_policy']}\", \"{row['name']}\" }},\n")
    out.append("};\n\nconst int32_t g_param_defaults[PARAM_ID_COUNT] = {\n")
    for row in rows:
        out.append(f"    {parse_int(row['default'])}, /* {param_id(row['c_name'])} */\n")
    out.append("};\n")
    return "".join(out)


def render_map_c(rows: list[dict], source: str) -> str:
    out = [f"/* 自动生成，请勿手动修改。源数据: {source} */\n\n#include \"param_table.h\"\n\ntypedef struct {{ uint16_t addr; param_id_t id; uint8_t reg_count; }} modbus_param_map_t;\n\nconst modbus_param_map_t g_modbus_param_map[PARAM_ID_COUNT] = {{\n"]
    for row in rows:
        out.append(f"    {{ 0x{parse_int(row['modbus_addr']):04X}U, {param_id(row['c_name'])}, {TYPES[row['data_type'].lower()]}U }},\n")
    out.append("};\n")
    return "".join(out)


if __name__ == "__main__":
    raise SystemExit(main())
