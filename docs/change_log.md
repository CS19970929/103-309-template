# Change Log

状态：部分验证

## 2026-05-27

- 老化模式 `关闭老化模式` 命令语义调整为“提前结束本轮老化时间”：板端调用完成路径，持久化 `DONE` 状态，并让剩余时间归零。
- 老化模式 `开启老化模式` 在完成态下会清零累计时间并开启新一轮，避免完成态返回 `BMS_ERROR`。
- CAN 用户上位机关闭老化模式确认文案改为明确提示“提前结束本轮老化时间，剩余时间将变为 0”。
- 同步更新 CAN App 服务、comm tool 串口协议和用户上位机说明文档。

参考源码：

- `103 + 309/Project/Source/FactoryAging.c`
- `tools/comm_tool_upgrade_ui.py`
- `tools/can_bms_host.py`
- `tools/comm_tool_host.py`

## 2026-05-27 BMS App IO 与 RTC 低功耗配置审查

状态：部分验证

- 新增 `docs/review/bms_app_io_low_power_compare_2026-05-27.md`，对比 commit `5d5564e0d706085e6d50a442588febdb8eaed21a`，梳理正常模式 IO、RTC STOP 前配置、低功耗唤醒后外设恢复路径。
- 本次只输出审查文档和验证计划，未修改源码。
- 当前静态审查未发现明显 IO 映射错配；`PB0 / AFE1_PRO_EN` 唤醒后未显式恢复、`PA3 / 2737_EN` 和 `PB14 / AFE1_CTL` 低功耗状态、RTC 周期 CAN 广播策略需要硬件或上板确认。

参考源码：

- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Runtime.c`
