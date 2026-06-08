#!/usr/bin/env python3
"""PC host tool for the UART-to-CAN BMS comm tool."""

from __future__ import annotations

import argparse
import math
import struct
import time
import zlib
from pathlib import Path
from typing import Optional

from can_tool.app_can import (
    APP_SET_ONCE_SOC_ADDR,
    CAN_APP_AGING_ACTION_RESET_TIME as APP_AGING_ACTION_RESET_TIME,
    CAN_APP_AGING_ACTION_START as APP_AGING_ACTION_START,
    CAN_APP_AGING_ACTION_STOP as APP_AGING_ACTION_STOP,
)
from can_tool.comm_serial import (
    CMD_BMS_AGING_CTRL,
    CMD_BMS_AGING_SET_HOURS,
    CMD_BMS_AGING_STATUS,
    CMD_BMS_READ,
    CMD_BMS_WRITE,
    CMD_CAN_DIAG,
    CMD_DEBUG_LOG,
    CMD_ENTER_IAP,
    CMD_FW_BEGIN,
    CMD_FW_DATA,
    CMD_FW_END,
    CMD_FW_INFO,
    CMD_GET_INFO,
    CMD_RAW_CAN_TX,
    CMD_SET_CAN,
    CMD_UPGRADE,
    CMD_UPGRADE_ABORT,
    CMD_UPGRADE_STATUS,
    DEBUG_LOG_EVENTS,
    DEBUG_LOG_MODULES,
    FLAG_ACK,
    FW_DATA_DEFAULT_CHUNK,
    FW_DATA_MAX_CHUNK,
    HEADER_SIZE,
    HEADER_STRUCT,
    MAGIC,
    MAX_PAYLOAD,
    STATUS_TEXT,
    VERSION,
    CommToolClient,
    Frame,
    encode_frame,
    read_frame,
    require_pyserial,
)
from can_tool.crc import crc16_modbus
from can_tool.firmware_image import (
    APP_BASE_ADDR,
    APP_FLASH_LIMIT,
    BMS_APP_BASE_ADDR,
    BMS_APP_FLASH_LIMIT,
    BMS_SRAM_LIMIT,
    COMM_TOOL_APP_BASE_ADDR,
    COMM_TOOL_APP_FLASH_LIMIT,
    IAP_BASE_ADDR,
    SRAM_BASE,
    SRAM_LIMIT,
    image_limit,
    image_sram_limit,
    iter_chunks,
    load_image,
    print_image_plan,
    vector_summary,
)
from can_tool.formatting import (
    aging_state_name,
    decode_can_esr,
    format_hex,
    format_remaining_minutes,
)



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
        if len(payload) >= 22:
            print(f"  IAP 节点: {payload[20]}")
            print(f"  BMS App CAN 地址: {payload[21]}")
    return 0


def cmd_set_can(args) -> int:
    if args.node_id <= 0 or args.node_id > 0x7F:
        raise SystemExit("node-id 必须在 1..127 之间")
    if args.app_can_addr < 0 or args.app_can_addr > 0x0F:
        raise SystemExit("app-can-addr 必须在 0..15 之间")
    payload = struct.pack("<IBBH", args.can_bitrate, args.node_id & 0xFF, args.app_can_addr & 0x0F, 0)
    with open_client(args) as client:
        client.command(CMD_SET_CAN, payload, timeout=args.long_timeout)
    print(f"CAN 参数已设置: bitrate={args.can_bitrate} node_id={args.node_id} app_can_addr={args.app_can_addr}")
    return 0


def cmd_fw_dry_run(args) -> int:
    path = Path(args.bin).resolve()
    image = load_image(path, args.app_address)
    print_image_plan(path, image, args.app_address, args.chunk_size)
    return 0


