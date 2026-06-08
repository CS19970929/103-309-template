"""BMS App standard-frame CAN service helpers."""

from __future__ import annotations

from .crc import crc16_modbus
from .formatting import format_bytes


CAN_APP_CMD_ID = 0x60
CAN_APP_ACK_ID = 0x61
CAN_APP_MAGIC = bytes([0xA5, 0x5A])
CAN_APP_ACK_MAGIC = bytes([0x5A, 0xA5])
CAN_APP_CMD_GET_STATUS = 0x01
CAN_APP_CMD_ENTER_IAP = 0x02
CAN_APP_CMD_READ_REG = 0x03
CAN_APP_CMD_WRITE_PREP = 0x04
CAN_APP_CMD_WRITE_COMMIT = 0x05
CAN_APP_CMD_READ_BLOCK = 0x06
CAN_APP_CMD_AGING_START = 0x07
CAN_APP_CMD_AGING_STOP = 0x08
CAN_APP_CMD_AGING_RESET_TIME = 0x09
CAN_APP_CMD_AGING_SET_HOURS = 0x0A
CAN_APP_CMD_READ_BLOCK_DATA = 0x86
CAN_APP_READ_BLOCK_MAX_WORDS = 120
CAN_APP_AGING_GUARD = 0xA9
CAN_APP_AGING_ACTION_START = 0x51
CAN_APP_AGING_ACTION_STOP = 0x50
CAN_APP_AGING_ACTION_RESET_TIME = 0x5A
APP_SET_ONCE_SOC_ADDR = 0x1005


def app_std_id(base_id: int, can_address: int) -> int:
    return ((can_address & 0x0F) << 7) | (base_id & 0x7F)


def build_app_command(cmd: int, arg0: int = 0, arg1: int = 0, arg2: int = 0) -> bytes:
    payload = bytearray(8)
    payload[0:2] = CAN_APP_MAGIC
    payload[2] = cmd & 0xFF
    payload[3] = arg0 & 0xFF
    payload[4] = arg1 & 0xFF
    payload[5] = arg2 & 0xFF
    crc = crc16_modbus(bytes(payload[:6]))
    payload[6] = (crc >> 8) & 0xFF
    payload[7] = crc & 0xFF
    return bytes(payload)


def validate_app_ack(data: bytes) -> tuple[int, int, int, int]:
    if len(data) != 8:
        raise SystemExit(f"App ACK 长度错误: {len(data)}")
    if data[0:2] != CAN_APP_ACK_MAGIC:
        raise SystemExit(f"App ACK magic 错误: {format_bytes(data)}")
    crc_expect = (data[6] << 8) | data[7]
    crc_actual = crc16_modbus(data[:6])
    if crc_expect != crc_actual:
        raise SystemExit(f"App ACK CRC 错误: expect=0x{crc_expect:04X} actual=0x{crc_actual:04X}")
    return data[2], data[3], data[4], data[5]
