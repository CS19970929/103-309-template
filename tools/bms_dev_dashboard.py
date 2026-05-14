#!/usr/bin/env python3
"""Local Tkinter dashboard for BMS development workflows."""

from __future__ import annotations

import os
import queue
import subprocess
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk


ROOT = Path(__file__).resolve().parents[1]
LOG_DIR = ROOT / "logs" / "bms_watch"


class ProcessRunner:
    def __init__(self, output_queue: "queue.Queue[tuple[str, str]]") -> None:
        self.output_queue = output_queue
        self.process: subprocess.Popen[str] | None = None
        self.lock = threading.Lock()

    def running(self) -> bool:
        with self.lock:
            return self.process is not None and self.process.poll() is None

    def run(self, title: str, command: list[str]) -> None:
        if self.running():
            self.output_queue.put(("error", "已有任务在运行，请先停止或等待结束。\n"))
            return
        thread = threading.Thread(target=self._worker, args=(title, command), daemon=True)
        thread.start()

    def stop(self) -> None:
        with self.lock:
            proc = self.process
        if proc is not None and proc.poll() is None:
            self.output_queue.put(("info", "\n[stop] terminating process...\n"))
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()

    def _worker(self, title: str, command: list[str]) -> None:
        self.output_queue.put(("info", f"\n===== {title} =====\n"))
        self.output_queue.put(("cmd", " ".join(command) + "\n"))
        env = os.environ.copy()
        env.setdefault("PYTHONIOENCODING", "utf-8")
        proc = subprocess.Popen(
            command,
            cwd=str(ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=env,
        )
        with self.lock:
            self.process = proc
        try:
            assert proc.stdout is not None
            for line in proc.stdout:
                self.output_queue.put(("out", line))
            code = proc.wait()
            self.output_queue.put(("info" if code == 0 else "error", f"\n[exit] {code}\n"))
        finally:
            with self.lock:
                if self.process is proc:
                    self.process = None


class Dashboard(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("BMS 本地开发工作台")
        self.geometry("1120x720")
        self.minsize(900, 560)

        self.queue: "queue.Queue[tuple[str, str]]" = queue.Queue()
        self.runner = ProcessRunner(self.queue)

        self.target_var = tk.StringVar(value="FD_Release")
        self.watch_mode_var = tk.StringVar(value="quick")
        self.watch_interval_var = tk.StringVar(value="300")
        self.watch_count_var = tk.StringVar(value="0")
        self.flash_confirm_var = tk.BooleanVar(value=False)

        self._build_ui()
        self.after(100, self._drain_queue)

    def _build_ui(self) -> None:
        root = ttk.Frame(self, padding=12)
        root.pack(fill="both", expand=True)

        toolbar = ttk.Frame(root)
        toolbar.pack(fill="x")
        ttk.Label(toolbar, text="目标").pack(side="left")
        ttk.Combobox(toolbar, textvariable=self.target_var, values=("FD_Release", "FD_Debug"), width=12, state="readonly").pack(side="left", padx=(6, 14))
        ttk.Button(toolbar, text="打开日志目录", command=self.open_log_dir).pack(side="right")
        ttk.Button(toolbar, text="生成 AI 日志包", command=self.collect_ai_logs).pack(side="right", padx=8)
        ttk.Button(toolbar, text="停止任务", command=self.runner.stop).pack(side="right", padx=8)

        actions = ttk.LabelFrame(root, text="常用动作", padding=10)
        actions.pack(fill="x", pady=(12, 8))
        ttk.Button(actions, text="Quick 验证", command=self.run_quick).grid(row=0, column=0, padx=4, pady=4, sticky="ew")
        ttk.Button(actions, text="编译", command=self.run_build).grid(row=0, column=1, padx=4, pady=4, sticky="ew")
        ttk.Button(actions, text="ST-Link 快照", command=self.run_probe).grid(row=0, column=2, padx=4, pady=4, sticky="ew")
        ttk.Button(actions, text="SOC 深度断点", command=self.run_deep_probe).grid(row=0, column=3, padx=4, pady=4, sticky="ew")
        ttk.Checkbutton(actions, text="允许烧录 App", variable=self.flash_confirm_var).grid(row=0, column=4, padx=10, pady=4, sticky="w")
        ttk.Button(actions, text="安全烧录", command=self.run_flash).grid(row=0, column=5, padx=4, pady=4, sticky="ew")
        for col in range(6):
            actions.columnconfigure(col, weight=1)

        watch = ttk.LabelFrame(root, text="长期监控", padding=10)
        watch.pack(fill="x", pady=(0, 8))
        ttk.Label(watch, text="模式").grid(row=0, column=0, sticky="w")
        ttk.Combobox(watch, textvariable=self.watch_mode_var, values=("quick", "probe", "deep-probe"), width=12, state="readonly").grid(row=0, column=1, padx=6)
        ttk.Label(watch, text="间隔秒").grid(row=0, column=2, sticky="w")
        ttk.Entry(watch, textvariable=self.watch_interval_var, width=8).grid(row=0, column=3, padx=6)
        ttk.Label(watch, text="次数 0=一直跑").grid(row=0, column=4, sticky="w")
        ttk.Entry(watch, textvariable=self.watch_count_var, width=8).grid(row=0, column=5, padx=6)
        ttk.Button(watch, text="启动监控", command=self.run_watch).grid(row=0, column=6, padx=8)
        for col in range(7):
            watch.columnconfigure(col, weight=1 if col == 6 else 0)

        self.output = tk.Text(root, wrap="none", font=("Consolas", 10), height=24)
        self.output.pack(fill="both", expand=True)
        self.output.tag_config("error", foreground="#b91c1c")
        self.output.tag_config("cmd", foreground="#1d4ed8")
        self.output.tag_config("info", foreground="#166534")
        self.output.tag_config("out", foreground="#111827")

        ttk.Label(
            root,
            text="说明：除“安全烧录”外，按钮只运行本地测试、构建或只读调试；安全烧录固定走 tools\\soc_flash_app_safe.ps1，地址 0x08004800。",
            foreground="#475569",
        ).pack(fill="x", pady=(8, 0))

    def ps(self, *args: str) -> list[str]:
        return ["powershell", "-ExecutionPolicy", "Bypass", *args]

    def workflow(self, *args: str) -> list[str]:
        return self.ps("-File", "tools\\bms_dev_workflow.ps1", *args)

    def run_quick(self) -> None:
        self.runner.run("Quick 验证", self.workflow("-Mode", "quick", "-Target", self.target_var.get()))

    def run_build(self) -> None:
        self.runner.run("编译", self.workflow("-Mode", "build", "-Target", self.target_var.get()))

    def run_probe(self) -> None:
        self.runner.run("ST-Link 快照", self.workflow("-Mode", "probe", "-Target", self.target_var.get()))

    def run_deep_probe(self) -> None:
        self.runner.run("SOC 深度断点", self.workflow("-Mode", "probe", "-Target", self.target_var.get(), "-DeepProbe"))

    def run_flash(self) -> None:
        if not self.flash_confirm_var.get():
            messagebox.showwarning("未允许烧录", "先勾选“允许烧录 App”。")
            return
        if not messagebox.askyesno("确认安全烧录", "将通过安全脚本写 App 到 0x08004800，不会写 0x08000000。继续？"):
            return
        self.runner.run("安全烧录", self.workflow("-Mode", "flash", "-Target", self.target_var.get()))

    def run_watch(self) -> None:
        try:
            interval = str(max(10, int(self.watch_interval_var.get())))
            count = str(max(0, int(self.watch_count_var.get())))
        except ValueError:
            messagebox.showerror("参数错误", "间隔和次数必须是整数。")
            return
        command = self.ps(
            "-File", "tools\\bms_watch.ps1",
            "-Mode", self.watch_mode_var.get(),
            "-Target", self.target_var.get(),
            "-IntervalSeconds", interval,
            "-Count", count,
        )
        self.runner.run("长期监控", command)

    def open_log_dir(self) -> None:
        LOG_DIR.mkdir(parents=True, exist_ok=True)
        os.startfile(str(LOG_DIR))

    def collect_ai_logs(self) -> None:
        self.runner.run("生成 AI 日志包", self.ps("-File", "tools\\bms_collect_ai_logs.ps1"))

    def _drain_queue(self) -> None:
        try:
            while True:
                tag, text = self.queue.get_nowait()
                self.output.insert("end", text, tag)
                self.output.see("end")
        except queue.Empty:
            pass
        self.after(100, self._drain_queue)


def main() -> int:
    app = Dashboard()
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
