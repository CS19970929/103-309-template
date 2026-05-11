#!/usr/bin/env python3
"""CAN BMS host tool.

Firmware upgrade follows "Feidao CAN protocol V1.6", section 7.
The protocol uses 29-bit extended CAN IDs:

    SrcID[28:24] | DstID[23:19] | CtrlCMD[18:16] | Index[15:8] | ChdIndex[7:0]
"""

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


APP_BASE_ADDR = 0x08004800
IAP_BASE_ADDR = 0x08000000
APP_MAX_END_EXCLUSIVE = 0x0801C000
APP_MAX_SIZE = APP_MAX_END_EXCLUSIVE - APP_BASE_ADDR

FEIDAO_NODE_IOT = 0x10
FEIDAO_NODE_CONTROLLER = 0x12
FEIDAO_NODE_METER = 0x13
FEIDAO_NODE_BATTERY = 0x14
FEIDAO_NODE_BROADCAST = 0x1F

FEIDAO_CTRL_WRITE = 0x00
FEIDAO_CTRL_READ = 0x01
FEIDAO_CTRL_ACK = 0x02
FEIDAO_CTRL_ERR_ACK = 0x03
FEIDAO_CTRL_LONG_START = 0x04
FEIDAO_CTRL_LONG_DATA = 0x05
FEIDAO_CTRL_LONG_END = 0x06
FEIDAO_CTRL_ALARM = 0x07

FEIDAO_UPGRADE_START_INDEX = 0x04
FEIDAO_UPGRADE_DATA_INDEX = 0x05
FEIDAO_UPGRADE_START_ACK_CHD = 0x01
FEIDAO_UPGRADE_CHUNK_CHD = 0x00

FEIDAO_UPGRADE_CHUNK_MAX_FRAMES = 256
FEIDAO_UPGRADE_CHUNK_BYTES = FEIDAO_UPGRADE_CHUNK_MAX_FRAMES * 8

FEIDAO_UPGRADE_STATUS_CHUNK_OK = 0x00
FEIDAO_UPGRADE_STATUS_CHUNK_CRC_ERROR = 0x01
FEIDAO_UPGRADE_STATUS_FILE_CRC_ERROR = 0x02
FEIDAO_UPGRADE_STATUS_DONE = 0x03
FEIDAO_UPGRADE_STATUS_OTHER_ERROR = 0xFF

CAN_APP_CMD_ID = 0x60
CAN_APP_ACK_ID = 0x61
CAN_APP_MAGIC = bytes([0xA5, 0x5A])
CAN_APP_ACK_MAGIC = bytes([0x5A, 0xA5])
CAN_APP_CMD_GET_STATUS = 0x01
CAN_APP_CMD_ENTER_IAP = 0x02


@dataclass(frozen=True)
class FeidaoCanId:
    src: int
    dst: int
    ctrl: int
    index: int
    chd_index: int


@dataclass(frozen=True)
class UpgradeChunk:
    index: int
    frame_count: int
    padded_payload: bytes
    crc16: int


@dataclass(frozen=True)
class UpgradePlan:
    image: bytes
    image_crc16: int
    image_crc32: int
    image_size: int
    total_data_frames: int
    total_long_packets: int
    chunks: tuple[UpgradeChunk, ...]
    host_node: int
    device_node: int


def crc16_modbus_update(crc: int, data: bytes) -> int:
    value_crc = crc & 0xFFFF
    for value in data:
        value_crc ^= value
        for _ in range(8):
            if value_crc & 0x0001:
                value_crc = (value_crc >> 1) ^ 0xA001
            else:
                value_crc >>= 1
    return value_crc & 0xFFFF


def crc16_modbus(data: bytes) -> int:
    return crc16_modbus_update(0xFFFF, data)


def be_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def be_i32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">i", data, offset)[0]


def signed_i8(value: int) -> int:
    return value - 256 if value & 0x80 else value


def check_node(value: int, name: str) -> int:
    if value < 0 or value > 0x1F:
        raise argparse.ArgumentTypeError(f"{name} must be 0..0x1F")
    return value


def parse_int(value: str) -> int:
    return int(value, 0)


def parse_node(value: str) -> int:
    return check_node(parse_int(value), "node")


def feidao_can_id(src: int, dst: int, ctrl: int, index: int, chd_index: int) -> int:
    return (
        ((src & 0x1F) << 24)
        | ((dst & 0x1F) << 19)
        | ((ctrl & 0x07) << 16)
        | ((index & 0xFF) << 8)
        | (chd_index & 0xFF)
    )


