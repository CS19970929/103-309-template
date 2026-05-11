#!/usr/bin/env python3
"""PC serial host for the STM32F103C8 CAN upgrader MCU."""

from __future__ import annotations

import argparse
import math
import struct
import sys
import time
import zlib
from pathlib import Path
from typing import Optional


APP_BASE_ADDR = 0x08004800
APP_MAX_END_EXCLUSIVE = 0x0801C000
APP_MAX_SIZE = APP_MAX_END_EXCLUSIVE - APP_BASE_ADDR

SOF = b"\x55\xAA"
VERSION = 0x01
MAX_PAYLOAD = 512
SERIAL_DATA_CHUNK = 480

FLAG_ACK = 0x01
FLAG_ERROR = 0x02

STATUS_TEXT = {
    0x00: "成功",
    0x01: "参数错误",
    0x02: "忙或当前状态不允许",
    0x03: "串口CRC错误",
    0x04: "CAN超时",
    0x05: "BMS返回错误",
    0x06: "BMS CRC错误",
    0x07: "固件非法",
    0x08: "缓存不足",
    0x09: "升级中止",
    0x0A: "保护触发",
    0xFF: "未知错误",
}

CMD_GET_DEVICE_INFO = 0x01
CMD_SET_CAN_CONFIG = 0x02
CMD_PING_BMS = 0x03
CMD_READ_BMS_SNAPSHOT = 0x10
CMD_CAN_OBJECT_READ = 0x11
CMD_CAN_OBJECT_WRITE = 0x12
CMD_PARAM_READ = 0x13
CMD_PARAM_WRITE = 0x14
CMD_ENTER_BMS_IAP = 0x20
CMD_UPGRADE_PREPARE = 0x21
CMD_UPGRADE_PACKET_DATA = 0x22
CMD_UPGRADE_PACKET_COMMIT = 0x23
CMD_UPGRADE_FINISH = 0x24
CMD_UPGRADE_ABORT = 0x25
CMD_GET_UPGRADE_STATUS = 0x26
CMD_CAN_RAW_TX = 0x30


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


def require_serial():
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "缺少 pyserial，请先安装：py -3.9 -m pip install pyserial\n"
            f"当前解释器：{sys.executable}"
        ) from exc
    return serial


def open_port(args: argparse.Namespace):
    serial = require_serial()
    return serial.Serial(args.port, args.baud, timeout=args.timeout, write_timeout=args.timeout)


def encode_frame(cmd: int, seq: int, payload: bytes = b"", flags: int = 0) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload too large: {len(payload)}")
    header = bytes([VERSION, cmd & 0xFF]) + struct.pack(">H", seq & 0xFFFF) + bytes([flags & 0xFF]) + struct.pack(">H", len(payload))
    header_crc = crc16_modbus(header)
    payload_crc = crc16_modbus(payload) if payload else 0xFFFF
    return SOF + header + struct.pack(">H", header_crc) + payload + struct.pack(">H", payload_crc)


def read_frame(port, timeout: float) -> tuple[int, int, int, bytes]:
    deadline = time.monotonic() + timeout
    sync = bytearray()
    while time.monotonic() < deadline:
        b = port.read(1)
        if not b:
            continue
        sync += b
        if len(sync) > 2:
            sync = sync[-2:]
        if bytes(sync) == SOF:
            break
    else:
        raise TimeoutError("等待升级器 MCU 响应超时")

    header = port.read(9)
    if len(header) != 9:
        raise TimeoutError("读取串口帧头超时")
    version, cmd = header[0], header[1]
    seq = struct.unpack(">H", header[2:4])[0]
    flags = header[4]
    payload_len = struct.unpack(">H", header[5:7])[0]
    header_crc = struct.unpack(">H", header[7:9])[0]
    if version != VERSION:
        raise RuntimeError(f"串口协议版本不匹配: {version}")
    if payload_len > MAX_PAYLOAD:
        raise RuntimeError(f"串口响应 payload 过大: {payload_len}")
    if header_crc != crc16_modbus(header[:7]):
        raise RuntimeError("串口响应 Header CRC 错误")
    payload = port.read(payload_len + 2)
    if len(payload) != payload_len + 2:
        raise TimeoutError("读取串口 payload 超时")
    body, payload_crc_raw = payload[:-2], payload[-2:]
    payload_crc = struct.unpack(">H", payload_crc_raw)[0]
    if payload_crc != (crc16_modbus(body) if body else 0xFFFF):
        raise RuntimeError("串口响应 Payload CRC 错误")
    return cmd, seq, flags, body


