#!/usr/bin/env python3
"""User-facing GUI for comm tool BMS CAN upgrade."""

from __future__ import annotations

import argparse
import math
import queue
import struct
import sys
import threading
import time
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional

import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from comm_tool_host import (
    APP_BASE_ADDR,
    APP_FLASH_LIMIT,
    CMD_BMS_READ,
    CMD_BMS_WRITE,
    CMD_CAN_DIAG,
    CMD_FW_BEGIN,
    CMD_FW_DATA,
    CMD_FW_END,
    CMD_FW_INFO,
    CMD_GET_INFO,
    CMD_SET_CAN,
    CMD_UPGRADE,
    CMD_UPGRADE_STATUS,
    CommToolClient,
    crc16_modbus,
    require_pyserial,
    vector_summary,
)


def resolve_repo_root() -> Path:
    if getattr(sys, "frozen", False):
        exe_dir = Path(sys.executable).resolve().parent
        if exe_dir.name.lower() == "dist":
            return exe_dir.parent
        if (exe_dir / "103 + 309").exists():
            return exe_dir
        if (Path.cwd() / "103 + 309").exists():
            return Path.cwd()
        return exe_dir
    return Path(__file__).resolve().parents[1]


REPO_ROOT = resolve_repo_root()
DEFAULT_BIN = REPO_ROOT / "103 + 309" / "Project" / "Users" / "Objects" / "FD_Release.bin"
DEFAULT_LONG_TIMEOUT = 180.0
DEFAULT_CHUNK_SIZE = 496
POST_UPGRADE_APP_READY_TIMEOUT = 15.0
POST_UPGRADE_APP_READY_INTERVAL = 1.0

BMS_OVERVIEW_ADDR = 0xD000
BMS_OVERVIEW_WORDS = 63
BMS_LIVE_WORDS = 88
CELL_VOLTAGE_NOT_PRESENT = 61001
BMS_EVENT_RECORD_ADDR = 0xC008
BMS_EVENT_RECORD_WORDS = 100
BMS_EVENT_RECORD_READ_CHUNK_WORDS = 20
BMS_READ_RETRY_COUNT = 3
BMS_READ_RETRY_DELAY_SECONDS = 0.3
BMS_LOG_CHUNK_GAP_SECONDS = 0.12
SH309_AFE_PARAM_ADDR = 0x2400
SH309_AFE_PARAM_WORDS = 24
SH309_TMOS_PARAM_ADDR = 0x2132
SH309_TMOS_PARAM_WORDS = 5
SH309_RESET_PROTECT_ADDR = 0x1002
SH309_RESET_AFE_ADDR = 0x1006
SH309_OV_UV_DELAY_MS = [100, 200, 300, 400, 600, 800, 1000, 2000, 3000, 4000, 6000, 8000, 10000, 20000, 30000, 40000]
SH309_CHG_SECOND_DELAY_MS = [10, 20, 40, 60, 80, 100, 200, 400, 600, 800, 1000, 2000, 4000, 8000, 10000, 20000]
SH309_DSG_SECOND_DELAY_MS = [50, 100, 200, 400, 600, 800, 1000, 2000, 4000, 6000, 8000, 10000, 15000, 20000, 30000, 40000]
SH309_SHORT_DELAY_US = [0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960]
SH309_COMMON_CURRENT_A = [20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 160, 180, 200, 220, 260, 300, 400, 500]
SH309_SHORT_CURRENT_A = [50, 80, 110, 140, 170, 200, 220, 230, 260, 290, 320, 350, 400, 500, 600, 800, 1000]
SH309_TMOS_DEFAULT_DISPLAY = {
    "SH309/MOS过温1(℃)": "75",
    "SH309/MOS过温2(℃)": "85",
    "SH309/MOS过温3(℃)": "95",
    "SH309/MOS恢复(℃)": "80",
    "SH309/延时(10ms)": "100",
}
BMS_EVENT_NAMES = [
    "NA",
    "BMS开机",
    "BMS休眠",
    "均衡开启",
    "保留4",
    "保留5",
    "单节过压保护",
    "总压过压保护",
    "充电过流保护",
    "单节低压保护",
    "总压低压保护",
    "放电过流保护",
    "充电低温保护",
    "放电低温保护",
    "充电高温保护",
    "放电高温保护",
    "压差过大保护",
    "短路保护",
    "AFE1报错",
    "AFE2报错",
    "EEPROM报错",
]

FAULT_BIT_NAMES = [
    "单体过压",
    "单体欠压",
    "总压过压",
    "总压欠压",
    "充电过流",
    "放电过流",
    "充电高温",
    "放电高温",
    "充电低温",
    "放电低温",
    "压差过大",
    "温差过大",
    "SOC低",
    "MOS过温",
    "保留1",
    "保留2",
]


@dataclass(frozen=True)
class BmsParamDef:
    key: str
    group: str
    name: str
    addr: int
    unit: str
    kind: str = "u16"


def _build_bms_param_defs() -> list[BmsParamDef]:
    defs: list[BmsParamDef] = []
    protect_groups = [
        ("单体过压", "mV", "u16"),
        ("单体欠压", "mV", "u16"),
        ("总压过压", "mV", "u16"),
        ("总压欠压", "mV", "u16"),
        ("充电过流", "A", "u16"),
        ("放电过流", "A", "u16"),
        ("充电高温", "℃", "temp"),
        ("充电低温", "℃", "temp"),
        ("放电高温", "℃", "temp"),
        ("放电低温", "℃", "temp"),
        ("MOS高温", "℃", "temp"),
        ("压差保护", "mV", "u16"),
        ("SOC低电", "%", "u16"),
    ]
    fields = [("一级阈值", 0), ("二级阈值", 1), ("三级阈值", 2), ("恢复阈值", 3), ("延时", 4)]
    for group_index, (group, unit, kind) in enumerate(protect_groups):
        base = 0x2100 + group_index * 5
        for field_name, offset in fields:
            defs.append(
                BmsParamDef(
                    key=f"{group}/{field_name}",
                    group=group,
                    name=field_name,
                    addr=base + offset,
                    unit=unit if offset < 4 else "raw",
                    kind=kind if offset < 4 else "u16",
                )
            )

    other_defs = [
        ("均衡参数", "开启电压", 0x2300, "mV", "u16"),
        ("均衡参数", "开启压差", 0x2301, "mV", "u16"),
        ("均衡参数", "关闭压差", 0x2302, "mV", "u16"),
        ("短路参数", "充电短路范围", 0x2308, "A", "x10"),
        ("短路参数", "放电短路范围", 0x2309, "A", "x10"),
        ("短路参数", "短路延时", 0x230A, "raw", "u16"),
        ("短路参数", "短路电流", 0x230B, "A", "x10"),
        ("限流参数", "SOC曲线选择", 0x230C, "raw", "u16"),
        ("限流参数", "永久密码", 0x230D, "raw", "u16"),
        ("限流参数", "限流压差", 0x230E, "mV", "u16"),
        ("限流参数", "限流电流", 0x230F, "A", "x10"),
        ("休眠参数", "正常休眠电压", 0x2310, "mV", "u16"),
        ("休眠参数", "正常休眠时间", 0x2311, "min", "u16"),
        ("休眠参数", "过放休眠电压", 0x2312, "mV", "u16"),
        ("休眠参数", "过放休眠时间", 0x2313, "min", "u16"),
        ("休眠参数", "充电电流过滤", 0x2314, "A", "x10"),
        ("休眠参数", "放电电流过滤", 0x2315, "A", "x10"),
        ("休眠参数", "RTC唤醒时间", 0x2316, "min", "u16"),
        ("休眠参数", "RTC休眠时间", 0x2317, "min", "u16"),
        ("SOC参数", "容量", 0x2318, "Ah", "x10"),
        ("SOC参数", "循环次数", 0x2319, "次", "u16"),
        ("SOC参数", "SOC_100电压", 0x231A, "mV", "u16"),
        ("SOC参数", "SOC_0电压", 0x231B, "mV", "u16"),
        ("系统参数", "电池串数", 0x231C, "串", "u16"),
        ("系统参数", "采样电阻", 0x231D, "mΩ", "u16"),
        ("系统参数", "采样电阻数", 0x231E, "个", "u16"),
        ("系统参数", "预充时间", 0x231F, "s", "u16"),
    ]
    for group, name, addr, unit, kind in other_defs:
        defs.append(BmsParamDef(f"{group}/{name}", group, name, addr, unit, kind))
    return defs


def _build_sh309_param_defs() -> list[BmsParamDef]:
    afe_fields = [
        ("单节过压(mv)", 0, "mV", "u16"),
        ("过压恢复(mv)", 1, "mV", "u16"),
        ("过压延时(ms)", 2, "ms", "ms10"),
        ("单节低压(mv)", 3, "mV", "u16"),
        ("低压恢复(mv)", 4, "mV", "u16"),
        ("低压延时(ms)", 5, "ms", "ms10"),
        ("一级充电过流(A)", 6, "A", "x10"),
        ("一级充电过流延时(ms)", 7, "ms", "ms10"),
        ("二级充电过流(A)", 8, "A", "x10"),
        ("二级充电过流延时(ms)", 9, "ms", "ms10"),
        ("一级放电过流(A)", 10, "A", "x10"),
        ("一级放电过流延时(ms)", 11, "ms", "ms10"),
        ("二级放电过流(A)", 12, "A", "x10"),
        ("二级放电过流延时(ms)", 13, "ms", "ms10"),
        ("充电高温(℃)", 14, "℃", "temp"),
        ("充电高温恢复(℃)", 15, "℃", "temp"),
        ("充电低温(℃)", 16, "℃", "temp"),
        ("充电低温恢复(℃)", 17, "℃", "temp"),
        ("放电高温(℃)", 18, "℃", "temp"),
        ("放电高温恢复(℃)", 19, "℃", "temp"),
        ("放电低温(℃)", 20, "℃", "temp"),
        ("放电低温恢复(℃)", 21, "℃", "temp"),
        ("短路电流(A)", 22, "A", "u16"),
        ("短路延时(us)", 23, "us", "u16"),
    ]
    tmos_fields = [
        ("MOS过温1(℃)", 0, "℃", "temp"),
        ("MOS过温2(℃)", 1, "℃", "temp"),
        ("MOS过温3(℃)", 2, "℃", "temp"),
        ("MOS恢复(℃)", 3, "℃", "temp"),
        ("延时(10ms)", 4, "10ms", "u16"),
    ]
    defs = [
        BmsParamDef(f"SH309/{name}", "SH309", name, SH309_AFE_PARAM_ADDR + offset, unit, kind)
        for name, offset, unit, kind in afe_fields
    ]
    defs.extend(
        BmsParamDef(f"SH309/{name}", "SH309", name, SH309_TMOS_PARAM_ADDR + offset, unit, kind)
        for name, offset, unit, kind in tmos_fields
    )
    return defs


BMS_PARAM_DEFS = _build_bms_param_defs()
SH309_PARAM_DEFS = _build_sh309_param_defs()
ALL_PARAM_DEFS = BMS_PARAM_DEFS + SH309_PARAM_DEFS
BMS_PARAM_BY_KEY = {param.key: param for param in ALL_PARAM_DEFS}
BMS_PARAM_ADDR_TO_KEY = {param.addr: param.key for param in ALL_PARAM_DEFS}
BMS_PARAM_PRESETS = {param.key: param.addr for param in ALL_PARAM_DEFS}


class UiEvent:
    def __init__(self, kind: str, payload=None):
        self.kind = kind
        self.payload = payload


def _temp_c(raw: int) -> float:
    return raw / 10.0 - 40.0


def _is_valid_cell_voltage(value: int) -> bool:
    return value != 0 and value != CELL_VOLTAGE_NOT_PRESENT


def _valid_cell_items(cells: list[int]) -> list[tuple[int, int]]:
    return [(index, value) for index, value in enumerate(cells) if _is_valid_cell_voltage(value)]


def _cell_stat_text(valid_cells: list[tuple[int, int]]) -> tuple[str, str, str]:
    if not valid_cells:
        return "--", "--", "--"
    max_index, max_mv = max(valid_cells, key=lambda item: item[1])
    min_index, min_mv = min(valid_cells, key=lambda item: item[1])
    return f"{max_mv}mV({max_index + 1})", f"{min_mv}mV({min_index + 1})", f"{max_mv - min_mv}mV"


def _fault_text(word: int) -> str:
    names = [name for index, name in enumerate(FAULT_BIT_NAMES) if (word & (1 << index)) != 0]
    return "、".join(names) if names else "无"


def _onoff_text(enabled: bool | None) -> str:
    if enabled is None:
        return "--"
    return "on" if enabled else "off"


def _format_number(value: float) -> str:
    if math.isfinite(value) and abs(value - round(value)) < 0.0001:
        return str(int(round(value)))
    return f"{value:.1f}".rstrip("0").rstrip(".")


def _param_display_value(param: BmsParamDef, raw: int) -> str:
    if param.kind == "temp":
        return _format_number(_temp_c(raw))
    if param.kind == "x10":
        return _format_number(raw / 10.0)
    if param.kind == "ms10":
        return str(raw * 10)
    return str(raw)


def _param_parse_display_value(param: BmsParamDef, text: str) -> int:
    stripped = text.strip()
    if not stripped:
        raise ValueError(f"{param.key} 不能为空")
    if param.kind == "temp":
        raw = int(round((float(stripped) + 40.0) * 10.0))
    elif param.kind == "x10":
        raw = int(round(float(stripped) * 10.0))
    elif param.kind == "ms10":
        raw = int(round(float(stripped) / 10.0))
    else:
        raw = int(stripped, 0)
    if raw < 0 or raw > 0xFFFF:
        raise ValueError(f"{param.key} 超出 0..65535")
    return raw


