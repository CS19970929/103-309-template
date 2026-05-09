---
name: bms-soc-module-optimizer
description: Optimize, debug, simulate, and validate BMS SOC modules for STM32 BMS projects, including coulomb counting, OCV calibration, full/empty anchors, SOH/cycles, Flash snapshots, RTC recovery, RS485/CAN SOC communication, online board monitoring, accelerated ride simulation, and reusable test reports.
---

# BMS SOC Module Optimizer

Use this skill for STM32 BMS SOC work: SOC accuracy, ride simulation, low-voltage behavior, charge-to-full confirmation, Flash/EEPROM persistence, RTC recovery, RS485/CAN readback, or production test tooling.

## Workflow

1. Inspect the project
   - Locate SOC files, storage files, protocol files, and project config.
   - Prefer existing tools: `tools/soc_replay_test.py`, `tools/soc_ride_sim_report.py`, `tools/soc_online_monitor.py`, `tools/soc_auto_test.ps1`.
   - Confirm build target and active chemistry/capacity/V100/V0.

2. Run host regression first
   - `py tools\soc_replay_test.py`
   - This must pass before changing board behavior.
   - Key coverage: startup snapshot, set-SOC, integration, full anchor, low-voltage tail, RTC OCV, display smoothing, sag holdoff, city ride, hill climb, cutoff, charge anchor.

3. Run accelerated ride simulation
   - `py tools\soc_ride_sim_report.py --report SOC_RIDE_SIM_REPORT.md --csv SOC_RIDE_SIM_SAMPLES.csv`
   - Use this for reusable project reports and before/after comparison.
   - Treat failed scenarios as algorithm risks unless the scenario parameters are clearly wrong for the product.

4. Build firmware
   - Use the repository's Keil/CMake workflow.
   - Record target, errors/warnings, code size, and artifact path.

5. Online board readback
   - Default current project: RS485 Modbus RTU, COM port from Windows, `19200 8N1`, slave `1`.
   - Read only first:
   - `py tools\soc_online_monitor.py --port COM4 --baud 19200 --samples 30 --interval 1 --csv SOC_ONLINE_MONITOR.csv`
   - Verify `0xD000`: voltage, current, SOC/SOH/capacity/cycles/faults.
   - Verify `0x2318~0x231B`: capacity, cycles, V100, V0.

6. Use the GUI upper-computer when observation matters
   - Launch: `py tools/soc_test_ui.py`
   - Use it to run accelerated scenarios, inspect SOC/voltage/current curves, monitor a board through RS485, and view/export reports.
   - Online tab supports reading `0xD000`, reading SOC params at `0x2318~0x231B`, and confirmed writes to `0x1005`.
   - The GUI is a wrapper over the same scripts, so command-line results and GUI results should match.

7. Write commands only when needed
   - `0x1005` set-SOC changes board state. Ask or state the exact value and restore plan before using it.
   - SOC parameter/table writes must be documented and validated by readback.

## MCU Test Interface Requirements

Minimum:
- Read `0xD000` status block: cell voltage max/min/delta, total voltage, charge/discharge current, SOC, SOH, capacity, cycles, faults, balance flags.
- Read SOC params: capacity, cycles, V100, V0.
- Set SOC once for calibration tests.
- Read/write OCV table for chemistry-specific tests.

Recommended:
- Add SOC debug read-only block: internal SOC, display SOC, capacity in `As*10`, timers, mode, last calibration reason, Flash save state.
- Add factory-only virtual sample injection: voltage/current samples feed the same SOC update path as real AFE samples.
- Add test time acceleration for long rest, RTC, ride, and low-voltage tests.

## Acceptance Rules

- Runtime automatic SOC calibration must move internal SOC by at most `1%` per decision.
- Charge should not publish unconfirmed `100%`; it should wait for full-voltage anchor.
- Heavy discharge sag must not falsely force empty while voltage is above the configured tail region.
- Near controller cutoff, SOC must converge to `0%` before or at protection.
- Display SOC may smooth, but communication and LED display must be consistent with published `SocElement`.
- Flash snapshot format and communication addresses must remain compatible unless the user explicitly requests a protocol revision.

## Report Expectations

For substantial SOC changes, produce:
- Code changes.
- Host regression output.
- Accelerated ride simulation report and CSV.
- Firmware build result.
- Online board readback result when hardware is available.
- A short Markdown note describing behavior changes and production risk.
