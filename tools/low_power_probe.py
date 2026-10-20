"""Observe the current AXF through an existing OpenOCD GDB server.

No flash, reset or application-variable/peripheral writes.
Each snapshot briefly halts and detaches/resumes the MCU. This is functional
debug evidence, not a current measurement or a timing-neutral observation.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--server", default="localhost:3333")
    parser.add_argument("--tcl-server", default="localhost:6666")
    parser.add_argument("--gdb", default="arm-none-eabi-gdb")
    parser.add_argument("--duration", type=float, default=300)
    parser.add_argument("--interval", type=float, default=30)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if not args.elf.is_file() or not 1 <= args.interval <= 60 or args.duration < 0:
        parser.error("ELF must exist; interval must be 1..60 seconds; duration must be nonnegative")
    gdb = shutil.which(args.gdb)
    if not gdb:
        parser.error("GDB executable not found")
    if args.out is None:
        base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/board-probe"
        base.mkdir(parents=True, exist_ok=True)
        args.out = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    args.out.mkdir(parents=True, exist_ok=True)
    # Use GDB printf: the installed Arm GDB has no embedded Python support.
    fields = ("magic", "version", "state", "target", "block_mask", "wake_mask", "first_wake",
              "exit_mask", "fault_mask", "prepare_count", "stop_count", "rtc_wake_count",
              "external_wake_count", "restore_count", "abort_count", "spurious_count",
              "quarantine_edges", "idle_seconds", "uptime_ms", "rtc_counter", "gpioa_idr",
              "gpiob_idr", "exti_imr", "exti_pr", "rtc_crl", "primask", "debug_profile",
              "iwdg_compiled", "comm_pending_mask", "storage_pending_mask", "storage_write_failures",
              "uart_partial_timeouts", "uart1_errors", "uart2_errors",
              "uart1_candidates", "uart1_rejected", "uart1_valid_frames", "wake_sequence",
              "wake_exti_pr", "wake_rtc_crl", "wake_icsr", "wake_dfsr", "wake_dhcsr", "wake_dbgmcu_cr")
    expressions = {name: "g_low_power_diag." + name for name in fields}
    expressions.update(hardfault_count="g_hardfault_record.count",
                       hardfault_pc="g_hardfault_record.pc", comm_count="RTC_ExtComCnt", pc="$pc",
                       debug_cr="*(unsigned int *)0xE0042004")
    fmt = "{" + ",".join('\\"' + name + '\\":%u' for name in expressions) + "}"
    commands = 'printf "LP_JSON=' + fmt + '\\n", ' + ",".join(expressions.values())
    started = time.monotonic()
    samples = []
    def resume_after_detach():
        # Some OpenOCD target configs deliberately halt on GDB detach. Verify
        # resume through Tcl after GDB exits, otherwise counters can look stable
        # simply because the debugger left the CPU halted.
        host, port = args.tcl_server.rsplit(":", 1)
        with socket.create_connection((host, int(port)), timeout=5) as connection:
            connection.sendall(b"resume; stm32f1x.cpu curstate\x1a")
            response = b""
            while b"\x1a" not in response and len(response) < 4096:
                data = connection.recv(4096)
                if not data:
                    break
                response += data
        if b"running" not in response:
            raise RuntimeError(f"Target did not resume after detach: {response!r}")
    while True:
        index = len(samples)
        try:
            result = subprocess.run(
                [gdb, "-nx", "--batch", str(args.elf.resolve()),
                 "-ex", "set pagination off", "-ex", "set remotetimeout 10",
                 "-ex", f"target remote {args.server}", "-ex", "monitor halt",
                 "-ex", commands, "-ex", "detach"],
                capture_output=True, timeout=25)
        finally:
            resume_after_detach()
        output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
        (args.out / f"snapshot-{index:03}.log").write_text(output, encoding="utf-8")
        line = next((line for line in output.splitlines() if line.startswith("LP_JSON=")), None)
        if result.returncode or line is None:
            raise RuntimeError(f"Snapshot failed; inspect {args.out}; target was not reset")
        sample = json.loads(line[len("LP_JSON="):])
        sample["elapsed_s"] = round(time.monotonic() - started, 1)
        samples.append(sample)
        (args.out / "samples.json").write_text(json.dumps(samples, indent=2), encoding="utf-8")
        print(json.dumps({key: sample[key] for key in
                         ("elapsed_s", "state", "stop_count", "rtc_wake_count", "external_wake_count",
                          "restore_count", "abort_count", "spurious_count", "block_mask", "wake_mask",
                          "fault_mask", "comm_count", "hardfault_count", "debug_cr")}), flush=True)
        if sample["elapsed_s"] >= args.duration:
            break
        time.sleep(min(args.interval, args.duration - sample["elapsed_s"]))
    print(f"Evidence: {args.out}", flush=True)


if __name__ == "__main__":
    main()