def _event_interval_text(delta: int) -> str:
    if delta == 0:
        return "NA"
    if delta == 171:
        return "1min以内"
    if delta <= 24:
        return f"{delta}h"
    if delta <= 168:
        return f"{delta // 24}d_{delta % 24}h"
    return "溢出"


def _read_bms_words_once(port: str, baud: int, addr: int, count: int) -> list[int]:
    payload = struct.pack("<HH", addr, count)
    with CommToolClient(port, baud, timeout=1.0) as client:
        resp = client.command(CMD_BMS_READ, payload, timeout=max(10.0, count * 0.08 + 3.0))
    if len(resp.payload) != count * 2:
        raise RuntimeError(f"BMS_READ 响应长度错误: {len(resp.payload)}")
    return list(struct.unpack("<" + "H" * count, resp.payload))


class BmsMonitorWindow(tk.Toplevel):
    def __init__(self, parent: "UpgradeUi"):
        super().__init__(parent)
        self.parent = parent
        self.title("BMS 实时监控")
        self.geometry("900x660")
        self.minsize(820, 580)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

        self.events: "queue.Queue[UiEvent]" = queue.Queue()
        self.worker: threading.Thread | None = None
        self.running = False
        self.closed = False
        self.parent_paused = False
        self.interval_var = tk.StringVar(value="2.0")
        self.state_var = tk.StringVar(value="未开始")
        self.summary_var = tk.StringVar(value="未读取")
        self.detail_var = tk.StringVar(value="")

        self._build_ui()
        self.after(80, self._after_events)

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=12)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(0, weight=2)
        root.columnconfigure(1, weight=1)
        root.rowconfigure(2, weight=1)

        toolbar = ttk.Frame(root)
        toolbar.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 8))
        ttk.Button(toolbar, text="开始", command=self.start).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(toolbar, text="暂停", command=self.stop).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(toolbar, text="刷新一次", command=self.refresh_once).pack(side=tk.LEFT, padx=(0, 16))
        ttk.Label(toolbar, text="间隔(s)").pack(side=tk.LEFT)
        ttk.Combobox(toolbar, textvariable=self.interval_var, values=["0.5", "1.0", "2.0", "5.0"], width=8).pack(
            side=tk.LEFT, padx=(6, 16)
        )
        ttk.Label(toolbar, textvariable=self.state_var).pack(side=tk.LEFT)

        ttk.Label(root, textvariable=self.summary_var, justify=tk.LEFT).grid(
            row=1, column=0, columnspan=2, sticky="ew", pady=(0, 8)
        )

        cell_box = ttk.LabelFrame(root, text="单体电压")
        cell_box.grid(row=2, column=0, sticky="nsew", padx=(0, 8))
        cell_box.rowconfigure(0, weight=1)
        cell_box.columnconfigure(0, weight=1)
        self.cell_tree = ttk.Treeview(cell_box, columns=("mv",), show="headings", height=18)
        self.cell_tree.heading("mv", text="mV")
        self.cell_tree.column("mv", anchor=tk.CENTER, width=100)
        self.cell_tree.grid(row=0, column=0, sticky="nsew")
        cell_scroll = ttk.Scrollbar(cell_box, orient=tk.VERTICAL, command=self.cell_tree.yview)
        cell_scroll.grid(row=0, column=1, sticky="ns")
        self.cell_tree.configure(yscrollcommand=cell_scroll.set)

        right = ttk.Frame(root)
        right.grid(row=2, column=1, sticky="nsew")
        right.rowconfigure(0, weight=1)
        right.rowconfigure(1, weight=1)
        right.columnconfigure(0, weight=1)

        temp_box = ttk.LabelFrame(right, text="温度")
        temp_box.grid(row=0, column=0, sticky="nsew", pady=(0, 8))
        temp_box.rowconfigure(0, weight=1)
        temp_box.columnconfigure(0, weight=1)
        self.temp_tree = ttk.Treeview(temp_box, columns=("value",), show="headings", height=10)
        self.temp_tree.heading("value", text="℃")
        self.temp_tree.column("value", anchor=tk.CENTER, width=100)
        self.temp_tree.grid(row=0, column=0, sticky="nsew")

        detail_box = ttk.LabelFrame(right, text="故障/均衡/容量")
        detail_box.grid(row=1, column=0, sticky="nsew")
        detail_box.columnconfigure(0, weight=1)
        ttk.Label(detail_box, textvariable=self.detail_var, justify=tk.LEFT).grid(
            row=0, column=0, sticky="nw", padx=10, pady=10
        )

        for index in range(32):
            self.cell_tree.insert("", tk.END, iid=f"cell{index}", values=(f"{index + 1:02d}: --",))
        for index in range(10):
            self.temp_tree.insert("", tk.END, iid=f"temp{index}", values=(f"T{index + 1}: --",))

    def set_paused_by_parent(self, paused: bool) -> None:
        self.parent_paused = paused
        if paused:
            self.state_var.set("主任务执行中，监控暂停")
        elif self.running:
            self.state_var.set("监控中")
            self._schedule_poll(100)

    def start(self) -> None:
        self.running = True
        self.state_var.set("监控中")
        self._schedule_poll(10)

    def stop(self) -> None:
        self.running = False
        self.state_var.set("已暂停")

    def refresh_once(self) -> None:
        self._schedule_poll(10, force=True)

    def _schedule_poll(self, delay_ms: int, force: bool = False) -> None:
        if self.closed:
            return
        self.after(delay_ms, lambda: self._poll_if_needed(force))

    def _poll_if_needed(self, force: bool = False) -> None:
        if self.closed or self.parent_paused:
            return
        if not force and not self.running:
            return
        if self.worker and self.worker.is_alive():
            return
        self.worker = threading.Thread(target=self._worker_poll, daemon=True)
        self.worker.start()

    def _worker_poll(self) -> None:
        try:
            if not self.parent.serial_lock.acquire(blocking=False):
                self.events.put(UiEvent("busy", "串口忙，等待主任务结束"))
                return
            try:
                port = self.parent.port_var.get().strip()
                baud = int(self.parent.baud_var.get().strip())
                words = _read_bms_words_once(port, baud, BMS_OVERVIEW_ADDR, BMS_OVERVIEW_WORDS)
            finally:
                self.parent.serial_lock.release()
            self.events.put(UiEvent("snapshot", words))
        except Exception as exc:
            self.events.put(UiEvent("error", str(exc)))

    def _interval_ms(self, minimum: float = 0.5) -> int:
        try:
            seconds = float(self.interval_var.get())
        except ValueError:
            seconds = 1.0
            self.interval_var.set("1.0")
        return int(max(minimum, seconds) * 1000)

    def _after_events(self) -> None:
        while True:
            try:
                event = self.events.get_nowait()
            except queue.Empty:
                break
            if event.kind == "snapshot":
                self._show_snapshot(event.payload)
                self.state_var.set("监控中" if self.running else "刷新完成")
                if self.running and not self.parent_paused:
                    self._schedule_poll(self._interval_ms(0.5))
            elif event.kind == "error":
                self.state_var.set(f"读取失败: {event.payload}")
                if self.running and not self.parent_paused:
                    self._schedule_poll(self._interval_ms(1.0))
            elif event.kind == "busy":
                self.state_var.set(str(event.payload))
                if self.running and not self.parent_paused:
                    self._schedule_poll(self._interval_ms(1.0))
        if not self.closed:
            self.after(80, self._after_events)

    def _show_snapshot(self, words: list[int]) -> None:
        cells = words[0:32]
        valid_cells = _valid_cell_items(cells)
        max_cell_text, min_cell_text, delta_cell_text = _cell_stat_text(valid_cells)
        total_v = words[37] / 100.0
        ichg = words[50] / 10.0
        idsg = words[51] / 10.0
        soc = words[52]
        soh = words[53]
        capacity_now = words[54] / 100.0
        capacity_full = words[55] / 100.0

        self.summary_var.set(
            f"SOC {soc}%   SOH {soh}%   总压 {total_v:.2f}V   "
            f"充电 {ichg:.1f}A   放电 {idsg:.1f}A\n"
            f"单体 max {max_cell_text}   min {min_cell_text}   "
            f"压差 {delta_cell_text}   有效串数 {len(valid_cells)}   "
            f"温度 max {_temp_c(words[48]):.1f}℃   min {_temp_c(words[49]):.1f}℃"
        )
        self.detail_var.set(
            f"容量: {capacity_now:.2f}/{capacity_full:.2f}Ah\n"
            f"出厂容量: {words[56] / 100.0:.2f}Ah\n"
            f"循环次数: {words[57]}\n"
            f"故障字: 0x{words[58]:04X}  0x{words[59]:04X}  0x{words[60]:04X}\n"
            f"均衡: 0x{words[61]:04X}  0x{words[62]:04X}"
        )

        for item in self.cell_tree.get_children():
            self.cell_tree.delete(item)
        for index, value in valid_cells:
            self.cell_tree.insert("", tk.END, iid=f"cell{index}", values=(f"{index + 1:02d}: {value} mV",))
        for index, raw in enumerate(words[38:48]):
            self.temp_tree.item(f"temp{index}", values=(f"T{index + 1}: {_temp_c(raw):.1f}",))

    def _on_close(self) -> None:
        self.running = False
        self.closed = True
        self.parent.monitor_window = None
        self.destroy()


class BmsLogWindow(tk.Toplevel):
    def __init__(self, parent: "UpgradeUi", records: list[tuple[int, int]]):
        super().__init__(parent)
        self.title("BMS 日志")
        self.geometry("620x620")
        self.minsize(560, 480)

        root = ttk.Frame(self, padding=12)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)

        self.tree = ttk.Treeview(root, columns=("event", "interval", "raw"), show="headings", height=22)
        self.tree.heading("event", text="事件")
        self.tree.heading("interval", text="与上次间隔")
        self.tree.heading("raw", text="原始值")
        self.tree.column("event", width=240, anchor=tk.W)
        self.tree.column("interval", width=120, anchor=tk.CENTER)
        self.tree.column("raw", width=120, anchor=tk.CENTER)
        self.tree.grid(row=0, column=0, sticky="nsew")

        scrollbar = ttk.Scrollbar(root, orient=tk.VERTICAL, command=self.tree.yview)
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.tree.configure(yscrollcommand=scrollbar.set)

        for index, (event, delta) in enumerate(records, start=1):
            if event == 0 and delta == 0:
                continue
            name = BMS_EVENT_NAMES[event] if event < len(BMS_EVENT_NAMES) else f"未知事件{event}"
            self.tree.insert(
                "",
                tk.END,
                values=(f"{index:03d}. {name}", _event_interval_text(delta), f"0x{event:02X} 0x{delta:02X}"),
            )