def cmd_fw_download(args) -> int:
    expected = f"0x{args.app_address:08X}"
    if args.confirm_app_address != expected:
        raise SystemExit(f"真实下载必须显式确认 App 地址: -ConfirmAppAddress {expected}")

    path = Path(args.bin).resolve()
    image = load_image(path, args.app_address)
    print_image_plan(path, image, args.app_address, args.chunk_size)

    crc16 = crc16_modbus(image)
    crc32 = zlib.crc32(image) & 0xFFFFFFFF
    begin_payload = struct.pack("<IIHI", args.app_address, len(image), crc16, crc32)

    with open_client(args) as client:
        start_time = time.monotonic()
        client.command(CMD_FW_BEGIN, begin_payload, timeout=args.long_timeout)
        total = math.ceil(len(image) / args.chunk_size)
        for index, (offset, chunk) in enumerate(iter_chunks(image, args.chunk_size), 1):
            client.command(CMD_FW_DATA, struct.pack("<I", offset) + chunk, timeout=args.long_timeout)
            if index == total or index % args.progress_every == 0:
                print(f"  下载进度: {index}/{total} offset=0x{offset:08X}")
        client.command(CMD_FW_END, struct.pack("<IHI", len(image), crc16, crc32), timeout=args.long_timeout)
    elapsed = max(0.001, time.monotonic() - start_time)
    print(f"固件已写入 comm tool 缓存并完成校验。耗时 {elapsed:.1f}s，速度 {len(image) / 1024.0 / elapsed:.1f} KiB/s")
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


def cmd_bms_write_soc(args) -> int:
    if args.soc < 0 or args.soc > 100:
        raise SystemExit("soc 必须在 0..100 之间")
    payload = struct.pack("<HHH", APP_SET_ONCE_SOC_ADDR, 1, args.soc)
    with open_client(args) as client:
        client.command(CMD_BMS_WRITE, payload, timeout=args.long_timeout)
    print(f"已写入 BMS SOC={args.soc}%")
    return 0


def cmd_bms_aging(args) -> int:
    action_map = {
        "start": APP_AGING_ACTION_START,
        "stop": APP_AGING_ACTION_STOP,
        "reset-time": APP_AGING_ACTION_RESET_TIME,
    }
    action = action_map[args.action]
    with open_client(args) as client:
        resp = client.command(CMD_BMS_AGING_CTRL, bytes([action]), timeout=args.long_timeout)
    if len(resp.payload) < 2:
        raise RuntimeError("BMS_AGING_CTRL 响应长度不足")
    state = resp.payload[0]
    remaining_hours = resp.payload[1]
    print(f"老化动作完成: action={args.action} state={aging_state_name(state)} remaining≈{remaining_hours}h")
    return 0


def cmd_bms_aging_status(args) -> int:
    with open_client(args) as client:
        resp = client.command(CMD_BMS_AGING_STATUS, timeout=args.long_timeout)
    if len(resp.payload) < 3:
        raise RuntimeError("BMS_AGING_STATUS 响应长度不足")
    state = resp.payload[0]
    remaining_minutes = struct.unpack_from("<H", resp.payload, 1)[0]
    print(
        "老化时间: "
        f"state={aging_state_name(state)} "
        f"remaining={format_remaining_minutes(remaining_minutes)} "
        f"({remaining_minutes}min)"
    )
    return 0