class UpgraderHost:
    def __init__(self, port, timeout: float):
        self.port = port
        self.timeout = timeout
        self.seq = 1

    def command(self, cmd: int, payload: bytes = b"", timeout: Optional[float] = None) -> bytes:
        seq = self.seq
        self.seq = (self.seq + 1) & 0xFFFF
        if self.seq == 0:
            self.seq = 1
        self.port.write(encode_frame(cmd, seq, payload))
        self.port.flush()
        resp_cmd, resp_seq, _flags, resp_payload = read_frame(self.port, timeout or self.timeout)
        if resp_cmd != cmd or resp_seq != seq:
            raise RuntimeError(f"响应不匹配: cmd=0x{resp_cmd:02X} seq={resp_seq}, expect cmd=0x{cmd:02X} seq={seq}")
        if len(resp_payload) < 2:
            raise RuntimeError("响应 payload 太短")
        status, detail = resp_payload[0], resp_payload[1]
        if status != 0:
            text = STATUS_TEXT.get(status, "未知")
            raise RuntimeError(f"升级器返回错误: status=0x{status:02X}({text}) detail=0x{detail:02X}")
        return resp_payload[2:]


def validate_image(image: bytes) -> None:
    if len(image) < 8:
        raise SystemExit("App bin 太短，缺少向量表")
    if len(image) > APP_MAX_SIZE:
        raise SystemExit(f"App bin 超出安全区: {len(image)} > {APP_MAX_SIZE}")
    msp = int.from_bytes(image[0:4], "little")
    reset = int.from_bytes(image[4:8], "little") & 0xFFFFFFFE
    if (msp & 0x2FFE0000) != 0x20000000:
        raise SystemExit(f"App MSP 非 SRAM 地址: 0x{msp:08X}")
    if reset < APP_BASE_ADDR or reset >= APP_MAX_END_EXCLUSIVE:
        raise SystemExit(f"App Reset_Handler 超出范围: 0x{reset:08X}")


def load_image(path: Path) -> bytes:
    image = path.read_bytes()
    if not image:
        raise SystemExit(f"bin 文件为空: {path}")
    validate_image(image)
    return image


def print_upgrade_plan(path: Path, image: bytes) -> None:
    file_crc = crc16_modbus(image)
    total_frames = math.ceil(len(image) / 8)
    total_packets = math.ceil(total_frames / 256)
    print("升级器 MCU 串口升级 dry-run")
    print(f"  bin: {path}")
    print(f"  App 地址: 0x{APP_BASE_ADDR:08X}")
    print(f"  App 安全上限: 0x{APP_MAX_END_EXCLUSIVE - 1:08X}")
    print(f"  文件大小: {len(image)} bytes")
    print(f"  CAN 数据帧数: {total_frames}")
    print(f"  CAN 长包数: {total_packets}")
    print(f"  CRC16-Modbus: 0x{file_crc:04X}")
    print(f"  CRC32(参考): 0x{zlib.crc32(image) & 0xFFFFFFFF:08X}")


def cmd_detect(_args: argparse.Namespace) -> int:
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        print("pyserial 未安装。安装命令: py -3.9 -m pip install pyserial")
        return 1
    ports = list(list_ports.comports())
    if not ports:
        print("未发现串口")
        return 0
    for item in ports:
        print(f"{item.device}: {item.description}")
    return 0


def cmd_info(args: argparse.Namespace) -> int:
    with open_port(args) as port:
        host = UpgraderHost(port, args.timeout)
        data = host.command(CMD_GET_DEVICE_INFO)
    if len(data) >= 16:
        name = data[16:].split(b"\x00", 1)[0].decode("ascii", errors="replace")
        print(f"升级器: {name}")
        print(f"  协议版本: {data[0]}")
        print(f"  固件版本: {data[1]}")
        print(f"  串口MTU: {struct.unpack('>H', data[2:4])[0]}")
        print(f"  长包缓存: {struct.unpack('>H', data[4:6])[0]} bytes")
        print(f"  能力: 0x{struct.unpack('>I', data[6:10])[0]:08X}")
        print(f"  CAN: {struct.unpack('>I', data[10:14])[0]} bps host=0x{data[14]:02X} device=0x{data[15]:02X}")
    else:
        print(data.hex(" "))
    return 0


