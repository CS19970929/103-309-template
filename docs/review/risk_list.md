# 103-309 BMS 风险清单

状态：部分验证

本文以当前源码为第一可信来源，记录本轮 BMS App IO 与 RTC 低功耗配置审查发现的风险。未修改源码，未做上板实测。

## 参考源码

- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Runtime.c`

## 2026-05-31 七 agent 综合风险

状态：部分验证。本节来自 7 个子 agent 的只读审查，并以当前源码做交叉核对。排序按安全优先、低功耗第二、可读性第三；本节不代表已允许修改源码。

| 风险 ID | 风险等级 | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 | 需要确认 |
|---|---|---|---|---|---|---|---|
| RISK-7A-P0-001 | P0 | App 链接区与持久化存储区缺少硬隔离 | `CommomSH367309_16series_103RCT6_C.sct:5-13` 为 `0x08004800 + 0x20000`；`Flash.h:7-29` 从 `0x0801C000` 定义存储页 | App 体积增长可能覆盖 AFE/RW/LOG/SOC 存储页 | 当前 scatter 与存储区重叠风险成立，需以 Keil 最终 map 再确认 | 修链接区上限、map 检查、bin size/vector 检查；继续禁止裸写 `0x08000000` | MCU 容量、App 允许最大结束地址、后 16KB 是否固定保留 |
| RISK-7A-P0-002 | P0 | IAP SRAM mailbox 未被 scatter 保留 | `Flash.c:12` 使用 `0x20004FE0`；scatter `RW_IRAM1` 覆盖到 `0x20005000` | mailbox 可能被 RW/ZI 覆盖，IAP 请求不稳定 | 需要修 scatter 或确认 IAP mailbox 机制 | 保留 `0x20004FE0` 尾部空间并加入 map 门禁 | IAP 固件是否固定读取 `0x20004FE0` |
| RISK-7A-P0-003 | P0 | AFE 参数写入缺少实时范围校验 | `SH367309_DataDeal.c:249-283` 直接写 AFE 参数 curValue 并保存；随后可触发 MTP 更新 | 非法保护阈值或温度参数可能进入硬件配置 | 写入口校验不足，安全边界高风险 | 写前复用上电加载同级 min/max 校验；非法值返回协议错误，不保存、不置写标志 | 量产是否允许主机写 AFE 参数；错误码兼容策略 |
| RISK-7A-P0-004 | P0 | AFE 通信失败后没有明确 fail-safe 动作 | `I2C_AFE1.c:617-645` 失败只置 `ERROR_AFE1`；`SH367309_Func.c:307-335` 读失败无明确处理 | AFE 状态未知时仍可能沿用旧数据和输出状态 | 必须定义安全策略后才能改 | 连续失败后标记数据无效、阻塞低功耗、关闭 CTLC/MOS 或复位，按确认策略执行 | AFE 失败时断输出/保持/休眠/复位的产品要求 |
| RISK-7A-P0-005 | P0/P1 | AFE watchdog 未启用 | `SH367309_Func.c:136-148` 注释未置 `ENWDT`，仅开启 CADC/MOS 控制位 | MCU 卡死或 I2C 异常时缺少 AFE 独立 watchdog 关 MOS/均衡保护 | 需要安全策略确认 | 评估启用 AFE watchdog；若不启用，写清风险接受依据和替代闭环 | 309 硬件和客户安全要求 |
| RISK-7A-P1-001 | P1 | `new_todo_logi()` 中 MOS/CTLC/UL 逻辑在量产路径运行 | `DataDeal.c:1086-1145` 每 200ms 运行，硬编码 `95/75C` 并调用 `close_ctlc()/open_ctlc()` | CTLC 恢复条件不完整，认证/保护逻辑可能误动作 | 生产路径中有未确认 TODO 和硬编码阈值 | 先确认需求；再改成显式配置隔离、补 AFE fault/MOS 读回条件，或删除历史路径 | UL/熔断/CTLC 是否当前客户需求 |
| RISK-7A-P1-002 | P1 | reset-sleep 唤醒源和合法唤醒判定不一致 | `conf.c:215-266` 配置 UART1/CHG/CMNT/MCU_WK EXTI；`SleepDeal.c:22-65` 只接受充电器/按键 | UART/CMNT 唤醒后可能继续 STOP，通信唤醒无效 | 低功耗唤醒矩阵必须确认 | 建立 wake source matrix；按确认扩展合法唤醒源或文档化限制 | 哪些源可进入正常运行，哪些只做 UI 预览 |
| RISK-7A-P1-003 | P1 | RS485 活动计数可能漏判通信活跃 | `SleepDeal.c:4` `RTC_ExtComCnt` 为非 volatile `UINT8`；`Sci_Upper.c:1465-1483` ISR 自增 | 计数回绕或优化导致低功耗误判空闲 | 通信与低功耗耦合风险明确 | 改为 `volatile` 宽计数或 last_activity_tick，并结合 `Sci_IsAnyPortBusy()` | 是否允许调整低功耗判定数据结构 |
| RISK-7A-P1-004 | P1 | CAN 发送和 RTC wake 服务可靠性/功耗冲突 | `Can_HDX.c:421-448` failed/timeout 后清 mailbox；`Can_HDX.c:1128-1182` RTC wake 最多等待约 1.5s | 关键帧可能丢失；低功耗唤醒窗口阻塞主循环/Modbus | 需要按帧类型定义策略 | 周期帧可丢，ACK/IAP/写寄存器/老化控制有限重试；RTC 服务改 bounded slice | 是否必须休眠中周期 CAN 可见；关键帧重试要求 |
| RISK-7A-P1-005 | P1 | 协议空实现可能正 ACK | `Sci_Upper.c:963-995` SOC 测试主体 `#if 0`；`Sci_Upper.c:1758-1805` 校准写 `#if 0`；`Sci_Upper.c:1876-1883` 铜损/RTC 写为空；`Sci_Upper.c:2015-2067` 校准 reset 主体 `#if 0`；`Sci_ModbusProcessFrame()` 默认 POS ACK | 上位机/CAN App 误判写入成功，测试和调参结果错误 | 需要修协议语义或文档确认 | 废弃入口显式返回不支持；测试入口只在 Factory/Test profile 开启；校准/RTC/铜损是否恢复需单独确认 | 这些地址是废弃、占位还是要恢复 |
| RISK-7A-P1-006 | P1 | SOC 校准默认不受故障态阻断，满电锚点缺少 taper/charger 条件 | `Project_Config.h:286-292` 两个 block 宏为 `0`；`SocEnhance.c:913-940` 满电确认按电压/压差；`SocEnhance.c:1268-1295` 逐步锚到 100% | 故障态可能误校准；满电 100% 可能过早 | 需要产品体验与安全策略确认 | 若安全优先，开启故障阻断并增加 charger-present/taper 条件；否则文档化当前体验策略 | SOC 校准和满电锚点真实需求 |
| RISK-7A-P1-007 | P1 | Flash/EEPROM 存储失败不可见 | `System_Monitor.c:128-148` `ERROR_EEPROM_STORE` 不递增；`Flash.c:496-660` 保存失败调用该错误；`app_lowpower.c:48-50` 只因 busy/pending 阻塞低功耗 | 参数/SOC/日志写失败可能不上报，低功耗也可能不阻塞 | 错误状态链路不完整 | 定义存储失败状态：可见、可清除、是否阻塞低功耗/保护动作 | 存储失败是否影响充放电和低功耗 |
| RISK-7A-P1-008 | P1 | 升级参数策略默认会清多类现场数据 | `Project_Config.h:411-447` policy 和多项 reset 开关默认开启；`EEPROM.c:255-287` SOC table reset 受 runtime table 门控，老化 reset 调用 host running 路径 | 升级后可能清保护参数、SOC、事件记录、老化时间；老化流程可能被重启/恢复 | 需要按批次/特殊包确认 | 把清参策略从量产默认中隔离，或文档化本批升级动作；SOC table 和老化 reset 口径必须写清 | `0x0005` 是否当前量产要求；老化 reset 是否允许进入 running-from-host |

