"""UART framing and command IDs for the STM32 comm tool."""

from __future__ import annotations

import struct
import sys
import time
from dataclasses import dataclass
from typing import Optional

from .crc import crc16_modbus


MAGIC = 0xAA55
VERSION = 1
FLAG_ACK = 0x01
MAX_PAYLOAD = 512
FW_DATA_MAX_CHUNK = MAX_PAYLOAD - 4
FW_DATA_DEFAULT_CHUNK = 496

CMD_GET_INFO = 0x01
CMD_SET_CAN = 0x02
CMD_BMS_READ = 0x10
CMD_BMS_WRITE = 0x11
CMD_BMS_AGING_CTRL = 0x12
CMD_BMS_AGING_STATUS = 0x13
CMD_BMS_AGING_SET_HOURS = 0x14
CMD_FW_BEGIN = 0x20
CMD_FW_DATA = 0x21
CMD_FW_END = 0x22
CMD_FW_INFO = 0x23
CMD_ENTER_IAP = 0x30
CMD_UPGRADE = 0x31
CMD_UPGRADE_STATUS = 0x32
CMD_UPGRADE_ABORT = 0x33
CMD_RAW_CAN_TX = 0x40
CMD_CAN_DIAG = 0x41
CMD_DEBUG_LOG = 0x42

STATUS_TEXT = {
    0x00: "OK",
    0x01: "CRC_ERROR",
    0x02: "UNSUPPORTED",
    0x03: "BAD_PARAM",
    0x04: "BAD_STATE",
    0x05: "FLASH_ERROR",
    0x06: "CAN_TIMEOUT",
    0x07: "BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)",
}

DEBUG_LOG_MODULES = {
    1: "APP",
    2: "UART",
    3: "CAN",
    4: "FLASH",
    5: "UPGRADE",
    6: "PROTOCOL",
}

DEBUG_LOG_EVENTS = {
    1: "BOOT",
    2: "CMD_RX",
    3: "CMD_TX",
    4: "BAD_FRAME",
    5: "CAN_SET",
    6: "CAN_TX_FAIL",
    7: "CAN_TX_TIMEOUT",
    8: "FW_BEGIN",
    9: "FW_END",
    10: "UPGRADE_START",
    11: "UPGRADE_PHASE",
    12: "UPGRADE_ERROR",
    13: "UPGRADE_ABORT",
}

HEADER_STRUCT = struct.Struct("<HBBHBBH")
HEADER_SIZE = HEADER_STRUCT.size


@dataclass
class Frame:
    version: int
    flags: int
    seq: int
    cmd: int
    status: int
    payload: bytes


def require_pyserial():
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "当前 Python 环境缺少 pyserial。\n"
            "请执行: py -3.9 -m pip install pyserial\n"
            f"当前解释器: {sys.executable}"
        ) from exc
    return serial


def encode_frame(seq: int, cmd: int, payload: bytes = b"", *, status: int = 0, flags: int = 0) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too large: {len(payload)} > {MAX_PAYLOAD}")
    header = HEADER_STRUCT.pack(MAGIC, VERSION, flags & 0xFF, seq & 0xFFFF, cmd & 0xFF, status & 0xFF, len(payload))
    body = header + payload
    return body + struct.pack("<H", crc16_modbus(body))


def read_frame(ser, timeout: float) -> Frame:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        first = ser.read(1)
        if first != b"\x55":
            continue
        second = ser.read(1)
        if second != b"\xAA":
            continue
        rest = ser.read(HEADER_SIZE - 2)
        if len(rest) != HEADER_SIZE - 2:
            break
        header = b"\x55\xAA" + rest
        magic, version, flags, seq, cmd, status, length = HEADER_STRUCT.unpack(header)
        if magic != MAGIC:
            continue
        if version != VERSION:
            raise RuntimeError(f"协议版本不匹配: device={version} host={VERSION}")
        if length > MAX_PAYLOAD:
            raise RuntimeError(f"payload 长度异常: {length}")
        payload_crc = ser.read(length + 2)
        if len(payload_crc) != length + 2:
            break
        payload = payload_crc[:length]
        expect_crc = struct.unpack("<H", payload_crc[length:])[0]
        actual_crc = crc16_modbus(header + payload)
        if expect_crc != actual_crc:
            raise RuntimeError(f"响应 CRC 错误: expect=0x{expect_crc:04X} actual=0x{actual_crc:04X}")
        return Frame(version, flags, seq, cmd, status, payload)
    raise TimeoutError("等待 comm tool 响应超时")


class CommToolClient:
    def __init__(self, port: str, baud: int, timeout: float):
        serial = require_pyserial()
        self._ser = serial.Serial(port=port, baudrate=baud, timeout=0.05, write_timeout=timeout)
        self._timeout = timeout
        self._seq = 0

    def close(self) -> None:
        self._ser.close()

    def command(self, cmd: int, payload: bytes = b"", timeout: Optional[float] = None) -> Frame:
        self._seq = (self._seq + 1) & 0xFFFF
        if self._seq == 0:
            self._seq = 1
        frame = encode_frame(self._seq, cmd, payload)
        self._ser.write(frame)
        self._ser.flush()

        deadline = time.monotonic() + (timeout if timeout is not None else self._timeout)
        while time.monotonic() < deadline:
            resp = read_frame(self._ser, max(0.05, deadline - time.monotonic()))
            if resp.seq != self._seq:
                continue
            if resp.cmd != cmd:
                continue
            if (resp.flags & FLAG_ACK) == 0:
                continue
            if resp.status != 0:
                status_name = STATUS_TEXT.get(resp.status, f"0x{resp.status:02X}")
                raise RuntimeError(f"comm tool 返回错误: {status_name}")
            return resp
        raise TimeoutError(f"等待命令 0x{cmd:02X} 响应超时")

    def __enter__(self) -> "CommToolClient":
        return self

    def __exit__(self, _exc_type, _exc, _tb) -> None:
        self.close()
