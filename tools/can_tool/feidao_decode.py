"""Decode Feidao extended broadcast frames used by the BMS app."""

from __future__ import annotations

from typing import Optional

from .formatting import (
    aging_state_name,
    be_i32,
    be_u16,
    be_u32,
    format_bytes,
    format_remaining_minutes,
    signed_i8,
)


FEIDAO_BROADCAST_BASE = 0x14F80200


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
        aging_state = data[2]
        aging_remaining_min = be_u16(data, 3)
        return (
            f"出厂容量raw={factory_cap} "
            f"老化={aging_state_name(aging_state)} 剩余={format_remaining_minutes(aging_remaining_min)} "
            f"日期=20{data[5]:02d}-{data[6]:02d}-{data[7]:02d}"
        )

    return f"广播通道={ch} 原始={format_bytes(data)}"
