# LED / LedBar 显示设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/LedBar.c`, `103 + 309/Project/Source/LedBar.h`, `103 + 309/Project/Source/DataDeal.c`, `103 + 309/Project/Source/conf/Project_Config.h`
最后更新时间：2026-06-07
未确认事项：充电图标真实含义、故障显示策略、长按休眠目标时长仍需用户确认。

## 1. 当前硬件模型

当前 `LedBar` 是 5 个 GPIO 的 Charlieplexing 显示路径，物理 pin 映射由 `LedBar.c` 私有持有：

- `GPIOB.11`
- `GPIO_SPI1_NSS`
- `GPIO_SPI1_SCK`
- `GPIO_SPI_MOSI`
- `GPIO_SEG_EN`

`LedBar.c` 内部定义 18 个 route：

- 百位 1 的上下两段。
- 十位 7 段。
- 个位 7 段。
- 充电图标。
- 百分号图标。

## 2. 扫描时基

- `LedBar_ScanTimerInit()` 使用 TIM4。
- TIM4 预分频到 100 kHz。
- `LedBar.c` 内部固定 `LEDBAR_SCAN_TIMER_100KHZ_TICKS = 50`，即当前扫描 update 周期约 0.5 ms。
- `TIM4_IRQHandler()` 调用文件内 `LedBar_Scan1ms()`，函数名保留旧语义，实际周期由 TIM4 配置决定。
- 2026-06-07 起，`scan_timer_initialized` 已删除；软件侧只用 `scan_timer_enabled` 表示 TIM4 是否正在扫描，关闭态重新显示时再配置 TIM4。

## 3. 主循环入口

`Runtime_RunOnce()` 调用 `APP_LedBar()`。

`APP_LedBar()` 当前流程：

1. 读取 `MCU_WK` 原始电平，用上一拍状态检测上升沿并请求 SOC 显示窗口。
2. 每 10 ms 读取按键原始电平，用上一拍状态检测按下边沿和长按。
3. 上电/唤醒后 armed startup display window。
4. 如果没有显示请求，则清屏并停扫描。
5. 有显示请求时按 100 ms 节拍刷新显示值。
6. 显示值取 `g_stCellInfoReport.SocElement.u16Soc` 限幅到 0..100。
7. 默认显示百分号图标。
8. 当前代码在放电 MOS 打开时置位 `LEDBAR_ICON_CHARGE_MASK`，这个命名和行为需要确认。
9. 当前没有故障闪烁或错误码显示策略。

`LedBar_Init()` 必须只执行一次。`APP_LedBar()` 是主循环前台任务，每轮都会被调用；如果无条件重新初始化，会清空 `startup_display_armed`、`soc_display_10ms`、扫描帧和按键滤波状态，导致数码管持续闪烁并可能阻塞低功耗。

当前跨模块公开入口只保留：

- `LedBar_Init()`
- `LedBar_SetSleep()`
- `LedBar_SaveSleepSoc()`
- `LedBar_ShowSleepSocPreview()`
- `LedBar_RequestSocDisplay()`
- `LedBar_PrepareForStop()`
- `LedBar_IsActiveForLowPower()`
- `APP_LedBar()`
- Debug 构建下的 `LedBar_GetDebugSnapshot()`

## 4. 按键和低功耗

当前长按阈值：

- `LEDBAR_KEY_LONG_PRESS_10MS = 50`
- 按 10 ms tick 计算约 500 ms。
- 2026-06-04 起，`key` 和 `MCU_WK` 的 3 tick 软件滤波计数已按试验要求删除；当前只保留首次原始电平预置和上一拍状态，避免开机已有电平被当成新边沿。

长按触发路径：

1. `LedBar_ServiceSwitch()` 检测按键。
2. 达到阈值后调用 `low_power_log_and_commit_sleep(DEEP_MODE)`。
3. `rtc_sleep.c` 检查 sleep mode 合法后调用 `RtcSleep_PortCommitResetSleep(DEEP_MODE)`。
4. `SleepDeal_Continue(DEEP_MODE)` 通过 `LowPowerSleep_SaveResetState()` 保存核心状态和 sleep SOC，写 BKP sleep flag 后进入 AFE sleep/reset 流程。

旧文档中“3 秒长按”与当前源码不一致，必须按需求表确认后才能改行为。

## 5. Sleep SOC 保存

`LedBar_SaveSleepSoc()` 将当前 SOC 写入 BKP：

- `BKP_DR4` 保存 `0x5A00 | soc`。
- `BKP_DR5` 保存反码。

`LedBar_LoadSleepSoc()` 校验 magic 和反码后返回 sleep 前 SOC，否则回退到当前运行 SOC。

## 6. 风险和建议

| 风险 | 当前证据 | 建议 |
|---|---|---|
| 充电图标语义可能反了 | 放电 MOS 打开时置位 `LEDBAR_ICON_CHARGE_MASK` | 需要硬件 UI 需求确认 |
| 故障显示未实现 | 当前 `APP_LedBar()` 不包含故障闪烁或错误码策略 | 确认是否需要故障闪烁/错误码 |
| 长按时长和旧文档冲突 | 当前约 500 ms，旧文档写 3 秒 | 必须用户确认后再改 |
| TIM4 实际扫描周期与函数名不一致 | 函数名 `Scan1ms`，配置约 0.5 ms | 文档保留事实，后续可重命名但不改行为 |
| 重复初始化导致闪烁 | `APP_LedBar()` 每轮调用，运行态必须保持 | 保留 `initialized`、扫描定时器和滤波首次预置保护 |
| 按键/`MCU_WK` 软件滤波已删除 | 当前直接用原始电平上升沿触发显示和长按计时 | 需上板观察抖动、毛刺是否造成显示窗口重复触发或长按误判；若体验差再恢复最小防抖 |
