# CAN and RTC Low Power Refactor - 2026-05-15

## Goal

This round is a behavior-equivalent cleanup for CAN and RTC low-power paths. It keeps the existing CAN frame protocol, RTC wake period policy, sleep mode selection, SOC logic, build profiles, Flash addresses, and host communication registers unchanged.

## Code Changes

- Added `LowPowerSleep.c/.h`.
  - `LowPowerSleep_SaveCoreState()` keeps the common pre-sleep save sequence:
    `Can_PrepareSleep -> SOC_SaveSnapshotBeforeSleep -> FactoryAging_SaveProgressBeforeSleep`.
  - `LowPowerSleep_SaveResetState()` keeps the reset-sleep sequence by calling the core save sequence and then `LedBar_SaveSleepSoc`.
- Simplified `SleepDeal_Continue()` and `rtc_sleep_prepare_rtc()` by using the shared save helpers.
- Simplified CAN RTC-wake flow inside `Can_HDX.c`:
  - `feidao_can_begin_rtc_wake_service()`
  - `feidao_can_queue_rtc_wake_frames()`
  - `feidao_can_finish_rtc_wake_service()`
  - `feidao_can_clear_sleep_runtime()`
  - `feidao_can_run_10ms_tasks()`
- Simplified RTC alarm access in `RTC.c`:
  - `RTC_EnableBackupAccess()`
  - `RTC_DisableAlarmInterrupt()`
  - `RTC_EnableAlarmAfterSeconds()`
- Updated the Keil project to compile `LowPowerSleep.c` in both `FD_Release` and `FD_Debug`.

## Behavior Boundaries

- CAN frame IDs, payload format, CRC, send period masks, bus-active detection, RTC probe frame mask, and IAP update flag address are unchanged.
- RTC wake period policy is unchanged: `RTC_GetWakeupPeriodSeconds()` still delegates to `Can_GetIdleRtcPeriodSeconds()` and applies the existing watchdog safe window.
- Normal, hiccup, deep sleep mode selection is unchanged.
- The RTC STOP path still saves only the core state before STOP. The reset sleep path still additionally saves LED bar SOC.
- App address remains `0x08004800`; IAP address remains `0x08000000`.
- Production profile remains `PROJECT_CFG_BUILD_PROFILE 0`; SOC test mode remains isolated.

## Verification Commands

Run after this refactor:

```powershell
py -3.9 tools\project_check.py --quiet
py -3.9 tools\soc_replay_test.py
py -3.9 tools\run_soc_host_c_test.py
.\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release
```

## Notes

- This round does not burn firmware and does not read COM4.
- If board validation is needed later, burn only the App image to `0x08004800` through:
  `.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash`
- `Can_HDX.c` had pre-existing local behavior edits in the working tree before this round. Keep those decisions separate from this structural cleanup when staging and committing.
