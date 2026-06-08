"""Formatting and small decode helpers for host tools."""

from __future__ import annotations

import struct


def be_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def be_i32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">i", data, offset)[0]


def signed_i8(value: int) -> int:
    return value - 256 if value & 0x80 else value


def format_hex(data: bytes) -> str:
    return " ".join(f"{value:02X}" for value in data)


def format_bytes(data: bytes) -> str:
    return format_hex(data)


def decode_can_esr(esr: int) -> str:
    lec_text = {
        0: "none",
        1: "stuff",
        2: "form",
        3: "ack",
        4: "bit-recessive",
        5: "bit-dominant",
        6: "crc",
        7: "software",
    }
    lec = (esr >> 4) & 0x07
    tec = (esr >> 16) & 0xFF
    rec = (esr >> 24) & 0xFF
    flags = []
    if esr & 0x01:
        flags.append("EWGF")
    if esr & 0x02:
        flags.append("EPVF")
    if esr & 0x04:
        flags.append("BOFF")
    flag_text = ",".join(flags) if flags else "none"
    return f"TEC={tec} REC={rec} LEC={lec_text.get(lec, str(lec))} flags={flag_text}"


def aging_state_name(value: int) -> str:
    return {
        0: "停止",
        1: "运行",
        2: "完成",
    }.get(value, f"未知({value})")


def format_remaining_minutes(minutes: int) -> str:
    hours, mins = divmod(minutes, 60)
    if hours:
        return f"{hours}h{mins:02d}min"
    return f"{mins}min"
