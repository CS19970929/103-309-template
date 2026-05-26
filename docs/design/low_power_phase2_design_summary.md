# RTC 低功耗第二阶段设计汇总

## 范围

本文件汇总第二阶段设计结论。第二阶段仍不修改源码，只定义后续最小实现边界、接口、状态机和集成策略。

依据文件：

- `docs/design/low_power_minimal_architecture.md`
- `docs/design/low_power_api_state_machine.md`
- `docs/design/low_power_integration_scope.md`
- `docs/low_power_rtc_final_report.md`
- `docs/low_power_rtc_migration_plan.md`

## 最小可行架构

第一版低功耗框架保持当前 STM32F103 项目的核心路径：

- 继续使用 Stop 模式作为主低功耗模式。
- 继续使用 RTC Alarm + EXTI Line17 作为周期唤醒源。
- 继续保留现有 `rtc_sleep.c` / `rtc_sleep_port.c` 的睡眠执行链路。
- 新增可复用外层框架，先收敛准入、阻塞原因、睡前睡后顺序和统计信息。
- 第一版不实现 CAN/USART Stop 唤醒。
- 第一版不改 Modbus/CAN 协议和帧格式。
- 第一版不改 SOC、保护、AFE、Flash、LED 的核心业务算法。

建议新增模块：

- `app_lowpower.c`
- `app_lowpower.h`
- `bsp_rtc.c`
- `bsp_rtc.h`
- `bsp_power.c`
- `bsp_power.h`
- `bsp_clock.c`
- `bsp_clock.h`

## 接口设计

建议第一版导出以下接口：

```c
void LP_Init(void);
void LP_Task(void);
uint8_t LP_CanSleep(void);
uint32_t LP_GetBlockReason(void);
void LP_SetWakeupPeriod(uint32_t seconds);
void LP_EnterStop(uint32_t seconds);
void LP_BeforeSleep(void);
void LP_AfterWakeup(void);
uint32_t LP_GetLastSleepSeconds(void);
```

接口职责：

- `LP_Init()`：初始化低功耗框架内部状态，不直接改变现有协议和业务模块。
- `LP_Task()`：周期运行状态机，用于 RUN/IDLE/PREPARE/RESTORE 之间切换。
- `LP_CanSleep()`：返回当前是否允许进入 Stop。
- `LP_GetBlockReason()`：返回禁止休眠原因位图，便于调试和文档化。
- `LP_SetWakeupPeriod()`：设置 RTC 周期唤醒周期。
- `LP_EnterStop()`：执行一次 Stop + RTC 周期唤醒。
- `LP_BeforeSleep()`：统一收口睡前动作。
- `LP_AfterWakeup()`：统一收口唤醒恢复动作。
- `LP_GetLastSleepSeconds()`：给 SOC/日志/调试读取上次睡眠秒数。

## 状态机设计

建议低功耗状态：

```c
typedef enum {
    LP_STATE_RUN = 0,
    LP_STATE_IDLE_CHECK,
    LP_STATE_PREPARE_SLEEP,
    LP_STATE_STOP_SLEEP,
    LP_STATE_WAKEUP_RESTORE,
    LP_STATE_DEEP_STANDBY,
    LP_STATE_ERROR
} LP_State_t;
```

第一版实际只落地：

- `LP_STATE_RUN`
- `LP_STATE_IDLE_CHECK`
- `LP_STATE_PREPARE_SLEEP`
- `LP_STATE_STOP_SLEEP`
- `LP_STATE_WAKEUP_RESTORE`
- `LP_STATE_ERROR`

`LP_STATE_DEEP_STANDBY` 仅预留。当前项目 `DEEP_MODE` 仍按现有实现处理，不在第三阶段最小实现中改为 Standby。

## 禁止休眠原因位图

建议位图：

