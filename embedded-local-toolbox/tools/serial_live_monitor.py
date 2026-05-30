#!/usr/bin/env python3
"""Serial live monitor with logging, keyword marks, and Modbus RTU frame hints."""

from __future__ import annotations

import argparse
import csv
import time
from pathlib import Path

from elt_common import crc16_modbus, md_table, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="串口实时监控；支持日志保存、关键字标记、Modbus 帧识别和 CSV/Markdown 输出")
    parser.add_argument("--port", help="串口号；不提供时读取 --demo-log")
    parser.add_argument("--baud", type=int, default=19200, help="波特率")
    parser.add_argument("--duration", type=float, default=5.0, help="监控时长，秒")
    parser.add_argument("--keywords", default="ERROR,FAULT,WARN", help="逗号分隔关键字")
    parser.add_argument("--demo-log", default="data/examples/serial_live_log.txt", help="离线串口日志")
    parser.add_argument("--raw-log", help="保存原始文本日志")
    parser.add_argument("--csv-out", help="保存 CSV 事件表")
    parser.add_argument("--md-out", help="保存 Markdown 报告")
    parser.add_argument("--force", action="store_true", help="允许覆盖输出文件")
    parser.add_argument("--hex", action="store_true", help="真实串口按字节十六进制显示")
    args = parser.parse_args()

    events = read_demo(args.demo_log, args.keywords) if not args.port else read_serial(args)
    if args.raw_log:
        write_raw_log(args.raw_log, events, args.force)
    if args.csv_out:
        write_csv(args.csv_out, events, args.force)
    report = render_report(events, args)
    write_text_report(args.md_out, report, args.force)
    return 0


def read_demo(path: str, keywords: str) -> list[dict]:
    events = []
    start = None
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        if " " in line:
            ts_text, payload = line.split(" ", 1)
            try:
                ts = float(ts_text)
            except ValueError:
                ts, payload = 0.0, line
        else:
            ts, payload = 0.0, line
        if start is None:
            start = ts
        events.append(classify_event(ts - start, payload, keywords))
    return events


def read_serial(args: argparse.Namespace) -> list[dict]:
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit("pyserial 未安装；无硬件自测请不要传 --port") from exc

    events = []
    start = time.time()
    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        while time.time() - start < args.duration:
            if args.hex:
                data = ser.read(256)
                if data:
                    payload = data.hex(" ").upper()
                    events.append(classify_event(time.time() - start, payload, args.keywords, raw=data))
            else:
                line = ser.readline()
                if line:
                    payload = line.decode("utf-8", errors="replace").rstrip("\r\n")
                    events.append(classify_event(time.time() - start, payload, args.keywords, raw=line))
    return events


def classify_event(ts: float, payload: str, keywords: str, raw: bytes | None = None) -> dict:
    keyword_hits = [kw.strip() for kw in keywords.split(",") if kw.strip() and kw.strip().lower() in payload.lower()]
    modbus = detect_modbus_frame(raw if raw is not None else parse_hex_payload(payload))
    return {"time_s": round(ts, 3), "payload": payload, "keywords": ",".join(keyword_hits), "modbus": modbus}


def parse_hex_payload(payload: str) -> bytes | None:
    text = payload.replace(" ", "")
    if len(text) < 8 or len(text) % 2:
        return None
    try:
        return bytes.fromhex(text)
    except ValueError:
        return None


def detect_modbus_frame(data: bytes | None) -> str:
    if not data or len(data) < 4:
        return ""
    got = data[-2] | (data[-1] << 8)
    expected = crc16_modbus(data[:-2])
    if got != expected:
        return ""
    func = data[1]
    return f"RTU slave={data[0]} func=0x{func:02X} len={len(data)} crc=ok"


def write_raw_log(path: str, events: list[dict], force: bool) -> None:
    target = Path(path)
    if target.exists() and not force:
        raise FileExistsError(f"{target} already exists; use --force to overwrite")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("\n".join(str(e["payload"]) for e in events) + "\n", encoding="utf-8")


def write_csv(path: str, events: list[dict], force: bool) -> None:
    target = Path(path)
    if target.exists() and not force:
        raise FileExistsError(f"{target} already exists; use --force to overwrite")
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("w", encoding="utf-8", newline="") as fp:
        writer = csv.DictWriter(fp, fieldnames=["time_s", "payload", "keywords", "modbus"], lineterminator="\n")
        writer.writeheader()
        writer.writerows(events)


def render_report(events: list[dict], args: argparse.Namespace) -> str:
    out = ["# 串口实时监控报告\n"]
    out.append(f"- mode: `{'serial' if args.port else 'demo'}`\n- baud: `{args.baud}`\n- duration_s: `{args.duration}`\n")
    out.append("## 事件\n")
    out.append(md_table(["时间(s)", "Payload", "关键字", "Modbus"], [(e["time_s"], e["payload"], e["keywords"], e["modbus"]) for e in events]))
    return "\n".join(out)


if __name__ == "__main__":
    raise SystemExit(main())
