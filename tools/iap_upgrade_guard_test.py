#!/usr/bin/env python3
"""Host-side guard tests for BMS CAN-IAP image validation."""

from __future__ import annotations

import struct
import sys
import tempfile
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import can_bms_host
import comm_tool_host


def make_image(path: Path, size: int = 256, msp: int = 0x20002000, reset: int = 0x08004881) -> Path:
    image = bytearray([0xFF] * size)
    struct.pack_into("<II", image, 0, msp, reset)
    path.write_bytes(image)
    return path


def expect_reject(title: str, func) -> None:
    try:
        func()
    except SystemExit:
        return
    raise AssertionError(f"{title}: expected rejection")


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        valid = make_image(root / "valid.bin")
        can_bms_host.load_image(valid, can_bms_host.APP_BASE_ADDR)
        comm_tool_host.load_image(valid, comm_tool_host.BMS_APP_BASE_ADDR)

        expect_reject(
            "wrong app address",
            lambda: can_bms_host.load_image(valid, 0x08000000),
        )
        expect_reject(
            "bad msp",
            lambda: can_bms_host.load_image(
                make_image(root / "bad_msp.bin", msp=0x20004FE0),
                can_bms_host.APP_BASE_ADDR,
            ),
        )
        expect_reject(
            "reset outside image",
            lambda: can_bms_host.load_image(
                make_image(root / "bad_reset.bin", reset=0x08006001),
                can_bms_host.APP_BASE_ADDR,
            ),
        )
        expect_reject(
            "image overflow",
            lambda: can_bms_host.load_image(
                make_image(
                    root / "overflow.bin",
                    size=(can_bms_host.APP_FLASH_LIMIT - can_bms_host.APP_BASE_ADDR + 1),
                ),
                can_bms_host.APP_BASE_ADDR,
            ),
        )

        expect_reject(
            "comm tool bad msp",
            lambda: comm_tool_host.load_image(
                make_image(root / "ct_bad_msp.bin", msp=0x20004FE0),
                comm_tool_host.BMS_APP_BASE_ADDR,
            ),
        )
        expect_reject(
            "comm tool reset outside image",
            lambda: comm_tool_host.load_image(
                make_image(root / "ct_bad_reset.bin", reset=0x08006001),
                comm_tool_host.BMS_APP_BASE_ADDR,
            ),
        )

    print("IAP image guard tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