def cmd_bms_aging_set_hours(args) -> int:
    if args.hours < 1 or args.hours > 168:
        raise SystemExit("--hours 必须在 1..168 范围内")
    payload = struct.pack("<H", args.hours)
    with open_client(args) as client:
        resp = client.command(CMD_BMS_AGING_SET_HOURS, payload, timeout=args.long_timeout)
    if len(resp.payload) < 4:
        raise RuntimeError("BMS_AGING_SET_HOURS 响应长度不足")
    state = resp.payload[0]
    remaining_hours = resp.payload[1]
    applied_hours = struct.unpack_from("<H", resp.payload, 2)[0]
    print(
        "老化时长修改完成: "
        f"duration={applied_hours}h "
        f"state={aging_state_name(state)} "
        f"remaining≈{remaining_hours}h"
    )
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
        last_percent = -1
        deadline = time.monotonic() + args.long_timeout
        while time.monotonic() < deadline:
            resp = client.command(CMD_UPGRADE_STATUS, timeout=2.0)
            payload = resp.payload
            if len(payload) < 13:
                raise RuntimeError("UPGRADE_STATUS 响应长度不足")
            state, percent, error = struct.unpack_from("<BBB", payload, 0)
            written, total, expect_seq = struct.unpack_from("<IIH", payload, 3)
            if percent != last_percent or state != 1:
                print(
                    f"  升级进度: state={state} percent={percent}% "
                    f"written={written}/{total} expect_seq={expect_seq} error=0x{error:02X}"
                )
                last_percent = percent
            if state == 2 and error == 0:
                print("comm tool 已完成 CAN 升级。")
                return 0
            if state in (3, 4) or error != 0:
                raise RuntimeError(
                    f"升级失败: state={state} percent={percent}% error=0x{error:02X} "
                    f"written={written}/{total} expect_seq={expect_seq}"
                )
            time.sleep(0.25)
    raise TimeoutError("等待升级完成超时")


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


def cmd_can_diag(args) -> int:
    payload = b"\x01" if args.clear else b"\x00"
    with open_client(args) as client:
        resp = client.command(CMD_CAN_DIAG, payload)
    data = resp.payload
    print(f"CAN_DIAG raw: {format_hex(data)}")
    if len(data) >= 62:
        (
            tx_count,
            tx_ok,
            tx_fail,
            tx_timeout,
            rx_count,
            rx_drop,
            last_esr,
            last_tsr,
            last_msr,
            last_rf0r,
            last_tx_id,
            last_rx_id,
        ) = struct.unpack_from("<IIIIIIIIIIII", data, 0)
        last_tx_ide, last_tx_dlc, last_tx_status, last_rx_ide, last_rx_dlc = struct.unpack_from("<BBBBB", data, 48)
        last_rx_data = data[53:61]
        node_id = data[61]
        print(f"  tx: count={tx_count} ok={tx_ok} fail={tx_fail} timeout={tx_timeout}")
        print(f"  rx: count={rx_count} drop={rx_drop}")
        print(f"  regs: ESR=0x{last_esr:08X} TSR=0x{last_tsr:08X} MSR=0x{last_msr:08X} RF0R=0x{last_rf0r:08X}")
        print(f"  esr decoded: {decode_can_esr(last_esr)}")
        print(
            f"  last tx: id=0x{last_tx_id:08X} ide={last_tx_ide} dlc={last_tx_dlc} "
            f"status=0x{last_tx_status:02X}"
        )
        print(
            f"  last rx: id=0x{last_rx_id:08X} ide={last_rx_ide} dlc={last_rx_dlc} "
            f"data={format_hex(last_rx_data)}"
        )
        print(f"  node_id={node_id}")
        if len(data) >= 63:
            print(f"  app_can_addr={data[62]}")
    return 0


