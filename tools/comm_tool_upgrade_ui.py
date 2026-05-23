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
DEFAULT_CHUNK_SIZE = 256
POST_UPGRADE_APP_READY_TIMEOUT = 15.0
POST_UPGRADE_APP_READY_INTERVAL = 1.0

BMS_OVERVIEW_ADDR = 0xD000
BMS_OVERVIEW_WORDS = 63
BMS_PARAM_PRESETS = {
    "单体过压一级阈值": 0x2100,
    "单体过压恢复阈值": 0x2103,
    "单体欠压一级阈值": 0x2105,
    "单体欠压恢复阈值": 0x2108,
    "充电过流一级阈值": 0x2114,
    "放电过流一级阈值": 0x2119,
    "充电高温一级阈值": 0x211E,
    "放电高温一级阈值": 0x2128,
    "MOS高温一级阈值": 0x2132,
    "压差保护一级阈值": 0x2137,
    "均衡开启电压": 0x2300,
    "均衡关闭压差": 0x2301,
    "额定容量": 0x2318,
    "串数": 0x231C,
    "采样电阻": 0x231D,
}


class UiEvent:
    def __init__(self, kind: str, payload=None):
        self.kind = kind
        self.payload = payload


def _temp_c(raw: int) -> float:
    return raw / 10.0 - 40.0


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
            port = self.parent.port_var.get().strip()
            baud = int(self.parent.baud_var.get().strip())
            words = _read_bms_words_once(port, baud, BMS_OVERVIEW_ADDR, BMS_OVERVIEW_WORDS)
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
        if not self.closed:
            self.after(80, self._after_events)

    def _show_snapshot(self, words: list[int]) -> None:
        cells = words[0:32]
        valid_cells = [value for value in cells if value != 0]
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
            f"单体 max {words[32]}mV({words[34]})   min {words[33]}mV({words[35]})   "
            f"压差 {words[36]}mV   有效串数 {len(valid_cells)}   "
            f"温度 max {_temp_c(words[48]):.1f}℃   min {_temp_c(words[49]):.1f}℃"
        )
        self.detail_var.set(
            f"容量: {capacity_now:.2f}/{capacity_full:.2f}Ah\n"
            f"出厂容量: {words[56] / 100.0:.2f}Ah\n"
            f"循环次数: {words[57]}\n"
            f"故障字: 0x{words[58]:04X}  0x{words[59]:04X}  0x{words[60]:04X}\n"
            f"均衡: 0x{words[61]:04X}  0x{words[62]:04X}"
        )

        for index, value in enumerate(cells):
            text = f"{index + 1:02d}: {value} mV" if value else f"{index + 1:02d}: --"
            self.cell_tree.item(f"cell{index}", values=(text,))
        for index, raw in enumerate(words[38:48]):
            self.temp_tree.item(f"temp{index}", values=(f"T{index + 1}: {_temp_c(raw):.1f}",))

    def _on_close(self) -> None:
        self.running = False
        self.closed = True
        self.parent.monitor_window = None
        self.destroy()


