# 主循环与出厂老化保守收口说明

日期：2026-05-15

## 目标

本次只做行为等价的结构整理，降低 `main.c` 阅读和调试负担，不改变 SOC 算法、CAN 协议、低功耗策略、Flash 地址、构建档位或通信寄存器。

## 调整内容

- 新增 `Runtime.c/.h`，把主循环任务顺序集中到 `Runtime_RunOnce()`。
- 新增 `FactoryAging.c/.h`，把出厂老化计时、BKP 进度、Flash 进度保存和完成标志逻辑从 `main.c` 平移出来。
- `main.c` 保留启动入口、外设初始化、变量初始化、`InitSci()/App_Sci()` 和工厂模式 MOS 控制入口。
- Keil 工程 `FD_Release`、`FD_Debug` 均新增 `FactoryAging.c` 和 `Runtime.c`，未改 Target、宏、scatter、输出名和烧录脚本。

## 行为边界

`Runtime_RunOnce()` 保持原有 Release 主循环顺序：

```text
SysTime_LatchTaskFlags
FactoryAging_Task
APP_LedBar
App_AFEGet
App_Sci
App_AnlogCal
App_LowPowerProcess
App_Can
StorageFlash_AppUseTest_Task
App_Heat_Cool_Ctrl
App_FlashUpdate
App_LogRecord
App_ProID_Deal
Feed_IWatchDog
```

`_DEBUG_CODE` 构建仍只执行：

```text
App_AFEGet
App_Sci
```

保留的外部接口：

- `Runtime_RunOnce()`
- `FactoryAging_Task()`
- `FactoryAging_IsActive()`
- `FactoryAging_SaveProgressBeforeSleep()`

## 安全规则

- IAP/Bootloader 地址仍为 `0x08000000`。
- App 地址仍为 `0x08004800`。
- App 烧录仍必须使用 `tools\soc_flash_app_safe.ps1`，禁止把裸 `FD_Debug.bin` 或 `FD_Release.bin` 写到 `0x08000000`。
- 量产配置仍保持 `PROJECT_CFG_BUILD_PROFILE 0` 和 `PROJECT_CFG_SOC_TEST_MODE_ENABLE 0`。

## 验证基线

变更前基线：

- `py -3.9 tools\project_check.py --quiet`：91 OK，0 warning，0 error。
- `FD_Release.bin`：64436 B。
- `FD_Release` 构建已有 1 个 CAN 未引用 warning：`feidao_can_host_service_iap_reset`。

本次验证应至少执行：

```powershell
py -3.9 tools\project_check.py --quiet
py -3.9 tools\soc_replay_test.py
py -3.9 tools\run_soc_host_c_test.py
.\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release
```

验收要求：

- 不新增 Release warning/error。
- `FD_Release.bin` 不大于 64436 B。
- 不烧录、不读取 COM4；板端验证另行通过安全脚本烧录到 `0x08004800` 后再执行。

## 后续方向

下一步可继续在行为等价前提下拆分 `System_Init.c` 的时基接口，或者把低功耗入口收口为单独 Power Manager。不要在同一提交里同时改 CAN 低功耗策略、SOC 算法和任务调度策略。
