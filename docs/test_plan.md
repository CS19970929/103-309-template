# Test Plan

状态：部分验证

## 老化模式提前结束

参考源码：

- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `tools/comm_tool_upgrade_ui.py`
- `tools/can_bms_host.py`

验证步骤：

1. 编译 BMS App Release，确认无错误。
2. 编译 `dist/BMS_CommTool_Upgrade_UI.exe`，确认输出文件名保持 `BMS_CommTool_Upgrade_UI.exe`。
3. 使用 CAN 用户上位机开启老化模式，读取老化时间，应显示状态为运行且剩余时间大于 0。
4. 点击 `关闭老化模式`，确认提示中说明会提前结束本轮老化时间。
5. 关闭命令完成后读取老化时间，应显示状态为完成、剩余时间为 0。
6. 断电重启后再次读取老化时间，应保持完成状态且不会自动恢复老化。
7. 如需再次开始老化，先执行 `重置老化时间` 或 `修改老化时间`，再确认状态重新进入运行。

## BMS App IO 与 RTC 低功耗配置审查

状态：部分验证

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

验证步骤：

1. 使用 Keil Release 配置重新编译 BMS App，确认 `FD_Release` 为 `0 Error(s), 0 Warning(s)`。
2. 正常模式测量 `PA3`、`PA10`、`PB0`、`PB4`、`PB5`、`PB14` 默认电平。
3. 正常模式通过 Modbus 读取 ADC/VBUS/电流/温度，确认 ADC 通道顺序为 `PB1 -> PA2 -> PA1` 且数值合理。
4. 正常模式确认 CAN 周期帧和 `0x14F80208` 老化广播。
5. 进入 RTC STOP，测整板低功耗电流，记录 IWDG 开启时 10 秒 RTC 周期唤醒波形。
6. RTC STOP 前后分别测 `PA3`、`PA10`、`PB14`、`PB4` 电平，确认低功耗状态符合原理图。
7. 通过 `PA0 / CHG_IN`、`PA9 / SW`、`PB12`、`PB13` 分别唤醒，确认唤醒源有效。
8. 唤醒后读取 Modbus `0xD000`、`0xD300`，确认 App 运行状态和 SOC 测试入口隔离状态。
9. 唤醒后确认 ADC 采样、AFE 电流/电压、CAN 周期帧、LedBar 显示恢复。
10. CAN 无设备场景确认 `PB4 / CMNT_EN` 不会长期打开；CAN 有设备场景确认 RTC 唤醒短时广播可被接收。
