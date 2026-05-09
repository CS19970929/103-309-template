#!/usr/bin/env python3
"""Tkinter UI for SOC simulation and online board monitoring."""

from __future__ import annotations

import csv
import queue
import threading
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import soc_ride_sim_report as sim
from soc_online_monitor import read_soc_params, read_soc_test_status, read_status, run_mcu_soc_test_sample, set_soc_once


ROOT = Path(__file__).resolve().parents[1]
REPORT_PATH = ROOT / "SOC_RIDE_SIM_REPORT.md"
SIM_CSV_PATH = ROOT / "SOC_RIDE_SIM_SAMPLES.csv"
ONLINE_CSV_PATH = ROOT / "SOC_ONLINE_MONITOR.csv"


class LineChart(tk.Canvas):
    def __init__(self, master, height=260, **kwargs):
        super().__init__(master, height=height, bg="#ffffff", highlightthickness=1,
                         highlightbackground="#cbd5e1", **kwargs)
        self.rows: list[dict[str, object]] = []
        self.series: list[tuple[str, str, str]] = []
        self.y_label = ""

    def set_data(self, rows: list[dict[str, object]], series: list[tuple[str, str, str]], y_label: str) -> None:
        self.rows = rows
        self.series = series
        self.y_label = y_label
        self.redraw()

    def redraw(self) -> None:
        self.delete("all")
        width = max(300, self.winfo_width())
        height = max(200, self.winfo_height())
        pad_l, pad_r, pad_t, pad_b = 52, 20, 18, 36
        x0, y0 = pad_l, height - pad_b
        x1, y1 = width - pad_r, pad_t
        self.create_rectangle(x0, y1, x1, y0, outline="#e2e8f0")
        self.create_text(8, y1, anchor="nw", text=self.y_label, fill="#475569", font=("Segoe UI", 9))
        if not self.rows or not self.series:
            self.create_text(width // 2, height // 2, text="暂无曲线数据", fill="#64748b")
            return

        times = [float(row.get("time_s", 0)) for row in self.rows]
        max_t = max(times) if times else 1.0
        min_t = min(times) if times else 0.0
        if max_t <= min_t:
            max_t = min_t + 1.0
        values: list[float] = []
        for key, _, _ in self.series:
            values.extend(float(row.get(key, 0)) for row in self.rows)
        min_v = min(values)
        max_v = max(values)
        if max_v <= min_v:
            max_v = min_v + 1.0
        margin = (max_v - min_v) * 0.08
        min_v -= margin
        max_v += margin

        for i in range(5):
            frac = i / 4
            y = y0 - frac * (y0 - y1)
            value = min_v + frac * (max_v - min_v)
            self.create_line(x0, y, x1, y, fill="#f1f5f9")
            self.create_text(x0 - 8, y, text=f"{value:.0f}", anchor="e", fill="#64748b", font=("Segoe UI", 8))
        for i in range(5):
            frac = i / 4
            x = x0 + frac * (x1 - x0)
            value = min_t + frac * (max_t - min_t)
            self.create_line(x, y0, x, y1, fill="#f8fafc")
            self.create_text(x, y0 + 14, text=f"{value:.0f}s", anchor="n", fill="#64748b", font=("Segoe UI", 8))

        for key, label, color in self.series:
            points: list[float] = []
            for row in self.rows:
                tx = float(row.get("time_s", 0))
                val = float(row.get(key, 0))
                x = x0 + (tx - min_t) * (x1 - x0) / (max_t - min_t)
                y = y0 - (val - min_v) * (y0 - y1) / (max_v - min_v)
                points.extend((x, y))
            if len(points) >= 4:
                self.create_line(*points, fill=color, width=2)
        lx = x0
        for _, label, color in self.series:
            self.create_line(lx, y1 - 8, lx + 18, y1 - 8, fill=color, width=3)
            self.create_text(lx + 24, y1 - 8, text=label, anchor="w", fill="#334155", font=("Segoe UI", 9))
            lx += 120


class SocTestUi(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("BMS SOC 自动化测试上位机")
        self.geometry("1180x760")
        self.minsize(980, 640)
        self.queue: queue.Queue[tuple[str, object]] = queue.Queue()
        self.online_stop = threading.Event()
        self.online_rows: list[dict[str, object]] = []
        self.sim_rows: list[dict[str, object]] = []
        self.mcu_rows: list[dict[str, object]] = []
        self._build_ui()
        self.after(100, self._poll_queue)

    def _build_ui(self) -> None:
        style = ttk.Style()
        style.configure("TButton", padding=(10, 5))
        style.configure("TLabel", font=("Segoe UI", 10))
        style.configure("Header.TLabel", font=("Segoe UI", 13, "bold"))

        root = ttk.Frame(self, padding=10)
        root.pack(fill="both", expand=True)
        header = ttk.Frame(root)
        header.pack(fill="x")
        ttk.Label(header, text="BMS SOC 自动化测试上位机", style="Header.TLabel").pack(side="left")
        self.status_var = tk.StringVar(value="就绪")
        ttk.Label(header, textvariable=self.status_var).pack(side="right")

        self.tabs = ttk.Notebook(root)
        self.tabs.pack(fill="both", expand=True, pady=(10, 0))
        self._build_sim_tab()
        self._build_mcu_test_tab()
        self._build_online_tab()
        self._build_report_tab()
        self._build_log_tab()

    def _build_sim_tab(self) -> None:
        tab = ttk.Frame(self.tabs, padding=10)
        self.tabs.add(tab, text="真实场景仿真")
        controls = ttk.Frame(tab)
        controls.pack(fill="x")
        ttk.Button(controls, text="运行全部场景", command=self.run_simulation).pack(side="left")
        ttk.Button(controls, text="导出报告", command=self.save_report_as).pack(side="left", padx=6)
        ttk.Label(controls, text="场景：城市骑行 / 爬坡 / 低压截止 / 充电回满，加速执行").pack(side="left", padx=12)

        body = ttk.Panedwindow(tab, orient="horizontal")
        body.pack(fill="both", expand=True, pady=(10, 0))
        left = ttk.Frame(body)
        right = ttk.Frame(body)
        body.add(left, weight=1)
        body.add(right, weight=2)

        columns = ("scenario", "duration", "true", "soc", "display", "error", "vmin", "current", "result")
        self.sim_tree = ttk.Treeview(left, columns=columns, show="headings", height=14)
        headings = {
            "scenario": "工况",
            "duration": "时长",
            "true": "真实SOC",
            "soc": "算法SOC",
            "display": "显示SOC",
            "error": "最大误差",
            "vmin": "最低Vmin",
            "current": "最大电流",
            "result": "结果",
        }
        widths = {"scenario": 120, "duration": 64, "true": 92, "soc": 84, "display": 72, "error": 70, "vmin": 72, "current": 74, "result": 60}
        for col in columns:
            self.sim_tree.heading(col, text=headings[col])
            self.sim_tree.column(col, width=widths[col], anchor="center")
        self.sim_tree.pack(fill="both", expand=True)

        self.soc_chart = LineChart(right, height=310)
        self.soc_chart.pack(fill="both", expand=True)
        self.volt_chart = LineChart(right, height=220)
        self.volt_chart.pack(fill="both", expand=True, pady=(8, 0))
        self.soc_chart.bind("<Configure>", lambda _e: self.soc_chart.redraw())
        self.volt_chart.bind("<Configure>", lambda _e: self.volt_chart.redraw())

    def _build_mcu_test_tab(self) -> None:
        tab = ttk.Frame(self.tabs, padding=10)
        self.tabs.add(tab, text="MCU加速测试")
        controls = ttk.Frame(tab)
        controls.pack(fill="x")
        ttk.Button(controls, text="读取测试状态", command=self.read_mcu_test_status).pack(side="left", padx=4)
        ttk.Button(controls, text="运行MCU真实骑行", command=self.run_mcu_ride_test).pack(side="left", padx=4)
        ttk.Button(controls, text="运行快变电流", command=self.run_mcu_pulse_test).pack(side="left", padx=4)
        ttk.Button(controls, text="关闭测试模式", command=self.disable_mcu_test_mode).pack(side="left", padx=4)
        ttk.Label(
            controls,
            text="测试固件专用：量产固件默认会拒绝0x2500写入，D300状态supported=0。",
        ).pack(side="left", padx=12)

        self.mcu_status_var = tk.StringVar(value="MCU测试状态：未读取")
        ttk.Label(tab, textvariable=self.mcu_status_var).pack(anchor="w", pady=(8, 0))

        columns = ("step", "scenario", "soc", "vmin", "vmax", "ichg", "idsg", "ticks", "result")
        self.mcu_tree = ttk.Treeview(tab, columns=columns, show="headings", height=12)
        headings = {
            "step": "step",
            "scenario": "scenario",
            "soc": "SOC",
            "vmin": "Vmin",
            "vmax": "Vmax",
            "ichg": "Ichg",
            "idsg": "Idsg",
            "ticks": "ticks",
            "result": "result",
        }
        for col in columns:
            self.mcu_tree.heading(col, text=headings[col])
            self.mcu_tree.column(col, width=92, anchor="center")
        self.mcu_tree.pack(fill="x", pady=(10, 8))

        self.mcu_chart = LineChart(tab, height=360)
        self.mcu_chart.pack(fill="both", expand=True)
        self.mcu_chart.bind("<Configure>", lambda _e: self.mcu_chart.redraw())

    def _build_online_tab(self) -> None:
        tab = ttk.Frame(self.tabs, padding=10)
        self.tabs.add(tab, text="在线板端监控")
        controls = ttk.Frame(tab)
        controls.pack(fill="x")
        self.port_var = tk.StringVar(value="COM4")
        self.baud_var = tk.StringVar(value="19200")
        self.slave_var = tk.StringVar(value="1")
        self.interval_var = tk.StringVar(value="1.0")
        self.samples_var = tk.StringVar(value="60")
        self.set_soc_var = tk.StringVar(value="80")
        for label, var, width in (
            ("串口", self.port_var, 8),
            ("波特率", self.baud_var, 8),
            ("地址", self.slave_var, 4),
            ("间隔(s)", self.interval_var, 6),
            ("样本", self.samples_var, 6),
        ):
            ttk.Label(controls, text=label).pack(side="left", padx=(0, 4))
            ttk.Entry(controls, textvariable=var, width=width).pack(side="left", padx=(0, 8))
        ttk.Button(controls, text="扫描串口", command=self.scan_ports).pack(side="left", padx=4)
        ttk.Button(controls, text="开始监控", command=self.start_online).pack(side="left", padx=4)
        ttk.Button(controls, text="停止", command=self.stop_online).pack(side="left", padx=4)
        ttk.Button(controls, text="读SOC参数", command=self.read_board_params).pack(side="left", padx=4)
        ttk.Label(controls, text="设置SOC").pack(side="left", padx=(12, 4))
        ttk.Entry(controls, textvariable=self.set_soc_var, width=4).pack(side="left")
        ttk.Button(controls, text="写入0x1005", command=self.write_set_soc_once).pack(side="left", padx=4)
        ttk.Button(controls, text="保存CSV", command=self.save_online_as).pack(side="left", padx=4)

        self.board_param_var = tk.StringVar(value="SOC参数：未读取")
        ttk.Label(tab, textvariable=self.board_param_var).pack(anchor="w", pady=(8, 0))

        columns = ("time", "soc", "soh", "vmin", "vmax", "ichg", "idsg", "cap", "fault")
        self.online_tree = ttk.Treeview(tab, columns=columns, show="headings", height=10)
        headings = {
            "time": "时间(s)",
            "soc": "SOC",
            "soh": "SOH",
            "vmin": "Vmin",
            "vmax": "Vmax",
            "ichg": "Ichg",
            "idsg": "Idsg",
            "cap": "容量",
            "fault": "故障",
        }
        for col in columns:
            self.online_tree.heading(col, text=headings[col])
            self.online_tree.column(col, width=90, anchor="center")
        self.online_tree.pack(fill="x", pady=(10, 8))
        self.online_chart = LineChart(tab, height=330)
        self.online_chart.pack(fill="both", expand=True)
        self.online_chart.bind("<Configure>", lambda _e: self.online_chart.redraw())

    def _build_report_tab(self) -> None:
        tab = ttk.Frame(self.tabs, padding=10)
        self.tabs.add(tab, text="报告")
        controls = ttk.Frame(tab)
        controls.pack(fill="x")
        ttk.Button(controls, text="刷新报告", command=self.load_report).pack(side="left")
        ttk.Button(controls, text="另存报告", command=self.save_report_as).pack(side="left", padx=6)
        self.report_text = tk.Text(tab, wrap="word", font=("Consolas", 10))
        self.report_text.pack(fill="both", expand=True, pady=(10, 0))
        self.load_report()

    def _build_log_tab(self) -> None:
        tab = ttk.Frame(self.tabs, padding=10)
        self.tabs.add(tab, text="日志")
        self.log_text = tk.Text(tab, wrap="word", font=("Consolas", 10), height=12)
        self.log_text.pack(fill="both", expand=True)

    def log(self, message: str) -> None:
        self.log_text.insert("end", time.strftime("[%H:%M:%S] ") + message + "\n")
        self.log_text.see("end")

    def set_status(self, text: str) -> None:
        self.status_var.set(text)
        self.log(text)

    def run_simulation(self) -> None:
        def worker() -> None:
            try:
                rows: list[dict[str, object]] = []
                results = [sim.run_scenario(name, rows) for name in sim.SCENARIOS]
                sim.write_csv(SIM_CSV_PATH, rows)
                sim.write_report(REPORT_PATH, results, SIM_CSV_PATH)
                self.queue.put(("sim_done", (results, rows)))
            except Exception as exc:
                self.queue.put(("error", f"仿真失败：{exc}"))
        self.set_status("正在运行真实场景仿真...")
        threading.Thread(target=worker, daemon=True).start()

    def start_online(self) -> None:
        if self.online_stop.is_set():
            self.online_stop.clear()
        self.online_rows = []
        for item in self.online_tree.get_children():
            self.online_tree.delete(item)
        try:
            port = self.port_var.get().strip()
            baud = int(self.baud_var.get())
            slave = int(self.slave_var.get())
            interval = float(self.interval_var.get())
            samples = int(self.samples_var.get())
        except ValueError:
            messagebox.showerror("参数错误", "串口参数必须是有效数字")
            return

        def worker() -> None:
            try:
                import serial  # type: ignore
                with serial.Serial(port, baud, timeout=max(0.2, interval)) as ser:
                    start = time.time()
                    for index in range(samples):
                        if self.online_stop.is_set():
                            break
                        row = read_status(ser, slave)
                        row["sample"] = index
                        row["time_s"] = round(time.time() - start, 2)
                        self.queue.put(("online_row", row))
                        if index + 1 < samples:
                            time.sleep(interval)
                self.queue.put(("online_done", None))
            except Exception as exc:
                self.queue.put(("error", f"在线监控失败：{exc}"))

        self.set_status(f"正在监控 {port}@{baud} ...")
        threading.Thread(target=worker, daemon=True).start()

    def stop_online(self) -> None:
        self.online_stop.set()
        self.set_status("已请求停止在线监控")

    def _open_serial(self):
        import serial  # type: ignore
        return serial.Serial(
            self.port_var.get().strip(),
            int(self.baud_var.get()),
            timeout=max(0.5, float(self.interval_var.get())),
        )

    def read_board_params(self) -> None:
        def worker() -> None:
            try:
                with self._open_serial() as ser:
                    params = read_soc_params(ser, int(self.slave_var.get()))
                self.queue.put(("params_done", params))
            except Exception as exc:
                self.queue.put(("error", f"读取 SOC 参数失败：{exc}"))
        self.set_status("正在读取板端 SOC 参数...")
        threading.Thread(target=worker, daemon=True).start()

    def read_mcu_test_status(self) -> None:
        def worker() -> None:
            try:
                with self._open_serial() as ser:
                    status = read_soc_test_status(ser, int(self.slave_var.get()))
                self.queue.put(("mcu_status", status))
            except Exception as exc:
                self.queue.put(("error", f"读取MCU测试状态失败：{exc}"))

        self.set_status("正在读取MCU SOC测试状态...")
        threading.Thread(target=worker, daemon=True).start()

    def _run_mcu_scenario_names(self, scenario_names: list[str]) -> None:
        self.mcu_rows = []
        for item in self.mcu_tree.get_children():
            self.mcu_tree.delete(item)

        def worker() -> None:
            try:
                rows: list[dict[str, object]] = []
                with self._open_serial() as ser:
                    slave = int(self.slave_var.get())
                    status = read_soc_test_status(ser, slave)
                    if status["supported"] != 1:
                        raise RuntimeError("当前固件未开启SOC测试模式，量产固件会正常拒绝该测试")
                    max_ticks = max(1, min(50, int(status["max_ticks_per_write"])))
                    step_index = 0
                    for name in scenario_names:
                        start_soc, factory = sim.SCENARIOS[name]
                        pack = sim.TruePack(start_soc)
                        for segment in factory():
                            remaining = segment.seconds * sim.TICKS_PER_SECOND
                            while remaining > 0:
                                ticks = min(max_ticks, remaining)
                                vmax, vmin = pack.voltage(segment.idsg_a10, segment.ichg_a10, segment.imbalance_mv)
                                status = run_mcu_soc_test_sample(
                                    ser,
                                    slave,
                                    vmax_mv=vmax,
                                    vmin_mv=vmin,
                                    ichg_a10=segment.ichg_a10,
                                    idsg_a10=segment.idsg_a10,
                                    ticks=ticks,
                                )
                                for _ in range(ticks):
                                    pack.step(segment.idsg_a10, segment.ichg_a10)
                                step_index += 1
                                row = {
                                    "time_s": step_index,
                                    "step": step_index,
                                    "scenario": name,
                                    "soc": status["soc"],
                                    "vmin_mv": vmin,
                                    "vmax_mv": vmax,
                                    "ichg_a10": segment.ichg_a10,
                                    "idsg_a10": segment.idsg_a10,
                                    "ticks": ticks,
                                    "result": status["last_result"],
                                }
                                rows.append(row)
                                self.queue.put(("mcu_row", row))
                                remaining -= ticks
                    self.queue.put(("mcu_done", rows))
            except Exception as exc:
                self.queue.put(("error", f"MCU加速测试失败：{exc}"))

        self.set_status("正在运行MCU SOC加速测试...")
        threading.Thread(target=worker, daemon=True).start()

    def run_mcu_ride_test(self) -> None:
        self._run_mcu_scenario_names(["city_commute", "hill_climb", "fast_current_pulses"])

    def run_mcu_pulse_test(self) -> None:
        self._run_mcu_scenario_names(["fast_current_pulses"])

    def disable_mcu_test_mode(self) -> None:
        def worker() -> None:
            try:
                with self._open_serial() as ser:
                    status = run_mcu_soc_test_sample(
                        ser,
                        int(self.slave_var.get()),
                        vmax_mv=3600,
                        vmin_mv=3600,
                        ichg_a10=0,
                        idsg_a10=0,
                        ticks=1,
                        enable=0,
                    )
                self.queue.put(("mcu_status", status))
            except Exception as exc:
                self.queue.put(("error", f"关闭MCU测试模式失败：{exc}"))

        self.set_status("正在关闭MCU SOC测试模式...")
        threading.Thread(target=worker, daemon=True).start()

    def write_set_soc_once(self) -> None:
        try:
            soc = int(self.set_soc_var.get())
        except ValueError:
            messagebox.showerror("参数错误", "SOC 必须是 0..100 的整数")
            return
        if not 0 <= soc <= 100:
            messagebox.showerror("参数错误", "SOC 必须是 0..100")
            return
        if not messagebox.askyesno(
            "确认写入",
            f"将通过 0x1005 把板端 SOC 设置为 {soc}%。这会改变 MCU 当前 SOC 状态，是否继续？",
        ):
            return

        def worker() -> None:
            try:
                with self._open_serial() as ser:
                    set_soc_once(ser, int(self.slave_var.get()), soc)
                    row = read_status(ser, int(self.slave_var.get()))
                    row["sample"] = len(self.online_rows)
                    row["time_s"] = 0
                self.queue.put(("set_soc_done", (soc, row)))
            except Exception as exc:
                self.queue.put(("error", f"设置 SOC 失败：{exc}"))
        self.set_status(f"正在写入一次 SOC={soc}% ...")
        threading.Thread(target=worker, daemon=True).start()

    def scan_ports(self) -> None:
        ports: list[str] = []
        try:
            from serial.tools import list_ports  # type: ignore
            ports = [port.device for port in list_ports.comports()]
        except Exception:
            ports = [f"COM{i}" for i in range(1, 17)]
        if ports:
            self.port_var.set(ports[0] if "COM4" not in ports else "COM4")
            messagebox.showinfo("串口扫描", "发现串口：\n" + "\n".join(ports))
        else:
            messagebox.showwarning("串口扫描", "未发现串口")

    def save_report_as(self) -> None:
        if not REPORT_PATH.exists():
            messagebox.showwarning("报告", "报告文件不存在，请先运行仿真")
            return
        target = filedialog.asksaveasfilename(
            title="保存报告",
            defaultextension=".md",
            filetypes=[("Markdown", "*.md"), ("All files", "*.*")],
        )
        if target:
            Path(target).write_text(REPORT_PATH.read_text(encoding="utf-8"), encoding="utf-8")
            self.set_status(f"报告已保存：{target}")

    def save_online_as(self) -> None:
        if not self.online_rows:
            messagebox.showwarning("在线监控", "暂无在线数据")
            return
        target = filedialog.asksaveasfilename(
            title="保存在线监控CSV",
            defaultextension=".csv",
            filetypes=[("CSV", "*.csv"), ("All files", "*.*")],
        )
        if target:
            self._write_online_csv(Path(target))
            self.set_status(f"在线CSV已保存：{target}")

    def _write_online_csv(self, path: Path) -> None:
        if not self.online_rows:
            return
        with path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(self.online_rows[0].keys()))
            writer.writeheader()
            writer.writerows(self.online_rows)

    def load_report(self) -> None:
        self.report_text.delete("1.0", "end")
        if REPORT_PATH.exists():
            self.report_text.insert("1.0", REPORT_PATH.read_text(encoding="utf-8"))
        else:
            self.report_text.insert("1.0", "暂无报告，请先运行真实场景仿真。")

    def _poll_queue(self) -> None:
        try:
            while True:
                kind, payload = self.queue.get_nowait()
                if kind == "sim_done":
                    results, rows = payload  # type: ignore[misc]
                    self._show_sim_results(results, rows)
                elif kind == "online_row":
                    self._show_online_row(payload)  # type: ignore[arg-type]
                elif kind == "online_done":
                    self._write_online_csv(ONLINE_CSV_PATH)
                    self.set_status(f"在线监控结束，CSV：{ONLINE_CSV_PATH.name}")
                elif kind == "params_done":
                    params = payload  # type: ignore[assignment]
                    self.board_param_var.set(
                        "SOC参数：容量={capacity_a10}*0.1Ah  循环={cycle_times}  V100={v100_mv}mV  V0={v0_mv}mV".format(**params)
                    )
                    self.set_status("板端 SOC 参数读取完成")
                elif kind == "set_soc_done":
                    soc, row = payload  # type: ignore[misc]
                    self.set_status(f"已写入一次 SOC={soc}%，读回显示 SOC={row['soc']}%")
                    self._show_online_row(row)
                elif kind == "mcu_status":
                    status = payload  # type: ignore[assignment]
                    self.mcu_status_var.set(
                        "MCU测试状态：supported={supported} enabled={enabled} tick={tick_ms}ms "
                        "max_ticks={max_ticks_per_write} total_ticks={total_ticks} soc={soc}% result={last_result}".format(**status)
                    )
                    self.set_status("MCU SOC测试状态已读取")
                elif kind == "mcu_row":
                    self._show_mcu_row(payload)  # type: ignore[arg-type]
                elif kind == "mcu_done":
                    self.mcu_rows = payload  # type: ignore[assignment]
                    self.set_status(f"MCU SOC加速测试完成：{len(self.mcu_rows)}步")
                elif kind == "error":
                    self.set_status(str(payload))
                    messagebox.showerror("错误", str(payload))
        except queue.Empty:
            pass
        self.after(100, self._poll_queue)

    def _show_sim_results(self, results, rows) -> None:
        self.sim_rows = rows
        for item in self.sim_tree.get_children():
            self.sim_tree.delete(item)
        for r in results:
            self.sim_tree.insert(
                "",
                "end",
                values=(
                    r.name,
                    r.duration_s,
                    f"{r.true_start:.1f}->{r.true_end:.2f}",
                    f"{r.soc_start}->{r.soc_end}",
                    r.display_end,
                    f"{r.max_abs_error:.2f}",
                    r.min_vmin,
                    r.max_current_a10,
                    "PASS" if r.passed else "FAIL",
                ),
            )
        self.soc_chart.set_data(
            rows,
            [
                ("true_soc", "真实SOC", "#2563eb"),
                ("internal_soc", "算法SOC", "#16a34a"),
                ("display_soc", "显示SOC", "#dc2626"),
            ],
            "SOC %",
        )
        self.volt_chart.set_data(
            rows,
            [
                ("vmin_mv", "Vmin", "#7c3aed"),
                ("vmax_mv", "Vmax", "#f97316"),
                ("idsg_a10", "Idsg(A*10)", "#0f766e"),
            ],
            "电压/电流",
        )
        self.load_report()
        self.set_status("真实场景仿真完成")

    def _show_online_row(self, row: dict[str, object]) -> None:
        self.online_rows.append(row)
        self.online_tree.insert(
            "",
            "end",
            values=(
                row["time_s"],
                row["soc"],
                row["soh"],
                row["vmin_mv"],
                row["vmax_mv"],
                row["ichg_a10"],
                row["idsg_a10"],
                f"{row['cap_now_ah100']}/{row['cap_full_ah100']}",
                f"{int(row['fault1']):04X}/{int(row['fault2']):04X}/{int(row['fault3']):04X}",
            ),
        )
        if len(self.online_tree.get_children()) > 200:
            self.online_tree.delete(self.online_tree.get_children()[0])
        self.online_tree.yview_moveto(1.0)
        self.online_chart.set_data(
            self.online_rows,
            [
                ("soc", "SOC", "#16a34a"),
                ("vmin_mv", "Vmin", "#7c3aed"),
                ("idsg_a10", "Idsg", "#dc2626"),
                ("ichg_a10", "Ichg", "#2563eb"),
            ],
            "在线数据",
        )
        self.status_var.set(f"在线：SOC={row['soc']}% Vmin={row['vmin_mv']}mV")


    def _show_mcu_row(self, row: dict[str, object]) -> None:
        self.mcu_rows.append(row)
        self.mcu_tree.insert(
            "",
            "end",
            values=(
                row["step"],
                row["scenario"],
                row["soc"],
                row["vmin_mv"],
                row["vmax_mv"],
                row["ichg_a10"],
                row["idsg_a10"],
                row["ticks"],
                row["result"],
            ),
        )
        if len(self.mcu_tree.get_children()) > 300:
            self.mcu_tree.delete(self.mcu_tree.get_children()[0])
        self.mcu_tree.yview_moveto(1.0)
        self.mcu_chart.set_data(
            self.mcu_rows,
            [
                ("soc", "SOC", "#16a34a"),
                ("vmin_mv", "Vmin", "#7c3aed"),
                ("idsg_a10", "Idsg", "#dc2626"),
                ("ichg_a10", "Ichg", "#2563eb"),
            ],
            "MCU测试数据",
        )


def main() -> int:
    app = SocTestUi()
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
