"""python-can adapter helpers."""

from __future__ import annotations

import sys


def require_python_can():
    try:
        import can  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "缺少 python-can，先安装：py -3.9 -m pip install python-can\n"
            f"当前解释器：{sys.executable}"
        ) from exc
    return can


def open_bus(interface: str, channel: str, bitrate: int):
    can = require_python_can()
    try:
        return can.Bus(interface=interface, channel=channel, bitrate=bitrate)
    except TypeError:
        return can.interface.Bus(bustype=interface, channel=channel, bitrate=bitrate)


def make_message(arbitration_id: int, data: bytes, extended: bool = True):
    can = require_python_can()
    return can.Message(
        arbitration_id=arbitration_id,
        data=bytearray(data),
        is_extended_id=extended,
        is_remote_frame=False,
    )
