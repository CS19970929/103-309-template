# D009 CAN/RTC 休眠迁移需求确认表

状态：部分验证

参考源码：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/Can_HDX.h`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/LedBar.h`

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-D009-CAN-RTC-001 | RTC 休眠下接有 CAN 对端且有 ACK 时，D009 必须约 1s 周期对外通信 | `RTC.c:380`, `Can_HDX.c:2365`, `Can_HDX.c:2370`, 参考分支 `Can_HDX.c:1118`, `Can_HDX.c:1128` | D009 已有 1s active period 接口，但发送状态机和 ACK/backoff 逻辑与参考分支不同 | 若 ACK 判定或 busy 语义不准，可能无法稳定 1s 广播或无法回睡 | MUST_KEEP | 是否确认“有 CAN 对端 ACK 时，RTC STOP 中也必须 1s 唤醒并广播”？ | 确认保留 | 待确认 |
| REQ-D009-CAN-RTC-002 | 无 CAN 对端或连续无 ACK 时，应回退低频探测，避免休眠中高功耗空发 | `Can_HDX.c:420`, `Can_HDX.c:454`, `Can_HDX.c:2365`; 参考分支 `Can_HDX.c:310`, `Can_HDX.c:421` | D009 当前无 ACK 6 次后 inactive，idle period 10s | 回退过慢会耗电，回退过快可能误判短时通信失败 | KEEP_BUT_REFACTOR | 是否确认无 ACK 场景保持 10s 探测，不要求持续 1s 广播？ | 确认 10s 探测 | 待确认 |
| REQ-D009-CAN-RTC-003 | 保留 D009 普通 SOC LED/socKey 硬件，不迁移当前分支数码管/Charlieplexing | `LedBar.h:7`, `conf_gpio.h:36-46`; 参考分支 `LedBar.h:7-23` | D009 为 PA3/PA2/PA4/PA7 四 LED，PA6 socKey | 直接覆盖 LedBar 会导致显示和唤醒 IO 错误 | MUST_KEEP | 是否确认本次不改 D009 普通 LED 硬件定义，只检查低功耗阻塞条件？ | 确认保留 D009 LED | 待确认 |
| REQ-D009-CAN-RTC-004 | 保留现有 CAN App、老化、IAP、寄存器读写协议兼容 | `Can_HDX.c:40-66`, `Can_HDX.c:1082`, `docs/D009_CAN_COMM_ADAPT_2026-05-25.md` | D009 已支持 0x60/0x61、老化命令、进入 IAP | 协议变更会破坏上位机和量产升级链路 | MUST_KEEP | 是否确认本次不改 CAN ID、命令号、payload 含义和寄存器映射？ | 确认协议不变 | 待确认 |
| REQ-D009-CAN-RTC-005 | CAN 忙闲判断不得让周期 pending mask 长期阻止 RTC 入睡 | D009 `Can_IsBusy()` 含 `s_u16FeidaoCanPendingMask`，参考分支 `Can_IsBusy()` 只看队列/邮箱/命令流 | D009 可能在周期帧 pending 时认为 CAN busy | 可能表现为达到 1s 周期后不再进入 STOP 或频繁浅睡 | CHANGE_NEEDED | 是否确认同步参考分支 busy 语义，让未开始发送的周期 pending 不阻止入睡？ | 确认修改 | 待确认 |
| REQ-D009-CAN-RTC-006 | RTC STOP 前后要集中清 RTC alarm/EXTI17/NVIC pending，并恢复运行态秒中断 | 参考分支 `RTC.c:290-321`, `RTC.c:439-445`; D009 `RTC.c:420-432`, `rtc_sleep.c:850-851` | D009 清理分散，缺少统一 restore API | stale pending 可能导致下一次 STOP 立即唤醒或运行态 RTC 中断异常 | KEEP_BUT_REFACTOR | 是否确认按参考分支增加/同步 RTC stop wakeup 清理接口？ | 确认同步 | 待确认 |
| REQ-D009-CAN-RTC-007 | `GPIO_CMNT_EN` 在 RTC IO 模拟输入后必须能由 CAN wake service 恢复输出并上电发送 | D009 `IOstatus_RTCMode()` 会处理 `GPIO_CMNT_EN`，`InitCan_GPIO()` 会恢复 PB4 输出 | 当前已有基础，但需随 CAN 初始化顺序复核 | 若 PB4 未恢复，RTC 唤醒窗口看不到 CAN_TX/CANH/CANL | MUST_KEEP | 是否确认以示波器验证 PB4、CAN_TX、CANH/CANL 三点？ | 确认验证 | 待确认 |
| REQ-D009-CAN-RTC-008 | 是否同步参考分支 `CAN_ABOM = ENABLE` 自动 bus-off 恢复策略 | D009 `Can_HDX.c:1985` 为 `DISABLE`，参考分支 `Can_HDX.c:1028` 为 `ENABLE` | 两分支 bus-off 管理不同 | 改变 bus-off 恢复行为可能影响故障诊断，但可提升自动恢复 | UNKNOWN | D009 是否允许启用 bxCAN 自动 bus-off 恢复？ | 建议确认后启用 | 待确认 |