def cmd_snapshot(args: argparse.Namespace) -> int:
    with open_port(args) as port:
        host = UpgraderHost(port, args.timeout)
        data = host.command(CMD_READ_BMS_SNAPSHOT)
    if len(data) < 31:
        print(data.hex(" "))
        return 0
    valid = struct.unpack(">I", data[0:4])[0]
    age = struct.unpack(">I", data[4:8])[0]
    voltage = struct.unpack(">I", data[8:12])[0]
    current = struct.unpack(">i", data[12:16])[0]
    soc, soh = data[16], data[17]
    temp = struct.unpack("b", data[18:19])[0]
    cycles = struct.unpack(">H", data[19:21])[0]
    print(f"valid=0x{valid:08X} age={age}ms")
    print(f"总压={voltage/1000:.3f}V 电流={current/1000:.3f}A SOC={soc}% SOH={soh}% 温度={temp}C 循环={cycles}")
    return 0


def cmd_read_object(args: argparse.Namespace) -> int:
    payload = bytes([args.index & 0xFF, args.chd & 0xFF]) + struct.pack(">H", int(args.can_timeout_ms))
    with open_port(args) as port:
        host = UpgraderHost(port, args.timeout)
        data = host.command(CMD_CAN_OBJECT_READ, payload)
    print(f"index=0x{data[0]:02X} chd=0x{data[1]:02X} data={data[2:10].hex(' ').upper()}")
    return 0


def cmd_write_object(args: argparse.Namespace) -> int:
    values = bytes(int(part, 16) for part in args.data.replace(",", " ").split())
    if len(values) != 8:
        raise SystemExit("--data 必须正好 8 字节，例如: 01 02 03 04 05 06 07 08")
    payload = bytes([args.index & 0xFF, args.chd & 0xFF]) + struct.pack(">H", int(args.can_timeout_ms)) + values
    with open_port(args) as port:
        host = UpgraderHost(port, args.timeout)
        data = host.command(CMD_CAN_OBJECT_WRITE, payload)
    print(f"写入完成: index=0x{data[0]:02X} chd=0x{data[1]:02X}")
    return 0


def cmd_read_param(args: argparse.Namespace) -> int:
    payload = struct.pack(">HH", args.param_id, int(args.can_timeout_ms))
    with open_port(args) as port:
        host = UpgraderHost(port, args.timeout)
        data = host.command(CMD_PARAM_READ, payload)
    param_id = struct.unpack(">H", data[0:2])[0]
    raw = struct.unpack(">i", data[2:6])[0]
    print(f"param=0x{param_id:04X} raw={raw} can={data[6:14].hex(' ').upper()}")
    return 0


def cmd_write_param(args: argparse.Namespace) -> int:
    confirm = 0xA5 if args.confirm else 0x00
    payload = struct.pack(">HiBH", args.param_id, args.raw_value, confirm, int(args.can_timeout_ms))
    with open_port(args) as port:
        host = UpgraderHost(port, args.timeout)
        data = host.command(CMD_PARAM_WRITE, payload)
    param_id = struct.unpack(">H", data[0:2])[0]
    raw = struct.unpack(">i", data[2:6])[0]
    print(f"写入完成: param=0x{param_id:04X} raw={raw} can={data[6:14].hex(' ').upper()}")
    return 0


def cmd_upgrade_dry_run(args: argparse.Namespace) -> int:
    path = Path(args.bin).resolve()
    image = load_image(path)
    print_upgrade_plan(path, image)
    return 0


def cmd_upgrade(args: argparse.Namespace) -> int:
    path = Path(args.bin).resolve()
    image = load_image(path)
    print_upgrade_plan(path, image)
    file_crc = crc16_modbus(image)
    total_frames = math.ceil(len(image) / 8)
    total_packets = math.ceil(total_frames / 256)
    with open_port(args) as port:
        host = UpgraderHost(port, args.timeout)
        if args.enter_iap:
            print("请求 BMS 进入 IAP...")
            host.command(CMD_ENTER_BMS_IAP, struct.pack(">H", int(args.can_timeout_ms)), timeout=args.iap_timeout)
            time.sleep(args.iap_delay)
        print("发送升级起始信息...")
        host.command(
            CMD_UPGRADE_PREPARE,
            struct.pack(">IHHH", len(image), file_crc, total_packets, int(args.can_timeout_ms)),
            timeout=args.iap_timeout,
        )
        for packet_index in range(total_packets):
            start = packet_index * 2048
            chunk = image[start : start + 2048]
            offset = 0
            while offset < len(chunk):
                part = chunk[offset : offset + SERIAL_DATA_CHUNK]
                payload = struct.pack(">HHH", packet_index, offset, len(part)) + part
                host.command(CMD_UPGRADE_PACKET_DATA, payload)
                offset += len(part)
            host.command(
                CMD_UPGRADE_PACKET_COMMIT,
                struct.pack(">HHH", packet_index, len(chunk), int(args.can_timeout_ms)),
                timeout=args.iap_timeout,
            )
            print(f"  长包进度 {packet_index + 1}/{total_packets}")
        print("等待 BMS 最终完成 ACK...")
        host.command(CMD_UPGRADE_FINISH, struct.pack(">H", int(args.final_timeout_ms)), timeout=args.final_timeout_ms / 1000.0 + 2.0)
    print("升级完成")
    return 0