class UpgradeUi(tk.Tk):
    def __init__(self, port: str, baud: int, bin_path: Path):
        super().__init__()
        self.title("BMS_V1.13.1 - CAN用户上位机")
        self.geometry("1180x760")
        self.minsize(1100, 700)

        self.events: "queue.Queue[UiEvent]" = queue.Queue()
        self.worker: threading.Thread | None = None
        self.serial_lock = threading.Lock()

        self.port_var = tk.StringVar(value=port)
        self.baud_var = tk.StringVar(value=str(baud))
        self.bin_var = tk.StringVar(value=str(bin_path))
        self.info_var = tk.StringVar(value="未连接")
        self.cache_var = tk.StringVar(value="未读取")
        self.image_var = tk.StringVar(value="未选择")
        self.result_var = tk.StringVar(value="等待操作")
        self.can_bitrate_var = tk.StringVar(value="250000")
        self.node_id_var = tk.StringVar(value="1")
        self.app_can_addr_var = tk.StringVar(value="0")
        self.upgrade_stage_var = tk.StringVar(value="升级进度: --")
        self.bms_addr_var = tk.StringVar(value="0xD000")
        self.bms_count_var = tk.StringVar(value="2")
        self.bms_values_var = tk.StringVar(value="")
        self.bms_result_var = tk.StringVar(value="未读取")
        self.bms_info_var = tk.StringVar(value="未读取")
        self.param_key_var = tk.StringVar(value=next(iter(BMS_PARAM_PRESETS)))
        self.param_value_var = tk.StringVar(value="")
        self.param_current_var = tk.StringVar(value="未读取")
        self.param_selected_var = tk.StringVar(value="未选择")
        self.param_edit_var = tk.StringVar(value="")
        self.param_dirty_var = tk.StringVar(value="未修改")
        self.comm_state_var = tk.StringVar(value="通信: 未连接")
        self.live_interval_var = tk.StringVar(value="2.0")
        self.live_button_var = tk.StringVar(value="开始监控")
        self.progress_var = tk.DoubleVar(value=0.0)
        self.active_port = port
        self.active_baud = baud
        self.active_bin = bin_path
        self.active_can_bitrate = 250000
        self.active_node_id = 1
        self.active_app_can_addr = 0
        self.active_bms_addr = 0xD000
        self.active_bms_count = 2
        self.active_bms_words: list[int] = []
        self.param_values: dict[str, int] = {}
        self.active_param_key = ""
        self.active_param_raw = 0
        self.active_param_values: dict[str, int] = {}
        self.active_param_range = ""
        self.monitor_window: BmsMonitorWindow | None = None
        self.live_running = False
        self.live_after_id: str | None = None
        self.live_worker: threading.Thread | None = None
        self.param_entry_vars: dict[str, tk.StringVar] = {}
        self.param_dirty: set[str] = set()
        self.param_loading = False
        self.log_tree: ttk.Treeview | None = None
        self.applied_target: tuple[int, int, int] | None = None

        self._build_ui()
        self._refresh_ports()
        self._after_events()
        self._describe_selected_bin()

    def _build_ui(self) -> None:
        self._configure_styles()

        root = ttk.Frame(self, padding=4)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=1)

        notebook = ttk.Notebook(root)
        notebook.grid(row=0, column=0, sticky="nsew")

        monitor_tab = ttk.Frame(notebook, padding=6)
        data_tab = ttk.Frame(notebook, padding=6)
        storage_tab = ttk.Frame(notebook, padding=6)
        params_tab = ttk.Frame(notebook, padding=6)
        system_tab = ttk.Frame(notebook, padding=6)
        other_tab = ttk.Frame(notebook, padding=6)

        notebook.add(monitor_tab, text="实时监控")
        notebook.add(data_tab, text="实时数据")
        notebook.add(storage_tab, text="存储信息")
        notebook.add(params_tab, text="参数设置")
        notebook.add(system_tab, text="系统状态")
        notebook.add(other_tab, text="其它功能")

        self._build_monitor_tab(monitor_tab)
        self._build_data_tab(data_tab)
        self._build_storage_tab(storage_tab)
        self._build_params_tab(params_tab)
        self._build_system_tab(system_tab)
        self._build_other_tab(other_tab)

        bottom = ttk.Frame(root)
        bottom.grid(row=1, column=0, sticky="ew", pady=(4, 0))
        bottom.columnconfigure(1, weight=1)
        ttk.Label(bottom, textvariable=self.result_var).grid(row=0, column=0, sticky="w")
        ttk.Label(bottom, textvariable=self.comm_state_var).grid(row=0, column=1, sticky="e", padx=(8, 0))

    def _configure_styles(self) -> None:
        self.style = ttk.Style(self)
        try:
            self.style.theme_use("clam")
        except tk.TclError:
            pass
        self.style.configure("TNotebook.Tab", padding=(12, 4))
        self.style.configure("Small.TLabel", font=("TkDefaultFont", 8))
        self.style.configure("Value.TLabel", font=("TkDefaultFont", 10, "bold"))

    def _build_monitor_tab(self, tab: ttk.Frame) -> None:
        tab.columnconfigure(0, weight=3)
        tab.columnconfigure(1, weight=2)
        tab.columnconfigure(2, weight=3)
        tab.rowconfigure(0, weight=1)

        left = ttk.LabelFrame(tab, text="单体电压(mV)")
        left.grid(row=0, column=0, sticky="nsew", padx=(0, 6))
        left.columnconfigure(0, weight=1)
        left.rowconfigure(1, weight=1)

        stat = ttk.Frame(left)
        stat.grid(row=0, column=0, sticky="ew", padx=8, pady=(6, 2))
        for col in range(4):
            stat.columnconfigure(col, weight=1)
        self.cell_stat_vars = {
            "max": tk.StringVar(value="最高电压 --"),
            "min": tk.StringVar(value="最低电压 --"),
            "avg": tk.StringVar(value="平均电压 --"),
            "delta": tk.StringVar(value="最大压差 --"),
        }
        ttk.Label(stat, textvariable=self.cell_stat_vars["max"], foreground="red").grid(row=0, column=0, sticky="w")
        ttk.Label(stat, textvariable=self.cell_stat_vars["min"], foreground="green").grid(row=0, column=1, sticky="w")
        ttk.Label(stat, textvariable=self.cell_stat_vars["avg"]).grid(row=1, column=0, sticky="w", pady=(3, 0))
        ttk.Label(stat, textvariable=self.cell_stat_vars["delta"]).grid(row=1, column=1, sticky="w", pady=(3, 0))

        cells_frame = ttk.Frame(left)
        cells_frame.grid(row=1, column=0, sticky="nsew", padx=8, pady=6)
        for col in range(4):
            cells_frame.columnconfigure(col, weight=1)
        self.cell_slots: list[tuple[ttk.Frame, int, int]] = []
        self.cell_value_vars: list[tk.StringVar] = []
        for index in range(32):
            row = index % 16
            col = (index // 16) * 2
            cell = ttk.Frame(cells_frame, borderwidth=1, relief="ridge", padding=(2, 2))
            number = tk.Label(cell, text=f"{index + 1:02d}", width=4, bg="#32f020", fg="red")
            number.grid(row=0, column=0, padx=(0, 4))
            value_var = tk.StringVar(value="--")
            ttk.Label(cell, textvariable=value_var, width=7, anchor="e").grid(row=0, column=1)
            cell.grid(row=row, column=col, sticky="w", padx=(0, 14), pady=3)
            self.cell_slots.append((cell, row, col))
            self.cell_value_vars.append(value_var)

        center = ttk.Frame(tab)
        center.grid(row=0, column=1, sticky="nsew", padx=6)
        center.columnconfigure(0, weight=1)
        center.rowconfigure(1, weight=1)

        basic = ttk.LabelFrame(center, text="基础信息")
        basic.grid(row=0, column=0, sticky="ew")
        basic.columnconfigure(1, weight=1)
        self.basic_vars = {
            "total_v": tk.StringVar(value="--"),
            "current": tk.StringVar(value="--"),
            "soh": tk.StringVar(value="--"),
            "cap_now": tk.StringVar(value="--"),
            "cap_full": tk.StringVar(value="--"),
            "cycle": tk.StringVar(value="--"),
            "soc": tk.StringVar(value="SOC:--"),
            "state": tk.StringVar(value="--"),
        }
        for row, (label, key, unit) in enumerate(
            [
                ("总压", "total_v", "V"),
                ("电流", "current", "A"),
                ("SOH", "soh", "%"),
                ("剩余容量", "cap_now", "mAh"),
                ("满电容量", "cap_full", "mAh"),
                ("循环次数", "cycle", ""),
            ]
        ):
            ttk.Label(basic, text=label).grid(row=row, column=0, sticky="w", padx=(12, 6), pady=7)
            ttk.Label(basic, textvariable=self.basic_vars[key], style="Value.TLabel").grid(row=row, column=1, sticky="w", pady=7)
            ttk.Label(basic, text=unit).grid(row=row, column=2, sticky="w", padx=(4, 10), pady=7)
        ttk.Label(basic, textvariable=self.basic_vars["soc"], font=("TkDefaultFont", 13, "bold")).grid(
            row=0, column=3, columnspan=2, sticky="s", padx=(12, 12)
        )
        self.battery_canvas = tk.Canvas(basic, width=84, height=170, highlightthickness=0)
        self.battery_canvas.grid(row=1, column=3, rowspan=5, padx=(10, 14), pady=(4, 10))
        ttk.Label(basic, textvariable=self.basic_vars["state"]).grid(row=6, column=3, sticky="n", pady=(0, 8))
        self._draw_battery(0)

        temp = ttk.LabelFrame(center, text="温度(℃)")
        temp.grid(row=1, column=0, sticky="nsew", pady=(8, 0))
        temp.columnconfigure(1, weight=1)
        self.temp_vars = {
            "max": tk.StringVar(value="--"),
            "min": tk.StringVar(value="--"),
            "t1": tk.StringVar(value="--"),
            "t2": tk.StringVar(value="--"),
            "mos": tk.StringVar(value="--"),
        }
        for row, (label, key, color) in enumerate(
            [("最高温度", "max", "red"), ("最低温度", "min", "green"), ("温度1", "t1", ""), ("温度2", "t2", ""), ("MOS温度", "mos", "")]
        ):
            ttk.Label(temp, text=label).grid(row=row, column=0, sticky="w", padx=(12, 8), pady=13)
            value_label = ttk.Label(temp, textvariable=self.temp_vars[key])
            if color:
                value_label.configure(foreground=color)
            value_label.grid(row=row, column=1, sticky="w", pady=13)
            ttk.Label(temp, text="℃").grid(row=row, column=2, sticky="w", padx=(4, 12), pady=13)

        right = ttk.Frame(tab)
        right.grid(row=0, column=2, sticky="nsew", padx=(6, 0))
        right.columnconfigure(0, weight=1)
        right.rowconfigure(3, weight=1)
        right.rowconfigure(4, weight=1)
        right.rowconfigure(5, weight=1)

        comm = ttk.LabelFrame(right, text="通信设置")
        comm.grid(row=0, column=0, sticky="ew")
        self._build_connection_panel(comm)

        sys_box = ttk.LabelFrame(right, text="系统状态")
        sys_box.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        self.status_badges: dict[str, tk.Label] = {}
        for col, name in enumerate(["充电MOS", "放电MOS", "加热", "冷凝"]):
            ttk.Label(sys_box, text=name).grid(row=0, column=col, sticky="n", padx=7, pady=(7, 2))
            badge = tk.Label(sys_box, text="--", width=8, relief="ridge", bg="#b0b0b0", fg="white")
            badge.grid(row=1, column=col, padx=7, pady=(0, 7))
            self.status_badges[name] = badge

        self.fault_vars = {
            "first": tk.StringVar(value="--"),
            "second": tk.StringVar(value="--"),
            "third": tk.StringVar(value="--"),
            "monitor": tk.StringVar(value="--"),
        }
        for row, (title, key) in enumerate([("一级告警", "first"), ("二级告警", "second"), ("三级保护", "third"), ("系统监控信息", "monitor")], start=2):
            box = ttk.LabelFrame(right, text=title)
            box.grid(row=row, column=0, sticky="nsew", pady=(8, 0))
            box.columnconfigure(0, weight=1)
            ttk.Label(box, textvariable=self.fault_vars[key], justify=tk.LEFT, wraplength=360).grid(
                row=0, column=0, sticky="nw", padx=8, pady=8
            )

    def _build_connection_panel(self, parent: ttk.LabelFrame) -> None:
        parent.columnconfigure(1, weight=1)
        ttk.Label(parent, text="串口").grid(row=0, column=0, padx=(10, 6), pady=(8, 4), sticky="w")
        self.port_combo = ttk.Combobox(parent, textvariable=self.port_var, width=12)
        self.port_combo.grid(row=0, column=1, sticky="w", pady=(8, 4))
        ttk.Button(parent, text="刷新", command=self._refresh_ports).grid(row=0, column=2, padx=6, pady=(8, 4))
        ttk.Button(parent, text="连接检测", command=self._check_connection).grid(row=0, column=3, padx=(0, 10), pady=(8, 4))
        ttk.Label(parent, text="波特率").grid(row=1, column=0, padx=(10, 6), pady=(4, 8), sticky="w")
        ttk.Entry(parent, textvariable=self.baud_var, width=10).grid(row=1, column=1, sticky="w", pady=(4, 8))
        ttk.Button(parent, textvariable=self.live_button_var, command=self._toggle_live_monitor).grid(
            row=1, column=2, padx=6, pady=(4, 8)
        )
        ttk.Label(parent, text="间隔(s)").grid(row=1, column=3, sticky="w", padx=(0, 4), pady=(4, 8))
        ttk.Entry(parent, textvariable=self.live_interval_var, width=5).grid(row=1, column=4, sticky="w", padx=(0, 10), pady=(4, 8))

    def _build_data_tab(self, tab: ttk.Frame) -> None:
        tab.columnconfigure(0, weight=1)
        tab.rowconfigure(1, weight=1)
        top = ttk.Frame(tab)
        top.grid(row=0, column=0, sticky="ew", pady=(0, 6))
        ttk.Button(top, text="读取一次", command=self._read_bms_overview).grid(row=0, column=0, padx=(0, 8))
        ttk.Button(top, textvariable=self.live_button_var, command=self._toggle_live_monitor).grid(row=0, column=1, padx=(0, 8))
        ttk.Label(top, textvariable=self.bms_info_var).grid(row=0, column=2, sticky="w")

        self.data_tree = ttk.Treeview(tab, columns=("name", "value", "unit"), show="headings")
        self.data_tree.heading("name", text="项目")
        self.data_tree.heading("value", text="数值")
        self.data_tree.heading("unit", text="单位")
        self.data_tree.column("name", width=220, anchor=tk.W)
        self.data_tree.column("value", width=140, anchor=tk.CENTER)
        self.data_tree.column("unit", width=80, anchor=tk.CENTER)
        self.data_tree.grid(row=1, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(tab, orient=tk.VERTICAL, command=self.data_tree.yview)
        scroll.grid(row=1, column=1, sticky="ns")
        self.data_tree.configure(yscrollcommand=scroll.set)

    def _build_storage_tab(self, tab: ttk.Frame) -> None:
        tab.columnconfigure(0, weight=1)
        tab.rowconfigure(1, weight=1)
        actions = ttk.Frame(tab)
        actions.grid(row=0, column=0, sticky="ew", pady=(0, 6))
        ttk.Button(actions, text="读取BMS日志", command=self._read_bms_log).grid(row=0, column=0, padx=(0, 8))
        ttk.Label(actions, text="显示板端存储的事件记录").grid(row=0, column=1, sticky="w")

        self.log_tree = ttk.Treeview(tab, columns=("index", "event", "interval", "raw"), show="headings")
        for col, title, width in [
            ("index", "序号", 70),
            ("event", "事件", 260),
            ("interval", "距离上次", 120),
            ("raw", "原始值", 120),
        ]:
            self.log_tree.heading(col, text=title)
            self.log_tree.column(col, width=width, anchor=tk.CENTER if col != "event" else tk.W)
        self.log_tree.grid(row=1, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(tab, orient=tk.VERTICAL, command=self.log_tree.yview)
        scroll.grid(row=1, column=1, sticky="ns")
        self.log_tree.configure(yscrollcommand=scroll.set)

    def _build_params_tab(self, tab: ttk.Frame) -> None:
        tab.columnconfigure(0, weight=1)
        tab.rowconfigure(1, weight=1)
        actions = ttk.Frame(tab)
        actions.grid(row=0, column=0, sticky="ew", pady=(0, 6))
        ttk.Label(actions, text="SH309").grid(row=0, column=0, padx=(0, 16))
        ttk.Button(actions, text="读取保护参数", command=self._read_protect_params).grid(row=0, column=1, padx=(0, 8))
        ttk.Button(actions, text="读取其它参数", command=self._read_other_params).grid(row=0, column=2, padx=(0, 8))
        ttk.Button(actions, text="一键读取", command=self._read_all_params).grid(row=0, column=3, padx=(0, 8))
        ttk.Button(actions, text="写入修改", command=self._write_dirty_params).grid(row=0, column=4, padx=(0, 8))
        ttk.Button(actions, text="清除修改标记", command=self._clear_param_dirty).grid(row=0, column=5, padx=(0, 8))
        ttk.Label(actions, textvariable=self.param_dirty_var, foreground="#b35c00").grid(row=0, column=6, sticky="w")

        param_tabs = ttk.Notebook(tab)
        param_tabs.grid(row=1, column=0, sticky="nsew")
        protect_tab = ttk.Frame(param_tabs)
        other_tab = ttk.Frame(param_tabs)
        param_tabs.add(protect_tab, text="保护参数")
        param_tabs.add(other_tab, text="其它参数")

        protect_inner = self._make_scroll_area(protect_tab)
        other_inner = self._make_scroll_area(other_tab)
        self._build_sh309_protect_page(protect_inner)
        self._build_param_groups(other_inner, [p for p in BMS_PARAM_DEFS if 0x2300 <= p.addr < 0x2400], columns=3)

    def _build_system_tab(self, tab: ttk.Frame) -> None:
        tab.columnconfigure(0, weight=1)
        tab.columnconfigure(1, weight=1)
        tab.rowconfigure(1, weight=1)
        ttk.Button(tab, text="读取BMS信息", command=self._read_bms_overview).grid(row=0, column=0, sticky="w", pady=(0, 8))
        ttk.Button(tab, text="CAN诊断", command=self._can_diag).grid(row=0, column=1, sticky="w", pady=(0, 8))
        cards = ttk.Frame(tab)
        cards.grid(row=1, column=0, columnspan=2, sticky="nsew")
        cards.columnconfigure(0, weight=1)
        cards.columnconfigure(1, weight=1)
        self._status_card(cards, "comm tool", self.info_var, 0)
        self._status_card(cards, "comm tool 缓存", self.cache_var, 1)

    def _build_other_tab(self, tab: ttk.Frame) -> None:
        tab.columnconfigure(0, weight=1)
        tab.rowconfigure(5, weight=1)

        file_box = ttk.LabelFrame(tab, text="升级文件")
        file_box.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        file_box.columnconfigure(1, weight=1)
        ttk.Label(file_box, text="BMS App bin").grid(row=0, column=0, padx=(10, 6), pady=8)
        ttk.Entry(file_box, textvariable=self.bin_var).grid(row=0, column=1, sticky="ew", pady=8)
        ttk.Button(file_box, text="选择", command=self._choose_bin).grid(row=0, column=2, padx=6, pady=8)
        ttk.Button(file_box, text="校验文件", command=self._describe_selected_bin).grid(row=0, column=3, padx=(0, 10), pady=8)

        status = ttk.Frame(tab)
        status.grid(row=1, column=0, sticky="ew", pady=(0, 8))
        status.columnconfigure(0, weight=1)
        status.columnconfigure(1, weight=1)
        status.columnconfigure(2, weight=1)
        self._status_card(status, "comm tool", self.info_var, 0)
        self._status_card(status, "当前文件", self.image_var, 1)
        self._status_card(status, "comm tool 缓存", self.cache_var, 2)

        target = ttk.LabelFrame(tab, text="目标设备")
        target.grid(row=2, column=0, sticky="ew", pady=(0, 8))
        target.columnconfigure(8, weight=1)
        ttk.Label(target, text="CAN波特率").grid(row=0, column=0, padx=(10, 6), pady=8)
        ttk.Combobox(
            target,
            textvariable=self.can_bitrate_var,
            values=("250000", "125000", "500000"),
            width=10,
            state="normal",
        ).grid(row=0, column=1, sticky="w", pady=8)
        ttk.Label(target, text="BMS地址").grid(row=0, column=2, padx=(14, 6), pady=8)
        ttk.Entry(target, textvariable=self.app_can_addr_var, width=8).grid(row=0, column=3, sticky="w", pady=8)
        ttk.Label(target, text="IAP节点").grid(row=0, column=4, padx=(14, 6), pady=8)
        ttk.Entry(target, textvariable=self.node_id_var, width=8).grid(row=0, column=5, sticky="w", pady=8)
        ttk.Button(target, text="应用设置", command=self._apply_can_settings).grid(row=0, column=6, padx=(14, 8), pady=8)
        ttk.Label(
            target,
            text="多设备时必须保证目标 BMS 地址唯一；多个 BMS 同地址不要同时升级。",
            foreground="#9a4d00",
        ).grid(row=0, column=7, columnspan=2, sticky="w", padx=(6, 10), pady=8)

        actions = ttk.Frame(tab)
        actions.grid(row=3, column=0, sticky="ew", pady=(0, 8))
        ttk.Button(actions, text="读取缓存", command=self._read_cache).grid(row=0, column=0, padx=(0, 8))
        ttk.Button(actions, text="写入缓存", command=self._download_only).grid(row=0, column=1, padx=(0, 8))
        ttk.Button(actions, text="一键升级", command=self._upgrade_selected).grid(row=0, column=2, padx=(0, 8))
        ttk.Button(actions, text="使用缓存升级", command=self._upgrade_cached_selected).grid(row=0, column=3, padx=(0, 8))
        ttk.Button(actions, text="读取BMS状态", command=self._read_bms_status).grid(row=0, column=4, padx=(0, 8))
        ttk.Button(actions, text="CAN诊断", command=self._can_diag).grid(row=0, column=5, padx=(0, 8))

        advanced = ttk.LabelFrame(tab, text="高级寄存器")
        advanced.grid(row=4, column=0, sticky="ew", pady=(0, 8))
        advanced.columnconfigure(6, weight=1)
        ttk.Label(advanced, text="地址").grid(row=0, column=0, padx=(10, 6), pady=8)
        ttk.Entry(advanced, textvariable=self.bms_addr_var, width=12).grid(row=0, column=1, sticky="w", pady=8)
        ttk.Label(advanced, text="数量").grid(row=0, column=2, padx=(12, 6), pady=8)
        ttk.Entry(advanced, textvariable=self.bms_count_var, width=8).grid(row=0, column=3, sticky="w", pady=8)
        ttk.Button(advanced, text="读取", command=self._read_bms_regs).grid(row=0, column=4, padx=(12, 8), pady=8)
        ttk.Label(advanced, text="写入值").grid(row=0, column=5, padx=(12, 6), pady=8)
        ttk.Entry(advanced, textvariable=self.bms_values_var).grid(row=0, column=6, sticky="ew", pady=8)
        ttk.Button(advanced, text="写入", command=self._write_bms_regs).grid(row=0, column=7, padx=(8, 10), pady=8)
        ttk.Label(advanced, textvariable=self.bms_result_var, justify=tk.LEFT).grid(
            row=1, column=0, columnspan=8, sticky="ew", padx=10, pady=(0, 8)
        )

        progress = ttk.Frame(tab)
        progress.grid(row=5, column=0, sticky="nsew")
        progress.columnconfigure(0, weight=1)
        progress.rowconfigure(3, weight=1)
        ttk.Progressbar(progress, variable=self.progress_var, maximum=100).grid(row=0, column=0, sticky="ew", pady=(0, 6))
        ttk.Label(progress, textvariable=self.upgrade_stage_var).grid(row=1, column=0, sticky="w", pady=(0, 4))
        ttk.Label(progress, text="运行日志").grid(row=2, column=0, sticky="w")
        self.log_text = tk.Text(progress, height=16, wrap="word")
        self.log_text.grid(row=3, column=0, sticky="nsew")
        self.log_text.configure(state="disabled")
        scrollbar = ttk.Scrollbar(progress, orient=tk.VERTICAL, command=self.log_text.yview)
        scrollbar.grid(row=3, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=scrollbar.set)

    def _make_scroll_area(self, parent: ttk.Frame) -> ttk.Frame:
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(0, weight=1)
        canvas = tk.Canvas(parent, highlightthickness=0)
        scrollbar = ttk.Scrollbar(parent, orient=tk.VERTICAL, command=canvas.yview)
        inner = ttk.Frame(canvas)
        window_id = canvas.create_window((0, 0), window=inner, anchor="nw")

        def _on_configure(_event=None) -> None:
            canvas.configure(scrollregion=canvas.bbox("all"))
            canvas.itemconfigure(window_id, width=canvas.winfo_width())

        inner.bind("<Configure>", _on_configure)
        canvas.bind("<Configure>", _on_configure)
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.grid(row=0, column=0, sticky="nsew")
        scrollbar.grid(row=0, column=1, sticky="ns")
        return inner

    def _build_param_groups(self, parent: ttk.Frame, params: list[BmsParamDef], columns: int) -> None:
        grouped: dict[str, list[BmsParamDef]] = {}
        for param in params:
            grouped.setdefault(param.group, []).append(param)
        for col in range(columns):
            parent.columnconfigure(col, weight=1)
        for index, (group, group_params) in enumerate(grouped.items()):
            frame = ttk.LabelFrame(parent, text=group)
            frame.grid(row=index // columns, column=index % columns, sticky="nsew", padx=6, pady=6)
            frame.columnconfigure(1, weight=1)
            for row, param in enumerate(group_params):
                ttk.Label(frame, text=param.name).grid(row=row, column=0, sticky="w", padx=(8, 6), pady=3)
                var = self.param_entry_vars.get(param.key)
                if var is None:
                    var = tk.StringVar(value="")
                    var.trace_add("write", lambda *_args, key=param.key: self._mark_param_dirty(key))
                    self.param_entry_vars[param.key] = var
                ttk.Entry(frame, textvariable=var, width=12).grid(row=row, column=1, sticky="ew", pady=3)
                ttk.Label(frame, text=param.unit, width=5).grid(row=row, column=2, sticky="w", padx=(4, 8), pady=3)

    def _build_sh309_protect_page(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(parent, text="SH309")
        frame.grid(row=0, column=0, sticky="nw", padx=6, pady=6)
        parent.columnconfigure(0, weight=1)
        for col in (1, 4):
            frame.columnconfigure(col, minsize=112)

        left_rows = [
            ("单节过压(mv)", "SH309/单节过压(mv)"),
            ("单节低压(mv)", "SH309/单节低压(mv)"),
            ("一级充电过流(A)", "SH309/一级充电过流(A)"),
            ("二级充电过流(A)", "SH309/二级充电过流(A)"),
            ("一级放电过流(A)", "SH309/一级放电过流(A)"),
            ("二级放电过流(A)", "SH309/二级放电过流(A)"),
            ("充电高温(℃)", "SH309/充电高温(℃)"),
            ("充电低温(℃)", "SH309/充电低温(℃)"),
            ("放电高温(℃)", "SH309/放电高温(℃)"),
            ("放电低温(℃)", "SH309/放电低温(℃)"),
            ("短路电流(A)", "SH309/短路电流(A)"),
            ("MOS过温1(℃)", "SH309/MOS过温1(℃)"),
            ("MOS过温3(℃)", "SH309/MOS过温3(℃)"),
            ("延时(10ms)", "SH309/延时(10ms)"),
        ]
        right_rows = [
            ("过压恢复(mv)", "SH309/过压恢复(mv)"),
            ("过压延时(ms)", "SH309/过压延时(ms)"),
            ("低压恢复(mv)", "SH309/低压恢复(mv)"),
            ("低压延时(ms)", "SH309/低压延时(ms)"),
            ("一级充电过流延时(ms)", "SH309/一级充电过流延时(ms)"),
            ("二级充电过流延时(ms)", "SH309/二级充电过流延时(ms)"),
            ("一级放电过流延时(ms)", "SH309/一级放电过流延时(ms)"),
            ("二级放电过流延时(ms)", "SH309/二级放电过流延时(ms)"),
            ("充电高温恢复(℃)", "SH309/充电高温恢复(℃)"),
            ("充电低温恢复(℃)", "SH309/充电低温恢复(℃)"),
            ("放电高温恢复(℃)", "SH309/放电高温恢复(℃)"),
            ("放电低温恢复(℃)", "SH309/放电低温恢复(℃)"),
            ("短路延时(us)", "SH309/短路延时(us)"),
            ("MOS过温2(℃)", "SH309/MOS过温2(℃)"),
            ("MOS恢复(℃)", "SH309/MOS恢复(℃)"),
        ]
        for row, (label, key) in enumerate(left_rows):
            self._add_sh309_param_field(frame, row, 0, label, key)
        for row, (label, key) in enumerate(right_rows):
            self._add_sh309_param_field(frame, row, 3, label, key)

        bottom = max(len(left_rows), len(right_rows)) + 1
        ttk.Button(frame, text="MOS过温重置", command=self._reset_sh309_tmos_defaults).grid(
            row=bottom, column=0, columnspan=2, sticky="w", padx=10, pady=(14, 10)
        )
        ttk.Button(frame, text="保护点重置", command=self._reset_sh309_protect_params).grid(
            row=bottom, column=2, columnspan=2, padx=18, pady=(14, 10)
        )
        ttk.Button(frame, text="读取", command=self._read_protect_params).grid(
            row=bottom, column=4, sticky="e", padx=(0, 8), pady=(14, 10)
        )
        ttk.Button(frame, text="设置", command=self._write_dirty_params).grid(
            row=bottom, column=5, sticky="e", padx=(0, 10), pady=(14, 10)
        )

    def _add_sh309_param_field(self, parent: ttk.Frame, row: int, column: int, label: str, key: str) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, sticky="w", padx=(10, 8), pady=6)
        var = self.param_entry_vars.get(key)
        if var is None:
            var = tk.StringVar(value="")
            var.trace_add("write", lambda *_args, item_key=key: self._mark_param_dirty(item_key))
            self.param_entry_vars[key] = var
        choices = self._sh309_field_choices(key)
        if choices:
            widget = ttk.Combobox(parent, textvariable=var, values=choices, width=12, state="normal")
        else:
            widget = ttk.Entry(parent, textvariable=var, width=14)
        widget.grid(row=row, column=column + 1, sticky="ew", padx=(0, 16), pady=6)

    def _sh309_field_choices(self, key: str) -> list[str]:
        if key in {"SH309/单节过压(mv)", "SH309/过压恢复(mv)"}:
            return [str(value) for value in range(1000, 5001, 5)]
        if key in {"SH309/单节低压(mv)", "SH309/低压恢复(mv)"}:
            return [str(value) for value in range(1000, 5001, 20)]
        if key in {"SH309/过压延时(ms)", "SH309/低压延时(ms)"}:
            return [str(value) for value in SH309_OV_UV_DELAY_MS]
        if key in {"SH309/二级充电过流延时(ms)"}:
            return [str(value) for value in SH309_CHG_SECOND_DELAY_MS]
        if key in {"SH309/二级放电过流延时(ms)"}:
            return [str(value) for value in SH309_DSG_SECOND_DELAY_MS]
        if key in {"SH309/二级充电过流(A)", "SH309/二级放电过流(A)"}:
            return [str(value) for value in SH309_COMMON_CURRENT_A]
        if key == "SH309/短路电流(A)":
            return [str(value) for value in SH309_SHORT_CURRENT_A]
        if key == "SH309/短路延时(us)":
            return [str(value) for value in SH309_SHORT_DELAY_US]
        if key in {
            "SH309/充电高温(℃)",
            "SH309/充电高温恢复(℃)",
            "SH309/放电高温(℃)",
            "SH309/放电高温恢复(℃)",
            "SH309/MOS过温1(℃)",
            "SH309/MOS过温2(℃)",
            "SH309/MOS过温3(℃)",
            "SH309/MOS恢复(℃)",
        }:
            return [str(value) for value in range(0, 161)]
        if key in {
            "SH309/充电低温(℃)",
            "SH309/充电低温恢复(℃)",
            "SH309/放电低温(℃)",
            "SH309/放电低温恢复(℃)",
        }:
            return [str(value) for value in range(-40, 41)]
        return []

    def _mark_param_dirty(self, key: str) -> None:
        if self.param_loading:
            return
        if key in BMS_PARAM_BY_KEY:
            self.param_dirty.add(key)
            self._update_param_dirty_label()

    def _update_param_dirty_label(self) -> None:
        if not self.param_dirty:
            self.param_dirty_var.set("未修改")
            return
        self.param_dirty_var.set(f"已修改 {len(self.param_dirty)} 项，未写入")

    def _clear_param_dirty(self) -> None:
        self.param_dirty.clear()
        self._update_param_dirty_label()

    def _write_dirty_params(self) -> None:
        if not self.param_dirty:
            messagebox.showinfo("参数设置", "没有检测到已修改的参数。")
            return
        try:
            values: dict[str, int] = {}
            for key in sorted(self.param_dirty, key=lambda item: BMS_PARAM_BY_KEY[item].addr):
                param = BMS_PARAM_BY_KEY[key]
                var = self.param_entry_vars.get(key)
                if var is None:
                    continue
                values[key] = _param_parse_display_value(param, var.get())
        except Exception as exc:
            messagebox.showerror("参数错误", str(exc))
            return
        preview = "\n".join(
            f"{BMS_PARAM_BY_KEY[key].group}/{BMS_PARAM_BY_KEY[key].name}: "
            f"{self.param_entry_vars[key].get().strip()} {BMS_PARAM_BY_KEY[key].unit}"
            for key in list(values)[:8]
        )
        if len(values) > 8:
            preview += f"\n... 共 {len(values)} 项"
        if not messagebox.askyesno("确认写入参数", f"确认写入以下已修改参数？\n\n{preview}"):
            return
        self.active_param_values = values
        self._run_worker("写入修改参数", self._worker_write_dirty_params)

    def _reset_sh309_tmos_defaults(self) -> None:
        if not messagebox.askyesno(
            "确认重置MOS过温",
            "将把 MOS 过温1/2/3、MOS恢复和延时写回当前项目默认值。\n\n确认继续？",
        ):
            return
        values: dict[str, int] = {}
        for key, display in SH309_TMOS_DEFAULT_DISPLAY.items():
            param = BMS_PARAM_BY_KEY[key]
            values[key] = _param_parse_display_value(param, display)
            var = self.param_entry_vars.get(key)
            if var is not None:
                var.set(display)
        self.active_param_values = values
        self._run_worker("MOS过温重置", self._worker_write_dirty_params)

    def _reset_sh309_protect_params(self) -> None:
        if not messagebox.askyesno(
            "确认保护点重置",
            "将同时执行 AFE 参数重置(0x1006)和保护参数重置(0x1002)，然后重新读取 SH309 参数。\n\n确认继续？",
        ):
            return
        self._run_worker("保护点重置", self._worker_reset_sh309_protect_params)

    def _toggle_live_monitor(self) -> None:
        if self.live_running:
            self._stop_live_monitor()
        else:
            self.live_running = True
            self.live_button_var.set("停止监控")
            self.comm_state_var.set("通信: 监控中")
            self._schedule_live_monitor(0)

    def _stop_live_monitor(self) -> None:
        self.live_running = False
        self.live_button_var.set("开始监控")
        if self.live_after_id is not None:
            try:
                self.after_cancel(self.live_after_id)
            except tk.TclError:
                pass
            self.live_after_id = None

    def _schedule_live_monitor(self, delay_ms: int | None = None) -> None:
        if not self.live_running:
            return
        if self.live_after_id is not None:
            return
        self.live_after_id = self.after(self._live_interval_ms() if delay_ms is None else delay_ms, self._poll_live_monitor)

    def _live_interval_ms(self) -> int:
        try:
            seconds = float(self.live_interval_var.get())
        except ValueError:
            seconds = 2.0
        seconds = max(1.0, min(seconds, 60.0))
        return int(seconds * 1000)

    def _poll_live_monitor(self) -> None:
        self.live_after_id = None
        if not self.live_running:
            return
        if (self.worker is not None and self.worker.is_alive()) or (
            self.live_worker is not None and self.live_worker.is_alive()
        ):
            self._schedule_live_monitor(500)
            return
        self.live_worker = threading.Thread(target=self._worker_live_guard, daemon=True)
        self.live_worker.start()

    def _worker_live_guard(self) -> None:
        if not self.serial_lock.acquire(blocking=False):
            self._emit("comm_state", "通信: 忙，等待当前任务")
            self._emit("live_done")
            return
        try:
            with self._open_client() as client:
                self._set_can_target(client)
                try:
                    words = self._read_bms_words(client, BMS_OVERVIEW_ADDR, BMS_LIVE_WORDS)
                except Exception:
                    words = self._read_bms_words(client, BMS_OVERVIEW_ADDR, BMS_OVERVIEW_WORDS)
            self._emit("live_snapshot", words)
            self._emit("progress", 100)
        except Exception as exc:
            self._emit("log", f"实时监控读取失败: {exc}")
            self._emit("comm_state", "通信: 异常")
        finally:
            self.serial_lock.release()
            self._emit("live_done")

    def _draw_battery(self, soc: int) -> None:
        soc = max(0, min(100, int(soc)))
        canvas = self.battery_canvas
        canvas.delete("all")
        canvas.create_rectangle(30, 4, 54, 14, outline="#666666", fill="#666666")
        canvas.create_rectangle(18, 14, 66, 160, outline="#666666", width=3)
        canvas.create_rectangle(23, 19, 61, 155, outline="#dddddd", fill="#f7f7f7")
        fill_height = int(132 * soc / 100)
        top = 151 - fill_height
        color = "#30c85a" if soc >= 20 else "#d93025"
        canvas.create_rectangle(27, top, 57, 151, outline=color, fill=color)

    def _set_badge(self, name: str, enabled: bool | None) -> None:
        badge = self.status_badges.get(name)
        if badge is None:
            return
        if enabled is None:
            badge.configure(text="--", bg="#b0b0b0", fg="white")
        elif enabled:
            badge.configure(text="on", bg="#36c85a", fg="white")
        else:
            badge.configure(text="off", bg="#d93025", fg="white")

    def _temp_display(self, raw: int) -> str:
        if raw == 0:
            return "--"
        return f"{_temp_c(raw):.1f}"

    def _show_main_snapshot(self, words: list[int]) -> None:
        if len(words) < BMS_OVERVIEW_WORDS:
            return
        cells = words[0:32]
        valid_cells = _valid_cell_items(cells)
        for index, (frame, row, col) in enumerate(self.cell_slots):
            if _is_valid_cell_voltage(cells[index]):
                self.cell_value_vars[index].set(str(cells[index]))
                frame.grid(row=row, column=col, sticky="w", padx=(0, 14), pady=3)
            else:
                frame.grid_remove()

        if valid_cells:
            max_index, max_mv = max(valid_cells, key=lambda item: item[1])
            min_index, min_mv = min(valid_cells, key=lambda item: item[1])
            avg_mv = sum(value for _, value in valid_cells) / len(valid_cells)
            self.cell_stat_vars["max"].set(f"最高电压 {max_mv}  {max_index + 1}")
            self.cell_stat_vars["min"].set(f"最低电压 {min_mv}  {min_index + 1}")
            self.cell_stat_vars["avg"].set(f"平均电压 {avg_mv:.2f}")
            self.cell_stat_vars["delta"].set(f"最大压差 {max_mv - min_mv}")
        else:
            for var in self.cell_stat_vars.values():
                var.set("--")

        ichg = words[50] / 10.0
        idsg = words[51] / 10.0
        net_current = ichg if ichg > 0 else -idsg
        soc = words[52]
        self.basic_vars["total_v"].set(f"{words[37] / 100.0:.2f}")
        self.basic_vars["current"].set(f"{net_current:.1f}")
        self.basic_vars["soh"].set(str(words[53]))
        self.basic_vars["cap_now"].set(str(words[54] * 10))
        self.basic_vars["cap_full"].set(str(words[55] * 10))
        self.basic_vars["cycle"].set(str(words[57]))
        self.basic_vars["soc"].set(f"SOC:{soc}%")
        self.basic_vars["state"].set("充电" if ichg > 0 else ("放电" if idsg > 0 else "静置"))
        self._draw_battery(soc)

        temp_values = words[38:48]
        self.temp_vars["max"].set(self._temp_display(words[48]))
        self.temp_vars["min"].set(self._temp_display(words[49]))
        self.temp_vars["t1"].set(self._temp_display(temp_values[0] if len(temp_values) > 0 else 0))
        self.temp_vars["t2"].set(self._temp_display(temp_values[1] if len(temp_values) > 1 else 0))
        self.temp_vars["mos"].set(self._temp_display(temp_values[3] if len(temp_values) > 3 else 0))

        status_low = words[84] if len(words) > 84 else None
        func_low = words[86] if len(words) > 86 else None
        self._set_badge("充电MOS", None if status_low is None else bool(status_low & (1 << 2)))
        self._set_badge("放电MOS", None if status_low is None else bool(status_low & (1 << 3)))
        self._set_badge("加热", None if status_low is None else bool(status_low & (1 << 8)))
        self._set_badge("冷凝", None if status_low is None else bool(status_low & (1 << 9)))

        self.fault_vars["first"].set(f"0x{words[58]:04X}  {_fault_text(words[58])}")
        self.fault_vars["second"].set(f"0x{words[59]:04X}  {_fault_text(words[59])}")
        self.fault_vars["third"].set(f"0x{words[60]:04X}  {_fault_text(words[60])}")
        monitor = f"均衡: 0x{words[61]:04X} 0x{words[62]:04X}"
        if status_low is not None:
            monitor += f"\n系统字: 0x{status_low:04X}"
        if func_low is not None:
            monitor += f"\n功能字: 0x{func_low:04X}"
        self.fault_vars["monitor"].set(monitor)

        self.bms_info_var.set(self._format_bms_overview(words[:BMS_OVERVIEW_WORDS]))
        self.comm_state_var.set("通信: 正在通讯" if self.live_running else "通信: 已响应")
        self._update_data_tree(words)

    def _update_data_tree(self, words: list[int]) -> None:
        tree = getattr(self, "data_tree", None)
        if tree is None:
            return
        for item in tree.get_children():
            tree.delete(item)
        rows: list[tuple[str, str, str]] = [
            ("总压", f"{words[37] / 100.0:.2f}", "V"),
            ("充电电流", f"{words[50] / 10.0:.1f}", "A"),
            ("放电电流", f"{words[51] / 10.0:.1f}", "A"),
            ("SOC", str(words[52]), "%"),
            ("SOH", str(words[53]), "%"),
            ("剩余容量", str(words[54] * 10), "mAh"),
            ("满电容量", str(words[55] * 10), "mAh"),
            ("循环次数", str(words[57]), "次"),
            ("一级告警字", f"0x{words[58]:04X}", ""),
            ("二级告警字", f"0x{words[59]:04X}", ""),
            ("三级保护字", f"0x{words[60]:04X}", ""),
        ]
        rows.extend((f"单体{index + 1:02d}", str(value), "mV") for index, value in _valid_cell_items(words[0:32]))
        for index, raw in enumerate(words[38:48]):
            if raw != 0:
                rows.append((f"温度{index + 1}", f"{_temp_c(raw):.1f}", "℃"))
        for name, value, unit in rows:
            tree.insert("", tk.END, values=(name, value, unit))

    def _update_log_tree(self, records: list[tuple[int, int]]) -> None:
        if self.log_tree is None:
            return
        for item in self.log_tree.get_children():
            self.log_tree.delete(item)
        for index, (event, delta) in enumerate(records, start=1):
            if event == 0 and delta == 0:
                continue
            name = BMS_EVENT_NAMES[event] if event < len(BMS_EVENT_NAMES) else f"未知事件{event}"
            self.log_tree.insert(
                "",
                tk.END,
                values=(f"{index:03d}", name, _event_interval_text(delta), f"0x{event:02X} 0x{delta:02X}"),
            )

    def _status_card(self, parent: ttk.Frame, title: str, var: tk.StringVar, column: int) -> None:
        frame = ttk.LabelFrame(parent, text=title)
        frame.grid(row=0, column=column, sticky="nsew", padx=(0 if column == 0 else 8, 0))
        frame.columnconfigure(0, weight=1)
        ttk.Label(frame, textvariable=var, justify=tk.LEFT).grid(row=0, column=0, sticky="ew", padx=10, pady=8)

    def _populate_param_tree(self) -> None:
        group_iids: dict[str, str] = {}
        for param in BMS_PARAM_DEFS:
            if param.group not in group_iids:
                iid = f"group:{param.group}"
                group_iids[param.group] = iid
                self.param_tree.insert("", tk.END, iid=iid, text=param.group, open=True, values=("", "", ""))
            self.param_tree.insert(
                group_iids[param.group],
                tk.END,
                iid=param.key,
                text=param.name,
                values=(f"0x{param.addr:04X}", "--", param.unit),
            )

    def _selected_param_key(self) -> str:
        selection = self.param_tree.selection() if hasattr(self, "param_tree") else ()
        if not selection:
            return ""
        key = selection[0]
        return key if key in BMS_PARAM_BY_KEY else ""

    def _on_param_select(self, _event=None) -> None:
        key = self._selected_param_key()
        if not key:
            self.param_selected_var.set("未选择")
            self.param_edit_var.set("")
            return
        param = BMS_PARAM_BY_KEY[key]
        raw = self.param_values.get(key)
        value_text = _param_display_value(param, raw) if raw is not None else ""
        self.param_selected_var.set(f"{param.group} / {param.name}\n0x{param.addr:04X}")
        self.param_edit_var.set(value_text)
        if raw is None:
            self.param_current_var.set("未读取")
        else:
            self.param_current_var.set(f"当前 {value_text} {param.unit}，原始值 {raw}")

    def _update_param_row(self, key: str, raw: int) -> None:
        param = BMS_PARAM_BY_KEY[key]
        self.param_values[key] = raw
        display = _param_display_value(param, raw)
        if hasattr(self, "param_tree"):
            self.param_tree.item(key, values=(f"0x{param.addr:04X}", display, param.unit))
        var = self.param_entry_vars.get(key)
        if var is not None:
            old_loading = self.param_loading
            self.param_loading = True
            try:
                var.set(display)
            finally:
                self.param_loading = old_loading

    def _set_busy(self, busy: bool) -> None:
        state = tk.DISABLED if busy else tk.NORMAL
        if self.monitor_window is not None:
            self.monitor_window.set_paused_by_parent(busy)
        for child in self.winfo_children():
            self._set_child_state(child, state)
        if busy:
            self.result_var.set("正在执行...")

    def _set_child_state(self, widget, state: str) -> None:
        for child in widget.winfo_children():
            if isinstance(child, ttk.Combobox):
                if state == tk.NORMAL and child is getattr(self, "param_combo", None):
                    child.configure(state="readonly")
                else:
                    child.configure(state=state)
            elif isinstance(child, (ttk.Button, ttk.Entry)):
                child.configure(state=state)
            self._set_child_state(child, state)

    def _refresh_ports(self) -> None:
        try:
            require_pyserial()
            from serial.tools import list_ports  # type: ignore

            ports = [port.device for port in list_ports.comports()]
            self.port_combo["values"] = ports
            if ports and not self.port_var.get():
                self.port_var.set(ports[0])
        except Exception as exc:
            self._log(f"串口枚举失败: {exc}")

    def _choose_bin(self) -> None:
        path = filedialog.askopenfilename(
            title="选择 BMS App bin",
            initialdir=str(DEFAULT_BIN.parent if DEFAULT_BIN.exists() else REPO_ROOT),
            filetypes=[("BIN 固件", "*.bin"), ("所有文件", "*.*")],
        )
        if path:
            self.bin_var.set(path)
            self._describe_selected_bin()

    def _check_connection(self) -> None:
        self._run_worker("连接检测", self._worker_check_connection)

    def _read_cache(self) -> None:
        self._run_worker("读取缓存", self._worker_read_cache)

    def _download_only(self) -> None:
        self._run_worker("写入缓存", self._worker_download_only)

    def _apply_can_settings(self) -> None:
        self._run_worker("应用目标设备设置", self._worker_apply_can_settings)

    def _upgrade_selected(self) -> None:
        path = Path(self.bin_var.get())
        if not path.exists():
            messagebox.showerror("文件不存在", "请选择有效的 BMS App bin 文件。")
            return
        if not messagebox.askyesno(
            "确认升级",
            "将先把当前选择的 bin 写入 comm tool 缓存，再通过 CAN 升级 BMS。\n\n"
            f"文件: {path}\n\n"
            "确认继续？",
        ):
            return
        self._run_worker("一键升级", self._worker_upgrade_selected)

    def _upgrade_cached_selected(self) -> None:
        path = Path(self.bin_var.get())
        if not path.exists():
            messagebox.showerror("文件不存在", "请选择有效的 BMS App bin 文件。")
            return
        if not messagebox.askyesno(
            "确认使用缓存升级",
            "将跳过写入 comm tool 缓存，仅在缓存大小和 CRC 与当前选择的 bin 完全一致时升级 BMS。\n\n"
            f"文件: {path}\n\n"
            "确认继续？",
        ):
            return
        self._run_worker("使用缓存升级", self._worker_upgrade_cached_selected)

    def _read_bms_status(self) -> None:
        self._run_worker("读取BMS状态", self._worker_read_bms_status)

    def _read_bms_overview(self) -> None:
        self._run_worker("读取BMS信息", self._worker_read_bms_overview)

    def _read_selected_param(self) -> None:
        key = self._selected_param_key()
        if not key:
            messagebox.showinfo("参数设置", "请先选择一个保护参数或其它参数。")
            return
        self.active_param_key = key
        self._run_worker("读取BMS参数", self._worker_read_selected_param)

    def _write_selected_param(self) -> None:
        try:
            key = self._selected_param_key()
            if not key:
                raise ValueError("请先选择一个保护参数或其它参数")
            param = BMS_PARAM_BY_KEY[key]
            value = _param_parse_display_value(param, self.param_edit_var.get())
        except Exception as exc:
            messagebox.showerror("参数错误", str(exc))
            return
        self.active_param_key = key
        self.active_param_raw = value
        if not messagebox.askyesno(
            "确认写入参数",
            f"参数: {param.group} / {param.name}\n地址: 0x{param.addr:04X}\n"
            f"显示值: {self.param_edit_var.get().strip()} {param.unit}\n板端原始值: {value} (0x{value:04X})\n\n"
            "确认写入 BMS？",
        ):
            return
        self._run_worker("写入BMS参数", self._worker_write_selected_param)

    def _read_protect_params(self) -> None:
        self.active_param_range = "protect"
        self._run_worker("读取保护参数", self._worker_read_param_range)

    def _read_other_params(self) -> None:
        self.active_param_range = "other"
        self._run_worker("读取其它参数", self._worker_read_param_range)

    def _read_all_params(self) -> None:
        self.active_param_range = "all"
        self._run_worker("读取全部参数", self._worker_read_param_range)

    def _read_bms_log(self) -> None:
        self._run_worker("读取BMS日志", self._worker_read_bms_log)

    def _read_bms_regs(self) -> None:
        try:
            self.active_bms_addr = self._parse_u16(self.bms_addr_var.get(), "地址")
            self.active_bms_count = self._parse_count(self.bms_count_var.get())
        except Exception as exc:
            messagebox.showerror("参数错误", str(exc))
            return
        self._run_worker("读取BMS寄存器", self._worker_read_bms_regs)

    def _write_bms_regs(self) -> None:
        try:
            self.active_bms_addr = self._parse_u16(self.bms_addr_var.get(), "地址")
            self.active_bms_words = self._parse_words(self.bms_values_var.get())
        except Exception as exc:
            messagebox.showerror("参数错误", str(exc))
            return
        if not messagebox.askyesno(
            "确认写入",
            f"将通过 comm tool/CAN 写入 BMS。\n\n"
            f"起始地址: 0x{self.active_bms_addr:04X}\n"
            f"数量: {len(self.active_bms_words)}\n\n"
            "板端仍会做地址、范围和权限检查，若参数越界会返回错误。",
        ):
            return
        self._run_worker("写入BMS寄存器", self._worker_write_bms_regs)

    def _can_diag(self) -> None:
        self._run_worker("CAN诊断", self._worker_can_diag)

    def _open_monitor(self) -> None:
        if self.monitor_window is not None and self.monitor_window.winfo_exists():
            self.monitor_window.lift()
            return
        self.monitor_window = BmsMonitorWindow(self)
        self.monitor_window.start()

    def _run_worker(self, name: str, target: Callable[[], None]) -> None:
        if self.worker and self.worker.is_alive():
            messagebox.showinfo("正在执行", "当前任务还没有结束。")
            return
        try:
            self.active_port = self.port_var.get().strip()
            self.active_baud = int(self.baud_var.get().strip())
            self.active_bin = Path(self.bin_var.get()).resolve()
            self.active_can_bitrate = int(self.can_bitrate_var.get().strip(), 0)
            self.active_node_id = int(self.node_id_var.get().strip(), 0)
            self.active_app_can_addr = int(self.app_can_addr_var.get().strip(), 0)
            if self.active_node_id <= 0 or self.active_node_id > 0x7F:
                raise ValueError("IAP节点必须是 1..127")
            if self.active_app_can_addr < 0 or self.active_app_can_addr > 0x0F:
                raise ValueError("BMS地址必须是 0..15")
        except Exception as exc:
            messagebox.showerror("参数错误", str(exc))
            return
        self.progress_var.set(0)
        self.upgrade_stage_var.set("升级进度: --")
        self._set_busy(True)
        self.worker = threading.Thread(target=self._worker_guard, args=(name, target), daemon=True)
        self.worker.start()

    def _parse_u16(self, text: str, name: str) -> int:
        value = int(text.strip(), 0)
        if value < 0 or value > 0xFFFF:
            raise ValueError(f"{name} 超出 0x0000..0xFFFF")
        return value

    def _parse_count(self, text: str) -> int:
        value = int(text.strip(), 0)
        if value <= 0 or value > 120:
            raise ValueError("数量必须是 1..120")
        return value

    def _parse_words(self, text: str) -> list[int]:
        parts = text.replace(",", " ").replace(";", " ").split()
        if not parts:
            raise ValueError("请填写至少一个写入值")
        if len(parts) > 120:
            raise ValueError("一次最多写入 120 个寄存器")
        return [self._parse_u16(part, "写入值") for part in parts]

    def _worker_guard(self, name: str, target: Callable[[], None]) -> None:
        locked = False
        try:
            self._emit("log", f"开始: {name}")
            if not self.serial_lock.acquire(blocking=False):
                self._emit("log", "串口正在被实时监控占用，等待当前读数结束")
                self.serial_lock.acquire()
            locked = True
            target()
            self._emit("result", f"{name} 完成")
        except Exception as exc:
            self._emit("error", f"{name} 失败: {exc}")
        finally:
            if locked:
                self.serial_lock.release()
            self._emit("busy", False)

    def _worker_check_connection(self) -> None:
        with self._open_client() as client:
            info = self._read_info(client)
            cache = self._read_cache_info(client)
        self._emit("info", info)
        self._emit("target", info)
        self._emit("cache", cache)
        self._emit("progress", 100)

    def _worker_apply_can_settings(self) -> None:
        with self._open_client() as client:
            self._set_can_target(client, force=True)
            info = self._read_info(client)
        self._emit("info", info)
        self._emit("target", info)
        self._emit("log", f"目标设备已设置: BMS地址={self.active_app_can_addr} IAP节点={self.active_node_id}")
        self._emit("progress", 100)

    def _worker_read_cache(self) -> None:
        with self._open_client() as client:
            cache = self._read_cache_info(client)
        self._emit("cache", cache)
        self._emit("progress", 100)

    def _worker_download_only(self) -> None:
        image = self._load_selected_image()
        with self._open_client() as client:
            self._download_image(client, image)
            cache = self._read_cache_info(client)
        self._assert_cache_matches(image, cache)
        self._emit("cache", cache)
        self._emit("progress", 100)

    def _worker_upgrade_selected(self) -> None:
        image = self._load_selected_image()
        with self._open_client() as client:
            self._set_can_target(client, force=True)
            info = self._read_info(client)
            self._emit("info", info)
            self._emit("target", info)
            self._download_image(client, image, progress_base=0, progress_span=45)
            cache = self._read_cache_info(client)
            self._assert_cache_matches(image, cache)
            self._emit("cache", cache)
            self._upgrade_bms_from_verified_cache(client, progress_base=50, progress_span=45)
        self._emit("progress", 100)

    def _worker_upgrade_cached_selected(self) -> None:
        image = self._load_selected_image()
        with self._open_client() as client:
            self._set_can_target(client)
            info = self._read_info(client)
            self._emit("info", info)
            self._emit("target", info)
            cache = self._read_cache_info(client)
            self._assert_cache_matches(image, cache)
            self._emit("cache", cache)
            self._emit("log", "缓存与当前文件一致，跳过串口下载")
            self._upgrade_bms_from_verified_cache(client, progress_base=5, progress_span=90)
        self._emit("progress", 100)

    def _worker_read_bms_status(self) -> None:
        with self._open_client() as client:
            self._set_can_target(client)
            status = self._read_bms_app_status(client)
        self._emit("log", f"BMS App 状态: SOC={status[0]} SOH={status[1]}")
        self._emit("progress", 100)

    def _worker_read_bms_overview(self) -> None:
        with self._open_client() as client:
            self._set_can_target(client)
            try:
                words = self._read_bms_words(client, BMS_OVERVIEW_ADDR, BMS_LIVE_WORDS)
            except Exception:
                words = self._read_bms_words(client, BMS_OVERVIEW_ADDR, BMS_OVERVIEW_WORDS)
        self._emit("live_snapshot", words)
        self._emit("bms_info", self._format_bms_overview(words[:BMS_OVERVIEW_WORDS]))
        self._emit("log", "BMS 信息读取完成")
        self._emit("progress", 100)

    def _worker_read_selected_param(self) -> None:
        key = self.active_param_key
        param = BMS_PARAM_BY_KEY[key]
        with self._open_client() as client:
            self._set_can_target(client)
            words = self._read_bms_words(client, param.addr, 1)
        value = words[0]
        self._emit("param_value", (key, value))
        self._emit("log", f"参数读取完成: {param.group}/{param.name}=0x{value:04X} ({value})")
        self._emit("progress", 100)

    def _worker_write_selected_param(self) -> None:
        key = self.active_param_key
        param = BMS_PARAM_BY_KEY[key]
        value = self.active_param_raw
        with self._open_client() as client:
            self._set_can_target(client)
            self._write_bms_words(client, param.addr, [value])
            words = self._read_bms_words(client, param.addr, 1)
        self._emit("param_value", (key, words[0]))
        self._emit("log", f"参数写入完成: {param.group}/{param.name}=0x{words[0]:04X} ({words[0]})")
        self._emit("progress", 100)

    def _worker_write_dirty_params(self) -> None:
        values = sorted(self.active_param_values.items(), key=lambda item: BMS_PARAM_BY_KEY[item[0]].addr)
        verified: dict[str, int] = {}
        with self._open_client() as client:
            self._set_can_target(client)
            for key, value in values:
                param = BMS_PARAM_BY_KEY[key]
                self._write_bms_words(client, param.addr, [value])
                words = self._read_bms_words(client, param.addr, 1)
                verified[key] = words[0]
                if words[0] != value:
                    raise RuntimeError(
                        f"{param.group}/{param.name} 写入后回读不一致: 写0x{value:04X} 回读0x{words[0]:04X}"
                    )
        self._emit("param_values", verified)
        self._emit("log", f"修改参数写入完成: {len(verified)} 项")
        self._emit("progress", 100)

    def _read_sh309_param_values(self, client: CommToolClient) -> dict[str, int]:
        values: dict[str, int] = {}
        for addr, count in ((SH309_AFE_PARAM_ADDR, SH309_AFE_PARAM_WORDS), (SH309_TMOS_PARAM_ADDR, SH309_TMOS_PARAM_WORDS)):
            words = self._read_bms_words(client, addr, count)
            for index, raw in enumerate(words):
                key = BMS_PARAM_ADDR_TO_KEY.get(addr + index)
                if key:
                    values[key] = raw
        return values

    def _worker_reset_sh309_protect_params(self) -> None:
        with self._open_client() as client:
            self._set_can_target(client)
            self._write_bms_words(client, SH309_RESET_AFE_ADDR, [1])
            self._write_bms_words(client, SH309_RESET_PROTECT_ADDR, [1])
            values = self._read_sh309_param_values(client)
        self._emit("param_values", values)
        self._emit("log", "SH309 保护点已重置并回读")
        self._emit("progress", 100)

    def _worker_read_param_range(self) -> None:
        ranges: list[tuple[int, int]]
        if self.active_param_range == "protect":
            ranges = [(SH309_AFE_PARAM_ADDR, SH309_AFE_PARAM_WORDS), (SH309_TMOS_PARAM_ADDR, SH309_TMOS_PARAM_WORDS)]
        elif self.active_param_range == "other":
            ranges = [(0x2300, 32)]
        else:
            ranges = [
                (SH309_AFE_PARAM_ADDR, SH309_AFE_PARAM_WORDS),
                (SH309_TMOS_PARAM_ADDR, SH309_TMOS_PARAM_WORDS),
                (0x2300, 32),
            ]
        values: dict[str, int] = {}
        with self._open_client() as client:
            self._set_can_target(client)
            for addr, count in ranges:
                words = self._read_bms_words(client, addr, count)
                for index, raw in enumerate(words):
                    key = BMS_PARAM_ADDR_TO_KEY.get(addr + index)
                    if key:
                        values[key] = raw
        self._emit("param_values", values)
        self._emit("log", f"参数读取完成: {len(values)} 项")
        self._emit("progress", 100)

    def _worker_read_bms_log(self) -> None:
        words: list[int] = []
        with self._open_client() as client:
            self._set_can_target(client)
            for offset in range(0, BMS_EVENT_RECORD_WORDS, BMS_EVENT_RECORD_READ_CHUNK_WORDS):
                count = min(BMS_EVENT_RECORD_READ_CHUNK_WORDS, BMS_EVENT_RECORD_WORDS - offset)
                addr = BMS_EVENT_RECORD_ADDR + offset
                label = f"BMS日志 {offset + 1}-{offset + count}"
                chunk = self._read_bms_words_with_retry(client, addr, count, label)
                words.extend(chunk)
                self._emit("progress", int(len(words) * 100 / BMS_EVENT_RECORD_WORDS))
                self._emit("log", f"读取BMS日志: {len(words)}/{BMS_EVENT_RECORD_WORDS}")
                if len(words) < BMS_EVENT_RECORD_WORDS:
                    time.sleep(BMS_LOG_CHUNK_GAP_SECONDS)
        records = [((word >> 8) & 0xFF, word & 0xFF) for word in words]
        valid_count = sum(1 for event, delta in records if event != 0 or delta != 0)
        self._emit("bms_log", records)
        self._emit("log", f"BMS 日志读取完成: {valid_count} 条")
        self._emit("progress", 100)

    def _worker_read_bms_regs(self) -> None:
        addr = self.active_bms_addr
        count = self.active_bms_count
        with self._open_client() as client:
            self._set_can_target(client)
            words = self._read_bms_words(client, addr, count)
        text = self._format_bms_words(addr, words)
        self._emit("bms_result", text)
        self._emit("log", "BMS 寄存器读取完成")
        self._emit("progress", 100)

    def _worker_write_bms_regs(self) -> None:
        addr = self.active_bms_addr
        words = self.active_bms_words
        with self._open_client() as client:
            self._set_can_target(client)
            self._write_bms_words(client, addr, words)
        text = self._format_bms_words(addr, words)
        self._emit("bms_result", "已写入:\n" + text)
        self._emit("log", f"BMS 寄存器写入完成: addr=0x{addr:04X} count={len(words)}")
        self._emit("progress", 100)

    def _worker_can_diag(self) -> None:
        with self._open_client() as client:
            resp = client.command(CMD_CAN_DIAG, b"\x00", timeout=2.0)
        self._emit("log", self._format_can_diag(resp.payload))
        self._emit("progress", 100)

    def _set_can_target(self, client: CommToolClient, force: bool = False) -> None:
        target = (self.active_can_bitrate, self.active_node_id, self.active_app_can_addr)
        if not force and self.applied_target == target:
            return
        payload = struct.pack(
            "<IBBH",
            self.active_can_bitrate,
            self.active_node_id & 0xFF,
            self.active_app_can_addr & 0x0F,
            0,
        )
        client.command(CMD_SET_CAN, payload, timeout=2.0)
        self.applied_target = target

    def _upgrade_state_text(self, status: dict) -> str:
        state_text = {
            0: "空闲",
            1: "升级中",
            2: "完成",
            3: "错误",
            4: "已终止",
        }.get(status["state"], f"状态{status['state']}")
        return (
            f"升级进度: {status['percent']}%  {status['written']}/{status['total']} bytes  "
            f"{state_text}  error=0x{status['error']:02X}"
        )

    def _upgrade_bms_from_verified_cache(
        self,
        client: CommToolClient,
        progress_base: int = 0,
        progress_span: int = 95,
    ) -> None:
        self._emit("log", "缓存校验通过，开始 CAN 升级 BMS")
        self._emit("upgrade_stage", "升级进度: 正在启动 BMS CAN-IAP")
        client.command(CMD_CAN_DIAG, b"\x01", timeout=2.0)
        client.command(CMD_UPGRADE, timeout=5.0)

        deadline = time.monotonic() + DEFAULT_LONG_TIMEOUT
        last_percent = -1
        last_log_time = 0.0
        status: dict | None = None
        while time.monotonic() < deadline:
            status = self._read_upgrade_status(client)
            self._emit("upgrade_stage", self._upgrade_state_text(status))
            self._emit("progress", min(99, progress_base + int(status["percent"] * progress_span / 100)))

            now = time.monotonic()
            if (
                status["percent"] != last_percent
                and (status["percent"] == 100 or status["percent"] - last_percent >= 5 or now - last_log_time >= 2.0)
            ):
                self._emit("log", self._format_upgrade_status(status))
                last_percent = status["percent"]
                last_log_time = now

            if status["state"] == 2 and status["error"] == 0:
                break
            if status["state"] in (3, 4) or status["error"] != 0:
                raise RuntimeError(self._format_upgrade_status(status))
            time.sleep(0.25)
        else:
            raise TimeoutError("等待 CAN 升级完成超时")

        if status is None or status["state"] != 2 or status["error"] != 0:
            raise RuntimeError(self._format_upgrade_status(status or {"state": 0, "percent": 0, "error": 0xFF, "written": 0, "total": 0, "expect_seq": 0}))

        self._emit("upgrade_stage", "升级进度: CAN升级完成，等待 BMS App 恢复")
        bms_status = self._wait_bms_status_after_upgrade(client)
        if bms_status is None:
            self._emit("log", "升级已完成，但 BMS App 状态暂未响应；请稍后点击“读取BMS状态”复核。")
        else:
            self._emit("log", f"BMS App 状态: SOC={bms_status[0]} SOH={bms_status[1]}")
        self._emit("upgrade_stage", "升级进度: 完成")

    def _open_client(self) -> CommToolClient:
        port = self.active_port
        if not port:
            raise RuntimeError("未选择串口")
        return CommToolClient(port, self.active_baud, timeout=1.0)

    def _read_info(self, client: CommToolClient) -> dict:
        payload = client.command(CMD_GET_INFO, timeout=2.0).payload
        if len(payload) < 20:
            raise RuntimeError("GET_INFO 响应长度不足")
        proto, major, minor, patch = struct.unpack_from("<BBBB", payload, 0)
        bitrate, cache_base, cache_size, flags = struct.unpack_from("<IIII", payload, 4)
        return {
            "proto": proto,
            "version": f"{major}.{minor}.{patch}",
            "bitrate": bitrate,
            "cache_base": cache_base,
            "cache_size": cache_size,
            "flags": flags,
            "node_id": payload[20] if len(payload) >= 21 else self.active_node_id,
            "app_can_addr": payload[21] if len(payload) >= 22 else self.active_app_can_addr,
        }

    def _read_cache_info(self, client: CommToolClient) -> dict:
        payload = client.command(CMD_FW_INFO, timeout=2.0).payload
        if len(payload) < 15:
            raise RuntimeError("FW_INFO 响应长度不足")
        app_addr, size, crc16, crc32, valid = struct.unpack_from("<IIHIB", payload, 0)
        return {
            "app_addr": app_addr,
            "size": size,
            "crc16": crc16,
            "crc32": crc32,
            "valid": valid,
        }

    def _read_upgrade_status(self, client: CommToolClient) -> dict:
        payload = client.command(CMD_UPGRADE_STATUS, timeout=2.0).payload
        if len(payload) < 13:
            raise RuntimeError("UPGRADE_STATUS 响应长度不足")
        state, percent, error = struct.unpack_from("<BBB", payload, 0)
        written, total, expect_seq = struct.unpack_from("<IIH", payload, 3)
        return {
            "state": state,
            "percent": percent,
            "error": error,
            "written": written,
            "total": total,
            "expect_seq": expect_seq,
        }

    def _read_bms_app_status(self, client: CommToolClient) -> tuple[int, int]:
        words = self._read_bms_words(client, 0xD034, 2)
        return words[0], words[1]

    def _read_bms_words(self, client: CommToolClient, addr: int, count: int) -> list[int]:
        payload = struct.pack("<HH", addr, count)
        resp = client.command(CMD_BMS_READ, payload, timeout=max(10.0, count * 1.5))
        if len(resp.payload) != count * 2:
            raise RuntimeError(f"BMS_READ 响应长度错误: {len(resp.payload)}")
        return list(struct.unpack("<" + "H" * count, resp.payload))

    def _read_bms_words_with_retry(self, client: CommToolClient, addr: int, count: int, label: str) -> list[int]:
        last_error: Exception | None = None
        for attempt in range(1, BMS_READ_RETRY_COUNT + 1):
            try:
                return self._read_bms_words(client, addr, count)
            except Exception as exc:
                last_error = exc
                if attempt >= BMS_READ_RETRY_COUNT:
                    break
                self._emit(
                    "log",
                    f"{label} 第 {attempt} 次失败，{BMS_READ_RETRY_DELAY_SECONDS:.1f}s 后重试: {exc}",
                )
                time.sleep(BMS_READ_RETRY_DELAY_SECONDS)
        raise RuntimeError(f"{label} 重试 {BMS_READ_RETRY_COUNT} 次仍失败: {last_error}") from last_error

    def _write_bms_words(self, client: CommToolClient, addr: int, words: list[int]) -> None:
        payload = struct.pack("<HH", addr, len(words))
        payload += struct.pack("<" + "H" * len(words), *words)
        client.command(CMD_BMS_WRITE, payload, timeout=max(10.0, len(words) * 2.5))

    def _wait_bms_status_after_upgrade(self, client: CommToolClient) -> Optional[tuple[int, int]]:
        deadline = time.monotonic() + POST_UPGRADE_APP_READY_TIMEOUT
        attempt = 0
        last_error = ""

        time.sleep(POST_UPGRADE_APP_READY_INTERVAL)
        while time.monotonic() < deadline:
            attempt += 1
            try:
                status = self._read_bms_app_status(client)
                if attempt > 1:
                    self._emit("log", f"BMS App 第 {attempt} 次确认成功")
                return status
            except Exception as exc:
                last_error = str(exc)
                self._emit("log", f"等待 BMS App 恢复响应: 第 {attempt} 次未响应")
                time.sleep(POST_UPGRADE_APP_READY_INTERVAL)

        if last_error:
            self._emit("log", f"BMS App 状态确认超时，最后错误: {last_error}")
        return None

    def _load_selected_image(self) -> bytes:
        path = self.active_bin
        if not path.exists():
            raise RuntimeError(f"找不到 bin 文件: {path}")
        image = path.read_bytes()
        if len(image) < 8:
            raise RuntimeError("bin 文件太小，缺少向量表")
        if APP_BASE_ADDR + len(image) > APP_FLASH_LIMIT:
            raise RuntimeError(f"bin 超出 App 区: end=0x{APP_BASE_ADDR + len(image):08X}")
        msp, reset, msp_ok, reset_ok = vector_summary(image)
        if not msp_ok or not reset_ok:
            raise RuntimeError(f"App 向量表非法: MSP=0x{msp:08X}, Reset=0x{reset:08X}")
        self._emit("image", self._image_info_text(path, image))
        return image

    def _download_image(
        self,
        client: CommToolClient,
        image: bytes,
        progress_base: int = 0,
        progress_span: int = 90,
    ) -> None:
        crc16 = crc16_modbus(image)
        crc32 = zlib.crc32(image) & 0xFFFFFFFF
        start_time = time.monotonic()
        total = math.ceil(len(image) / DEFAULT_CHUNK_SIZE)
        client.command(CMD_FW_BEGIN, struct.pack("<IIHI", APP_BASE_ADDR, len(image), crc16, crc32), timeout=5.0)
        for index, offset in enumerate(range(0, len(image), DEFAULT_CHUNK_SIZE), start=1):
            chunk = image[offset : offset + DEFAULT_CHUNK_SIZE]
            client.command(CMD_FW_DATA, struct.pack("<I", offset) + chunk, timeout=5.0)
            if index == total or index % 4 == 0:
                self._emit("progress", min(99, progress_base + int(index * progress_span / total)))
                self._emit("log", f"写入缓存: {index}/{total}")
        client.command(CMD_FW_END, struct.pack("<IHI", len(image), crc16, crc32), timeout=5.0)
        elapsed = max(0.001, time.monotonic() - start_time)
        self._emit("log", f"写入缓存完成: {len(image)} bytes, {elapsed:.1f}s, {len(image) / 1024.0 / elapsed:.1f} KiB/s")

    def _assert_cache_matches(self, image: bytes, cache: dict) -> None:
        crc16 = crc16_modbus(image)
        crc32 = zlib.crc32(image) & 0xFFFFFFFF
        if cache["app_addr"] != APP_BASE_ADDR or cache["size"] != len(image):
            raise RuntimeError("comm tool 缓存地址或大小与所选文件不一致")
        if cache["crc16"] != crc16 or cache["crc32"] != crc32 or cache["valid"] != 1:
            raise RuntimeError("comm tool 缓存 CRC 与所选文件不一致")

    def _describe_selected_bin(self) -> None:
        path = Path(self.bin_var.get()).resolve()
        try:
            if not path.exists():
                self.image_var.set("文件不存在")
                return
            image = path.read_bytes()
            self.image_var.set(self._image_info_text(path, image))
        except Exception as exc:
            self.image_var.set(f"校验失败: {exc}")

    def _image_info_text(self, path: Path, image: bytes) -> str:
        crc16 = crc16_modbus(image)
        crc32 = zlib.crc32(image) & 0xFFFFFFFF
        msp, reset, msp_ok, reset_ok = vector_summary(image)
        return (
            f"{path.name}\n"
            f"{len(image)} bytes\n"
            f"CRC16=0x{crc16:04X} CRC32=0x{crc32:08X}\n"
            f"MSP=0x{msp:08X} {'OK' if msp_ok else 'BAD'}\n"
            f"Reset=0x{reset:08X} {'OK' if reset_ok else 'BAD'}"
        )

    def _format_bms_words(self, addr: int, words: list[int]) -> str:
        return "  ".join(f"0x{addr + index:04X}=0x{value:04X}({value})" for index, value in enumerate(words))

    def _format_bms_overview(self, words: list[int]) -> str:
        if len(words) < BMS_OVERVIEW_WORDS:
            raise RuntimeError("BMS 信息长度不足")

        cells = words[0:32]
        valid_cells = _valid_cell_items(cells)
        max_cell_text, min_cell_text, delta_cell_text = _cell_stat_text(valid_cells)
        cell_text = " ".join(f"{index + 1}:{value}" for index, value in valid_cells)
        if not cell_text:
            cell_text = "无有效单体电压"

        total_v = words[37] / 100.0
        ichg = words[50] / 10.0
        idsg = words[51] / 10.0
        soc = words[52]
        soh = words[53]
        capacity_now = words[54] / 100.0
        capacity_full = words[55] / 100.0
        capacity_factory = words[56] / 100.0

        return (
            f"SOC {soc}%  SOH {soh}%  总压 {total_v:.2f}V  "
            f"充电 {ichg:.1f}A  放电 {idsg:.1f}A\n"
            f"单体: max {max_cell_text}  min {min_cell_text}  "
            f"压差 {delta_cell_text}  有效串数 {len(valid_cells)}\n"
            f"温度: max {_temp_c(words[48]):.1f}℃  min {_temp_c(words[49]):.1f}℃  "
            f"容量 {capacity_now:.2f}/{capacity_full:.2f}Ah  出厂 {capacity_factory:.2f}Ah  循环 {words[57]}\n"
            f"故障字: 0x{words[58]:04X} 0x{words[59]:04X} 0x{words[60]:04X}  "
            f"均衡: 0x{words[61]:04X} 0x{words[62]:04X}\n"
            f"单体电压(mV): {cell_text}"
        )

    def _format_upgrade_status(self, status: dict) -> str:
        return (
            f"升级状态: state={status['state']} percent={status['percent']}% "
            f"error=0x{status['error']:02X} written={status['written']}/{status['total']} "
            f"expect_seq={status['expect_seq']}"
        )

    def _format_can_diag(self, data: bytes) -> str:
        if len(data) < 62:
            return f"CAN_DIAG 响应长度不足: {len(data)}"
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
        node_id = data[61]
        app_can_addr = data[62] if len(data) >= 63 else 0
        return (
            "CAN 诊断:\n"
            f"  tx={tx_count} ok={tx_ok} fail={tx_fail} timeout={tx_timeout}\n"
            f"  rx={rx_count} drop={rx_drop}\n"
            f"  target: BMS地址={app_can_addr} IAP节点={node_id}\n"
            f"  ESR=0x{last_esr:08X} TSR=0x{last_tsr:08X} MSR=0x{last_msr:08X} RF0R=0x{last_rf0r:08X}\n"
            f"  last_tx=0x{last_tx_id:08X} last_rx=0x{last_rx_id:08X}"
        )

    def _format_info(self, info: dict) -> str:
        return (
            f"固件 {info['version']}\n"
            f"协议 {info['proto']}\n"
            f"CAN {info['bitrate']}\n"
            f"BMS地址 {info.get('app_can_addr', 0)}  IAP节点 {info.get('node_id', 1)}\n"
            f"缓存 0x{info['cache_base']:08X} + {info['cache_size']}"
        )

    def _format_cache(self, cache: dict) -> str:
        return (
            f"地址 0x{cache['app_addr']:08X}\n"
            f"{cache['size']} bytes\n"
            f"CRC16=0x{cache['crc16']:04X} CRC32=0x{cache['crc32']:08X}\n"
            f"有效={cache['valid']}"
        )

    def _emit(self, kind: str, payload=None) -> None:
        self.events.put(UiEvent(kind, payload))

    def _after_events(self) -> None:
        while True:
            try:
                event = self.events.get_nowait()
            except queue.Empty:
                break
            self._handle_event(event)
        self.after(80, self._after_events)

    def _handle_event(self, event: UiEvent) -> None:
        if event.kind == "log":
            self._log(str(event.payload))
        elif event.kind == "error":
            self._log(str(event.payload))
            self.result_var.set(str(event.payload))
            messagebox.showerror("操作失败", str(event.payload))
        elif event.kind == "result":
            self.result_var.set(str(event.payload))
            self._log(str(event.payload))
        elif event.kind == "busy":
            self._set_busy(bool(event.payload))
        elif event.kind == "progress":
            self.progress_var.set(float(event.payload))
        elif event.kind == "info":
            self.info_var.set(self._format_info(event.payload))
            self.comm_state_var.set("通信: 已连接")
        elif event.kind == "target":
            self.can_bitrate_var.set(str(event.payload.get("bitrate", self.active_can_bitrate)))
            self.node_id_var.set(str(event.payload.get("node_id", self.active_node_id)))
            self.app_can_addr_var.set(str(event.payload.get("app_can_addr", self.active_app_can_addr)))
            self.applied_target = (
                int(event.payload.get("bitrate", self.active_can_bitrate)),
                int(event.payload.get("node_id", self.active_node_id)),
                int(event.payload.get("app_can_addr", self.active_app_can_addr)),
            )
        elif event.kind == "cache":
            self.cache_var.set(self._format_cache(event.payload))
        elif event.kind == "image":
            self.image_var.set(str(event.payload))
        elif event.kind == "bms_info":
            self.bms_info_var.set(str(event.payload))
        elif event.kind == "bms_result":
            self.bms_result_var.set(str(event.payload))
        elif event.kind == "param_value":
            key, value = event.payload
            self._update_param_row(key, value)
            self.param_dirty.discard(key)
            self._update_param_dirty_label()
            param = BMS_PARAM_BY_KEY[key]
            display = _param_display_value(param, value)
            self.param_current_var.set(f"当前 {display} {param.unit}，原始值 {value}")
            if self._selected_param_key() == key:
                self.param_edit_var.set(display)
        elif event.kind == "param_values":
            for key, value in event.payload.items():
                self._update_param_row(key, value)
                self.param_dirty.discard(key)
            self._update_param_dirty_label()
            self.param_current_var.set(f"已读取 {len(event.payload)} 项")
            self._on_param_select()
        elif event.kind == "bms_log":
            self._update_log_tree(event.payload)
        elif event.kind == "live_snapshot":
            self._show_main_snapshot(event.payload)
        elif event.kind == "live_done":
            self._schedule_live_monitor()
        elif event.kind == "comm_state":
            self.comm_state_var.set(str(event.payload))
        elif event.kind == "upgrade_stage":
            self.upgrade_stage_var.set(str(event.payload))

    def _log(self, text: str) -> None:
        now = time.strftime("%H:%M:%S")
        self.log_text.configure(state="normal")
        self.log_text.insert(tk.END, f"[{now}] {text}\n")
        self.log_text.see(tk.END)
        self.log_text.configure(state="disabled")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="BMS CAN 升级图形上位机")
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--bin", default=str(DEFAULT_BIN))
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_test:
        require_pyserial()
        if len(SH309_PARAM_DEFS) != (SH309_AFE_PARAM_WORDS + SH309_TMOS_PARAM_WORDS):
            raise RuntimeError("SH309 参数定义数量不正确")
        for key in SH309_TMOS_DEFAULT_DISPLAY:
            if key not in BMS_PARAM_BY_KEY:
                raise RuntimeError(f"SH309 默认值键不存在: {key}")
        for key in ("SH309/单节过压(mv)", "SH309/MOS过温1(℃)", "SH309/延时(10ms)"):
            if key not in BMS_PARAM_BY_KEY:
                raise RuntimeError(f"SH309 UI 参数键不存在: {key}")
        root = tk.Tk()
        root.withdraw()
        root.destroy()
        print("comm_tool_upgrade_ui self-test OK")
        return 0
    app = UpgradeUi(args.port, args.baud, Path(args.bin))
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
