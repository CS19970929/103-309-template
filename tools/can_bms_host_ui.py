#!/usr/bin/env python3
"""User-facing GUI for BMS CAN status, SOC write, and aging control."""

from __future__ import annotations

import argparse
import queue
import sys
import threading
import time
from pathlib import Path
from typing import Callable

import tkinter as tk
from tkinter import messagebox, ttk

import can_bms_host as host


APP_TITLE = "BMS CAN 上位机"
DEFAULT_BITRATE = 250000
DEFAULT_TIMEOUT = 1.0
DEFAULT_ACK_TIMEOUT = 2.0


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


class CanBmsHostUi:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("980x660")
        self.root.minsize(900, 580)
        self.repo_root = resolve_repo_root()
        self.log_queue: queue.Queue[str] = queue.Queue()
        self.listen_stop = threading.Event()
        self.listen_thread: threading.Thread | None = None

        self.interface_var = tk.StringVar(value="pcan")
        self.channel_var = tk.StringVar(value="PCAN_USBBUS1")
        self.bitrate_var = tk.StringVar(value=str(DEFAULT_BITRATE))
        self.can_address_var = tk.StringVar(value="0")
        self.timeout_var = tk.StringVar(value=str(DEFAULT_TIMEOUT))
        self.ack_timeout_var = tk.StringVar(value=str(DEFAULT_ACK_TIMEOUT))
        self.soc_var = tk.StringVar(value="80")
        self.status_var = tk.StringVar(value="未连接")

        self._build_ui()
        self.root.after(100, self._drain_log_queue)

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=10)
        outer.pack(fill=tk.BOTH, expand=True)

        conn = ttk.LabelFrame(outer, text="CAN 连接")
        conn.pack(fill=tk.X)
        conn.columnconfigure(1, weight=1)
        conn.columnconfigure(3, weight=1)

        ttk.Label(conn, text="接口").grid(row=0, column=0, padx=6, pady=6, sticky=tk.W)
        ttk.Combobox(
            conn,
            textvariable=self.interface_var,
            values=("pcan", "kvaser", "slcan", "virtual"),
            width=12,
        ).grid(row=0, column=1, padx=6, pady=6, sticky=tk.EW)
        ttk.Label(conn, text="通道").grid(row=0, column=2, padx=6, pady=6, sticky=tk.W)
        ttk.Entry(conn, textvariable=self.channel_var, width=22).grid(row=0, column=3, padx=6, pady=6, sticky=tk.EW)
        ttk.Label(conn, text="波特率").grid(row=0, column=4, padx=6, pady=6, sticky=tk.W)
        ttk.Entry(conn, textvariable=self.bitrate_var, width=10).grid(row=0, column=5, padx=6, pady=6)

        ttk.Label(conn, text="CAN 地址").grid(row=1, column=0, padx=6, pady=6, sticky=tk.W)
        ttk.Entry(conn, textvariable=self.can_address_var, width=10).grid(row=1, column=1, padx=6, pady=6, sticky=tk.W)
        ttk.Label(conn, text="收发超时").grid(row=1, column=2, padx=6, pady=6, sticky=tk.W)
        ttk.Entry(conn, textvariable=self.timeout_var, width=10).grid(row=1, column=3, padx=6, pady=6, sticky=tk.W)
        ttk.Label(conn, text="ACK 超时").grid(row=1, column=4, padx=6, pady=6, sticky=tk.W)
        ttk.Entry(conn, textvariable=self.ack_timeout_var, width=10).grid(row=1, column=5, padx=6, pady=6)

        actions = ttk.LabelFrame(outer, text="常用功能")
        actions.pack(fill=tk.X, pady=(10, 0))
        actions.columnconfigure(1, weight=1)

        ttk.Button(actions, text="检测环境", command=self.detect_environment).grid(row=0, column=0, padx=6, pady=8, sticky=tk.EW)
        ttk.Button(actions, text="读取 SOC/SOH", command=self.read_status).grid(row=0, column=1, padx=6, pady=8, sticky=tk.EW)

        ttk.Label(actions, text="写 SOC").grid(row=0, column=2, padx=6, pady=8, sticky=tk.E)
        ttk.Entry(actions, textvariable=self.soc_var, width=8).grid(row=0, column=3, padx=6, pady=8)
        ttk.Button(actions, text="写入 SOC", command=self.write_soc).grid(row=0, column=4, padx=6, pady=8, sticky=tk.EW)

        ttk.Button(actions, text="开启老化模式", command=self.aging_start).grid(row=1, column=0, padx=6, pady=8, sticky=tk.EW)
        ttk.Button(actions, text="关闭老化模式", command=self.aging_stop).grid(row=1, column=1, padx=6, pady=8, sticky=tk.EW)
        ttk.Button(actions, text="重置老化时间", command=self.aging_reset_time).grid(row=1, column=2, padx=6, pady=8, sticky=tk.EW)
        ttk.Button(actions, text="开始监听", command=self.start_listen).grid(row=1, column=3, padx=6, pady=8, sticky=tk.EW)
        ttk.Button(actions, text="停止监听", command=self.stop_listen).grid(row=1, column=4, padx=6, pady=8, sticky=tk.EW)

        ttk.Label(outer, textvariable=self.status_var).pack(anchor=tk.W, pady=(8, 4))

        log_frame = ttk.LabelFrame(outer, text="运行日志")
        log_frame.pack(fill=tk.BOTH, expand=True)
        log_frame.rowconfigure(0, weight=1)
        log_frame.columnconfigure(0, weight=1)
        self.log_text = tk.Text(log_frame, wrap=tk.NONE, height=18)
        y_scroll = ttk.Scrollbar(log_frame, orient=tk.VERTICAL, command=self.log_text.yview)
        x_scroll = ttk.Scrollbar(log_frame, orient=tk.HORIZONTAL, command=self.log_text.xview)
        self.log_text.configure(yscrollcommand=y_scroll.set, xscrollcommand=x_scroll.set)
        self.log_text.grid(row=0, column=0, sticky=tk.NSEW)
        y_scroll.grid(row=0, column=1, sticky=tk.NS)
        x_scroll.grid(row=1, column=0, sticky=tk.EW)

        footer = ttk.Frame(outer)
        footer.pack(fill=tk.X, pady=(8, 0))
        ttk.Button(footer, text="清空日志", command=self.clear_log).pack(side=tk.RIGHT)
        ttk.Label(footer, text=f"仓库: {self.repo_root}").pack(side=tk.LEFT)

    def _make_args(self) -> argparse.Namespace:
        return argparse.Namespace(
            interface=self.interface_var.get().strip(),
            channel=self.channel_var.get().strip(),
            bitrate=int(self.bitrate_var.get().strip(), 0),
            can_address=int(self.can_address_var.get().strip(), 0),
            timeout=float(self.timeout_var.get().strip()),
            ack_timeout=float(self.ack_timeout_var.get().strip()),
        )

    def _log(self, text: str) -> None:
        self.log_queue.put(f"{time.strftime('%H:%M:%S')} {text}")

    def _drain_log_queue(self) -> None:
        while True:
            try:
                line = self.log_queue.get_nowait()
            except queue.Empty:
                break
            self.log_text.insert(tk.END, line + "\n")
            self.log_text.see(tk.END)
        self.root.after(100, self._drain_log_queue)

    def clear_log(self) -> None:
        self.log_text.delete("1.0", tk.END)

    def _run_worker(self, name: str, func: Callable[[], None]) -> None:
        def worker() -> None:
            self._log(f"{name} 开始")
            try:
                func()
            except SystemExit as exc:
                self._log(f"{name} 失败: {exc}")
                self.root.after(0, lambda: messagebox.showerror(name, str(exc)))
            except Exception as exc:
                self._log(f"{name} 失败: {exc}")
                self.root.after(0, lambda: messagebox.showerror(name, str(exc)))
            else:
                self._log(f"{name} 完成")

        threading.Thread(target=worker, daemon=True).start()

    def detect_environment(self) -> None:
        def action() -> None:
            can = host.require_python_can()
            self._log(f"python-can: {getattr(can, '__version__', 'unknown')}")
            self._log(f"Python: {sys.executable}")
            try:
                configs = can.detect_available_configs()
            except Exception as exc:
                self._log(f"CAN 适配器探测失败: {exc}")
                return
            if not configs:
                self._log("未自动探测到 CAN 适配器，请检查驱动和接口/通道配置")
                return
            for item in configs:
                self._log(f"探测到: {item}")

        self._run_worker("检测环境", action)

    def read_status(self) -> None:
        def action() -> None:
            args = self._make_args()
            payload = host.build_app_command(host.CAN_APP_CMD_GET_STATUS)
            data = host.send_app_command(args, host.CAN_APP_CMD_GET_STATUS, payload)
            _cmd, _status, soc, soh = host.validate_app_ack(data)
            text = f"SOC={soc}% SOH={soh}%"
            self.status_var.set(text)
            self._log(text)

        self._run_worker("读取 SOC/SOH", action)

    def write_soc(self) -> None:
        try:
            soc = int(self.soc_var.get().strip(), 0)
        except ValueError:
            messagebox.showerror("写入 SOC", "SOC 必须是 0..100 的整数")
            return
        if soc < 0 or soc > 100:
            messagebox.showerror("写入 SOC", "SOC 必须是 0..100 的整数")
            return
        if not messagebox.askyesno("确认写入 SOC", f"确认写入一次 SOC={soc}%？"):
            return

        def action() -> None:
            args = self._make_args()
            data = host.send_app_write_word(args, host.APP_SET_ONCE_SOC_ADDR, soc)
            host.validate_app_ack(data)
            self._log(f"已写入 SOC={soc}%")

        self._run_worker("写入 SOC", action)

    def _aging_command(self, name: str, cmd: int, action_code: int) -> None:
        if not messagebox.askyesno(name, f"确认{name}？"):
            return

        def action() -> None:
            args = self._make_args()
            payload = host.build_app_command(cmd, host.CAN_APP_AGING_GUARD, action_code, args.can_address)
            data = host.send_app_command(args, cmd, payload)
            _cmd, _status, state, remaining_hours = host.validate_app_ack(data)
            text = f"{name}: 老化状态={host.aging_state_name(state)} 剩余约={remaining_hours}h"
            self.status_var.set(text)
            self._log(text)

        self._run_worker(name, action)

    def aging_start(self) -> None:
        self._aging_command("开启老化模式", host.CAN_APP_CMD_AGING_START, host.CAN_APP_AGING_ACTION_START)

    def aging_stop(self) -> None:
        self._aging_command("关闭老化模式", host.CAN_APP_CMD_AGING_STOP, host.CAN_APP_AGING_ACTION_STOP)

    def aging_reset_time(self) -> None:
        self._aging_command("重置老化时间", host.CAN_APP_CMD_AGING_RESET_TIME, host.CAN_APP_AGING_ACTION_RESET_TIME)

    def start_listen(self) -> None:
        if self.listen_thread is not None and self.listen_thread.is_alive():
            self._log("监听已经在运行")
            return
        self.listen_stop.clear()
        self.listen_thread = threading.Thread(target=self._listen_loop, daemon=True)
        self.listen_thread.start()

    def stop_listen(self) -> None:
        self.listen_stop.set()
        self._log("已请求停止监听")

    def _listen_loop(self) -> None:
        try:
            args = self._make_args()
            bus = host.open_bus(args.interface, args.channel, args.bitrate)
        except Exception as exc:
            self._log(f"监听启动失败: {exc}")
            self.root.after(0, lambda: messagebox.showerror("开始监听", str(exc)))
            return

        self._log(f"开始监听 CAN: interface={args.interface} channel={args.channel} bitrate={args.bitrate}")
        try:
            while not self.listen_stop.is_set():
                msg = bus.recv(timeout=0.2)
                if msg is None:
                    continue
                data = bytes(msg.data)
                frame_type = "EXT" if msg.is_extended_id else "STD"
                line = f"{frame_type} 0x{msg.arbitration_id:08X} [{msg.dlc}] {host.format_bytes(data)}"
                decoded = host.decode_feidao_broadcast(msg.arbitration_id, data) if msg.is_extended_id else None
                if decoded:
                    line += f" | {decoded}"
                self._log(line)
        finally:
            shutdown = getattr(bus, "shutdown", None)
            if callable(shutdown):
                shutdown()
            self._log("CAN 监听已停止")


def main() -> int:
    root = tk.Tk()
    ttk.Style().theme_use("clam")
    CanBmsHostUi(root)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
