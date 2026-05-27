# RTC/STOP 低功耗优化需求确认表

状态：已按用户确认执行，部分验证

日期：2026-05-27

用户确认：2026-05-27，用户回复“开始，并允许stlink长期监控测试，release关闭DBGMCU_CR_DBG_STOP，直接帮我调通”。

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| LP-RTC-001 | 空闲、无充放电、无通信、无按键、无 Flash 写入、无故障时，系统应在配置延时后进入 RTC/STOP 低功耗 | `Runtime.c` 调用 `LP_Task()`；`rtc_sleep.c` 每秒选择 sleep mode；`conf.c` 使用 `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)` | 修复后 Release 条件下进入 STOP，普通 ST-Link attach 失败 | 功耗偏高，App_Can 打开后更明显 | MUST_KEEP | 是否确认空闲后必须进入 RTC/STOP，而不是长期 RUN？ | 确认保留 | 已确认 |
| LP-RTC-002 | 启动/唤醒后 SOC 显示窗口应保留，用户可看到电量 | `Project_Config.h` 中 `PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS = 1000`；`LedBar_ServiceStartupDisplayWindow()` 启动一次显示窗口 | 显示窗口保留，窗口结束后不再被启动锁存标志永久阻塞 | 若直接关掉启动显示，会影响体验 | MUST_KEEP | 是否保留启动/唤醒后约 10 秒 SOC 显示？ | 确认保留 | 已确认 |
| LP-RTC-003 | `startup_display_armed` 只应表示“启动显示已触发”，不应表示“显示仍活跃” | `LedBar.c` 中 `startup_display_armed` 只在首次服务时置 1；ST-Link 显示窗口归零后该值仍为 1 | 已从 `LedBar_IsActiveForLowPower()` 阻塞条件中移除 | 修复错误条件后，设备会在显示结束并满足空闲条件时进入 STOP | CHANGE_NEEDED | 是否确认把 `startup_display_armed` 从低功耗阻塞条件中移除？ | 确认修改 | 已确认 |
| LP-RTC-004 | LED/数码管真实显示活动期间仍应阻塞低功耗，避免显示中途被 STOP 打断 | `LedBar_IsActiveForLowPower()` 当前检查 `soc_display_10ms`、`frame_mask`、`scan_timer_enabled` | 真实显示窗口和扫描活动仍会阻塞低功耗 | 若去掉真实显示条件，会出现按键显示/启动显示被打断 | MUST_KEEP | 是否确认只让真实显示窗口和扫描活动阻塞低功耗？ | 确认保留 | 已确认 |
| LP-RTC-005 | ST-Link STOP 调试只用于定位，量产功耗实测必须关闭 DBGMCU STOP/SLEEP/STANDBY 调试保持 | `System_Init.c` 已在 Release 中清除 DBGMCU 低功耗调试位；现场读到 `DBGMCU_CR = 0x00000000` | Release 条件下 STOP 后普通 ST-Link 不能稳定 attach | 若带 ST-Link/DBG_STOP 测功耗，结论会偏高 | MUST_KEEP | 是否确认最终功耗测试要脱开 ST-Link 或清除 DBGMCU 低功耗调试位？ | 确认保留 | 已确认 |
| LP-RTC-006 | 本次先做最小修复和必要文档，不做大规模 `rtc_sleep` 重构 | `rtc_sleep.c` 已有状态机和 port 层；当前已定位单点阻塞 | 已执行最小修复，未改保护、通信、AFE 和协议 | 大重构会扩大风险，影响保护/唤醒/通信 | KEEP_BUT_REFACTOR | 是否确认先修 `LedBar_IsActiveForLowPower()`，再按实测决定是否继续简化 `rtc_sleep`？ | 确认最小修复优先 | 已确认 |

## 执行结果

1. 已修改 `LedBar_IsActiveForLowPower()`，移除 `startup_display_armed` 阻塞条件。
2. 已让 `EnableLowPowerDebug()` 在 Release 中清除 `DBGMCU_CR_DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP`。
3. 已编译 `FD_Release`，结果 `0 Error(s), 0 Warning(s)`。
4. 已用安全脚本烧录 App 到 `0x08004800`。
5. 已用 ST-Link 验证 Release 中 `DBGMCU_CR = 0x00000000`；显示窗口后目标进入 STOP，普通 ST-Link attach 失败，符合 Release 低功耗调试关闭后的预期。
