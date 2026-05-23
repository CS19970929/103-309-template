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


class UiEvent:
    def __init__(self, kind: str, payload=None):
        self.kind = kind
        self.payload = payload


class UpgradeUi(tk.Tk):
    def __init__(self, port: str, baud: int, bin_path: Path):
        super().__init__()
        self.title("BMS CAN 升级工具")
        self.geometry("920x680")
        self.minsize(860, 620)

        self.events: "queue.Queue[UiEvent]" = queue.Queue()
        self.worker: threading.Thread | None = None

        self.port_var = tk.StringVar(value=port)
        self.baud_var = tk.StringVar(value=str(baud))
        self.bin_var = tk.StringVar(value=str(bin_path))
        self.info_var = tk.StringVar(value="未连接")
        self.cache_var = tk.StringVar(value="未读取")
        self.image_var = tk.StringVar(value="未选择")
        self.result_var = tk.StringVar(value="等待操作")
        self.progress_var = tk.DoubleVar(value=0.0)
        self.active_port = port
        self.active_baud = baud
        self.active_bin = bin_path

        self._build_ui()
        self._refresh_ports()
        self._after_events()
        self._describe_selected_bin()

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=12)
        root.pack(fill=tk.BOTH, expand=True)
        root.columnconfigure(0, weight=1)
        root.rowconfigure(4, weight=1)

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
        ttk.Button(actions, text="CAN诊断", command=self._can_diag).grid(row=0, column=5, sticky="w")

        progress = ttk.Frame(root)
        progress.grid(row=4, column=0, sticky="nsew")
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
        for child in self.winfo_children():
            self._set_child_state(child, state)
        if busy:
            self.result_var.set("正在执行...")

    def _set_child_state(self, widget, state: str) -> None:
        for child in widget.winfo_children():
            if isinstance(child, (ttk.Button, ttk.Entry, ttk.Combobox)):
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

    def _can_diag(self) -> None:
        self._run_worker("CAN诊断", self._worker_can_diag)

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
            status = self._read_bms_status(client)
        self._emit("log", f"BMS App 状态: SOC={status[0]} SOH={status[1]}")
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

    def _read_bms_status(self, client: CommToolClient) -> tuple[int, int]:
        payload = struct.pack("<HH", 0xD000, 2)
        resp = client.command(CMD_BMS_READ, payload, timeout=10.0)
        if len(resp.payload) < 4:
            raise RuntimeError("BMS_READ 响应长度不足")
        return struct.unpack_from("<HH", resp.payload, 0)

    def _wait_bms_status_after_upgrade(self, client: CommToolClient) -> Optional[tuple[int, int]]:
        deadline = time.monotonic() + POST_UPGRADE_APP_READY_TIMEOUT
        attempt = 0
        last_error = ""

        time.sleep(POST_UPGRADE_APP_READY_INTERVAL)
        while time.monotonic() < deadline:
            attempt += 1
            try:
                status = self._read_bms_status(client)
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
