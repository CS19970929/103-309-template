#!/usr/bin/env python3
"""CAN BMS host tool for status listening and CAN-IAP upgrade framing."""

from __future__ import annotations

import argparse
import math
import struct
import sys
import time
import zlib
from pathlib import Path
from typing import Iterable, Optional


APP_BASE_ADDR = 0x08004800
IAP_BASE_ADDR = 0x08000000

FEIDAO_BROADCAST_BASE = 0x14F80200
CAN_IAP_NODE_DEFAULT = 1
CAN_IAP_HOST_CTRL_BASE = 0x14F8F000
CAN_IAP_DEVICE_ACK_BASE = 0x14F8F100
CAN_IAP_DATA_BASE = 0x14000000
CAN_IAP_PROTOCOL_VERSION = 1

CAN_APP_CMD_ID = 0x60
CAN_APP_ACK_ID = 0x61
CAN_APP_MAGIC = bytes([0xA5, 0x5A])
CAN_APP_ACK_MAGIC = bytes([0x5A, 0xA5])
CAN_APP_CMD_GET_STATUS = 0x01
CAN_APP_CMD_ENTER_IAP = 0x02

CMD_HELLO = 0x01
CMD_START = 0x02
CMD_COMMIT = 0x03
CMD_END = 0x04
CMD_ABORT = 0x05
CMD_ACK = 0x79
CMD_NACK = 0x1F


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


def be_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def be_i32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">i", data, offset)[0]


def signed_i8(value: int) -> int:
    return value - 256 if value & 0x80 else value


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


def format_bytes(data: bytes) -> str:
    return " ".join(f"{value:02X}" for value in data)


def decode_feidao_broadcast(arbitration_id: int, data: bytes) -> Optional[str]:
    if (arbitration_id & 0x1FFFFF00) != FEIDAO_BROADCAST_BASE:
        return None

    ch = arbitration_id & 0xFF
    if ch == 0 and len(data) >= 8:
        voltage_mv = be_u32(data, 0)
        current_ma = be_i32(data, 4)
        return f"总压={voltage_mv / 1000:.3f}V 电流={current_ma / 1000:.3f}A"
    if ch == 1 and len(data) >= 8:
        real_cap = be_u32(data, 0)
        design_cap = be_u32(data, 4)
        return f"实际容量raw={real_cap} 设计容量raw={design_cap}"
    if ch == 2 and len(data) >= 8:
        status = data[0]
        soc = data[1]
        temp_c = signed_i8(data[2])
        charge_time = be_u16(data, 3)
        bat_type = data[5]
        return f"充电状态={status} SOC={soc}% 温度={temp_c}C 剩余充电时间={charge_time}min 类型={bat_type}"
    if ch == 3 and len(data) >= 3:
        soh = data[0]
        cycles = be_u16(data, 1)
        return f"SOH={soh}% 循环={cycles}"
    if ch == 4 and len(data) >= 2:
        return f"协议版本={data[0]} 软件版本={data[1]}"
    if ch == 5 and len(data) >= 8:
        work_status = data[0]
        exception_status = data[1]
        cap_full = be_u16(data, 2)
        cap_now = be_u16(data, 4)
        cap_design = be_u16(data, 6)
        return (
            f"工作状态=0x{work_status:02X} 异常=0x{exception_status:02X} "
            f"满充容量raw={cap_full} 当前容量raw={cap_now} 设计容量raw={cap_design}"
        )
    if ch == 8 and len(data) >= 8:
        factory_cap = be_u16(data, 0)
        return f"出厂容量raw={factory_cap} 日期=20{data[5]:02d}-{data[6]:02d}-{data[7]:02d}"

    return f"广播通道={ch} 原始={format_bytes(data)}"


def iap_ctrl_id(node_id: int) -> int:
    return CAN_IAP_HOST_CTRL_BASE | (node_id & 0xFF)


def iap_ack_id(node_id: int) -> int:
    return CAN_IAP_DEVICE_ACK_BASE | (node_id & 0xFF)


def iap_data_id(node_id: int, seq: int) -> int:
    return CAN_IAP_DATA_BASE | ((seq & 0xFFFF) << 8) | (node_id & 0xFF)


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


