#!/usr/bin/env python3
"""Decode offline CAN logs with a JSON protocol description."""

from __future__ import annotations

import argparse

from elt_common import format_hex, md_table, parse_int, read_json, write_text_report


def main() -> int:
    parser = argparse.ArgumentParser(description="离线 CAN 日志解析；协议由 JSON 配置")
    parser.add_argument("--protocol", default="data/examples/can_protocol.json", help="CAN 协议 JSON")
    parser.add_argument("--log", default="data/examples/can_log.txt", help="CAN 日志文本")
    parser.add_argument("--out", help="Markdown 报告输出路径；默认打印到 stdout")
    parser.add_argument("--force", action="store_true", help="允许覆盖已有报告文件")
    args = parser.parse_args()
    frames = load_can_log(args.log)
    protocol = read_json(args.protocol)
    decoded = [decode_frame(frame, protocol) for frame in frames]
    write_text_report(args.out, render_report(decoded), args.force)
    return 0


def load_can_log(path: str) -> list[dict]:
    frames = []
    with open(path, "r", encoding="utf-8") as fp:
        for line in fp:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.replace(",", " ").split()
            if len(parts) < 3:
                continue
            ts = float(parts[0])
            can_id = parse_int(parts[1])
            data_text = "".join(parts[2:]).replace(" ", "")
            data = bytes.fromhex(data_text)
            frames.append({"ts": ts, "id": can_id, "data": data})
    return frames


def decode_frame(frame: dict, protocol: dict) -> dict:
    msg = protocol.get("messages", {}).get(format_hex(frame["id"], 3)) or protocol.get("messages", {}).get(format_hex(frame["id"], 8))
    if not msg:
        return {"ts": frame["ts"], "id": format_hex(frame["id"], 3), "name": "UNKNOWN", "signals": ""}
    values = []
    for sig in msg.get("signals", []):
        start = int(sig.get("byte", 0))
        length = int(sig.get("length", 1))
        raw = int.from_bytes(frame["data"][start:start + length], byteorder=sig.get("endian", "big"), signed=bool(sig.get("signed", False)))
        value = raw * float(sig.get("scale", 1)) + float(sig.get("offset", 0))
        values.append(f"{sig['name']}={value:g}{sig.get('unit', '')}")
    return {"ts": frame["ts"], "id": format_hex(frame["id"], 3), "name": msg.get("name", ""), "signals": ", ".join(values)}


def render_report(decoded: list[dict]) -> str:
    out = ["# CAN 离线解码报告\n"]
    out.append(md_table(["时间(s)", "CAN ID", "消息", "信号"], [(d["ts"], d["id"], d["name"], d["signals"]) for d in decoded]))
    return "\n".join(out)


if __name__ == "__main__":
    raise SystemExit(main())