class UpgradeUi(tk.Tk):
    def __init__(self, port: str, baud: int, bin_path: Path):
        super().__init__()
        self.title("BMS comm tool 上位机")
        self.geometry("980x760")
        self.minsize(920, 700)

        self.events: "queue.Queue[UiEvent]" = queue.Queue()
        self.worker: threading.Thread | None = None

        self.port_var = tk.StringVar(value=port)
        self.baud_var = tk.StringVar(value=str(baud))
        self.bin_var = tk.StringVar(value=str(bin_path))
        self.info_var = tk.StringVar(value="未连接")
        self.cache_var = tk.StringVar(value="未读取")
        self.image_var = tk.StringVar(value="未选择")
        self.result_var = tk.StringVar(value="等待操作")
        self.bms_addr_var = tk.StringVar(value="0xD000")
        self.bms_count_var = tk.StringVar(value="2")
        self.bms_values_var = tk.StringVar(value="")
        self.bms_result_var = tk.StringVar(value="未读取")
        self.bms_info_var = tk.StringVar(value="未读取")
        self.param_key_var = tk.StringVar(value=next(iter(BMS_PARAM_PRESETS)))
        self.param_value_var = tk.StringVar(value="")
        self.param_current_var = tk.StringVar(value="未读取")
        self.progress_var = tk.DoubleVar(value=0.0)
        self.active_port = port
        self.active_baud = baud
        self.active_bin = bin_path
        self.active_bms_addr = 0xD000
        self.active_bms_count = 2
        self.active_bms_words: list[int] = []
        self.monitor_window: BmsMonitorWindow | None = None

        self._build_ui()
        self._refresh_ports()
        self._after_events()
        self._describe_selected_bin()

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=12)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(5, weight=1)

        conn = ttk.LabelFrame(root, text="连接")
        conn.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        conn.columnconfigure(1, weight=1)
        conn.columnconfigure(4, weight=1)

        ttk.Label(conn, text="串口").grid(row=0, column=0, padx=(10, 6), pady=10)
        self.port_combo = ttk.Combobox(conn, textvariable=self.port_var, width=18)
        self.port_combo.grid(row=0, column=1, sticky="w", pady=10)
        ttk.Button(conn, text="刷新", command=self._refresh_ports).grid(row=0, column=2, padx=6, pady=10)
        ttk.Label(conn, text="波特率").grid(row=0, column=3, padx=(18, 6), pady=10)
        ttk.Entry(conn, textvariable=self.baud_var, width=12).grid(row=0, column=4, sticky="w", pady=10)
        ttk.Button(conn, text="连接检测", command=self._check_connection).grid(row=0, column=5, padx=10, pady=10)

        file_box = ttk.LabelFrame(root, text="升级文件")
        file_box.grid(row=1, column=0, sticky="ew", pady=(0, 8))
        file_box.columnconfigure(1, weight=1)

        ttk.Label(file_box, text="BMS App bin").grid(row=0, column=0, padx=(10, 6), pady=10)
        ttk.Entry(file_box, textvariable=self.bin_var).grid(row=0, column=1, sticky="ew", pady=10)
        ttk.Button(file_box, text="选择", command=self._choose_bin).grid(row=0, column=2, padx=6, pady=10)
        ttk.Button(file_box, text="校验文件", command=self._describe_selected_bin).grid(row=0, column=3, padx=(0, 10), pady=10)

        status = ttk.Frame(root)
        status.grid(row=2, column=0, sticky="ew", pady=(0, 8))
        status.columnconfigure(0, weight=1)
        status.columnconfigure(1, weight=1)
        status.columnconfigure(2, weight=1)

        self._status_card(status, "comm tool", self.info_var, 0)
        self._status_card(status, "当前文件", self.image_var, 1)
        self._status_card(status, "comm tool 缓存", self.cache_var, 2)

        actions = ttk.Frame(root)
        actions.grid(row=3, column=0, sticky="ew", pady=(0, 8))
        actions.columnconfigure(5, weight=1)

        ttk.Button(actions, text="读取缓存", command=self._read_cache).grid(row=0, column=0, padx=(0, 8))
        ttk.Button(actions, text="写入缓存", command=self._download_only).grid(row=0, column=1, padx=(0, 8))
        ttk.Button(actions, text="一键升级", command=self._upgrade_selected).grid(row=0, column=2, padx=(0, 8))
        ttk.Button(actions, text="使用缓存升级", command=self._upgrade_cached_selected).grid(row=0, column=3, padx=(0, 8))
        ttk.Button(actions, text="读取BMS状态", command=self._read_bms_status).grid(row=0, column=4, padx=(0, 8))
        ttk.Button(actions, text="实时监控", command=self._open_monitor).grid(row=0, column=5, padx=(0, 8), sticky="w")
        ttk.Button(actions, text="CAN诊断", command=self._can_diag).grid(row=0, column=6, sticky="w")

        bms_box = ttk.LabelFrame(root, text="BMS 信息和参数")
        bms_box.grid(row=4, column=0, sticky="ew", pady=(0, 8))
        bms_box.columnconfigure(1, weight=1)
        bms_box.columnconfigure(6, weight=1)

        ttk.Button(bms_box, text="读取BMS信息", command=self._read_bms_overview).grid(
            row=0, column=0, padx=(10, 8), pady=10, sticky="nw"
        )
        ttk.Label(bms_box, textvariable=self.bms_info_var, justify=tk.LEFT).grid(
            row=0, column=1, columnspan=7, sticky="ew", padx=(0, 10), pady=10
        )

        ttk.Separator(bms_box).grid(row=1, column=0, columnspan=8, sticky="ew", padx=10, pady=(0, 8))
        ttk.Label(bms_box, text="常用参数").grid(row=2, column=0, padx=(10, 6), pady=(0, 8))
        self.param_combo = ttk.Combobox(
            bms_box,
            textvariable=self.param_key_var,
            values=list(BMS_PARAM_PRESETS.keys()),
            state="readonly",
            width=24,
        )
        self.param_combo.grid(row=2, column=1, sticky="w", pady=(0, 8))
        ttk.Button(bms_box, text="读取参数", command=self._read_selected_param).grid(row=2, column=2, padx=8, pady=(0, 8))
        ttk.Label(bms_box, textvariable=self.param_current_var).grid(row=2, column=3, sticky="w", padx=(0, 12), pady=(0, 8))
        ttk.Label(bms_box, text="新值").grid(row=2, column=4, padx=(8, 6), pady=(0, 8))
        ttk.Entry(bms_box, textvariable=self.param_value_var, width=12).grid(row=2, column=5, sticky="w", pady=(0, 8))
        ttk.Button(bms_box, text="写入参数", command=self._write_selected_param).grid(row=2, column=6, sticky="w", padx=8, pady=(0, 8))

        ttk.Separator(bms_box).grid(row=3, column=0, columnspan=8, sticky="ew", padx=10, pady=(0, 8))
        ttk.Label(bms_box, text="高级地址").grid(row=4, column=0, padx=(10, 6), pady=(0, 8))
        ttk.Entry(bms_box, textvariable=self.bms_addr_var, width=12).grid(row=4, column=1, sticky="w", pady=(0, 8))
        ttk.Label(bms_box, text="数量").grid(row=4, column=2, padx=(12, 6), pady=(0, 8))
        ttk.Entry(bms_box, textvariable=self.bms_count_var, width=8).grid(row=4, column=3, sticky="w", pady=(0, 8))
        ttk.Button(bms_box, text="读取", command=self._read_bms_regs).grid(row=4, column=4, padx=(12, 8), pady=(0, 8))
        ttk.Label(bms_box, text="写入值").grid(row=4, column=5, padx=(12, 6), pady=(0, 8))
        ttk.Entry(bms_box, textvariable=self.bms_values_var).grid(row=4, column=6, sticky="ew", pady=(0, 8))
        ttk.Button(bms_box, text="写入", command=self._write_bms_regs).grid(row=4, column=7, padx=(8, 10), pady=(0, 8))
        ttk.Label(bms_box, textvariable=self.bms_result_var, justify=tk.LEFT).grid(
            row=5, column=0, columnspan=8, sticky="ew", padx=10, pady=(0, 10)
        )

        progress = ttk.Frame(root)
        progress.grid(row=5, column=0, sticky="nsew")
        progress.columnconfigure(0, weight=1)
        progress.rowconfigure(2, weight=1)

        ttk.Label(progress, textvariable=self.result_var).grid(row=0, column=0, sticky="w")
        ttk.Progressbar(progress, variable=self.progress_var, maximum=100).grid(row=1, column=0, sticky="ew", pady=(6, 8))

        self.log_text = tk.Text(progress, height=18, wrap="word")
        self.log_text.grid(row=2, column=0, sticky="nsew")
        self.log_text.configure(state="disabled")

        scrollbar = ttk.Scrollbar(progress, orient=tk.VERTICAL, command=self.log_text.yview)
        scrollbar.grid(row=2, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=scrollbar.set)

    def _status_card(self, parent: ttk.Frame, title: str, var: tk.StringVar, column: int) -> None:
        frame = ttk.LabelFrame(parent, text=title)
        frame.grid(row=0, column=column, sticky="nsew", padx=(0 if column == 0 else 8, 0))
        frame.columnconfigure(0, weight=1)
        ttk.Label(frame, textvariable=var, justify=tk.LEFT).grid(row=0, column=0, sticky="ew", padx=10, pady=8)

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
        self._run_worker("读取BMS参数", self._worker_read_selected_param)

    def _write_selected_param(self) -> None:
        try:
            key = self.param_key_var.get()
            if key not in BMS_PARAM_PRESETS:
                raise ValueError("请选择有效的参数")
            value = self._parse_u16(self.param_value_var.get(), "新值")
        except Exception as exc:
            messagebox.showerror("参数错误", str(exc))
            return
        if not messagebox.askyesno(
            "确认写入参数",
            f"参数: {key}\n地址: 0x{BMS_PARAM_PRESETS[key]:04X}\n新值: {value} (0x{value:04X})\n\n"
            "确认写入 BMS？",
        ):
            return
        self._run_worker("写入BMS参数", self._worker_write_selected_param)

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
            "量产固件默认关闭写权限，若板端拒绝会返回错误。",
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
        except Exception as exc:
            messagebox.showerror("参数错误", str(exc))
            return
        self.progress_var.set(0)
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
        try:
            self._emit("log", f"开始: {name}")
            target()
            self._emit("result", f"{name} 完成")
        except Exception as exc:
            self._emit("error", f"{name} 失败: {exc}")
        finally:
            self._emit("busy", False)

    def _worker_check_connection(self) -> None:
        with self._open_client() as client:
            info = self._read_info(client)
            cache = self._read_cache_info(client)
        self._emit("info", info)
        self._emit("cache", cache)
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
            info = self._read_info(client)
            self._emit("info", info)
            self._download_image(client, image)
            cache = self._read_cache_info(client)
            self._assert_cache_matches(image, cache)
            self._emit("cache", cache)
            self._upgrade_bms_from_verified_cache(client)
        self._emit("progress", 100)

    def _worker_upgrade_cached_selected(self) -> None:
        image = self._load_selected_image()
        with self._open_client() as client:
            info = self._read_info(client)
            self._emit("info", info)
            cache = self._read_cache_info(client)
            self._assert_cache_matches(image, cache)
            self._emit("cache", cache)
            self._emit("log", "缓存与当前文件一致，跳过串口下载")
            self._upgrade_bms_from_verified_cache(client)
        self._emit("progress", 100)

    def _worker_read_bms_status(self) -> None:
        with self._open_client() as client:
            status = self._read_bms_app_status(client)
        self._emit("log", f"BMS App 状态: SOC={status[0]} SOH={status[1]}")
        self._emit("progress", 100)

    def _worker_read_bms_overview(self) -> None:
        with self._open_client() as client:
            words = self._read_bms_words(client, BMS_OVERVIEW_ADDR, BMS_OVERVIEW_WORDS)
        self._emit("bms_info", self._format_bms_overview(words))
        self._emit("log", "BMS 信息读取完成")
        self._emit("progress", 100)

    def _worker_read_selected_param(self) -> None:
        key = self.param_key_var.get()
        addr = BMS_PARAM_PRESETS[key]
        with self._open_client() as client:
            words = self._read_bms_words(client, addr, 1)
        value = words[0]
        self._emit("param_value", (value, addr))
        self._emit("log", f"参数读取完成: {key}=0x{value:04X} ({value})")
        self._emit("progress", 100)

    def _worker_write_selected_param(self) -> None:
        key = self.param_key_var.get()
        addr = BMS_PARAM_PRESETS[key]
        value = self._parse_u16(self.param_value_var.get(), "新值")
        with self._open_client() as client:
            self._write_bms_words(client, addr, [value])
            words = self._read_bms_words(client, addr, 1)
        self._emit("param_value", (words[0], addr))
        self._emit("log", f"参数写入完成: {key}=0x{words[0]:04X} ({words[0]})")
        self._emit("progress", 100)

    def _worker_read_bms_regs(self) -> None:
        addr = self.active_bms_addr
        count = self.active_bms_count
        with self._open_client() as client:
            words = self._read_bms_words(client, addr, count)
        text = self._format_bms_words(addr, words)
        self._emit("bms_result", text)
        self._emit("log", "BMS 寄存器读取完成")
        self._emit("progress", 100)

    def _worker_write_bms_regs(self) -> None:
        addr = self.active_bms_addr
        words = self.active_bms_words
        with self._open_client() as client:
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

    def _upgrade_bms_from_verified_cache(self, client: CommToolClient) -> None:
        self._emit("log", "缓存校验通过，开始 CAN 升级 BMS")
        client.command(CMD_CAN_DIAG, b"\x01", timeout=2.0)
        client.command(CMD_UPGRADE, timeout=DEFAULT_LONG_TIMEOUT)
        status = self._read_upgrade_status(client)
        self._emit("log", self._format_upgrade_status(status))
        if status["state"] != 2 or status["error"] != 0:
            raise RuntimeError(self._format_upgrade_status(status))
        bms_status = self._wait_bms_status_after_upgrade(client)
        if bms_status is None:
            self._emit("log", "升级已完成，但 BMS App 状态暂未响应；请稍后点击“读取BMS状态”复核。")
        else:
            self._emit("log", f"BMS App 状态: SOC={bms_status[0]} SOH={bms_status[1]}")

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

    def _download_image(self, client: CommToolClient, image: bytes) -> None:
        crc16 = crc16_modbus(image)
        crc32 = zlib.crc32(image) & 0xFFFFFFFF
        total = math.ceil(len(image) / DEFAULT_CHUNK_SIZE)
        client.command(CMD_FW_BEGIN, struct.pack("<IIHI", APP_BASE_ADDR, len(image), crc16, crc32), timeout=5.0)
        for index, offset in enumerate(range(0, len(image), DEFAULT_CHUNK_SIZE), start=1):
            chunk = image[offset : offset + DEFAULT_CHUNK_SIZE]
            client.command(CMD_FW_DATA, struct.pack("<I", offset) + chunk, timeout=5.0)
            if index == total or index % 4 == 0:
                self._emit("progress", min(90, int(index * 90 / total)))
                self._emit("log", f"写入缓存: {index}/{total}")
        client.command(CMD_FW_END, struct.pack("<IHI", len(image), crc16, crc32), timeout=5.0)

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
        valid_cells = [value for value in cells if value != 0]
        cell_text = " ".join(f"{index + 1}:{value}" for index, value in enumerate(cells) if value != 0)
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
            f"单体: max {words[32]}mV({words[34]})  min {words[33]}mV({words[35]})  "
            f"压差 {words[36]}mV  有效串数 {len(valid_cells)}\n"
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
        return (
            "CAN 诊断:\n"
            f"  tx={tx_count} ok={tx_ok} fail={tx_fail} timeout={tx_timeout}\n"
            f"  rx={rx_count} drop={rx_drop}\n"
            f"  ESR=0x{last_esr:08X} TSR=0x{last_tsr:08X} MSR=0x{last_msr:08X} RF0R=0x{last_rf0r:08X}\n"
            f"  last_tx=0x{last_tx_id:08X} last_rx=0x{last_rx_id:08X}"
        )

    def _format_info(self, info: dict) -> str:
        return (
            f"固件 {info['version']}\n"
            f"协议 {info['proto']}\n"
            f"CAN {info['bitrate']}\n"
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
        elif event.kind == "cache":
            self.cache_var.set(self._format_cache(event.payload))
        elif event.kind == "image":
            self.image_var.set(str(event.payload))
        elif event.kind == "bms_info":
            self.bms_info_var.set(str(event.payload))
        elif event.kind == "bms_result":
            self.bms_result_var.set(str(event.payload))
        elif event.kind == "param_value":
            value, addr = event.payload
            self.param_current_var.set(f"当前 0x{addr:04X}=0x{value:04X} ({value})")
            self.param_value_var.set(str(value))

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
