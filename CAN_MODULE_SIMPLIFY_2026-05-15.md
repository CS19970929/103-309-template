# CAN Module Simplify - 2026-05-15

## Goal

This round simplifies `Can_HDX.c` around the CAN path that is currently used by the firmware. It removes disabled legacy request/response code and keeps the active Feidao periodic sender and low-power CAN transceiver workflow.

## Removed

- Legacy standard-frame request/response dispatcher:
  - `Can_ReceiveDeal()`
  - `Can_TransmitDeal()`
  - `CanTxType_Flag`
  - `Can_Status_Flag`
- Legacy response/test frame functions:
  - `CAN_TX_Test()`
  - `CAN_TX_0x00()` through `CAN_TX_0x11()`
- Inactive host-command/IAP reset helper path:
  - `feidao_can_host_*`
  - `FEIDAO_CAN_HOST_*`
  - host IAP reset runtime fields
- Header definitions that only served the removed legacy path.

## Kept

- Feidao extended-frame periodic send path:
  - voltage/current and SOC at 1000 ms
  - capacity, SOH, version, status, factory time at 5000 ms
- CAN transceiver power sequencing through `GPIO_CMNT_EN`.
- No-ACK inactive detection and RTC probe behavior.
- RTC wake service entry `Can_RtcWakeService()`.
- BusOff detection and recover timing, simplified to module-local state.
- RX interrupt still drains FIFO0 and marks the bus active.

## Behavior Boundary

- CAN frame payloads and periods for the active Feidao path are unchanged.
- CAN bitrate remains 250 kbit/s.
- `CAN_NART` remains enabled to avoid repeated retransmission when no external CAN device is connected.
- App address remains `0x08004800`; IAP address remains `0x08000000`.
- This round does not burn firmware and does not read COM4.

## Verification

Run:

```powershell
py -3.9 tools\project_check.py --quiet
py -3.9 tools\soc_replay_test.py
py -3.9 tools\run_soc_host_c_test.py
.\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release
```