def decode_feidao_id(arbitration_id: int) -> FeidaoCanId:
    return FeidaoCanId(
        src=(arbitration_id >> 24) & 0x1F,
        dst=(arbitration_id >> 19) & 0x1F,
        ctrl=(arbitration_id >> 16) & 0x07,
        index=(arbitration_id >> 8) & 0xFF,
        chd_index=arbitration_id & 0xFF,
    )


def require_python_can():
    try:
        import can  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "缺少 python-can，请先安装：py -3.9 -m pip install python-can\n"
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
    decoded_id = decode_feidao_id(arbitration_id)
    if (
        decoded_id.src != FEIDAO_NODE_BATTERY
        or decoded_id.dst != FEIDAO_NODE_BROADCAST
        or decoded_id.ctrl != FEIDAO_CTRL_WRITE
        or decoded_id.index != 0x02
    ):
        return None

    ch = decoded_id.chd_index
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


def validate_image_vector(image: bytes) -> None:
    if len(image) < 8:
        raise SystemExit("App bin 太短，缺少向量表")
    msp = int.from_bytes(image[0:4], "little")
    reset = int.from_bytes(image[4:8], "little")
    reset_addr = reset & 0xFFFFFFFE
    if (msp & 0x2FFE0000) != 0x20000000:
        raise SystemExit(f"App 向量表 MSP 非 SRAM 地址: 0x{msp:08X}")
    if reset_addr < APP_BASE_ADDR or reset_addr >= APP_MAX_END_EXCLUSIVE:
        raise SystemExit(
            f"App Reset_Handler 超出允许范围: 0x{reset:08X}; "
            f"允许 0x{APP_BASE_ADDR:08X}..0x{APP_MAX_END_EXCLUSIVE - 1:08X}"
        )


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
    if len(image) > APP_MAX_SIZE:
        raise SystemExit(
            f"App bin 超出安全 App 区：{len(image)} bytes > {APP_MAX_SIZE} bytes，"
            f"允许范围 0x{APP_BASE_ADDR:08X}..0x{APP_MAX_END_EXCLUSIVE - 1:08X}"
        )
    validate_image_vector(image)
    return image


def build_upgrade_plan(image: bytes, host_node: int, device_node: int, long_index_base: int) -> UpgradePlan:
    image_size = len(image)
    total_data_frames = math.ceil(image_size / 8)
    total_long_packets = math.ceil(total_data_frames / FEIDAO_UPGRADE_CHUNK_MAX_FRAMES)
    chunks: list[UpgradeChunk] = []

    for chunk_seq in range(total_long_packets):
        offset = chunk_seq * FEIDAO_UPGRADE_CHUNK_BYTES
        remaining = image_size - offset
        frame_count = min(FEIDAO_UPGRADE_CHUNK_MAX_FRAMES, math.ceil(remaining / 8))
        padded_len = frame_count * 8
        payload = image[offset : offset + padded_len]
        if len(payload) < padded_len:
            payload = payload + bytes(padded_len - len(payload))
        chunks.append(
            UpgradeChunk(
                index=long_index_base + chunk_seq,
                frame_count=frame_count,
                padded_payload=payload,
                crc16=crc16_modbus(payload),
            )
        )

    return UpgradePlan(
        image=image,
        image_crc16=crc16_modbus(image),
        image_crc32=zlib.crc32(image) & 0xFFFFFFFF,
        image_size=image_size,
        total_data_frames=total_data_frames,
        total_long_packets=total_long_packets,
        chunks=tuple(chunks),
        host_node=host_node,
        device_node=device_node,
    )


def upgrade_start_frame(plan: UpgradePlan) -> tuple[int, bytes]:
    payload = (
        plan.total_long_packets.to_bytes(2, "big")
        + plan.image_crc16.to_bytes(2, "big")
        + plan.image_size.to_bytes(4, "big")
    )
    can_id = feidao_can_id(
        plan.host_node,
        plan.device_node,
        FEIDAO_CTRL_WRITE,
        FEIDAO_UPGRADE_START_INDEX,
        0x00,
    )
    return can_id, payload


