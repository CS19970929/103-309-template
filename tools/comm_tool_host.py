#!/usr/bin/env python3
"""PC host tool for the UART-to-CAN BMS comm tool."""

from __future__ import annotations

import argparse
import math
import struct
import sys
import time
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional


MAGIC = 0xAA55
VERSION = 1
FLAG_ACK = 0x01
MAX_PAYLOAD = 512

APP_BASE_ADDR = 0x08004800
IAP_BASE_ADDR = 0x08000000
APP_FLASH_LIMIT = 0x08020000
SRAM_BASE = 0x20000000
SRAM_LIMIT = 0x20005000

CMD_GET_INFO = 0x01
CMD_SET_CAN = 0x02
CMD_BMS_READ = 0x10
CMD_BMS_WRITE = 0x11
CMD_FW_BEGIN = 0x20
CMD_FW_DATA = 0x21
CMD_FW_END = 0x22
CMD_FW_INFO = 0x23
CMD_ENTER_IAP = 0x30
CMD_UPGRADE = 0x31
CMD_UPGRADE_STATUS = 0x32
CMD_UPGRADE_ABORT = 0x33
CMD_RAW_CAN_TX = 0x40

STATUS_TEXT = {
    0x00: "OK",
    0x01: "CRC_ERROR",
    0x02: "UNSUPPORTED",
    0x03: "BAD_PARAM",
    0x04: "BAD_STATE",
    0x05: "FLASH_ERROR",
    0x06: "CAN_TIMEOUT",
    0x07: "BMS_ERROR",
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


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


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


def format_hex(data: bytes) -> str:
    return " ".join(f"{value:02X}" for value in data)


def load_image(path: Path, app_address: int) -> bytes:
    if app_address == IAP_BASE_ADDR:
        raise SystemExit("拒绝 App 地址 0x08000000，该地址是 IAP/Bootloader 起始地址。")
    if app_address != APP_BASE_ADDR:
        raise SystemExit(f"当前阶段 App 地址必须是 0x{APP_BASE_ADDR:08X}，实际为 0x{app_address:08X}。")
    if not path.exists():
        raise SystemExit(f"找不到 bin 文件: {path}")
    image = path.read_bytes()
    if not image:
        raise SystemExit(f"bin 文件为空: {path}")
    if app_address + len(image) > APP_FLASH_LIMIT:
        raise SystemExit(
            f"bin 超出 App 区: end=0x{app_address + len(image):08X}, limit=0x{APP_FLASH_LIMIT:08X}"
        )
    return image


def vector_summary(image: bytes) -> tuple[int, int, bool, bool]:
    if len(image) < 8:
        return 0, 0, False, False
    msp, reset = struct.unpack_from("<II", image, 0)
    msp_ok = SRAM_BASE <= msp < SRAM_LIMIT
    reset_ok = APP_BASE_ADDR <= reset < APP_FLASH_LIMIT and (reset & 1) == 1
    return msp, reset, msp_ok, reset_ok


def print_image_plan(path: Path, image: bytes, app_address: int, chunk_size: int) -> None:
    crc16 = crc16_modbus(image)
    crc32 = zlib.crc32(image) & 0xFFFFFFFF
    msp, reset, msp_ok, reset_ok = vector_summary(image)
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


def open_client(args) -> CommToolClient:
    return CommToolClient(args.port, args.baud, args.timeout)


def cmd_list_ports(_args) -> int:
    require_pyserial()
    from serial.tools import list_ports  # type: ignore

    ports = list(list_ports.comports())
    if not ports:
        print("未发现串口。")
        return 0
    for port in ports:
        print(f"{port.device}\t{port.description}")
    return 0


def cmd_info(args) -> int:
    with open_client(args) as client:
        resp = client.command(CMD_GET_INFO)
    payload = resp.payload
    print(f"GET_INFO raw: {format_hex(payload)}")
    if len(payload) >= 20:
        proto, fw_major, fw_minor, fw_patch = struct.unpack_from("<BBBB", payload, 0)
        bitrate, cache_base, cache_size, flags = struct.unpack_from("<IIII", payload, 4)
        print(f"  协议版本: {proto}")
        print(f"  固件版本: {fw_major}.{fw_minor}.{fw_patch}")
        print(f"  CAN 波特率: {bitrate}")
        print(f"  缓存区: 0x{cache_base:08X} + {cache_size} bytes")
        print(f"  flags: 0x{flags:08X}")
    return 0


def cmd_fw_dry_run(args) -> int:
    path = Path(args.bin).resolve()
    image = load_image(path, args.app_address)
    print_image_plan(path, image, args.app_address, args.chunk_size)
    return 0


def cmd_fw_download(args) -> int:
    expected = f"0x{APP_BASE_ADDR:08X}"
    if args.confirm_app_address != expected:
        raise SystemExit(f"真实下载必须显式确认 App 地址: -ConfirmAppAddress {expected}")

    path = Path(args.bin).resolve()
    image = load_image(path, args.app_address)
    print_image_plan(path, image, args.app_address, args.chunk_size)

    crc16 = crc16_modbus(image)
    crc32 = zlib.crc32(image) & 0xFFFFFFFF
    begin_payload = struct.pack("<IIHI", args.app_address, len(image), crc16, crc32)

    with open_client(args) as client:
        client.command(CMD_FW_BEGIN, begin_payload, timeout=args.long_timeout)
        total = math.ceil(len(image) / args.chunk_size)
        for index, (offset, chunk) in enumerate(iter_chunks(image, args.chunk_size), 1):
            client.command(CMD_FW_DATA, struct.pack("<I", offset) + chunk, timeout=args.long_timeout)
            if index == total or index % args.progress_every == 0:
                print(f"  下载进度: {index}/{total} offset=0x{offset:08X}")
        client.command(CMD_FW_END, struct.pack("<IHI", len(image), crc16, crc32), timeout=args.long_timeout)
    print("固件已写入 comm tool 缓存并完成校验。")
    return 0


def cmd_fw_info(args) -> int:
    with open_client(args) as client:
        resp = client.command(CMD_FW_INFO)
    payload = resp.payload
    print(f"FW_INFO raw: {format_hex(payload)}")
    if len(payload) >= 15:
        app_addr, size, crc16, crc32, valid = struct.unpack_from("<IIHIB", payload, 0)
        print(f"  App 地址: 0x{app_addr:08X}")
        print(f"  大小: {size} bytes")
        print(f"  CRC16: 0x{crc16:04X}")
        print(f"  CRC32: 0x{crc32:08X}")
        print(f"  有效: {valid}")
    return 0


def cmd_bms_read(args) -> int:
    payload = struct.pack("<HH", args.address, args.count)
    with open_client(args) as client:
        resp = client.command(CMD_BMS_READ, payload, timeout=args.long_timeout)
    if len(resp.payload) % 2 != 0:
        raise RuntimeError("BMS_READ 响应长度不是 2 字节对齐")
    words = list(struct.unpack("<" + "H" * (len(resp.payload) // 2), resp.payload))
    for index, value in enumerate(words):
        print(f"0x{args.address + index:04X}: 0x{value:04X} ({value})")
    return 0


def cmd_bms_write(args) -> int:
    words = [int(value, 0) & 0xFFFF for value in args.values]
    payload = struct.pack("<HH", args.address, len(words))
    payload += struct.pack("<" + "H" * len(words), *words)
    with open_client(args) as client:
        client.command(CMD_BMS_WRITE, payload, timeout=args.long_timeout)
    print(f"已写入 BMS: addr=0x{args.address:04X} words={len(words)}")
    return 0


def cmd_enter_iap(args) -> int:
    if not args.confirm_enter_iap:
        raise SystemExit("进入 IAP 会让 BMS App 复位，请添加 -ConfirmEnterIap。")
    with open_client(args) as client:
        client.command(CMD_ENTER_IAP, timeout=args.long_timeout)
    print("BMS 已接受进入 IAP 请求。")
    return 0


def cmd_upgrade(args) -> int:
    if not args.confirm_upgrade:
        raise SystemExit("一键升级会擦写 BMS App，请添加 -ConfirmUpgrade。")
    with open_client(args) as client:
        client.command(CMD_UPGRADE, timeout=args.long_timeout)
    print("comm tool 已开始使用缓存固件升级 BMS。")
    return 0


def cmd_upgrade_status(args) -> int:
    with open_client(args) as client:
        resp = client.command(CMD_UPGRADE_STATUS)
    payload = resp.payload
    print(f"UPGRADE_STATUS raw: {format_hex(payload)}")
    if len(payload) >= 12:
        state, percent, error = struct.unpack_from("<BBB", payload, 0)
        written, total, expect_seq = struct.unpack_from("<IIH", payload, 3)
        print(f"  state={state} percent={percent}% error=0x{error:02X}")
        print(f"  written={written} total={total} expect_seq={expect_seq}")
    return 0


def cmd_abort(args) -> int:
    with open_client(args) as client:
        client.command(CMD_UPGRADE_ABORT)
    print("已发送升级终止命令。")
    return 0


def add_serial_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--port", required=True, help="comm tool 串口，如 COM4")
    parser.add_argument("--baud", type=int, default=115200, help="串口波特率")
    parser.add_argument("--timeout", type=float, default=1.0, help="普通命令超时秒数")
    parser.add_argument("--long-timeout", type=float, default=5.0, help="Flash/CAN 长命令超时秒数")


def add_fw_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--bin", required=True, help="BMS App bin 文件")
    parser.add_argument("--app-address", type=lambda value: int(value, 0), default=APP_BASE_ADDR)
    parser.add_argument("--chunk-size", type=int, default=256)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PC 串口 comm tool 上位机")
    sub = parser.add_subparsers(dest="command", required=True)

    p_ports = sub.add_parser("list-ports", help="列出串口")
    p_ports.set_defaults(func=cmd_list_ports)

    p_info = sub.add_parser("info", help="读取 comm tool 信息")
    add_serial_args(p_info)
    p_info.set_defaults(func=cmd_info)

    p_dry = sub.add_parser("fw-dry-run", help="检查 BMS App bin 和分块")
    add_fw_args(p_dry)
    p_dry.set_defaults(func=cmd_fw_dry_run)

    p_download = sub.add_parser("fw-download", help="下载 BMS App 到 comm tool 缓存")
    add_serial_args(p_download)
    add_fw_args(p_download)
    p_download.add_argument("--confirm-app-address", default="")
    p_download.add_argument("--progress-every", type=int, default=16)
    p_download.set_defaults(func=cmd_fw_download)

    p_fw_info = sub.add_parser("fw-info", help="查询 comm tool 缓存固件")
    add_serial_args(p_fw_info)
    p_fw_info.set_defaults(func=cmd_fw_info)

    p_read = sub.add_parser("bms-read", help="通过 comm tool 读取 BMS 寄存器")
    add_serial_args(p_read)
    p_read.add_argument("--address", type=lambda value: int(value, 0), required=True)
    p_read.add_argument("--count", type=lambda value: int(value, 0), required=True)
    p_read.set_defaults(func=cmd_bms_read)

    p_write = sub.add_parser("bms-write", help="通过 comm tool 写 BMS 寄存器")
    add_serial_args(p_write)
    p_write.add_argument("--address", type=lambda value: int(value, 0), required=True)
    p_write.add_argument("values", nargs="+")
    p_write.set_defaults(func=cmd_bms_write)

    p_enter = sub.add_parser("enter-iap", help="让 BMS App 进入 IAP")
    add_serial_args(p_enter)
    p_enter.add_argument("--confirm-enter-iap", action="store_true")
    p_enter.set_defaults(func=cmd_enter_iap)

    p_upgrade = sub.add_parser("upgrade", help="使用 comm tool 缓存固件一键升级 BMS")
    add_serial_args(p_upgrade)
    p_upgrade.add_argument("--confirm-upgrade", action="store_true")
    p_upgrade.set_defaults(func=cmd_upgrade)

    p_status = sub.add_parser("upgrade-status", help="读取升级状态")
    add_serial_args(p_status)
    p_status.set_defaults(func=cmd_upgrade_status)

    p_abort = sub.add_parser("upgrade-abort", help="终止升级")
    add_serial_args(p_abort)
    p_abort.set_defaults(func=cmd_abort)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())

