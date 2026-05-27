# RTC/STOP 低功耗调研与现场定位记录

状态：部分验证

日期：2026-05-27

## 参考资料

- TI BQ76952 Technical Reference Manual：保护板 AFE 常见做法是 NORMAL、SLEEP、DEEPSLEEP、SHUTDOWN 分层；SLEEP 在小电流/无负载时降低测量频率并保持保护，SHUTDOWN 用于运输或长期存储。
- ST AN2821：STM32F10xxx 低功耗不影响 RTC，RTC alarm 可从低功耗自动唤醒；STOP 中 CPU 和大部分时钟关闭，SRAM/寄存器保持。
- ST AN2629：STOP 入口前要清 EXTI pending/RTC alarm flag；ADC/DAC 未关闭会继续耗电；低功耗调压器功耗更低但唤醒更慢。
- Analog Devices ADBMS1818 产品资料：多串电池监控器在 sleep mode 下供电电流可降到微安级，说明行业通常让 AFE 自身承担低功耗状态。

## 行业保护板低功耗共性

1. 运行态只在有电流、通信、按键、充电器、故障处理、Flash 写入、显示窗口时保持 MCU 全速运行。
2. 空闲态进入 MCU STOP，由 RTC 周期唤醒做短任务，任务结束后再次 STOP。
3. AFE 进入自身 sleep/low-power 模式，保持必要保护或周期测量；严重低压、运输、长期存储再进入更深的 shutdown/ship/deep sleep。
4. 通信外设按需上电，CAN/RS485/上位机不应因为“功能打开”长期阻塞 STOP。
5. LED/数码管只在启动、按键、唤醒、故障提示等有限窗口显示，窗口结束必须彻底熄灭并释放低功耗。
6. ST-Link 调试保持 STOP 需要 DBGMCU 低功耗调试位，但该模式会抬高功耗，只能用于定位，不能作为最终功耗实测条件。

## 当前固件证据

参考源码：

- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/System_Init.c`

运行链路：

- `Runtime_RunOnce()` 每轮先执行 `APP_LedBar()`，再执行 `LP_Task()`。
- `LP_Task()` 调用 `rtc_sleep()`。
- `rtc_sleep()` 每秒检查一次空闲条件，`LP_CanSleep() == 0` 时标记 `LOW_POWER_RTC_BLOCK_FRAMEWORK`。
- `LP_BuildBlockReason()` 当前会因为 `LedBar_IsActiveForLowPower() != 0` 设置 `LP_BLOCK_LED_ACTIVE`。

## ST-Link 现场快照

工具：OpenOCD + ST-Link V2，仅暂停读取 RAM，未烧录。

关键地址来自当前 `FD_Release.axf`：

- `s_ledbar = 0x2000037c`
- `g_stLowPowerRtcStatus = 0x20000490`
- `s_lp_runtime = 0x200007b8`
- `DBGMCU_CR = 0xE0042004`

快照结论：

- `g_stLowPowerRtcStatus.mode = NO_SLEEP`
- `g_stLowPowerRtcStatus.blockReason = LOW_POWER_RTC_BLOCK_FRAMEWORK`
- `s_lp_runtime.block_reason = 0x100 = LP_BLOCK_LED_ACTIVE`
- 启动显示窗口倒计时阶段，`soc_display_10ms` 会递减。
- 窗口归零后，`frame_mask = 0`、`scan_timer_enabled = 0`，但 `startup_display_armed = 1` 仍保持。
- `LedBar_IsActiveForLowPower()` 当前把 `startup_display_armed != 0` 也当作活跃条件，因此窗口结束后仍永久阻塞低功耗。

## 初步判断

当前“进不了 rtc_sleep”的直接原因不是 CAN，也不是 RTC alarm 配置，而是 LedBar 的低功耗活跃判断把一次性启动显示触发标志误当作持续显示活跃标志。

最小、安全的源码修复方向：

- 保留启动后显示 SOC 10 秒的用户体验。
- 保留 `startup_display_armed` 作为“一次性触发启动显示窗口”的内部锁存标志。
- 从 `LedBar_IsActiveForLowPower()` 的活跃条件中移除 `startup_display_armed`，只让 `soc_display_10ms`、`frame_mask`、`scan_timer_enabled` 等真实显示活动阻塞低功耗。
- 不改 Modbus/CAN 协议，不改外部寄存器，不改 AFE 保护策略。

## 修复与验证

源码变更：

- `103 + 309/Project/Source/LedBar.c`：`LedBar_IsActiveForLowPower()` 不再把 `startup_display_armed` 作为低功耗阻塞条件。
- `103 + 309/Project/Source/System_Init.c`：新增 Release 清除 DBGMCU 低功耗调试保持位，Debug 构建仍可打开。
- `103 + 309/Project/Source/AppInit.c`：启动阶段统一调用 `EnableLowPowerDebug()`，避免 Release 继承调试器留下的 STOP 调试位。
- `103 + 309/Project/Source/System_Init.h`：补充 `EnableLowPowerDebug()` 声明。

编译验证：

- Keil `FD_Release` rebuild 通过，`0 Error(s), 0 Warning(s)`。
- `FD_Release.bin` 大小 54760 bytes。
- 链接基址保持 `0x08004800`。

板端验证：

- 使用 `tools/soc_flash_app_safe.ps1` 烧录，脚本确认 App 写入地址 `0x08004800`，IAP `0x08000000` 未覆盖。
- Release 运行约 25 秒后，ST-Link 读到 `DBGMCU_CR = 0x00000000`，说明 Release 已关闭 STOP/SLEEP/STANDBY 调试保持位。
- 继续运行后普通 ST-Link attach 持续失败，符合 MCU 已进入 STOP 且 DBG_STOP 关闭后的预期。

注意：

- 需要用 ST-Link 长时间观察 STOP 内部状态时，可以在调试会话中临时打开 DBG_STOP，但该状态会抬高功耗，不能作为最终功耗实测条件。
