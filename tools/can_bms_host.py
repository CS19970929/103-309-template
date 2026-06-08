#!/usr/bin/env python3
"""CAN BMS host tool for status listening and CAN-IAP upgrade framing."""

from __future__ import annotations

import argparse
import math
import sys
import time
from pathlib import Path
from typing import Optional

from can_tool.app_can import (
    APP_SET_ONCE_SOC_ADDR,
    CAN_APP_ACK_ID,
    CAN_APP_ACK_MAGIC,
    CAN_APP_AGING_ACTION_RESET_TIME,
    CAN_APP_AGING_ACTION_START,
    CAN_APP_AGING_ACTION_STOP,
    CAN_APP_AGING_GUARD,
    CAN_APP_CMD_AGING_RESET_TIME,
    CAN_APP_CMD_AGING_SET_HOURS,
    CAN_APP_CMD_AGING_START,
    CAN_APP_CMD_AGING_STOP,
    CAN_APP_CMD_ENTER_IAP,
    CAN_APP_CMD_GET_STATUS,
    CAN_APP_CMD_ID,
    CAN_APP_CMD_READ_BLOCK,
    CAN_APP_CMD_READ_BLOCK_DATA,
    CAN_APP_CMD_READ_REG,
    CAN_APP_CMD_WRITE_COMMIT,
    CAN_APP_CMD_WRITE_PREP,
    CAN_APP_READ_BLOCK_MAX_WORDS,
    CAN_APP_MAGIC,
    app_std_id,
    build_app_command,
    validate_app_ack,
)
from can_tool.can_iap import (
    CAN_IAP_DATA_BASE,
    CAN_IAP_DEVICE_ACK_BASE,
    CAN_IAP_HOST_CTRL_BASE,
    CAN_IAP_NODE_DEFAULT,
    CAN_IAP_PROTOCOL_VERSION,
    CMD_ACK,
    CMD_ABORT,
    CMD_COMMIT,
    CMD_END,
    CMD_HELLO,
    CMD_NACK,
    CMD_START,
    build_iap_control_frames,
    iap_ack_id,
    iap_ctrl_id,
    iap_data_id,
    iter_iap_blocks,
    iter_iap_data_frames,
    print_upgrade_plan,
)
from can_tool.crc import crc16_modbus
from can_tool.feidao_decode import FEIDAO_BROADCAST_BASE, decode_feidao_broadcast
from can_tool.firmware_image import (
    APP_BASE_ADDR,
    APP_FLASH_LIMIT,
    IAP_BASE_ADDR,
    SRAM_BASE,
    BMS_SRAM_LIMIT as SRAM_LIMIT,
    load_image as _load_image,
)
from can_tool.formatting import (
    aging_state_name,
    be_i32,
    be_u16,
    be_u32,
    format_bytes,
    format_remaining_minutes,
    signed_i8,
)
from can_tool.python_can_bus import make_message, open_bus, require_python_can


def load_image(bin_path: Path, app_address: int) -> bytes:
    return _load_image(bin_path, app_address, allow_comm_tool=False)


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


def send_app_command_on_bus(bus, args: argparse.Namespace, cmd: int, payload: bytes, *, print_ack: bool = True) -> bytes:
    req_id = app_std_id(CAN_APP_CMD_ID, args.can_address)
    ack_id = app_std_id(CAN_APP_ACK_ID, args.can_address)

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
            if print_ack:
                print(f"App ACK: id=0x{ack_id:03X} data={format_bytes(data)}")
            return data
    raise SystemExit(f"等待 App ACK 超时: id=0x{ack_id:03X}")


def send_app_command(args: argparse.Namespace, cmd: int, payload: bytes) -> bytes:
    bus = open_bus(args.interface, args.channel, args.bitrate)
    try:
        return send_app_command_on_bus(bus, args, cmd, payload)
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


