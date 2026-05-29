# D009 CAN/RTC 休眠迁移风险清单

状态：部分验证

参考源码：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/LedBar.c`

| Risk ID | 风险 | Evidence from code | 影响 | 缓解措施 |
|---|---|---|---|---|
| RISK-D009-CAN-001 | 整文件覆盖会带入错误硬件显示逻辑 | D009 4 路 SOC LED，参考分支 5 脚 Charlieplexing | LED、按键、唤醒和低功耗阻塞错误 | 只迁 CAN/RTC 逻辑，保留 D009 LedBar |
| RISK-D009-CAN-002 | CAN pending mask 被视为 busy，可能阻止 RTC 入睡 | D009 `Can_IsBusy()` 检查 `s_u16FeidaoCanPendingMask` | 无法稳定进入 STOP 或频繁醒着等待 | 同步参考分支 busy 语义或使用 sleep-blocked 专用判断 |
| RISK-D009-CAN-003 | RTC alarm/EXTI17 pending 清理分散 | D009 `RTC.c` 缺少集中 disable/restore API | 下一轮 STOP 立即唤醒或 RTC 秒中断异常 | 同步 `RTC_DisableStopWakeup()`、`RTC_RestoreRunInterrupts()` |
| RISK-D009-CAN-004 | 无 ACK 判定过慢或过快 | D009 6 次无 ACK 判 inactive，参考分支可配置阈值 | 误判影响 1s 广播或功耗 | 使用配置宏并通过实测确认阈值 |
| RISK-D009-CAN-005 | RTC 唤醒窗口中 PB4 未恢复输出 | STOP 前 IO 进入低功耗模式 | 看不到 transceiver 上电脉冲和 CAN_TX | 确认 `InitCan_GPIO()` 在 `Can_RtcWakeService()` 前执行 |
| RISK-D009-CAN-006 | 修改 `CAN_ABOM` 改变故障恢复语义 | D009 `CAN_ABOM = DISABLE` | bus-off 诊断与恢复行为变化 | 用户确认后再改 |
| RISK-D009-CAN-007 | 看门狗安全窗口裁剪 RTC 周期 | `RTC_GetWakeupPeriodSeconds()` 会按 IWDG 裁剪 | 目标 10s idle 可能被缩短 | 保留安全裁剪，并在测试中记录实际周期 |
