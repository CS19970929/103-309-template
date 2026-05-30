#!/usr/bin/env python3
"""
Check a BMS parameter table before code or document generation.
"""

from __future__ import print_function

import argparse
import sys
from pathlib import Path

from param_table_common import DATA_TYPES, ACCESS_VALUES, SAVE_POLICY_VALUES
from param_table_common import ParamTableError, load_param_table, validate_params


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "data" / "param_tables" / "example_bms_params.csv"


def main():
    parser = argparse.ArgumentParser(description="检查 BMS 参数表 CSV/JSON")
    parser.add_argument(
        "input",
        nargs="?",
        default=str(DEFAULT_INPUT),
        help="参数表路径，默认 data/param_tables/example_bms_params.csv",
    )
    parser.add_argument("--list-rules", action="store_true", help="打印合法枚举值")
    args = parser.parse_args()

    if args.list_rules:
        print("data_type: {0}".format(", ".join(sorted(DATA_TYPES))))
        print("access: {0}".format(", ".join(sorted(ACCESS_VALUES))))
        print("save_policy: {0}".format(", ".join(sorted(SAVE_POLICY_VALUES))))

    try:
        rows = load_param_table(args.input)
        params, errors = validate_params(rows)
    except ParamTableError as exc:
        print("ERROR: {0}".format(exc), file=sys.stderr)
        return 2

    if errors:
        print("参数表检查失败: {0} 个错误".format(len(errors)), file=sys.stderr)
        for error in errors:
            print(
                "ERROR row {row} field {field}: {message}".format(**error),
                file=sys.stderr,
            )
        return 1

    print("参数表检查通过: {0} 个参数".format(len(params)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
