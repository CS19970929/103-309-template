# S4 低功耗唤醒与通信用户确认包

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/SleepDeal.c`, `103 + 309/Project/Source/conf/conf.c`, `103 + 309/Project/Source/rtc_sleep.c`, `103 + 309/Project/Source/app_lowpower.c`, `103 + 309/Project/Source/Sci_Upper.c`, `103 + 309/Project/Source/Can_HDX.c`, `103 + 309/Project/Source/RTC.c`, `docs/review/low_power_comm_wake_gate_plan_2026-05-31.md`
最后更新时间：2026-05-31
未确认事项：UART1/CMNT/MCU_WK 在 reset-sleep 下是否应进入正常运行、按键唤醒保持时间、RS485 通信活跃真相源、CAN bus active 是否阻塞 STOP、休眠中 CAN 周期可见性、关键 CAN ACK 重试策略、IWDG 与 RTC wake period 的功耗取舍。

## 1. 本确认包目标

本确认包只用于确认 S4 低功耗/通信的产品需求和安全边界，不代表已经修复低功耗问题，也不允许直接修改源码。

确认前禁止改动：

1. `SleepDeal.c` 中 BKP sleep flag 值、sleep-startup 语义和 `MCU_RESET()` 休眠路径。
2. `conf.c` 中唤醒 GPIO 电平、EXTI trigger 和 NVIC 入口。
3. `rtc_sleep.c`、`app_lowpower.c` 中 STOP 进入/退出顺序、IWDG 喂狗节奏和 Flash busy 阻塞。
4. `Can_HDX.c` 中 CAN ID、payload、App command、`0x14F80208` 老化广播含义。
5. Modbus/CAN/上位机客户可见协议。

确认后的第一步也只应处理 wake source matrix，不应把通信活跃计数、CAN 重试和 IWDG 周期策略混在同一批源码提交中。

## 2. 当前源码事实

| Fact ID | 当前源码事实 | 证据 | 判断 |
|---|---|---|---|
| S4-FACT-001 | `main -> Runtime_RunOnce -> Runtime_RunIoAndPowerTasks -> LP_Task()` 是运行态低功耗入口之一 | `Runtime.c:62-67`, `app_lowpower.c:90-102` | MUST_KEEP |
| S4-FACT-002 | reset-sleep 通过 BKP `BKP_DR2/BKP_DR3` 保存 sleep flag，写 flag 后 AFE sleep 并 MCU reset | `SleepDeal.c:83-139` | MUST_KEEP |
| S4-FACT-003 | `InitWakeUp_NormalMode()` 配置 UART1 RX `PB7` rising、CHG_IN `PA0` falling、INT_WK_CMNT `PB12` rising、MCU_WK `PB13` rising | `conf.c:215-266` | 当前 EXTI 覆盖多源 |
| S4-FACT-004 | `IsSleepWakeupValid()` 当前只把 charger active 和 key hold 视为合法退出 sleep-startup 的条件 | `SleepDeal.c:22-80` | 与 EXTI 源不完全一致 |
| S4-FACT-005 | 按键唤醒阈值源码为 `DI1_LONG_PRESS_WAKE_10MS 50`，即约 500 ms；旁边注释仍像旧口径 | `SleepDeal.c:9`, `SleepDeal.c:51-55` | 需要按产品体验确认 |
| S4-FACT-006 | 运行态低功耗 block reason 会因 `Sci_IsAnyPortBusy()`、`Can_IsBusy()`、Flash busy、fault、LED active、IWDG unsafe 等阻塞 | `app_lowpower.c:17-75` | MUST_KEEP |
| S4-FACT-007 | `RTC_ExtComCnt` 是全局 `UINT8`，USART RX ISR 每字节自增，存在 8-bit 回绕和非 `volatile` 风险 | `SleepDeal.c:4`, `Sci_Upper.c:1465-1483` | CHANGE_NEEDED |
| S4-FACT-008 | CAN TX failed/timeout 会记录并清当前 mailbox；未按周期帧/关键 ACK 做类型化重试 | `Can_HDX.c:421-448` | 需要可靠性策略确认 |
| S4-FACT-009 | `Can_RtcWakeService()` RTC 唤醒时会短时上电 CAN，active bus 发 1000ms/5000ms 周期帧，idle bus 发 probe，最多等待 `150 * 10ms` | `Can_HDX.c:1128-1182` | 功耗与在线可见性冲突 |
| S4-FACT-010 | CAN bus active 当前主要影响 RTC wake period/service，不直接作为 `LP_BLOCK_COMM` 阻塞 STOP；CAN busy 才阻塞 | `app_lowpower.c:32-35`, `Can_HDX.c:1120-1125` | 旧文档口径需防误改 |

## 3. 用户必须确认的决策

| Decision ID | 需要确认的问题 | 可选方向 | Codex 建议 | User decision placeholder |
|---|---|---|---|---|
| S4-DEC-001 | UART1 RX 在 reset-sleep 下唤醒后是否应进入正常运行？ | 进入正常运行 / 只唤醒预览后继续睡 / 只用于 runtime HICCUP | 默认保持现状，先补 matrix；若上位机要求首帧唤醒设备，再单独允许进入运行 | 待确认 |
| S4-DEC-002 | INT_WK_CMNT 在 reset-sleep 下是否是合法启动源？ | 进入正常运行 / 只唤醒 CAN/CMNT 预览 / 不作为合法启动 | 默认保持现状，先确认 CMNT 硬件语义和客户在线需求 | 待确认 |
| S4-DEC-003 | MCU_WK 在 reset-sleep 下是否是合法启动源？ | 进入正常运行 / 只阻塞 runtime sleep / 保持当前不合法启动 | 默认保持当前运行态阻塞，reset-sleep 是否启动由硬件需求确认 | 待确认 |
| S4-DEC-004 | 按键唤醒保持时间按哪个口径执行？ | 当前 500 ms / 3 s / 其他产品值 | 建议以当前 500 ms 作为源码事实，若改为 3 s 必须同步 LED/交互测试 | 待确认 |
| S4-DEC-005 | RS485 通信活跃真相源是否允许替换？ | `volatile uint16_t/uint32_t` 计数 / last-activity tick / 保持现状只加观测 | 建议改为 ISR-safe last-activity tick，并继续保留 `Sci_IsAnyPortBusy()` 阻塞 | 待确认 |
| S4-DEC-006 | CAN bus active 是否应阻塞 STOP？ | 阻塞 STOP / 只调整 RTC period/service / 完全不影响低功耗 | 建议继续不永久阻塞 STOP，只控制 RTC wake period/service | 待确认 |
| S4-DEC-007 | 休眠中是否必须保持 CAN 周期可见？ | 保持 1s/5s 可见 / 只发 probe / 关闭休眠 CAN / 按客户模式配置 | 建议保留当前默认，实测功耗后再决定是否降频或配置化 | 待确认 |
| S4-DEC-008 | CAN 关键帧是否需要有限重试？ | 所有帧同策略可丢 / 只对 ACK/IAP/写寄存器/老化控制重试 / 不重试但上位机重发 | 建议周期帧可丢，关键 ACK 做有限重试，且不能改 CAN ID/payload | 待确认 |
| S4-DEC-009 | IWDG 开启时 RTC wake period 是否必须保持 `<=10s`？ | 保持当前安全窗口 / 为极低功耗调整 IWDG 策略 / 分产品配置 | 建议保持当前 IWDG 安全窗口，功耗优化放到实测后单独阶段 | 待确认 |

## 4. 用户填写模板

| Decision ID | 用户决策 | 备注/约束 |
|---|---|---|
| S4-DEC-001 |  |  |
| S4-DEC-002 |  |  |
| S4-DEC-003 |  |  |
| S4-DEC-004 |  |  |
| S4-DEC-005 |  |  |
| S4-DEC-006 |  |  |
| S4-DEC-007 |  |  |
| S4-DEC-008 |  |  |
| S4-DEC-009 |  |  |

## 5. 用户确认后的执行计划

| 阶段 | 文件范围 | 任务 | 禁止改动 | 验证 | 回滚 |
|---|---|---|---|---|---|
| S4-D1 | `SleepDeal.c`, `conf.c`, `rtc_sleep_port.c`, 低功耗文档 | 只处理 wake source matrix 和合法启动源 | 不改 BKP flag 值、不改唤醒 GPIO 电平、不改 CAN/Modbus 协议 | STOP/reset-sleep 下 CHG/key/UART1/CMNT/MCU_WK 分源实测；唤醒后 `0xD000` 可读 | 单独 revert S4-D1 commit |
| S4-D2 | `SleepDeal.c`, `Sci_Upper.c`, `rtc_sleep.c` | 替换 `RTC_ExtComCnt` 真相源或补 ISR-safe 通信活跃标记 | 不改串口波特率、Modbus 帧格式和 `Sci_IsAnyPortBusy()` 语义 | 连续 Modbus 收发不入 STOP；空闲后可入 STOP；1 秒内高字节量不漏判 | 单独 revert S4-D2 commit |
| S4-D3 | `Can_HDX.c`, `RTC.c`, `docs/protocol/can_protocol.md` | 调整 CAN RTC period/slice 或关键 ACK 重试 | 不改 CAN ID、payload、App command、`0x14F80208` 含义 | CAN 周期帧、App ACK/IAP/read/write/read_block、老化广播抓包回归 | 单独 revert S4-D3 commit |
| S4-D4 | `RTC.c`, IWDG/低功耗文档和测试计划 | 评估 IWDG 与 RTC wake period 取舍 | 不和 wake source、通信活跃、CAN 重试混改 | 8h 运行、STOP 电流、IWDG 不复位、RTC 周期准确 | 单独 revert S4-D4 commit |

## 6. 当前验证边界

本确认包只完成源码阅读和文档化确认：

- 未修改 `.c/.h`、Keil 工程、编译宏、协议行为或烧录脚本。
- 未运行 Keil/ARMCC Release 编译。
- 未连接 COM4、CAN adapter、ST-Link 或实物 BMS 板。
- 未做 STOP 电流、分源唤醒、Modbus 首帧恢复、CAN 抓包或 IWDG 长稳测试。

因此，本文件只能作为 S4 进入源码阶段前的用户确认输入，不能作为低功耗通信问题已修复的证明。

