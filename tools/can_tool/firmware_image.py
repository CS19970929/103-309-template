"""Firmware image range and vector-table validation."""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path
from typing import Iterable, Optional

from .crc import crc16_modbus


BMS_APP_BASE_ADDR = 0x08004800
COMM_TOOL_APP_BASE_ADDR = 0x08008000
APP_BASE_ADDR = BMS_APP_BASE_ADDR
IAP_BASE_ADDR = 0x08000000
BMS_APP_FLASH_LIMIT = 0x0801F800
COMM_TOOL_APP_FLASH_LIMIT = 0x08018000
APP_FLASH_LIMIT = BMS_APP_FLASH_LIMIT
SRAM_BASE = 0x20000000
SRAM_LIMIT = 0x20010000
BMS_SRAM_LIMIT = 0x20004FE0

FW_DATA_MAX_CHUNK = 512 - 4
FW_DATA_DEFAULT_CHUNK = 496


def image_limit(app_address: int) -> Optional[int]:
    if app_address == BMS_APP_BASE_ADDR:
        return BMS_APP_FLASH_LIMIT
    if app_address == COMM_TOOL_APP_BASE_ADDR:
        return COMM_TOOL_APP_FLASH_LIMIT
    return None


def image_sram_limit(app_address: int) -> int:
    if app_address == BMS_APP_BASE_ADDR:
        return BMS_SRAM_LIMIT
    return SRAM_LIMIT


def vector_summary(image: bytes, app_address: int = APP_BASE_ADDR) -> tuple[int, int, bool, bool]:
    if len(image) < 8:
        return 0, 0, False, False
    msp, reset = struct.unpack_from("<II", image, 0)
    msp_ok = SRAM_BASE <= msp < image_sram_limit(app_address)
    reset_thumb_ok = (reset & 1) == 1
    return msp, reset, msp_ok, reset_thumb_ok


def load_image(path: Path, app_address: int, *, allow_comm_tool: bool = True) -> bytes:
    if app_address == IAP_BASE_ADDR:
        raise SystemExit("拒绝升级地址 0x08000000：该地址是 IAP/Bootloader 起始地址。")
    if (not allow_comm_tool) and app_address != BMS_APP_BASE_ADDR:
        raise SystemExit(
            f"拒绝升级地址 0x{app_address:08X}：当前项目 App 固定地址必须是 0x{BMS_APP_BASE_ADDR:08X}。"
        )
    limit = image_limit(app_address)
    if limit is None:
        if allow_comm_tool:
            raise SystemExit(
                f"App 地址只允许 BMS 0x{BMS_APP_BASE_ADDR:08X} 或 comm tool 0x{COMM_TOOL_APP_BASE_ADDR:08X}，"
                f"实际为 0x{app_address:08X}。"
            )
        raise SystemExit(
            f"拒绝升级地址 0x{app_address:08X}：当前项目 App 固定地址必须是 0x{BMS_APP_BASE_ADDR:08X}。"
        )
    if not path.exists():
        raise SystemExit(f"找不到 bin 文件：{path}")
    image = path.read_bytes()
    if not image:
        raise SystemExit(f"bin 文件为空：{path}")
    if len(image) < 8:
        raise SystemExit(f"bin 文件太小，缺少向量表：{path}")
    if app_address + len(image) > limit:
        raise SystemExit(
            f"bin 超出 App 区: end=0x{app_address + len(image):08X}, limit=0x{limit:08X}"
        )
    msp, reset, msp_ok, reset_thumb_ok = vector_summary(image, app_address)
    reset_entry = reset & ~1
    if (not msp_ok) or (not reset_thumb_ok) or (reset_entry < app_address) or (reset_entry >= app_address + len(image)):
        raise SystemExit(
            f"App 向量表非法: MSP=0x{msp:08X}, Reset=0x{reset:08X}, "
            f"镜像区=0x{app_address:08X}..0x{app_address + len(image):08X}"
        )
    return image


def print_image_plan(path: Path, image: bytes, app_address: int, chunk_size: int) -> None:
    if chunk_size <= 0 or chunk_size > FW_DATA_MAX_CHUNK:
        raise ValueError(f"chunk-size 必须在 1..{FW_DATA_MAX_CHUNK} 之间")
    crc16 = crc16_modbus(image)
    crc32 = zlib.crc32(image) & 0xFFFFFFFF
    limit = image_limit(app_address)
    msp, reset, msp_ok, reset_thumb_ok = vector_summary(image, app_address)
    reset_entry = reset & ~1
    reset_ok = reset_thumb_ok and (limit is not None) and (app_address <= reset_entry < app_address + len(image))
    chunks = math.ceil(len(image) / chunk_size)
    print("comm tool 固件下载 dry-run")
    print(f"  bin: {path}")
    print(f"  App 地址: 0x{app_address:08X}")
    print(f"  大小: {len(image)} bytes")
    print(f"  分块: {chunks} x {chunk_size} bytes")
    print(f"  CRC16-Modbus: 0x{crc16:04X}")
    print(f"  CRC32: 0x{crc32:08X}")
    print(f"  初始 MSP: 0x{msp:08X} {'OK' if msp_ok else 'BAD'}")
    print(f"  ResetHandler: 0x{reset:08X} {'OK' if reset_ok else 'BAD'}")
    print("  结果: 仅检查文件和分块，未写入 comm tool。")


def iter_chunks(image: bytes, chunk_size: int) -> Iterable[tuple[int, bytes]]:
    for offset in range(0, len(image), chunk_size):
        yield offset, image[offset : offset + chunk_size]