```c
#define LP_BLOCK_CHARGE       (1UL << 0)
#define LP_BLOCK_DISCHARGE    (1UL << 1)
#define LP_BLOCK_COMM         (1UL << 2)
#define LP_BLOCK_KEY          (1UL << 3)
#define LP_BLOCK_AFE_BUSY     (1UL << 4)
#define LP_BLOCK_FLASH_BUSY   (1UL << 5)
#define LP_BLOCK_UPGRADE      (1UL << 6)
#define LP_BLOCK_FAULT        (1UL << 7)
#define LP_BLOCK_LED_ACTIVE   (1UL << 8)
#define LP_BLOCK_IWDG_UNSAFE  (1UL << 9)
```

第三阶段最小实现建议先覆盖：

- `LP_BLOCK_COMM`
- `LP_BLOCK_AFE_BUSY`
- `LP_BLOCK_FLASH_BUSY`
- `LP_BLOCK_LED_ACTIVE`
- `LP_BLOCK_IWDG_UNSAFE`

保护、充放电、按键、升级和故障位图在第一版保留接口和文档定义，避免一开始大规模侵入业务模块。

## 睡前顺序

建议 `LP_BeforeSleep()` 顺序：

1. 重新计算禁止休眠原因。
2. 如果通信活跃、Flash 忙、AFE 忙、LED 显示未结束或 IWDG 不安全，则退出。
3. 喂一次 IWDG。
4. 停止 SysTick 或进入当前项目已有睡眠节拍处理。
5. 停止或降噪 LED 扫描。
6. 停止 ADC。
7. 通知 CAN/USART 进入休眠前状态，但不启用 CAN/USART Stop 唤醒。
8. 配置 RTC Alarm。
9. 清 RTC/EXTI pending。
10. 进入 Stop。

## 唤醒后顺序

建议 `LP_AfterWakeup()` 顺序：

1. 立即恢复系统时钟。
2. 恢复 RTC 访问同步。
3. 恢复 IO/ADC/USART/CAN/TIM3/LED。
4. 恢复 AFE IIC 通信。
5. 重新同步 AFE/保护/MOS 状态。
6. 更新睡眠时间统计。
7. 喂 IWDG。
8. 回到 `LP_STATE_RUN`。

当前项目已有 `InitRunAfterStopWakeup()` 可作为第一版恢复路径基础。后续新增 `bsp_clock.c` 时，可先封装现有 `cpu_frequency_conf()` 和 `InitRunAfterStopWakeup()`，不重写时钟树。

## 第三阶段建议修改文件

建议新增：

- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/app_lowpower.h`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/bsp_rtc.h`
- `103 + 309/Project/Source/bsp_power.c`
- `103 + 309/Project/Source/bsp_power.h`
- `103 + 309/Project/Source/bsp_clock.c`
- `103 + 309/Project/Source/bsp_clock.h`

建议少量修改：

- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Users/FD.uvprojx`

如果 Keil 工程实际使用了其他 `.uvprojx`，以当前构建工程为准追加新增 C 文件。

## 第三阶段不建议修改

第三阶段最小实现不建议修改：

- CAN/Modbus 协议寄存器、帧格式和上位机协议。
- SOC 核心估算算法。
- AFE 保护阈值、MOS 控制策略和保护动作。
- Flash 参数结构、日志布局和升级标志语义。
- App/IAP 地址、scatter 文件和烧录脚本。
- `DEEP_MODE` 的复位式低功耗策略。
- CAN/USART Stop 唤醒。
- Standby 模式。

## 验收条件

第三阶段完成后至少满足：

- 通信活跃时不进入 Stop。
- Flash 擦写或保存未完成时不进入 Stop。
- RTC 唤醒周期小于 IWDG 安全窗口。
- Stop 唤醒后系统时钟恢复。
- Stop 唤醒后 SysTick/TIM3/ADC/USART/CAN/LED 恢复。
- Stop 唤醒后 AFE/MOS/保护状态重新同步。
- SOC 可读取上次睡眠秒数。
- 不破坏现有 Modbus/CAN 协议。
- 不破坏现有 SOC、保护、AFE、Flash、LED 功能。