def add_serial_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--port", required=True, help="升级器 MCU 串口，例如 COM8")
    parser.add_argument("--baud", type=int, default=115200, help="串口波特率，默认 115200")
    parser.add_argument("--timeout", type=float, default=2.0, help="串口响应超时秒数")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="STM32F103C8 CAN 升级器 MCU 串口上位工具")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("detect", help="列出可见串口").set_defaults(func=cmd_detect)

    p_info = sub.add_parser("info", help="读取升级器信息")
    add_serial_args(p_info)
    p_info.set_defaults(func=cmd_info)

    p_snapshot = sub.add_parser("snapshot", help="读取 BMS 广播缓存快照")
    add_serial_args(p_snapshot)
    p_snapshot.set_defaults(func=cmd_snapshot)

    p_ro = sub.add_parser("read-object", help="按飞道 CAN Index/ChdIndex 读取对象")
    add_serial_args(p_ro)
    p_ro.add_argument("--index", type=lambda v: int(v, 0), required=True)
    p_ro.add_argument("--chd", type=lambda v: int(v, 0), required=True)
    p_ro.add_argument("--can-timeout-ms", type=int, default=1000)
    p_ro.set_defaults(func=cmd_read_object)

    p_wo = sub.add_parser("write-object", help="按飞道 CAN Index/ChdIndex 写 8 字节对象")
    add_serial_args(p_wo)
    p_wo.add_argument("--index", type=lambda v: int(v, 0), required=True)
    p_wo.add_argument("--chd", type=lambda v: int(v, 0), required=True)
    p_wo.add_argument("--data", required=True, help="8字节十六进制，例如: 01 02 03 04 05 06 07 08")
    p_wo.add_argument("--can-timeout-ms", type=int, default=1000)
    p_wo.set_defaults(func=cmd_write_object)

    p_rp = sub.add_parser("read-param", help="按参数 ID 读取参数")
    add_serial_args(p_rp)
    p_rp.add_argument("--param-id", type=lambda v: int(v, 0), required=True)
    p_rp.add_argument("--can-timeout-ms", type=int, default=1000)
    p_rp.set_defaults(func=cmd_read_param)

    p_wp = sub.add_parser("write-param", help="按参数 ID 写参数")
    add_serial_args(p_wp)
    p_wp.add_argument("--param-id", type=lambda v: int(v, 0), required=True)
    p_wp.add_argument("--raw-value", type=int, required=True)
    p_wp.add_argument("--can-timeout-ms", type=int, default=1000)
    p_wp.add_argument("--confirm", action="store_true", help="关键参数写入确认")
    p_wp.set_defaults(func=cmd_write_param)

    p_dry = sub.add_parser("upgrade-dry-run", help="检查 App bin 并计算分包")
    p_dry.add_argument("--bin", required=True)
    p_dry.set_defaults(func=cmd_upgrade_dry_run)

    p_up = sub.add_parser("upgrade", help="通过升级器 MCU 串口升级 BMS")
    add_serial_args(p_up)
    p_up.add_argument("--bin", required=True)
    p_up.add_argument("--enter-iap", action="store_true", help="升级前先请求 BMS App 进入 IAP")
    p_up.add_argument("--iap-delay", type=float, default=0.5, help="进入 IAP 后等待秒数")
    p_up.add_argument("--can-timeout-ms", type=int, default=3000)
    p_up.add_argument("--iap-timeout", type=float, default=8.0)
    p_up.add_argument("--final-timeout-ms", type=int, default=10000)
    p_up.set_defaults(func=cmd_upgrade)
    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