def load_image(bin_path: Path, app_address: int) -> bytes:
    if app_address == IAP_BASE_ADDR:
        raise SystemExit("拒绝升级地址 0x08000000：该地址是 IAP/Bootloader 起始地址。")
    if app_address != APP_BASE_ADDR:
        raise SystemExit(f"拒绝升级地址 0x{app_address:08X}：当前项目 App 固定地址必须是 0x{APP_BASE_ADDR:08X}。")
    if not bin_path.exists():
        raise SystemExit(f"找不到 bin 文件：{bin_path}")
    image = bin_path.read_bytes()
    if not image:
        raise SystemExit(f"bin 文件为空：{bin_path}")
    return image


def print_upgrade_plan(bin_path: Path, image: bytes, node_id: int) -> None:
    image_crc16 = crc16_modbus(image)
    image_crc32 = zlib.crc32(image) & 0xFFFFFFFF
    frame_count = math.ceil(len(image) / 8)
    print("CAN-IAP 升级 dry-run")
    print(f"  bin: {bin_path}")
    print(f"  App 起始地址: 0x{APP_BASE_ADDR:08X}")
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


def cmd_detect(_args: argparse.Namespace) -> int:
    try:
        import can  # type: ignore
    except ImportError:
        print("python-can: 未安装")
        print("安装命令: py -3.9 -m pip install python-can")
        print(f"当前解释器: {sys.executable}")
        return 1

    print(f"python-can: {getattr(can, '__version__', 'unknown')}")
    print(f"当前解释器: {sys.executable}")
    try:
        configs = can.detect_available_configs()
    except Exception as exc:  # pragma: no cover - depends on local driver stack
        print(f"接口探测失败: {exc}")
        configs = []
    if configs:
        print("可探测到的 CAN 配置:")
        for config in configs:
            print(f"  {config}")
    else:
        print("未自动探测到 CAN 适配器。若已连接 PCAN/Kvaser/CANable，请确认驱动和 interface/channel 参数。")
    return 0


def cmd_listen(args: argparse.Namespace) -> int:
    bus = open_bus(args.interface, args.channel, args.bitrate)
    deadline = None if args.duration <= 0 else time.monotonic() + args.duration
    print(f"开始监听 CAN: interface={args.interface} channel={args.channel} bitrate={args.bitrate}")
    try:
        while deadline is None or time.monotonic() < deadline:
            msg = bus.recv(timeout=args.timeout)
            if msg is None:
                continue
            data = bytes(msg.data)
            frame_type = "EXT" if msg.is_extended_id else "STD"
            line = f"{time.strftime('%H:%M:%S')} {frame_type} 0x{msg.arbitration_id:08X} [{msg.dlc}] {format_bytes(data)}"
            decoded = decode_feidao_broadcast(msg.arbitration_id, data) if msg.is_extended_id else None
            if decoded:
                line += f" | {decoded}"
            print(line)
    finally:
        shutdown = getattr(bus, "shutdown", None)
        if callable(shutdown):
            shutdown()
    return 0


def send_app_command(args: argparse.Namespace, cmd: int, payload: bytes) -> bytes:
    bus = open_bus(args.interface, args.channel, args.bitrate)
    req_id = app_std_id(CAN_APP_CMD_ID, args.can_address)
    ack_id = app_std_id(CAN_APP_ACK_ID, args.can_address)
    try:
        bus.send(make_message(req_id, payload, extended=False), timeout=args.timeout)
        deadline = time.monotonic() + args.ack_timeout
        while time.monotonic() < deadline:
            msg = bus.recv(timeout=min(0.1, max(0.0, deadline - time.monotonic())))
            if msg is None:
                continue
            if (not msg.is_extended_id) and msg.arbitration_id == ack_id:
                data = bytes(msg.data)
                ack_cmd, status, _value0, _value1 = validate_app_ack(data)
                if ack_cmd != cmd:
                    raise SystemExit(f"App ACK 命令不匹配: expect=0x{cmd:02X} actual=0x{ack_cmd:02X}")
                if status != 0:
                    raise SystemExit(f"App 返回错误: cmd=0x{cmd:02X} status=0x{status:02X}")
                print(f"App ACK: id=0x{ack_id:03X} data={format_bytes(data)}")
                return data
        raise SystemExit(f"等待 App ACK 超时: id=0x{ack_id:03X}")
    finally:
        shutdown = getattr(bus, "shutdown", None)
        if callable(shutdown):
            shutdown()


