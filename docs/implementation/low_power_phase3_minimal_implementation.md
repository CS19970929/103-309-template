# RTC 低功耗第三阶段最小实现记录

## 范围

本次进入第三阶段最小实现，目标是把第二阶段设计落到当前 STM32F103 BMS 项目中，同时保持改动边界可控。

本次实现遵守以下限制：

- 不修改 Modbus/RS485 协议。
- 不修改 CAN 帧格式和协议语义。
- 不修改 SOC 核心估算算法。
- 不修改 AFE/MOS 保护策略。
- 不修改 Flash 地址、存储格式和日志结构。
- 不修改 IAP/App 地址、scatter 文件和烧录脚本。
- 不启用 CAN/USART Stop 唤醒。
- 不把当前 `DEEP_MODE` 改成硬件 Standby。

## 新增模块

新增低功耗框架与 BSP 封装：

- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/app_lowpower.h`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/bsp_rtc.h`
- `103 + 309/Project/Source/bsp_power.c`
- `103 + 309/Project/Source/bsp_power.h`
- `103 + 309/Project/Source/bsp_clock.c`
- `103 + 309/Project/Source/bsp_clock.h`

Keil 工程同步加入：

- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`

新增 C 文件已加入 `FD_Release` 和 `FD_Debug` 两个 target 的 `Source` group，避免单 target 可编译、另一个 target 缺符号。

## 接入点

### 初始化

文件：`103 + 309/Project/Source/AppInit.c`

`AppInit_Boot()` 保持原有 `Init_RTC()` 调用不变，在其后新增：

```c
LP_Init();
```

这样不改变现有 RTC 初始化时序，`LP_Init()` 只初始化框架运行状态。

### 周期任务

文件：`103 + 309/Project/Source/Runtime.c`

在 `Runtime_RunIoAndPowerTasks()` 中用：

```c
LP_Task();
```

替换原来的：

```c
App_LowPowerProcess();
```

`LP_Task()` 内部继续复用原 `rtc_sleep()`，因此现有 Stop + RTC 主路径没有被推翻。

## 阻塞位图

文件：`103 + 309/Project/Source/app_lowpower.h`

新增位图：

- `LP_BLOCK_CHARGE`
- `LP_BLOCK_DISCHARGE`
- `LP_BLOCK_COMM`
- `LP_BLOCK_KEY`
- `LP_BLOCK_AFE_BUSY`
- `LP_BLOCK_FLASH_BUSY`
- `LP_BLOCK_UPGRADE`
- `LP_BLOCK_FAULT`
- `LP_BLOCK_LED_ACTIVE`
- `LP_BLOCK_IWDG_UNSAFE`

文件：`103 + 309/Project/Source/app_lowpower.c`

`LP_CanSleep()` 汇总以下当前项目依据：

- 电流：`RtcSleep_PortGetChargeCurrentMa()`、`RtcSleep_PortGetDischargeCurrentMa()`
- 通信：`Sci_IsAnyPortBusy()`、`Can_IsBusy()`、`Can_IsBusActive()`
- 按键/外部唤醒：`RtcSleep_PortIsMcuWakeActive()`
- AFE：`RtcSleep_PortIsAfeSleepBlocked()`
- Flash：`StorageFlash_IsBusy()`、`u8FlashUpdateE2PROM`
- 升级：`u8FlashUpdateFlag`
- 保护故障：`g_stCellInfoReport.unMdlFault_Third.all`
- LED：`LedBar_IsActiveForLowPower()`
- IWDG：`BSP_RTC_IsWakeupPeriodSafe()`

## 旧 RTC 睡眠路径兼容

文件：`103 + 309/Project/Source/rtc_sleep.c`

本次没有删除旧 `rtc_sleep()`，只在两个位置挂接新框架：

1. `low_power_select_sleep_mode()` 在原有 current、MCU wake、老化、外部通信、AFE 阻塞之后，再调用 `LP_CanSleep()`。
2. `rtc_sleep()` 在真正执行 `HICCUP_MODE` 前再次调用 `LP_CanSleep()`，防止进入 Stop 前状态刚好变化。

新增旧状态枚举：

- `LOW_POWER_RTC_BLOCK_FRAMEWORK`

用于把新框架位图阻塞映射到旧 `g_stLowPowerRtcStatus.blockReason` 单字节状态。

## RTC 与 IWDG

文件：`103 + 309/Project/Source/RTC.c`

本次恢复 RTC 唤醒周期的 IWDG 安全约束：

- 最小周期：`1s`
- `wdog_enable` 开启时最大周期：`10s`

新增接口：

- `RTC_SetWakeupPeriodSeconds()`
- `RTC_IsWakeupPeriodSafe()`

`RTC_GetWakeupPeriodSeconds()` 仍兼容原有 `Can_GetIdleRtcPeriodSeconds()`，但支持 `LP_SetWakeupPeriod()` 通过 BSP 设置覆盖周期。

## Flash 忙状态

文件：`103 + 309/Project/Source/Flash.c`

新增：

- `StorageFlash_IsBusy()`

并在以下写入路径设置 busy 标志：

- `FlashWriteOneHalfWord()`
- `StorageFlash_SaveSocData()`
- `StorageFlash_SaveAfeData()`
- `StorageFlash_SaveRwParamData()`
- `StorageFlash_SaveLogData()`
- `StorageFlash_SaveFactoryAgingData()`

说明：当前 Flash 写入是同步函数，busy 标志主要用于框架状态表达和后续异步化扩展；本次不改变 Flash 地址、格式、擦写算法。

## LED 活跃状态

文件：`103 + 309/Project/Source/LedBar.c`

新增：

- `LedBar_IsActiveForLowPower()`

判断依据：

- 单段测试显示
- SOC 显示窗口
- 启动显示窗口
- 当前显示帧
- 扫描定时器状态

若 LED 正在显示，则 `LP_CanSleep()` 返回禁止休眠。

## 睡眠秒数接口

文件：`103 + 309/Project/Source/app_lowpower.c`

`LP_GetLastSleepSeconds()` 返回最近一次完整 RTC Stop 睡眠秒数。

文件：`103 + 309/Project/Source/rtc_sleep.c`

旧 `rtc_sleep_run_hiccup_cycle()` 退出前调用：

```c
LP_RecordLastSleepSeconds(s_u32RtcSleepElapsedSeconds);
```

用于把旧路径统计到的新接口中。

## 编译验证

命令：

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -b "E:\TODO\103 + 309 - 副本\103 + 309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx" -t "FD_Release" -j0 -o "E:\TODO\103 + 309 - 副本\103 + 309\Project\Users\codex_low_power_build.log"
```

结果：

- Target：`FD_Release`
- Compiler：ARMCC `V5.06 update 7 build 960`
- Error：`0`
- Warning：`0`
- Code：`51076`
- RO-data：`2964`
- RW-data：`816`
- ZI-data：`6040`
- BIN：`103 + 309/Project/Users/Objects/FD_Release.bin`

未执行：

- 未烧录。
- 未上板验证。
- 未进行 CAN/Modbus 在线测试。

## 后续上板重点

1. 空闲无通信时能进入 Stop，并由 RTC Alarm 唤醒。
2. CAN 或 Modbus 活跃时 `LP_BLOCK_COMM` 生效，不进入 Stop。
3. Flash 保存、参数更新或升级 pending 时禁止进入 Stop。
4. LED 显示窗口期间禁止进入 Stop。
5. RTC 周期大于 10s 时被 IWDG 安全策略阻止或裁剪。
6. 唤醒后 CAN、USART、TIM3、ADC、AFE IIC 恢复正常。
7. SOC 能通过 `LP_GetLastSleepSeconds()` 读取最近一次睡眠秒数。
