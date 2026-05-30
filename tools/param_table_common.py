#!/usr/bin/env python3
"""
Common helpers for BMS parameter table tools.

The input table is treated as the single source of truth for Modbus address
allocation, C symbols, default values, and generated documents.
"""

from __future__ import print_function

import csv
import json
import re
from pathlib import Path


REQUIRED_FIELDS = [
    "group",
    "name",
    "c_name",
    "modbus_addr",
    "data_type",
    "scale",
    "unit",
    "min",
    "max",
    "default",
    "access",
    "save_policy",
    "description",
]

DATA_TYPES = {
    "bool": {"c_type": "uint8_t", "reg_count": 1, "min": 0, "max": 1},
    "u8": {"c_type": "uint8_t", "reg_count": 1, "min": 0, "max": 0xFF},
    "s8": {"c_type": "int8_t", "reg_count": 1, "min": -0x80, "max": 0x7F},
    "u16": {"c_type": "uint16_t", "reg_count": 1, "min": 0, "max": 0xFFFF},
    "s16": {"c_type": "int16_t", "reg_count": 1, "min": -0x8000, "max": 0x7FFF},
    "u32": {"c_type": "uint32_t", "reg_count": 2, "min": 0, "max": 0xFFFFFFFF},
    "s32": {"c_type": "int32_t", "reg_count": 2, "min": -0x80000000, "max": 0x7FFFFFFF},
}

ACCESS_VALUES = {"ro", "rw", "wo"}
SAVE_POLICY_VALUES = {"none", "runtime", "flash", "eeprom", "nvm", "factory"}
C_NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


class ParamTableError(Exception):
    pass


def load_param_table(path):
    src = Path(path)
    if not src.exists():
        raise ParamTableError("参数表不存在: {0}".format(src))

    if src.suffix.lower() == ".csv":
        rows = _load_csv(src)
    elif src.suffix.lower() == ".json":
        rows = _load_json(src)
    else:
        raise ParamTableError("仅支持 CSV/JSON 参数表: {0}".format(src))

    return _normalize_rows(rows)


def _load_csv(path):
    with path.open("r", encoding="utf-8-sig", newline="") as fp:
        reader = csv.DictReader(fp)
        if reader.fieldnames is None:
            raise ParamTableError("CSV 表头为空: {0}".format(path))
        missing = [field for field in REQUIRED_FIELDS if field not in reader.fieldnames]
        if missing:
            raise ParamTableError("CSV 缺少字段: {0}".format(", ".join(missing)))
        return [dict(row) for row in reader]


def _load_json(path):
    with path.open("r", encoding="utf-8") as fp:
        data = json.load(fp)
    if isinstance(data, dict):
        rows = data.get("parameters")
    else:
        rows = data
    if not isinstance(rows, list):
        raise ParamTableError("JSON 顶层必须是数组，或包含 parameters 数组")
    for index, row in enumerate(rows, 1):
        if not isinstance(row, dict):
            raise ParamTableError("JSON 第 {0} 行不是对象".format(index))
    missing = []
    for row in rows:
        for field in REQUIRED_FIELDS:
            if field not in row and field not in missing:
                missing.append(field)
    if missing:
        raise ParamTableError("JSON 缺少字段: {0}".format(", ".join(missing)))
    return rows


def _normalize_rows(rows):
    normalized = []
    for index, row in enumerate(rows, 2):
        item = {}
        for field in REQUIRED_FIELDS:
            value = row.get(field, "")
            if value is None:
                value = ""
            item[field] = str(value).strip()
        item["_row"] = index
        normalized.append(item)
    return normalized