def cmd_app_read_status(args: argparse.Namespace) -> int:
    payload = build_app_command(CAN_APP_CMD_GET_STATUS)
    data = send_app_command(args, CAN_APP_CMD_GET_STATUS, payload)
    _cmd, _status, soc, soh = validate_app_ack(data)
    print(f"基础状态: SOC={soc}% SOH={soh}%")
    return 0


def cmd_app_enter_iap(args: argparse.Namespace) -> int:
    if not args.confirm_enter_iap:
        raise SystemExit("进入 IAP 会让 App 复位，请显式添加 --confirm-enter-iap 或 PowerShell -ConfirmEnterIap")
    payload = build_app_command(CAN_APP_CMD_ENTER_IAP, 0xC3, 0x3C, args.can_address)
    data = send_app_command(args, CAN_APP_CMD_ENTER_IAP, payload)
    _cmd, _status, addr_hi, addr_lo = validate_app_ack(data)
    print(f"App 已接受进入 IAP 请求，ACK 地址提示=0x{addr_hi:02X}{addr_lo:02X}，约 200ms 后复位。")
    return 0


def cmd_upgrade_dry_run(args: argparse.Namespace) -> int:
    bin_path = Path(args.bin).resolve()
    image = load_image(bin_path, args.app_address)
    print_upgrade_plan(bin_path, image, args.node_id)
    return 0


