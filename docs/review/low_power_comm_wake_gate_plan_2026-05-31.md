# 低功耗唤醒与通信门禁执行方案

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/SleepDeal.c`, `103 + 309/Project/Source/conf/conf.c`, `103 + 309/Project/Source/rtc_sleep.c`, `103 + 309/Project/Source/rtc_sleep_port.c`, `103 + 309/Project/Source/app_lowpower.c`, `103 + 309/Project/Source/Sci_Upper.c`, `103 + 309/Project/Source/Can_HDX.c`, `103 + 309/Project/Source/RTC.c`, `docs/design/low_power_design.md`, `docs/protocol/can_protocol.md`
最后更新时间：2026-05-31
未确认事项：reset-sleep 下 UART1/CMNT/MCU_WK 是否应进入正常运行、RS485 活跃判定是否允许改为宽计数或 last-activity tick、休眠中是否必须保持 CAN 周期可见、CAN 关键 ACK/IAP/写寄存器帧是否需要重试。

## 1. 目标

本方案只定义低功耗唤醒、通信活跃和 CAN RTC 服务的确认项与门禁，不修改源码、唤醒源电平、BKP boot flag、CAN ID、Modbus 寄存器或上位机协议。

S4 阶段目标是把三类低功耗/通信风险拆成可执行的小阶段：

1. reset-sleep 合法唤醒源必须与 EXTI 配置一致，至少形成 wake source matrix。
2. 通信活跃判定不能因 `UINT8` 回绕或非 `volatile` 漏判，避免上位机通信中误入 STOP。
3. CAN RTC wake service 的周期可见性、关键帧可靠性和功耗目标必须明确取舍。

## 2. 当前源码事实

| ID | 当前事实 | 证据 | 判断 |
|---|---|---|---|
| FACT-LP-001 | reset-sleep 使用 BKP flag 记录 `NORMAL/HICCUP/DEEP/CHARGER_WAKE` 并 MCU reset 后在 `IsSleepStartUp()` 处理 | `SleepDeal.c` | MUST_KEEP，不能改 flag 含义 |
| FACT-LP-002 | `InitWakeUp_Base()` 配置 CHG_IN `PA0` falling 和 SW `PA9` falling | `conf.c` | MUST_KEEP |
| FACT-LP-003 | `InitWakeUp_NormalMode()` 额外配置 UART1 RX `PB7` rising、INT_WK_CMNT `PB12` rising、MCU_WK `PB13` rising | `conf.c` | 当前 EXTI 覆盖多源 |
| FACT-LP-004 | `IsSleepWakeupValid()` 只把 charger active 和 key hold 视为合法退出 sleep-startup 的条件 | `SleepDeal.c` | 与 EXTI 源不完全一致 |
| FACT-LP-005 | key hold 阈值为 `DI1_LONG_PRESS_WAKE_10MS 50`，约 500 ms；注释存在旧口径 | `SleepDeal.c` | 需要产品体验确认 |
| FACT-LP-006 | RTC runtime HICCUP STOP 周期唤醒后会恢复外设，若是 RTC wake 且无异常，则做 SOC RTC 补偿并运行 CAN RTC wake service，然后继续 sleep loop | `rtc_sleep.c`, `rtc_sleep_port.c` | 当前保留休眠中周期服务能力 |
| FACT-LP-007 | RTC wake period 默认来自 `Can_GetIdleRtcPeriodSeconds()`，IWDG 开启时被限制到 `<= 10s` | `RTC.c` | 功耗与在线可见性冲突 |
| FACT-LP-008 | `RTC_ExtComCnt` 是全局 `UINT8`，USART RX ISR 每字节自增，低功耗每秒比较一次判断外部通信活动 | `SleepDeal.c`, `Sci_Upper.c`, `rtc_sleep.c` | 存在回绕/非 volatile 风险 |
| FACT-LP-009 | `LP_BuildBlockReason()` 当前用 `Sci_IsAnyPortBusy()` 和 `Can_IsBusy()` 阻塞低功耗；CAN bus active 不直接作为 block bit，而是影响 RTC wake period/service | `app_lowpower.c`, `Can_HDX.c`, `RTC.c` | 旧“CAN active 永久阻塞”表述需要修正 |
| FACT-LP-010 | `Can_RtcWakeService()` 最多等待 `150 * 10ms`，active bus 发 1000ms/5000ms 周期帧，idle bus 发 probe | `Can_HDX.c` | 需要功耗和关键帧策略确认 |

## 3. Wake Source Matrix

| Wake source | GPIO/来源 | EXTI/入口 | 当前是否唤醒 STOP | 当前是否被 `IsSleepWakeupValid()` 视为合法启动 | 风险 |
|---|---|---|---|---|---|
| CHG_IN | `PA0` | Base/Normal/RTC/Deep falling | 是 | 是，立即返回合法并写 charger wake flag | 行为清晰 |
| SW / key | `PA9` / `MCUI_ENI_DI1` | Base/Normal/RTC/Deep falling | 是 | 是，但需要持续约 500 ms | 注释与阈值口径需确认 |
| UART1 RX | `PB7` | Normal/RTC rising | 是 | 否；若无 charger/key，会再次 STOP | 上位机首帧可能只唤醒但不进入运行 |
| INT_WK_CMNT | `PB12` | Normal/RTC rising | 是 | 否；若无 charger/key，会再次 STOP | CAN/CMNT 唤醒可能无效 |
| MCU_WK | `PB13` | Normal/RTC rising | 是 | 否；但运行态低功耗会因 MCU_WK active 阻塞 | 主开关/外部唤醒语义不统一 |
| RTC Alarm | RTC | HICCUP runtime sleep | 是 | reset-sleep startup 路径不把 RTC alarm 当作合法退出；runtime HICCUP 会继续周期服务 | 必须区分 reset-sleep 与 runtime HICCUP |

## 4. 当前缺口

| 缺口 | 影响 | 当前处理 |
|---|---|---|
| EXTI 配置源多于合法启动源 | UART/CMNT/MCU_WK 可唤醒 STOP，但可能不进入正常运行 | 先确认哪些源应进入正常运行，哪些只允许 UI 预览或保持睡眠 |
| key hold 代码值与旧注释口径不一致 | 用户体验和唤醒可靠性不清 | 先按源码记录 500 ms，后续由用户确认 |
| `RTC_ExtComCnt` 为 `UINT8` 且非 `volatile` | 1 秒内 256 字节倍数回绕可能漏判；编译优化也可能带来不确定性 | 后续建议改为 `volatile uint16_t/uint32_t` 或 last-activity tick |
| `LP_BuildBlockReason()` 不直接把 `Can_IsBusActive()` 当 block | CAN active 与低功耗关系容易被旧文档误读 | 文档修正为 CAN busy 阻塞，bus active 调整 RTC 服务周期 |
| `Can_RtcWakeService()` 最多 1.5 s | 保留 CAN 可见性但增加周期唤醒功耗，关键 ACK 是否可靠仍需确认 | 先确认客户在线需求，再决定是否分类重试或缩短 slice |

## 5. 后续门禁规则草案

这些规则是待确认的执行方案，不代表本轮已经修改代码。

| Gate ID | 门禁规则 | 检查来源 | 建议失败策略 |
|---|---|---|---|
| G-LP-001 | 每个 EXTI wake source 必须在 wake source matrix 中声明：唤醒 STOP、合法启动、UI 预览、保持睡眠中的一种 | `conf.c`, `SleepDeal.c`, `rtc_sleep_port.c` | 未声明时禁止改唤醒代码 |
| G-LP-002 | reset-sleep 合法启动条件变更不得改 BKP flag 值和 sleep flag 语义 | `SleepDeal.c` | fail |
| G-LP-003 | 通信活跃计数必须可 ISR 安全读取，不能使用易漏判的 8-bit 非 volatile 真相源 | `Sci_Upper.c`, `rtc_sleep.c` | fail |
| G-LP-004 | `Sci_IsAnyPortBusy()` 与 `Can_IsBusy()` 必须继续阻塞进入 STOP | `app_lowpower.c` | fail |
| G-LP-005 | CAN bus active 的作用必须明确为“调整 RTC period/service”或“阻塞 STOP”，不能同时有冲突文档 | `Can_HDX.c`, `RTC.c`, `docs/*` | fail |
| G-LP-006 | CAN RTC wake service 变更不得改变默认 CAN ID、payload、App command 语义、`0x14F80208` 老化广播含义 | `Can_HDX.c`, `CanFeidaoFrames.c`, `tools/can_bms_host.py` | fail |
| G-LP-007 | IWDG 开启时 RTC wake period 必须保持 `<= 10s`，除非同时调整 IWDG 策略并实测 | `RTC.c` | fail |

## 6. 需要用户确认的需求表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-LP-WAKE-001 | reset-sleep 合法唤醒源必须确认 | `conf.c` 配置 UART1/CHG/CMNT/MCU_WK；`SleepDeal.c` 只接受 charger/key | 多个源能唤醒 STOP，但不一定进入正常运行 | 上位机或 CMNT 唤醒后设备继续睡，首帧丢失 | UNKNOWN | UART1/CMNT/MCU_WK 是否应进入正常运行？ | 建议建立 source-by-source 策略；默认先保持现状 | 待确认 |
| REQ-LP-WAKE-002 | key hold 唤醒时长必须确认 | `DI1_LONG_PRESS_WAKE_10MS 50` | 约 500 ms；注释存在旧口径 | 用户体验和误触发边界不清 | UNKNOWN | 按键唤醒是 500 ms、3 s，还是其他值？ | 先按当前 500 ms 记录，确认后再改 | 待确认 |
| REQ-LP-COMM-001 | 通信活跃真相源必须 ISR 安全且不易回绕 | `RTC_ExtComCnt` 为 `UINT8`; USART RX ISR 自增；每秒比较 | 8-bit 回绕或非 volatile 可能漏判 | 通信中误入 STOP | CHANGE_NEEDED | 是否允许改为 `volatile uint16_t/uint32_t` 或 last-activity tick？ | 建议改 last-activity tick，并保留 `Sci_IsAnyPortBusy()` | 待确认 |
| REQ-LP-CAN-001 | CAN active 对低功耗的作用必须确认 | `LP_BuildBlockReason()` 查 `Can_IsBusy()`；`RTC_GetWakeupPeriodSeconds()` 查 `Can_GetIdleRtcPeriodSeconds()` | CAN busy 阻塞；CAN active 调整 RTC wake 周期 | 文档/实现口径不一致会误改 | CONFLICT | CAN active 是否应阻塞 STOP，还是只调整 RTC 周期？ | 建议继续不永久阻塞 STOP，只控制 RTC service 周期 | 待确认 |
| REQ-LP-CAN-002 | 休眠中 CAN 周期可见性必须确认 | `Can_RtcWakeService()` active 发 1000ms/5000ms，idle 发 probe | 保留在线可见性但增加功耗 | 低功耗与客户通信需求冲突 | CONFLICT | 休眠中是否必须 1s/5s CAN 可见？ | 保留当前默认，实测后再缩短/关闭 | 待确认 |
| REQ-LP-CAN-003 | CAN 关键帧是否需要类型化重试必须确认 | `feidao_can_service_tx()` failed/timeout 后记录并清 mailbox | 周期帧可丢，关键 ACK 也可能丢 | IAP/写寄存器/老化 ACK 丢失 | CHANGE_NEEDED | ACK/IAP/写寄存器/老化控制是否需要有限重试？ | 建议只对关键 ACK 做有限重试，周期帧继续可丢 | 待确认 |
| REQ-LP-IWDG-001 | IWDG 与 RTC wake period 的取舍必须确认 | `RTC_GetWakeupPeriodSeconds()` 限制 `<=10s` | IWDG 安全优先，功耗偏高 | 极低功耗目标受限 | CONFLICT | 是否允许为极低功耗调整 IWDG 策略？ | 默认保持 IWDG 安全窗口，先实测功耗 | 待确认 |

## 7. 分阶段执行计划

| 阶段 | 文件范围 | 动作 | 禁止改动 | 验证 | 回滚 |
|---|---|---|---|---|---|
| S4-D0 | `docs/review/*`, `docs/design/low_power_design.md`, `docs/README.md` | 建立 wake/comm/CAN gate plan，修正文档口径 | 禁止改 `.c/.h`、唤醒源电平、CAN ID、协议 | `python3 tools/project_check.py -q`, `git diff --check` | 回滚本轮文档 patch |
| S4-D1 | `SleepDeal.c`, `conf.c`, `rtc_sleep_port.c` | 用户确认后只处理 wake source matrix 与合法启动源 | 不改 BKP flag、不改外部协议、不改 RTC/CAN service | STOP/reset-sleep 下 CHG/key/UART/CMNT/MCU_WK 实测 | 单独 revert |
| S4-D2 | `SleepDeal.c`, `Sci_Upper.c`, `rtc_sleep.c` | 用户确认后替换 `RTC_ExtComCnt` 真相源 | 不改 Modbus 帧格式和串口波特率 | 连续 Modbus 收发中不入 STOP，空闲后可入 STOP | 单独 revert |
| S4-D3 | `Can_HDX.c`, `RTC.c`, `docs/protocol/can_protocol.md` | 用户确认后调整 CAN RTC period/slice 或关键 ACK 重试 | 不改 CAN ID/payload/App command 含义 | CAN 周期帧、App ACK/IAP/read/write/read_block、老化广播回归 | 单独 revert |
| S4-D4 | `RTC.c`, IWDG 文档 | 用户确认后评估 IWDG/RTC 周期策略 | 不和 wake source、CAN 重试混改 | 8h 运行、STOP 电流、IWDG 不复位 | 单独 revert |

## 8. 当前验证边界

本轮只做到源码和文档层面的证据同步：

- 未修改低功耗、串口、CAN、RTC 或 IWDG 源码。
- 未连接 COM4、CAN adapter、ST-Link 或真实 BMS 板。
- 未做 STOP 电流、唤醒源、CAN 抓包、Modbus 首帧恢复测试。
- 未运行 Keil/ARMCC 真构建。

因此，本轮不能声称低功耗通信问题已修复，只能作为后续确认和小步实现的输入。

## 9. 下一步建议

1. 用户先确认 `REQ-LP-WAKE-001`：UART1/CMNT/MCU_WK 在 reset-sleep 下到底是合法启动源、只唤醒预览，还是应继续睡。
2. 确认后优先做 S4-D1 wake source matrix，不碰 `RTC_ExtComCnt` 和 CAN。
3. 第二步再做 S4-D2 通信活跃真相源，把 `UINT8` 计数替换为更稳的 ISR 活动标记或 last-activity tick。
4. CAN RTC service 和 IWDG 策略放到后续独立阶段，避免和唤醒源逻辑混在一个提交里。
