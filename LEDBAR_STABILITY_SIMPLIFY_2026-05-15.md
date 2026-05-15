# LED 数码管稳定性优化记录 - 2026-05-15

## 目标

本轮聚焦 `LedBar.c` 的可读性和显示稳定性，处理两个现场现象：

- 数码管偶尔闪烁。
- 未充电时充电图标偶尔亮一下。

本轮不修改 SOC 算法、不修改低功耗进入策略、不修改 App/IAP 地址、不烧录、不读取 COM4。

## 关键边界

- `APP_LedBar()` 仍是主循环入口。
- `TIM4_IRQHandler()` 仍负责 Charlieplexing 扫描。
- `GPIO_MCU_WK` 仍只表示需要保持显示，不再参与充电图标判断。
- 充电图标只由 `GPIO_CHG_IN` 触发，不再用瞬时电流或 MCU_WK 兜底。

## 本轮变化

- `LedBar_ApplyFrame()` 不再在每次非空帧刷新时停止 TIM4、重配 GPIO、再重启扫描。
- 非空帧更新改为短暂屏蔽 TIM4 中断后替换当前帧，减少业务刷新造成的可见空窗。
- 新增 `LedBar_FrameEquals()`，相同帧不重复刷新，避免无意义的扫描扰动。
- 新增 `LedBar_ReadChargeRaw()`，把充电来源收口到充电检测 GPIO。
- `LedBar_ServiceChargeFilter()` 首次初始化不再直接采用瞬时 raw 状态，必须经过 100ms 节拍滤波后才更新充电图标。
- `PROJECT_CFG_LEDBAR_CHARGE_ON_FILTER_100MS` 从 `1` 调整为 `3`，单次 100ms 毛刺不会点亮充电图标。

## 保留行为

- 按键显示窗口仍为 `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS`。
- 启动/唤醒显示窗口仍为 `PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS`。
- MCU_WK 高电平仍保持 SOC 显示。
- 充电断开滤波仍为 `PROJECT_CFG_LEDBAR_CHARGE_OFF_FILTER_100MS=5`。
- 熄屏/休眠时仍关闭 TIM4，并将 LED 引脚置为低功耗确定状态。

## 验证

固定执行：

```powershell
py -3.9 tools\project_check.py --quiet
py -3.9 tools\soc_replay_test.py
py -3.9 tools\run_soc_host_c_test.py
.\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release
```

板端复测建议：

- 未接充电器，仅触发 MCU_WK/按键显示，确认充电图标不亮。
- 接入充电器后观察约 300ms 内充电图标稳定点亮。
- SOC 数值变化或图标变化时，确认数码管没有明显整屏闪一下。
- 进入 RTC/STOP 前后确认无残留段位误亮。

## 本轮构建结果

- `project_check` 通过：91 OK，0 warning，0 error。
- SOC Python replay 通过：47 项。
- SOC host C test 通过。
- `FD_Release.bin=63636B`。
- `LedBar.c` 无编译 warning/error。
- 当前 Release 全量构建仍有 7 个 warning，其中 6 个为既有业务文件 warning，另 1 个为 `easylogger/src/elog.c` 文件末尾无换行；均不来自本轮 LED 变更。