def chunk_start_frame(plan: UpgradePlan, chunk: UpgradeChunk) -> tuple[int, bytes]:
    payload = (
        chunk.index.to_bytes(2, "big")
        + chunk.frame_count.to_bytes(2, "big")
        + bytes(4)
    )
    can_id = feidao_can_id(
        plan.host_node,
        plan.device_node,
        FEIDAO_CTRL_LONG_START,
        FEIDAO_UPGRADE_DATA_INDEX,
        FEIDAO_UPGRADE_CHUNK_CHD,
    )
    return can_id, payload


def iter_chunk_data_frames(plan: UpgradePlan, chunk: UpgradeChunk) -> Iterable[tuple[int, bytes, int]]:
    for seq in range(chunk.frame_count):
        payload = chunk.padded_payload[seq * 8 : (seq + 1) * 8]
        can_id = feidao_can_id(
            plan.host_node,
            plan.device_node,
            FEIDAO_CTRL_LONG_DATA,
            FEIDAO_UPGRADE_DATA_INDEX,
            seq & 0xFF,
        )
        yield can_id, payload, seq


def chunk_end_frame(plan: UpgradePlan, chunk: UpgradeChunk) -> tuple[int, bytes]:
    payload = chunk.crc16.to_bytes(2, "big") + bytes(6)
    can_id = feidao_can_id(
        plan.host_node,
        plan.device_node,
        FEIDAO_CTRL_LONG_END,
        FEIDAO_UPGRADE_DATA_INDEX,
        FEIDAO_UPGRADE_CHUNK_CHD,
    )
    return can_id, payload


def print_upgrade_plan(bin_path: Path, plan: UpgradePlan) -> None:
    start_id, start_payload = upgrade_start_frame(plan)
    print("CAN-IAP 升级 dry-run（飞道 CAN 通信协议 V1.6 第七节）")
    print(f"  bin: {bin_path}")
    print(f"  App 起始地址: 0x{APP_BASE_ADDR:08X}")
    print(f"  App 安全上限: 0x{APP_MAX_END_EXCLUSIVE - 1:08X}")
    print(f"  镜像大小: {plan.image_size} bytes")
    print(f"  数据帧数: {plan.total_data_frames}")
    print(f"  长包总包数: {plan.total_long_packets}")
    print(f"  文件 CRC16-Modbus: 0x{plan.image_crc16:04X}")
    print(f"  CRC32(参考): 0x{plan.image_crc32:08X}")
    print(f"  Host/SrcID: 0x{plan.host_node:02X}")
    print(f"  Device/DstID: 0x{plan.device_node:02X}")
    print(f"  A-0 起始帧: id=0x{start_id:08X} data={format_bytes(start_payload)}")
    if plan.chunks:
        first = plan.chunks[0]
        last = plan.chunks[-1]
        first_start_id, first_start_payload = chunk_start_frame(plan, first)
        first_data_id, first_data_payload, _seq = next(iter_chunk_data_frames(plan, first))
        last_end_id, last_end_payload = chunk_end_frame(plan, last)
        print(
            f"  首个长包: index={first.index} frames={first.frame_count} "
            f"crc16=0x{first.crc16:04X}"
        )
        print(f"    B-0 起始: id=0x{first_start_id:08X} data={format_bytes(first_start_payload)}")
        print(f"    首帧数据: id=0x{first_data_id:08X} data={format_bytes(first_data_payload)}")
        print(
            f"  末个长包: index={last.index} frames={last.frame_count} "
            f"crc16=0x{last.crc16:04X}"
        )
        print(f"    B-0 结束: id=0x{last_end_id:08X} data={format_bytes(last_end_payload)}")
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
        print("未自动探测到 CAN 适配器。请确认 PCAN/Kvaser/CANable 驱动和 interface/channel 参数。")
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
            if msg.is_extended_id:
                decoded_id = decode_feidao_id(msg.arbitration_id)
                line += (
                    f" | src=0x{decoded_id.src:02X} dst=0x{decoded_id.dst:02X} "
                    f"ctrl=0x{decoded_id.ctrl:X} index=0x{decoded_id.index:02X} chd=0x{decoded_id.chd_index:02X}"
                )
                decoded = decode_feidao_broadcast(msg.arbitration_id, data)
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


def build_plan_from_args(args: argparse.Namespace) -> tuple[Path, UpgradePlan]:
    device_node = args.device_node
    if args.node_id is not None:
        device_node = args.node_id
    bin_path = Path(args.bin).resolve()
    image = load_image(bin_path, args.app_address)
    plan = build_upgrade_plan(image, args.host_node, device_node, args.long_index_base)
    return bin_path, plan


