# 103-309 BMS 风险清单

状态：部分验证

本文以当前源码为第一可信来源，记录本轮 BMS App IO 与 RTC 低功耗配置审查发现的风险。未修改源码，未做上板实测。2026-06-02 追加低功耗需求对齐风险，详见 `docs/review/low_power_requirement_alignment_2026-06-02.md`。

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
- `103 + 309/Project/Source/System_Init.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/DataDeal.h`

## BMS App IO 与 RTC 低功耗风险

| 风险 ID | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 |
|---|---|---|---|---|---|
| RISK-RTC-IO-001 | `PB0 / AFE1_PRO_EN` 在 `InitIO()` 中配置，但 RTC 唤醒恢复的 `InitIO_rtc()` 未显式恢复 | `conf.c:InitIO()`, `conf.c:InitIO_rtc()` | 如果 PB0 控制 AFE 保护或供电，唤醒后可能状态不确定 | 旧 commit 同样存在，需硬件确认 | 核对原理图并上板测 PB0 唤醒前后状态 |
| RISK-RTC-IO-002 | `PA3 / 2737_EN` 在 RTC 模式排除模拟输入 | `conf.c:IOstatus_RTCMode()` | 若该脚应关闭，可能增加休眠电流 | UNKNOWN | 测 STOP 电流，并确认 PA3 休眠期硬件要求 |
| RISK-RTC-IO-003 | `PB14 / AFE1_CTL` 在 RTC 模式排除模拟输入 | `conf.c:IOstatus_RTCMode()` | 可能影响 AFE 控制或低功耗漏电 | UNKNOWN | 核对 AFE 控制脚原理图，测 STOP 前后电平 |
| RISK-RTC-CAN-001 | RTC 周期唤醒后不再主动广播 CAN | `Can_HDX.c`, `rtc_sleep.c`, `RTC.c` | 休眠中 CAN 不再周期可见；换取更低功耗和更简单低功耗链路 | 用户已确认 | 上板验证 CMNT 睡前关闭、唤醒恢复后打开，确认外部唤醒后 CAN 正常恢复 |
| RISK-RTC-IWDG-001 | IWDG 开启时 RTC 唤醒周期最大 10 秒 | `RTC.c` | 与极低功耗目标可能冲突 | CONFLICT | 结合整机功耗目标确认 IWDG 与 RTC 周期策略 |
| RISK-RTC-IWDG-002 | `PROJECT_CFG_WDOG_ENABLE` 与 IWDG 实际行为必须一致 | `Project_Config.h`, `AppInit.c`, `System_Init.c` | 若宏和实际硬件状态漂移，RTC wake 安全窗口会误判 | 已处理，需构建/长稳验证 | 当前默认 `1`；`Init_IWDG()` 和 `IWDG_Feed()` 已按宏门控 |
| RISK-RTC-CAN-002 | CAN busy 查询不能被 debug 消费 | `Can_HDX.c`, `SystemDebug.c`, `Runtime.c`, `rtc_sleep.c` | 若 debug 先更新 CAN 接收计数，低功耗可能误判通信空闲 | 已处理，需通信回归 | 低功耗使用 `Can_IsBusy()`；debug/heartbeat 使用无副作用 `Can_PeekBusy()` |
| RISK-RTC-DBG-001 | DBGMCU 低功耗调试保持只能显式打开 | `conf.h`, `System_Init.c` | 若调试宏误带入 Release，STOP/SLEEP/STANDBY 调试保持会抬高功耗 | 已处理，仍需实测确认 | Release 默认关闭 DBGMCU 低功耗调试位，仅 Debug/显式宏打开；功耗实测读 `DBGMCU->CR` |
| RISK-RTC-AGING-001 | 工厂老化 active 只阻塞 HICCUP RTC STOP | `rtc_sleep.c`, `FactoryAging.c` | 若误扩展为阻塞 deep，会影响低压/关机；若失效则老化计时可能被 RTC STOP 打断 | 已处理，需上板验证 | 保持当前窄范围实现：老化 running 不进 HICCUP RTC STOP，但不阻塞 `DEEP_MODE/NORMAL_MODE` reset sleep |
| RISK-RTC-AFE-001 | AFE sleep block 主判断未接入 | `rtc_sleep.c`, `rtc_sleep_afe_sh367309.c` | AFE 异常、PCHG、保护状态下可能进入 STOP | UNKNOWN | 确认 SH367309 状态和 HICCUP sleep 关系，再决定接入 RTC block 或删除保留 reason |
| RISK-RTC-PARAM-001 | 上位机可写的普通休眠/RTC 参数当前未进入主策略 | `DataDeal.h`, `Sci_Upper.c`, `rtc_sleep.c`, `RTC.c` | 参数读写与真实行为不一致，维护和调试误导 | CHANGE_NEEDED | 确认保留/接入/删除，不要继续保留“看似有效”的参数 |
| RISK-RTC-WRAPPER-001 | `app_lowpower.c` 曾暴露多组非主路径 wrapper | `Runtime.c`, `rtc_sleep.c`, `rtc_sleep.h` | 增加低功耗入口数量和阅读成本 | 已处理 | 已删除未使用 wrapper 和 `app_lowpower.c/h`，只保留真实 `Runtime_RunOnce()->rtc_sleep()` 主路径 |
