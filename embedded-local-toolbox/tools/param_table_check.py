#!/usr/bin/env python3
"""Check CSV maintained embedded parameter tables."""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict

from elt_common import parse_int, read_csv


FIELDS = ["group", "name", "c_name", "modbus_addr", "data_type", "scale", "unit", "min", "max", "default", "access", "save_policy", "description"]
TYPES = {"bool": 1, "u8": 1, "s8": 1, "u16": 1, "s16": 1, "u32": 2, "s32": 2}
TYPE_RANGE = {"bool": (0, 1), "u8": (0, 255), "s8": (-128, 127), "u16": (0, 65535), "s16": (-32768, 32767), "u32": (0, 0xFFFFFFFF), "s32": (-0x80000000, 0x7FFFFFFF)}
ACCESS = {"ro", "rw", "wo"}
SAVE = {"none", "runtime", "flash", "eeprom", "nvm", "factory"}
C_NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def main() -> int:
    parser = argparse.ArgumentParser(description="检查参数表 CSV：地址冲突、类型、默认值、枚举和描述")
    parser.add_argument("csv", nargs="?", default="data/examples/param_table.csv", help="参数表 CSV")
    parser.add_argument("--quiet", action="store_true", help="只返回退出码")
    args = parser.parse_args()
    rows = read_csv(args.csv)
    errors = check_rows(rows)
    if errors and not args.quiet:
        print(f"参数表检查失败：{len(errors)} 个错误", file=sys.stderr)
        for err in errors:
            print(f"ERROR row {err[0]} {err[1]}: {err[2]}", file=sys.stderr)
    elif not args.quiet:
        print(f"参数表检查通过：{len(rows)} 个参数")
    return 1 if errors else 0


def check_rows(rows: list[dict]) -> list[tuple[int, str, str]]:
    errors = []
    used_c = {}
    used_addr = {}
    for idx, row in enumerate(rows, 2):
        for field in FIELDS:
            if not row.get(field, "").strip():
                errors.append((idx, field, "字段不能为空"))
        c_name = row.get("c_name", "")
        if c_name and not C_NAME_RE.match(c_name):
            errors.append((idx, "c_name", "不是合法 C 标识符"))
        if c_name in used_c:
            errors.append((idx, "c_name", f"与第 {used_c[c_name]} 行重复"))
        used_c[c_name] = idx
        data_type = row.get("data_type", "").lower()
        if data_type not in TYPES:
            errors.append((idx, "data_type", f"非法类型 {data_type}"))
            continue
        access = row.get("access", "").lower()
        save = row.get("save_policy", "").lower()
        if access not in ACCESS:
            errors.append((idx, "access", f"非法访问权限 {access}"))
        if save not in SAVE:
            errors.append((idx, "save_policy", f"非法保存策略 {save}"))
        try:
            addr = parse_int(row["modbus_addr"])
            min_v = parse_int(row["min"])
            max_v = parse_int(row["max"])
            default = parse_int(row["default"])
            float(row["scale"])
        except ValueError as exc:
            errors.append((idx, "number", str(exc)))
            continue
        for addr_i in range(addr, addr + TYPES[data_type]):
            if addr_i in used_addr:
                errors.append((idx, "modbus_addr", f"地址 0x{addr_i:04X} 与第 {used_addr[addr_i]} 行冲突"))
            used_addr[addr_i] = idx
        type_min, type_max = TYPE_RANGE[data_type]
        if min_v > max_v:
            errors.append((idx, "min/max", "min 大于 max"))
        if default < min_v or default > max_v:
            errors.append((idx, "default", "默认值超出 min/max"))
        for field, value in [("min", min_v), ("max", max_v), ("default", default)]:
            if value < type_min or value > type_max:
                errors.append((idx, field, f"超出 {data_type} 类型范围"))
    return errors


if __name__ == "__main__":
    raise SystemExit(main())