def cmd_upgrade_dry_run(args: argparse.Namespace) -> int:
    bin_path, plan = build_plan_from_args(args)
    print_upgrade_plan(bin_path, plan)
    return 0


def wait_start_ack(bus, plan: UpgradePlan, timeout: float) -> None:
    expected_id = feidao_can_id(
        plan.device_node,
        plan.host_node,
        FEIDAO_CTRL_ACK,
        FEIDAO_UPGRADE_START_INDEX,
        FEIDAO_UPGRADE_START_ACK_CHD,
    )
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        msg = bus.recv(timeout=min(0.1, max(0.0, deadline - time.monotonic())))
        if msg is None:
            continue
        if msg.is_extended_id and msg.arbitration_id == expected_id:
            data = bytes(msg.data)
            if len(data) < 1:
                continue
            if data[0] == 1:
                print(f"IAP 起始 ACK: ready id=0x{expected_id:08X} data={format_bytes(data)}")
                return
            raise SystemExit(f"IAP 不可升级: status=0x{data[0]:02X} data={format_bytes(data)}")
    raise SystemExit(f"等待 IAP 起始 ACK 超时: id=0x{expected_id:08X}")


def wait_chunk_ack(bus, plan: UpgradePlan, timeout: float, final: bool = False) -> int:
    expected_id = feidao_can_id(
        plan.device_node,
        plan.host_node,
        FEIDAO_CTRL_LONG_START,
        FEIDAO_UPGRADE_DATA_INDEX,
        FEIDAO_UPGRADE_CHUNK_CHD,
    )
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        msg = bus.recv(timeout=min(0.1, max(0.0, deadline - time.monotonic())))
        if msg is None:
            continue
        if msg.is_extended_id and msg.arbitration_id == expected_id:
            data = bytes(msg.data)
            if len(data) < 1:
                continue
            status = data[0]
            if status == FEIDAO_UPGRADE_STATUS_CHUNK_OK:
                print(f"IAP 长包 ACK: status=0 id=0x{expected_id:08X}")
                if not final:
                    return status
                continue
            if status == FEIDAO_UPGRADE_STATUS_DONE:
                print(f"IAP 完成 ACK: status=3 id=0x{expected_id:08X}")
                return status
            if status == FEIDAO_UPGRADE_STATUS_CHUNK_CRC_ERROR:
                raise SystemExit(f"IAP 长包 CRC 错误: data={format_bytes(data)}")
            if status == FEIDAO_UPGRADE_STATUS_FILE_CRC_ERROR:
                raise SystemExit(f"IAP 文件总 CRC 错误: data={format_bytes(data)}")
            raise SystemExit(f"IAP 返回错误: status=0x{status:02X} data={format_bytes(data)}")
    raise SystemExit(f"等待 IAP 长包 ACK 超时: id=0x{expected_id:08X}")


def send_frame(bus, can_id: int, payload: bytes, timeout: float, gap_s: float) -> None:
    bus.send(make_message(can_id, payload), timeout=timeout)
    if gap_s > 0:
        time.sleep(gap_s)