def send_app_read_words(args: argparse.Namespace, addr: int, count: int) -> list[int]:
    if count <= 0 or count > CAN_APP_READ_BLOCK_MAX_WORDS:
        raise SystemExit(f"--count 必须在 1..{CAN_APP_READ_BLOCK_MAX_WORDS} 范围内")
    if addr < 0 or addr > 0xFFFF or addr + count - 1 > 0xFFFF:
        raise SystemExit("--address/--count 超出 16-bit 寄存器地址范围")

    bus = open_bus(args.interface, args.channel, args.bitrate)
    ack_id = app_std_id(CAN_APP_ACK_ID, args.can_address)
    try:
        payload = build_app_command(CAN_APP_CMD_READ_BLOCK, (addr >> 8) & 0xFF, addr & 0xFF, count)
        ack = send_app_command_on_bus(bus, args, CAN_APP_CMD_READ_BLOCK, payload)
        _cmd, _status, ack_count, _unused = validate_app_ack(ack)
        if ack_count != count:
            raise SystemExit(f"READ_BLOCK ACK 数量不匹配: expect={count} actual={ack_count}")

        words: list[Optional[int]] = [None] * count
        block_timeout = getattr(args, "block_timeout", max(2.0, args.ack_timeout + count * 0.05))
        deadline = time.monotonic() + block_timeout
        while time.monotonic() < deadline and any(value is None for value in words):
            msg = bus.recv(timeout=min(0.1, max(0.0, deadline - time.monotonic())))
            if msg is None or msg.is_extended_id or msg.arbitration_id != ack_id:
                continue
            block_data = bytes(msg.data)
            block_cmd, seq, value_hi, value_lo = validate_app_ack(block_data)
            if block_cmd != CAN_APP_CMD_READ_BLOCK_DATA:
                continue
            if seq < count and words[seq] is None:
                words[seq] = (value_hi << 8) | value_lo

        missing = [index for index, value in enumerate(words) if value is None]
        if missing:
            raise SystemExit(f"READ_BLOCK 数据帧缺失: seq={missing}")
        return [int(value) for value in words]
    finally:
        shutdown = getattr(bus, "shutdown", None)
        if callable(shutdown):
            shutdown()


def cmd_app_read_reg(args: argparse.Namespace) -> int:
    words = send_app_read_words(args, args.address, 1)
    print(f"0x{args.address:04X}: 0x{words[0]:04X} ({words[0]})")
    return 0


def cmd_app_read_block(args: argparse.Namespace) -> int:
    words = send_app_read_words(args, args.address, args.count)
    for index, value in enumerate(words):
        print(f"0x{args.address + index:04X}: 0x{value:04X} ({value})")
    return 0


def send_app_write_word_on_bus(bus, args: argparse.Namespace, addr: int, value: int) -> bytes:
    prep = build_app_command(
        CAN_APP_CMD_WRITE_PREP,
        (addr >> 8) & 0xFF,
        addr & 0xFF,
        (value >> 8) & 0xFF,
    )
    send_app_command_on_bus(bus, args, CAN_APP_CMD_WRITE_PREP, prep)
    commit = build_app_command(
        CAN_APP_CMD_WRITE_COMMIT,
        (addr >> 8) & 0xFF,
        addr & 0xFF,
        value & 0xFF,
    )
    return send_app_command_on_bus(bus, args, CAN_APP_CMD_WRITE_COMMIT, commit)


def send_app_write_word(args: argparse.Namespace, addr: int, value: int) -> bytes:
    bus = open_bus(args.interface, args.channel, args.bitrate)
    try:
        return send_app_write_word_on_bus(bus, args, addr, value)
    finally:
        shutdown = getattr(bus, "shutdown", None)
        if callable(shutdown):
            shutdown()


def cmd_app_write_reg(args: argparse.Namespace) -> int:
    if not args.confirm_write_reg:
        raise SystemExit("写寄存器会修改板端参数，请显式添加 --confirm-write-reg")
    if args.address < 0 or args.address > 0xFFFF:
        raise SystemExit("--address 超出 16-bit 寄存器地址范围")
    value = args.value & 0xFFFF
    send_app_write_word(args, args.address, value)
    print(f"已写入寄存器: 0x{args.address:04X}=0x{value:04X} ({value})")
    return 0