def wait_ack(bus, node_id: int, expect_cmd: int, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    expected_id = iap_ack_id(node_id)
    while time.monotonic() < deadline:
        msg = bus.recv(timeout=min(0.1, max(0.0, deadline - time.monotonic())))
        if msg is None:
            continue
        data = bytes(msg.data)
        if msg.is_extended_id and msg.arbitration_id == expected_id and len(data) >= 2:
            if data[0] == CMD_ACK and data[1] == expect_cmd:
                return True
            if data[0] == CMD_NACK and data[1] == expect_cmd:
                code = data[5] if len(data) > 5 else 0
                raise SystemExit(f"IAP 返回 NACK: cmd=0x{expect_cmd:02X} code=0x{code:02X}")
    raise SystemExit(f"等待 IAP ACK 超时: cmd=0x{expect_cmd:02X}")


def cmd_upgrade(args: argparse.Namespace) -> int:
    if args.confirm_app_address != f"0x{APP_BASE_ADDR:08X}":
        raise SystemExit("真实发送升级帧必须显式添加：-ConfirmAppAddress 0x08004800 或 --confirm-app-address 0x08004800")

    bin_path = Path(args.bin).resolve()
    image = load_image(bin_path, args.app_address)
    print_upgrade_plan(bin_path, image, args.node_id)
    print("开始发送 CAN-IAP 帧。确认 IAP 固件已经实现本文档协议，否则设备不会升级。")

    bus = open_bus(args.interface, args.channel, args.bitrate)
    gap = max(args.gap_ms, 0) / 1000.0
    controls = build_iap_control_frames(image, args.node_id)
    try:
        for can_id, payload, name in controls[:2]:
            bus.send(make_message(can_id, payload), timeout=args.timeout)
            print(f"已发送 {name}: id=0x{can_id:08X} data={format_bytes(payload)}")
            if args.wait_ack:
                wait_ack(bus, args.node_id, payload[0], args.ack_timeout)
            if gap:
                time.sleep(gap)

        sent = 0
        total_data_frames = math.ceil(len(image) / 8)
        for data_frames, commit in iter_iap_blocks(image, args.node_id):
            for can_id, payload, seq in data_frames:
                bus.send(make_message(can_id, payload), timeout=args.timeout)
                sent += 1
                if sent % args.progress_every == 0:
                    print(f"  data {sent}/{total_data_frames} seq={seq}")
                if gap:
                    time.sleep(gap)

            can_id, payload, block_seq = commit
            bus.send(make_message(can_id, payload), timeout=args.timeout)
            print(f"  COMMIT block={block_seq} data={format_bytes(payload)}")
            if args.wait_ack:
                wait_ack(bus, args.node_id, CMD_COMMIT, args.ack_timeout)
            if gap:
                time.sleep(gap)

        can_id, payload, name = controls[2]
        bus.send(make_message(can_id, payload), timeout=args.timeout)
        print(f"已发送 {name}: id=0x{can_id:08X} data={format_bytes(payload)}")
        if args.wait_ack:
            wait_ack(bus, args.node_id, payload[0], args.ack_timeout)
    finally:
        shutdown = getattr(bus, "shutdown", None)
        if callable(shutdown):
            shutdown()
    print("发送完成。是否升级成功以 IAP ACK、板端重启和 App 版本读取为准。")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="BMS CAN 上位机起步工具")
    sub = parser.add_subparsers(dest="command", required=True)

    p_detect = sub.add_parser("detect", help="检测 python-can 环境")
    p_detect.set_defaults(func=cmd_detect)

    p_listen = sub.add_parser("listen", help="监听并解析现有电池 CAN 广播")
    add_can_args(p_listen)
    p_listen.add_argument("--duration", type=float, default=10.0, help="监听秒数，0 表示一直监听")
    p_listen.set_defaults(func=cmd_listen)

    p_app_status = sub.add_parser("app-read-status", help="通过 App CAN 命令读取基础 SOC/SOH")
    add_can_args(p_app_status)
    add_app_args(p_app_status)
    p_app_status.set_defaults(func=cmd_app_read_status)

    p_app_iap = sub.add_parser("app-enter-iap", help="通过 App CAN 命令写升级标志并复位进入 IAP")
    add_can_args(p_app_iap)
    add_app_args(p_app_iap)
    p_app_iap.add_argument("--confirm-enter-iap", action="store_true", help="确认让 App 复位进入 IAP")
    p_app_iap.set_defaults(func=cmd_app_enter_iap)

    p_dry = sub.add_parser("upgrade-dry-run", help="检查 App bin 并生成 CAN-IAP 分包计划")
    add_upgrade_args(p_dry)
    p_dry.set_defaults(func=cmd_upgrade_dry_run)

    p_upgrade = sub.add_parser("upgrade", help="按 CAN-IAP 协议发送升级帧")
    add_can_args(p_upgrade)
    add_upgrade_args(p_upgrade)
    p_upgrade.add_argument("--confirm-app-address", default="", help="真实发送时必须填写 0x08004800")
    p_upgrade.add_argument("--wait-ack", action="store_true", help="等待 IAP ACK")
    p_upgrade.add_argument("--ack-timeout", type=float, default=2.0, help="等待 ACK 超时秒数")
    p_upgrade.add_argument("--gap-ms", type=float, default=1.0, help="帧间隔毫秒")
    p_upgrade.add_argument("--progress-every", type=int, default=256, help="每多少帧打印一次进度")
    p_upgrade.set_defaults(func=cmd_upgrade)

    return parser


def add_can_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--interface", default="pcan", help="python-can interface，如 pcan/kvaser/slcan/virtual")
    parser.add_argument("--channel", default="PCAN_USBBUS1", help="CAN 通道，如 PCAN_USBBUS1/0/COM3/test")
    parser.add_argument("--bitrate", type=int, default=250000, help="CAN 波特率，当前固件默认 250000")
    parser.add_argument("--timeout", type=float, default=1.0, help="发送/接收超时秒数")


def add_app_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--can-address", type=lambda value: int(value, 0), default=0, help="App 标准帧地址高位，当前工程默认 0")
    parser.add_argument("--ack-timeout", type=float, default=2.0, help="等待 App ACK 超时秒数")


def add_upgrade_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--bin", required=True, help="App bin 文件路径")
    parser.add_argument("--node-id", type=lambda value: int(value, 0), default=CAN_IAP_NODE_DEFAULT, help="节点 ID，默认 1")
    parser.add_argument("--app-address", type=lambda value: int(value, 0), default=APP_BASE_ADDR, help="App 起始地址，固定 0x08004800")


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