def cmd_upgrade(args: argparse.Namespace) -> int:
    if args.confirm_app_address != f"0x{APP_BASE_ADDR:08X}":
        raise SystemExit("真实发送升级帧必须显式添加：-ConfirmAppAddress 0x08004800 或 --confirm-app-address 0x08004800")

    bin_path, plan = build_plan_from_args(args)
    print_upgrade_plan(bin_path, plan)
    print("开始发送 PDF V1.6 第七节 CAN-IAP 升级帧。")

    bus = open_bus(args.interface, args.channel, args.bitrate)
    gap_s = max(args.gap_ms, 0.0) / 1000.0
    wait_ack = not args.no_wait_ack

    try:
        start_id, start_payload = upgrade_start_frame(plan)
        send_frame(bus, start_id, start_payload, args.timeout, gap_s)
        print(f"已发送 A-0 起始帧: id=0x{start_id:08X} data={format_bytes(start_payload)}")
        if wait_ack:
            wait_start_ack(bus, plan, args.ack_timeout)

        sent_frames = 0
        for chunk_number, chunk in enumerate(plan.chunks, start=1):
            start_id, start_payload = chunk_start_frame(plan, chunk)
            send_frame(bus, start_id, start_payload, args.timeout, gap_s)
            print(
                f"已发送 B-0 长包起始: {chunk_number}/{plan.total_long_packets} "
                f"index={chunk.index} frames={chunk.frame_count} data={format_bytes(start_payload)}"
            )

            for data_id, data_payload, seq in iter_chunk_data_frames(plan, chunk):
                send_frame(bus, data_id, data_payload, args.timeout, gap_s)
                sent_frames += 1
                if args.progress_every > 0 and sent_frames % args.progress_every == 0:
                    print(f"  数据帧进度 {sent_frames}/{plan.total_data_frames} seq={seq}")

            end_id, end_payload = chunk_end_frame(plan, chunk)
            send_frame(bus, end_id, end_payload, args.timeout, gap_s)
            print(f"已发送 B-0 长包结束: index={chunk.index} crc16=0x{chunk.crc16:04X}")
            if wait_ack:
                wait_chunk_ack(bus, plan, args.ack_timeout, final=False)

        if wait_ack:
            status = wait_chunk_ack(bus, plan, args.final_ack_timeout, final=True)
            if status != FEIDAO_UPGRADE_STATUS_DONE:
                raise SystemExit(f"IAP 未返回升级完成状态: status=0x{status:02X}")
    finally:
        shutdown = getattr(bus, "shutdown", None)
        if callable(shutdown):
            shutdown()
    print("发送完成。升级是否成功以 IAP 完成 ACK、设备重启和 App 版本读回为准。")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="BMS CAN 上位机工具")
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

    p_dry = sub.add_parser("upgrade-dry-run", help="检查 App bin 并生成 PDF V1.6 CAN-IAP 分包计划")
    add_upgrade_args(p_dry)
    p_dry.set_defaults(func=cmd_upgrade_dry_run)

    p_upgrade = sub.add_parser("upgrade", help="按 PDF V1.6 第七节协议发送升级帧")
    add_can_args(p_upgrade)
    add_upgrade_args(p_upgrade)
    p_upgrade.add_argument("--confirm-app-address", default="", help="真实发送时必须填写 0x08004800")
    p_upgrade.add_argument("--no-wait-ack", action="store_true", help="实验用途：发送时不等待 IAP ACK")
    p_upgrade.add_argument("--wait-ack", action="store_true", help="兼容旧参数；当前默认等待 ACK")
    p_upgrade.add_argument("--ack-timeout", type=float, default=3.0, help="等待每次 ACK 超时秒数")
    p_upgrade.add_argument("--final-ack-timeout", type=float, default=8.0, help="等待最终完成 ACK 超时秒数")
    p_upgrade.add_argument("--gap-ms", type=float, default=1.0, help="帧间隔毫秒")
    p_upgrade.add_argument("--progress-every", type=int, default=256, help="每多少帧打印一次进度")
    p_upgrade.set_defaults(func=cmd_upgrade)

    return parser


def add_can_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--interface", default="pcan", help="python-can interface，如 pcan/kvaser/slcan/virtual")
    parser.add_argument("--channel", default="PCAN_USBBUS1", help="CAN 通道，如 PCAN_USBBUS1/0/COM3/test")
    parser.add_argument("--bitrate", type=int, default=250000, help="CAN 波特率，飞道协议默认 250000")
    parser.add_argument("--timeout", type=float, default=1.0, help="发送/接收超时秒数")


def add_app_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--can-address", type=parse_int, default=0, help="App 标准帧地址高位，当前工程默认 0")
    parser.add_argument("--ack-timeout", type=float, default=2.0, help="等待 App ACK 超时秒数")


def add_upgrade_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--bin", required=True, help="App bin 文件路径")
    parser.add_argument("--host-node", type=parse_node, default=FEIDAO_NODE_IOT, help="主节点 SrcID，默认 IOT 0x10")
    parser.add_argument("--device-node", type=parse_node, default=FEIDAO_NODE_BATTERY, help="从节点 DstID，默认电池 0x14")
    parser.add_argument("--node-id", type=parse_node, default=None, help="兼容旧参数；等同于 --device-node")
    parser.add_argument("--long-index-base", type=parse_int, default=0, choices=(0, 1), help="长包索引起始值，默认 0")
    parser.add_argument("--app-address", type=parse_int, default=APP_BASE_ADDR, help="App 起始地址，固定 0x08004800")


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
