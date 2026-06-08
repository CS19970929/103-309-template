"""CAN-IAP extended-frame protocol helpers."""

from __future__ import annotations

import math
import zlib
from pathlib import Path
from typing import Iterable

from .crc import crc16_modbus
from .firmware_image import APP_BASE_ADDR, APP_FLASH_LIMIT
from .formatting import format_bytes


CAN_IAP_NODE_DEFAULT = 1
CAN_IAP_HOST_CTRL_BASE = 0x14F8F000
CAN_IAP_DEVICE_ACK_BASE = 0x14F8F100
CAN_IAP_DATA_BASE = 0x14000000
CAN_IAP_PROTOCOL_VERSION = 1

CMD_HELLO = 0x01
CMD_START = 0x02
CMD_COMMIT = 0x03
CMD_END = 0x04
CMD_ABORT = 0x05
CMD_ACK = 0x79
CMD_NACK = 0x1F


def iap_ctrl_id(node_id: int) -> int:
    return CAN_IAP_HOST_CTRL_BASE | (node_id & 0xFF)


def iap_ack_id(node_id: int) -> int:
    return CAN_IAP_DEVICE_ACK_BASE | (node_id & 0xFF)


def iap_data_id(node_id: int, seq: int) -> int:
    return CAN_IAP_DATA_BASE | ((seq & 0xFFFF) << 8) | (node_id & 0xFF)


def build_iap_control_frames(image: bytes, node_id: int) -> list[tuple[int, bytes, str]]:
    image_size = len(image)
    image_crc = crc16_modbus(image)
    frame_count = math.ceil(image_size / 8)
    return [
        (
            iap_ctrl_id(node_id),
            bytes([CMD_HELLO, CAN_IAP_PROTOCOL_VERSION, node_id & 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]),
            "HELLO",
        ),
        (
            iap_ctrl_id(node_id),
            bytes([CMD_START, CAN_IAP_PROTOCOL_VERSION])
            + image_size.to_bytes(4, "big")
            + image_crc.to_bytes(2, "big"),
            "START",
        ),
        (
            iap_ctrl_id(node_id),
            bytes([CMD_END])
            + frame_count.to_bytes(2, "big")
            + image_crc.to_bytes(2, "big")
            + bytes([0xFF, 0xFF, 0xFF]),
            "END",
        ),
    ]


def iter_iap_data_frames(image: bytes, node_id: int) -> Iterable[tuple[int, bytes, int]]:
    frame_count = math.ceil(len(image) / 8)
    for seq in range(frame_count):
        chunk = image[seq * 8 : (seq + 1) * 8]
        if len(chunk) < 8:
            chunk = chunk + bytes([0xFF] * (8 - len(chunk)))
        yield iap_data_id(node_id, seq), chunk, seq


def iter_iap_blocks(
    image: bytes, node_id: int, block_size: int = 256
) -> Iterable[tuple[list[tuple[int, bytes, int]], tuple[int, bytes, int]]]:
    seq = 0
    block_seq = 0
    for offset in range(0, len(image), block_size):
        block = image[offset : offset + block_size]
        data_frames: list[tuple[int, bytes, int]] = []
        for inner in range(math.ceil(len(block) / 8)):
            chunk = block[inner * 8 : (inner + 1) * 8]
            if len(chunk) < 8:
                chunk = chunk + bytes([0xFF] * (8 - len(chunk)))
            data_frames.append((iap_data_id(node_id, seq), chunk, seq))
            seq += 1

        block_crc = crc16_modbus(block)
        commit_payload = (
            bytes([CMD_COMMIT])
            + block_seq.to_bytes(2, "big")
            + len(block).to_bytes(2, "big")
            + block_crc.to_bytes(2, "big")
            + bytes([0xFF])
        )
        yield data_frames, (iap_ctrl_id(node_id), commit_payload, block_seq)
        block_seq += 1


def print_upgrade_plan(bin_path: Path, image: bytes, node_id: int) -> None:
    image_crc16 = crc16_modbus(image)
    image_crc32 = zlib.crc32(image) & 0xFFFFFFFF
    frame_count = math.ceil(len(image) / 8)
    print("CAN-IAP 升级 dry-run")
    print(f"  bin: {bin_path}")
    print(f"  App 起始地址: 0x{APP_BASE_ADDR:08X}")
    print(f"  App 区上限: 0x{APP_FLASH_LIMIT:08X}")
    print(f"  镜像大小: {len(image)} bytes")
    print(f"  数据帧数: {frame_count}")
    print(f"  CRC16-Modbus: 0x{image_crc16:04X}")
    print(f"  CRC32(参考): 0x{image_crc32:08X}")
    print(f"  控制帧ID: 0x{iap_ctrl_id(node_id):08X}")
    print(f"  应答帧ID: 0x{iap_ack_id(node_id):08X}")
    if frame_count:
        first_id, first_data, _ = next(iter_iap_data_frames(image, node_id))
        last_id, last_data, _ = list(iter_iap_data_frames(image, node_id))[-1]
        print(f"  首个数据帧: id=0x{first_id:08X} data={format_bytes(first_data)}")
        print(f"  末个数据帧: id=0x{last_id:08X} data={format_bytes(last_data)}")
    print("  结果: 仅分包检查，未发送 CAN 帧，未擦写 Flash。")