## BMS App IO 与 RTC 低功耗风险

| 风险 ID | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 |
|---|---|---|---|---|---|
| RISK-RTC-IO-001 | `PB0 / AFE1_PRO_EN` 在 `InitIO()` 中配置，但 RTC 唤醒恢复的 `InitIO_rtc()` 未显式恢复 | `conf.c:InitIO()`, `conf.c:InitIO_rtc()` | 如果 PB0 控制 AFE 保护或供电，唤醒后可能状态不确定 | 旧 commit 同样存在，需硬件确认 | 核对原理图并上板测 PB0 唤醒前后状态 |
| RISK-RTC-IO-002 | `PA3 / 2737_EN` 在 RTC 模式排除模拟输入 | `conf.c:IOstatus_RTCMode()` | 若该脚应关闭，可能增加休眠电流 | UNKNOWN | 测 STOP 电流，并确认 PA3 休眠期硬件要求 |
| RISK-RTC-IO-003 | `PB14 / AFE1_CTL` 在 RTC 模式排除模拟输入 | `conf.c:IOstatus_RTCMode()` | 可能影响 AFE 控制或低功耗漏电 | UNKNOWN | 核对 AFE 控制脚原理图，测 STOP 前后电平 |
| RISK-RTC-CAN-001 | RTC 周期唤醒后可短时上电 CAN 服务广播 | `Can_HDX.c`, `rtc_sleep.c` | 增加周期唤醒功耗，但提升休眠通信可见性 | 需求未确认 | 由客户确认休眠中是否必须周期 CAN 广播 |
| RISK-RTC-IWDG-001 | IWDG 开启时 RTC 唤醒周期最大 10 秒 | `RTC.c` | 与极低功耗目标可能冲突 | CONFLICT | 结合整机功耗目标确认 IWDG 与 RTC 周期策略 |