def cmd_app_write_regs(args: argparse.Namespace) -> int:
    if not args.confirm_write_reg:
        raise SystemExit("连续写寄存器会修改板端参数，请显式添加 --confirm-write-reg")
    values = [int(value, 0) & 0xFFFF for value in args.values]
    if not values:
        raise SystemExit("至少提供一个写入值")
    if args.address < 0 or args.address + len(values) - 1 > 0xFFFF:
        raise SystemExit("--address/values 超出 16-bit 寄存器地址范围")

    bus = open_bus(args.interface, args.channel, args.bitrate)
    try:
        for index, value in enumerate(values):
            send_app_write_word_on_bus(bus, args, args.address + index, value)
    finally:
        shutdown = getattr(bus, "shutdown", None)
        if callable(shutdown):
            shutdown()
    print(f"已连续写入寄存器: addr=0x{args.address:04X} words={len(values)}")
    return 0


def cmd_app_write_soc(args: argparse.Namespace) -> int:
    if not args.confirm_write_soc:
        raise SystemExit("写 SOC 是常用但有副作用的功能，请显式添加 --confirm-write-soc 或 PowerShell -ConfirmWriteSoc")
    if args.soc < 0 or args.soc > 100:
        raise SystemExit("--soc 必须在 0..100 范围内")

    data = send_app_write_word(args, APP_SET_ONCE_SOC_ADDR, args.soc)
    validate_app_ack(data)
    print(f"已通过 CAN App 常用功能写 SOC: {args.soc}% (寄存器 0x{APP_SET_ONCE_SOC_ADDR:04X})")
    return 0


def send_app_aging_command(args: argparse.Namespace, cmd: int, action: int, title: str) -> int:
    payload = build_app_command(cmd, CAN_APP_AGING_GUARD, action, args.can_address)
    data = send_app_command(args, cmd, payload)
    _cmd, _status, state, remaining_hours = validate_app_ack(data)
    print(f"{title}: 老化状态={aging_state_name(state)} 剩余约={remaining_hours}h")
    return 0


def cmd_app_aging_start(args: argparse.Namespace) -> int:
    if not args.confirm_aging_start:
        raise SystemExit("开启老化模式会切换 MOS 状态，请显式添加 --confirm-aging-start 或 PowerShell -ConfirmAgingStart")
    return send_app_aging_command(
        args,
        CAN_APP_CMD_AGING_START,
        CAN_APP_AGING_ACTION_START,
        "已开启老化模式",
    )


def cmd_app_aging_stop(args: argparse.Namespace) -> int:
    if not args.confirm_aging_stop:
        raise SystemExit("关闭老化模式会提前结束本轮老化时间并将剩余时间清零，请显式添加 --confirm-aging-stop 或 PowerShell -ConfirmAgingStop")
    return send_app_aging_command(
        args,
        CAN_APP_CMD_AGING_STOP,
        CAN_APP_AGING_ACTION_STOP,
        "已提前结束老化模式",
    )


def cmd_app_aging_reset_time(args: argparse.Namespace) -> int:
    if not args.confirm_aging_reset_time:
        raise SystemExit("重置老化时间会清零已累计时间，请显式添加 --confirm-aging-reset-time 或 PowerShell -ConfirmAgingResetTime")
    return send_app_aging_command(
        args,
        CAN_APP_CMD_AGING_RESET_TIME,
        CAN_APP_AGING_ACTION_RESET_TIME,
        "已重置老化模式时间",
    )