def cmd_debug_log(args) -> int:
    payload = bytes([args.count & 0xFF, 1 if args.clear else 0])
    with open_client(args) as client:
        resp = client.command(CMD_DEBUG_LOG, payload)
    data = resp.payload
    print(f"DEBUG_LOG raw: {format_hex(data)}")
    if len(data) < 6:
        raise RuntimeError("DEBUG_LOG response too short")

    enabled = data[0]
    count = data[1]
    capacity = data[2]
    entry_size = data[3]
    dropped = struct.unpack_from("<H", data, 4)[0]
    print(f"  enabled={enabled} count={count} capacity={capacity} entry_size={entry_size} dropped={dropped}")
    if enabled == 0:
        print("  debug log is disabled in this firmware build")
        return 0
    if entry_size != 12:
        raise RuntimeError(f"unsupported DEBUG_LOG entry size: {entry_size}")

    offset = 6
    for _ in range(count):
        if offset + entry_size > len(data):
            break
        seq = struct.unpack_from("<H", data, offset)[0]
        tick_ms = struct.unpack_from("<I", data, offset + 2)[0]
        module = data[offset + 6]
        event = data[offset + 7]
        value0 = struct.unpack_from("<H", data, offset + 8)[0]
        value1 = struct.unpack_from("<H", data, offset + 10)[0]
        module_text = DEBUG_LOG_MODULES.get(module, f"MOD_{module}")
        event_text = DEBUG_LOG_EVENTS.get(event, f"EVT_{event}")
        print(
            f"  #{seq:05d} {tick_ms:10d}ms {module_text:<8} {event_text:<16} "
            f"v0=0x{value0:04X} v1=0x{value1:04X}"
        )
        offset += entry_size
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
    parser.add_argument("--bin", required=True, help="BMS App 或 comm tool App bin 文件")
    parser.add_argument("--app-address", type=lambda value: int(value, 0), default=BMS_APP_BASE_ADDR)
    parser.add_argument("--chunk-size", type=int, default=FW_DATA_DEFAULT_CHUNK)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="PC 串口 comm tool 上位机")
    sub = parser.add_subparsers(dest="command", required=True)

    p_ports = sub.add_parser("list-ports", help="列出串口")
    p_ports.set_defaults(func=cmd_list_ports)

    p_info = sub.add_parser("info", help="读取 comm tool 信息")
    add_serial_args(p_info)
    p_info.set_defaults(func=cmd_info)

    p_set_can = sub.add_parser("set-can", help="设置 comm tool CAN 参数和目标 BMS 地址")
    add_serial_args(p_set_can)
    p_set_can.add_argument("--can-bitrate", type=int, default=250000)
    p_set_can.add_argument("--node-id", type=lambda value: int(value, 0), default=1)
    p_set_can.add_argument("--app-can-addr", type=lambda value: int(value, 0), default=0)
    p_set_can.set_defaults(func=cmd_set_can)

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

    p_write_soc = sub.add_parser("bms-write-soc", help="常用功能：写入一次 BMS SOC")
    add_serial_args(p_write_soc)
    p_write_soc.add_argument("--soc", type=int, required=True)
    p_write_soc.set_defaults(func=cmd_bms_write_soc)

    p_aging = sub.add_parser("bms-aging", help="常用功能：开启/提前结束/重置 BMS 老化模式")
    add_serial_args(p_aging)
    p_aging.add_argument("action", choices=("start", "stop", "reset-time"))
    p_aging.set_defaults(func=cmd_bms_aging)

    p_aging_status = sub.add_parser("bms-aging-status", help="读取 BMS 老化模式剩余时间广播")
    add_serial_args(p_aging_status)
    p_aging_status.set_defaults(func=cmd_bms_aging_status)

    p_aging_set_hours = sub.add_parser("bms-aging-set-hours", help="常用功能：修改 BMS 老化时长，单位小时，并重置老化时间")
    add_serial_args(p_aging_set_hours)
    p_aging_set_hours.add_argument("--hours", type=int, required=True)
    p_aging_set_hours.set_defaults(func=cmd_bms_aging_set_hours)

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

    p_diag = sub.add_parser("can-diag", help="读取 comm tool CAN 诊断计数")
    add_serial_args(p_diag)
    p_diag.add_argument("--clear", action="store_true", help="读取前先清空诊断计数")
    p_diag.set_defaults(func=cmd_can_diag)

    p_debug_log = sub.add_parser("debug-log", help="read comm tool debug ring log")
    add_serial_args(p_debug_log)
    p_debug_log.add_argument("--count", type=int, default=32, help="max latest records to read")
    p_debug_log.add_argument("--clear", action="store_true", help="clear debug log after reading")
    p_debug_log.set_defaults(func=cmd_debug_log)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
