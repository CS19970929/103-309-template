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

## BMS App IO 与 RTC 低功耗风险

| 风险 ID | 风险描述 | 代码证据 | 影响 | 当前判断 | 建议处理 |
|---|---|---|---|---|---|
| RISK-RTC-IO-001 | `PB0 / AFE1_PRO_EN` 在 `InitIO()` 中配置，但 RTC 唤醒恢复的 `InitIO_rtc()` 未显式恢复 | `conf.c:InitIO()`, `conf.c:InitIO_rtc()` | 如果 PB0 控制 AFE 保护或供电，唤醒后可能状态不确定 | 旧 commit 同样存在，需硬件确认 | 核对原理图并上板测 PB0 唤醒前后状态 |
| RISK-RTC-IO-002 | `PA3 / 2737_EN` 在 RTC 模式排除模拟输入 | `conf.c:IOstatus_RTCMode()` | 若该脚应关闭，可能增加休眠电流 | UNKNOWN | 测 STOP 电流，并确认 PA3 休眠期硬件要求 |
| RISK-RTC-IO-003 | `PB14 / AFE1_CTL` 在 RTC 模式排除模拟输入 | `conf.c:IOstatus_RTCMode()` | 可能影响 AFE 控制或低功耗漏电 | UNKNOWN | 核对 AFE 控制脚原理图，测 STOP 前后电平 |
| RISK-RTC-CAN-001 | RTC 周期唤醒后可短时上电 CAN 服务广播 | `Can_HDX.c`, `rtc_sleep.c` | 增加周期唤醒功耗，但提升休眠通信可见性 | 需求未确认 | 由客户确认休眠中是否必须周期 CAN 广播 |
| RISK-RTC-IWDG-001 | IWDG 开启时 RTC 唤醒周期最大 10 秒 | `RTC.c` | 与极低功耗目标可能冲突 | CONFLICT | 结合整机功耗目标确认 IWDG 与 RTC 周期策略 |