def validate_params(rows):
    errors = []
    seen_c_names = {}
    occupied_addrs = {}
    normalized = []

    for row in rows:
        item = dict(row)
        row_no = item["_row"]

        for field in REQUIRED_FIELDS:
            if item[field] == "":
                errors.append(_err(row_no, field, "字段不能为空"))

        c_name = item["c_name"]
        if c_name and not C_NAME_RE.match(c_name):
            errors.append(_err(row_no, "c_name", "不是合法 C 标识符"))
        if c_name:
            if c_name in seen_c_names:
                errors.append(_err(row_no, "c_name", "与第 {0} 行重复".format(seen_c_names[c_name])))
            else:
                seen_c_names[c_name] = row_no

        data_type = item["data_type"].lower()
        if data_type not in DATA_TYPES:
            errors.append(_err(row_no, "data_type", "非法类型: {0}".format(item["data_type"])))
            type_info = None
        else:
            item["data_type"] = data_type
            type_info = DATA_TYPES[data_type]
            item["reg_count"] = type_info["reg_count"]
            item["c_type"] = type_info["c_type"]

        access = item["access"].lower()
        if access not in ACCESS_VALUES:
            errors.append(_err(row_no, "access", "非法访问属性: {0}".format(item["access"])))
        else:
            item["access"] = access

        save_policy = item["save_policy"].lower()
        if save_policy not in SAVE_POLICY_VALUES:
            errors.append(_err(row_no, "save_policy", "非法保存策略: {0}".format(item["save_policy"])))
        else:
            item["save_policy"] = save_policy

        addr = _parse_int_field(item, "modbus_addr", errors)
        min_value = _parse_int_field(item, "min", errors)
        max_value = _parse_int_field(item, "max", errors)
        default_value = _parse_int_field(item, "default", errors)
        scale_value = _parse_scale_field(item, errors)

        if addr is not None:
            if addr < 0 or addr > 0xFFFF:
                errors.append(_err(row_no, "modbus_addr", "地址超出 0x0000~0xFFFF"))
            elif type_info is not None:
                reg_count = type_info["reg_count"]
                if addr + reg_count - 1 > 0xFFFF:
                    errors.append(_err(row_no, "modbus_addr", "多寄存器参数超出 Modbus 地址范围"))
                else:
                    for offset in range(reg_count):
                        used_addr = addr + offset
                        if used_addr in occupied_addrs:
                            errors.append(
                                _err(
                                    row_no,
                                    "modbus_addr",
                                    "地址 {0} 与第 {1} 行冲突".format(
                                        format_addr(used_addr), occupied_addrs[used_addr]
                                    ),
                                )
                            )
                        else:
                            occupied_addrs[used_addr] = row_no
                item["modbus_addr_value"] = addr

        if min_value is not None and max_value is not None:
            if min_value > max_value:
                errors.append(_err(row_no, "min/max", "min 大于 max"))

        if type_info is not None:
            for field, value in (("min", min_value), ("max", max_value), ("default", default_value)):
                if value is None:
                    continue
                if value < type_info["min"] or value > type_info["max"]:
                    errors.append(
                        _err(
                            row_no,
                            field,
                            "数值超出 {0} 类型范围 {1}~{2}".format(
                                data_type, type_info["min"], type_info["max"]
                            ),
                        )
                    )

        if min_value is not None and max_value is not None and default_value is not None:
            if default_value < min_value or default_value > max_value:
                errors.append(_err(row_no, "default", "默认值超出 min/max"))

        if not item["description"]:
            errors.append(_err(row_no, "description", "缺少描述"))

        if addr is not None:
            item["modbus_addr_value"] = addr
        if min_value is not None:
            item["min_value"] = min_value
        if max_value is not None:
            item["max_value"] = max_value
        if default_value is not None:
            item["default_value"] = default_value
        if scale_value is not None:
            item["scale_value"] = scale_value
        normalized.append(item)

    normalized.sort(key=lambda row: (row.get("modbus_addr_value", 0), row["c_name"]))
    return normalized, errors


def _parse_int_field(item, field, errors):
    value = item[field]
    if value == "":
        return None
    try:
        return int(value, 0)
    except ValueError:
        errors.append(_err(item["_row"], field, "必须是整数，支持十进制或 0x 十六进制"))
        return None


def _parse_scale_field(item, errors):
    value = item["scale"]
    if value == "":
        return None
    try:
        scale = float(value)
    except ValueError:
        errors.append(_err(item["_row"], "scale", "必须是数字"))
        return None
    if scale <= 0:
        errors.append(_err(item["_row"], "scale", "必须大于 0"))
    return scale


def _err(row, field, message):
    return {"row": row, "field": field, "message": message}


def format_addr(value):
    return "0x{0:04X}".format(value)


def enum_name(prefix, c_name):
    return "{0}_{1}".format(prefix, c_name.upper())


def c_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def physical_value(row, field):
    raw = row.get("{0}_value".format(field))
    scale = row.get("scale_value")
    if raw is None or scale is None:
        return ""
    value = raw * scale
    if abs(value - int(value)) < 0.0000001:
        return str(int(value))
    return ("{0:.6f}".format(value)).rstrip("0").rstrip(".")
