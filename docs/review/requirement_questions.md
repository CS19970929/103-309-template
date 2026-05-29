# D009 CAN/RTC 休眠迁移待确认问题

状态：部分验证

参考源码：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/LedBar.c`

| ID | 模块 | 问题 | 代码证据 | 分类 | 风险 | 建议 |
|---|---|---|---|---|---|---|
| Q-D009-001 | CAN/RTC | 有 CAN 对端 ACK 时，RTC STOP 中是否必须 1s 周期广播？ | `Can_GetIdleRtcPeriodSeconds()` 返回 active 1s | UNKNOWN | 功耗与可见性冲突 | 确认必须 1s |
| Q-D009-002 | CAN/RTC | 无 ACK 时是否保持 10s 探测？ | `FEIDAO_CAN_RTC_IDLE_PERIOD_SECONDS` 当前为 10s | UNKNOWN | 无设备场景功耗 | 确认 10s 探测 |
| Q-D009-003 | CAN | 是否允许启用 `CAN_ABOM` 自动 bus-off 恢复？ | D009 为 `DISABLE`，参考分支为 `ENABLE` | UNKNOWN | bus-off 行为变化 | 若现场更重视自动恢复，建议启用 |
| Q-D009-004 | LED/低功耗 | 本次是否只保留 D009 普通 LED，不迁移参考分支数码管逻辑？ | D009 `LedBar` 为 4 路 SOC LED | MUST_KEEP | 覆盖会导致 IO 错误 | 确认不迁移显示硬件 |
| Q-D009-005 | 验证 | 是否具备 CAN 对端 ACK 设备与示波器验证条件？ | 需验证 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL | UNKNOWN | 编译通过不代表 RTC 窗口真实发帧 | 建议列为上板必测 |
