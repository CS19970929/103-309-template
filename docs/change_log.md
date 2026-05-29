# Change Log

## 2026-05-29

- 修正 D009 RTC 轻休眠入口经常显示 `g_stLowPowerRtcStatus.blockReason = 8` 的问题：将框架层 `LP_BLOCK_*` 原始 bitmask 记录到 `frameworkBlockReason`，并把通信忙、Flash 忙、升级、系统故障、IWDG 窗口等映射为更具体的 RTC 阻塞原因。
- RTC 轻休眠入口不再直接被 `g_stCellInfoReport.unMdlFault_Third` 的软件/历史故障统一挡住；真正的 AFE 硬件异常仍由 `RtcSleep_PortIsAfeSleepBlocked()` 检查并以 `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` 阻塞。
- 修正主循环调用 `LP_Task()` 时缺少 `app_lowpower.h` 导致的 Keil 隐式声明 warning。

- 完成 D009 CAN/RTC 休眠逻辑移植：有 CAN ACK 对端时 RTC STOP 休眠按 1s 周期唤醒并发送飞道 1s 周期帧；无 ACK 连续回退后进入 10s 低频探测。
- 按当前参考分支拆分 RTC 低功耗模块，新增 `bsp_rtc`、`bsp_clock`、`bsp_power`、`app_lowpower`、`LowPowerSleep`、`rtc_sleep_port`、`rtc_sleep_afe_sh367309`，并保留 D009 普通 SOC LED/socKey 硬件适配。
- 修正 RTC STOP alarm 清理/恢复流程：进入 STOP 前关闭 alarm 并清 EXTI/NVIC pending，STOP 后恢复 RTC 秒中断，避免 RTC pending 残留影响下一轮休眠。
- Keil 工程新增 RTC 拆分模块源文件，工程文件保持无 BOM UTF-8；`FD_Release` 编译通过，0 error / 8 warning，生成 `FD_Release.axf` 和 `FD_Release.bin`。

参考源码：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c`
- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/LedBar.c`

状态：部分验证

## 2026-05-27

- D009 同步老化模式 `关闭老化模式` 命令语义：提前结束本轮老化时间，板端持久化 `DONE` 状态，并让剩余时间归零。
- D009 老化模式 `开启老化模式` 在完成态下会清零累计时间并开启新一轮，避免完成态返回 `BMS_ERROR`。
- CAN 用户上位机关闭老化模式确认文案改为明确提示“提前结束本轮老化时间，剩余时间将变为 0”。
- 同步更新 CAN App 服务、comm tool 串口协议和用户上位机说明文档。

参考源码：

- `103 + 309/Project/Source/FactoryAging.c`
- `tools/comm_tool_upgrade_ui.py`
- `tools/can_bms_host.py`
- `tools/comm_tool_host.py`