def cmd_app_aging_set_hours(args: argparse.Namespace) -> int:
    if not args.confirm_aging_set_hours:
        raise SystemExit("修改老化时长会持久化新时长并清零累计时间，请显式添加 --confirm-aging-set-hours 或 PowerShell -ConfirmAgingSetHours")
    if args.aging_hours < 1 or args.aging_hours > 168:
        raise SystemExit("--aging-hours 必须在 1..168 范围内")

    payload = build_app_command(
        CAN_APP_CMD_AGING_SET_HOURS,
        CAN_APP_AGING_GUARD,
        args.aging_hours,
        args.can_address,
    )
    data = send_app_command(args, CAN_APP_CMD_AGING_SET_HOURS, payload)
    _cmd, _status, state, remaining_hours = validate_app_ack(data)
    print(
        f"已修改老化时长: {args.aging_hours}h "
        f"老化状态={aging_state_name(state)} 剩余约={remaining_hours}h"
    )
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

    p_app_read_reg = sub.add_parser("app-read-reg", help="通过 CAN App 读取单个寄存器")
    add_can_args(p_app_read_reg)
    add_app_args(p_app_read_reg)
    p_app_read_reg.add_argument("--address", type=lambda value: int(value, 0), required=True, help="寄存器地址")
    p_app_read_reg.set_defaults(func=cmd_app_read_reg)

    p_app_read_block = sub.add_parser("app-read-block", help="通过 CAN App 连续读取寄存器")
    add_can_args(p_app_read_block)
    add_app_args(p_app_read_block)
    p_app_read_block.add_argument("--address", type=lambda value: int(value, 0), required=True, help="起始寄存器地址")
    p_app_read_block.add_argument("--count", type=int, required=True, help="读取 word 数，1..120")
    p_app_read_block.add_argument("--block-timeout", type=float, default=5.0, help="等待块读数据帧超时秒数")
    p_app_read_block.set_defaults(func=cmd_app_read_block)

    p_app_write_reg = sub.add_parser("app-write-reg", help="通过 CAN App 写单个寄存器")
    add_can_args(p_app_write_reg)
    add_app_args(p_app_write_reg)
    p_app_write_reg.add_argument("--address", type=lambda value: int(value, 0), required=True, help="寄存器地址")
    p_app_write_reg.add_argument("--value", type=lambda value: int(value, 0), required=True, help="写入 word 值")
    p_app_write_reg.add_argument("--confirm-write-reg", action="store_true", help="确认写寄存器")
    p_app_write_reg.set_defaults(func=cmd_app_write_reg)

    p_app_write_regs = sub.add_parser("app-write-regs", help="通过 CAN App 连续写多个寄存器")
    add_can_args(p_app_write_regs)
    add_app_args(p_app_write_regs)
    p_app_write_regs.add_argument("--address", type=lambda value: int(value, 0), required=True, help="起始寄存器地址")
    p_app_write_regs.add_argument("values", nargs="+", help="写入 word 值列表，如 0x1234 100")
    p_app_write_regs.add_argument("--confirm-write-reg", action="store_true", help="确认写寄存器")
    p_app_write_regs.set_defaults(func=cmd_app_write_regs)

    p_app_write_soc = sub.add_parser("app-write-soc", help="常用功能：通过 CAN App 单独写入一次 SOC")
    add_can_args(p_app_write_soc)
    add_app_args(p_app_write_soc)
    p_app_write_soc.add_argument("--soc", type=int, required=True, help="目标 SOC 百分比，范围 0..100")
    p_app_write_soc.add_argument("--confirm-write-soc", action="store_true", help="确认写入一次 SOC")
    p_app_write_soc.set_defaults(func=cmd_app_write_soc)

    p_aging_start = sub.add_parser("app-aging-start", help="单独开启老化模式")
    add_can_args(p_aging_start)
    add_app_args(p_aging_start)
    p_aging_start.add_argument("--confirm-aging-start", action="store_true", help="确认开启老化模式")
    p_aging_start.set_defaults(func=cmd_app_aging_start)

    p_aging_stop = sub.add_parser("app-aging-stop", help="提前结束本轮老化模式时间")
    add_can_args(p_aging_stop)
    add_app_args(p_aging_stop)
    p_aging_stop.add_argument("--confirm-aging-stop", action="store_true", help="确认提前结束老化时间")
    p_aging_stop.set_defaults(func=cmd_app_aging_stop)

    p_aging_reset = sub.add_parser("app-aging-reset-time", help="单独重置老化模式累计时间")
    add_can_args(p_aging_reset)
    add_app_args(p_aging_reset)
    p_aging_reset.add_argument("--confirm-aging-reset-time", action="store_true", help="确认清零老化累计时间")
    p_aging_reset.set_defaults(func=cmd_app_aging_reset_time)

    p_aging_set_hours = sub.add_parser("app-aging-set-hours", help="修改老化时长，单位小时，并清零累计时间")
    add_can_args(p_aging_set_hours)
    add_app_args(p_aging_set_hours)
    p_aging_set_hours.add_argument("--aging-hours", type=int, required=True, help="老化时长小时数，范围 1..168")
    p_aging_set_hours.add_argument("--confirm-aging-set-hours", action="store_true", help="确认修改老化时长并清零累计时间")
    p_aging_set_hours.set_defaults(func=cmd_app_aging_set_hours)

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
