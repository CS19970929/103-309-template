# Test Plan

## CAN 一键升级缓存写入阶段 BMS 保活

状态：部分验证

参考源码：

- `tools/comm_tool_upgrade_ui.py`
- `tools/comm_tool_host.py`

验证步骤：

1. 打开 `dist/BMS_CommTool_Upgrade_UI.exe`，选择 D009 BMS App bin，执行“一键升级”。
2. 在日志中确认写入缓存前出现“写入 comm tool 缓存期间启用 BMS 保活”，并在缓存写入期间周期出现 `BMS 保活` 日志。
3. 用调试器或日志观察 BMS 侧 `g_stLowPowerRtcStatus.blockReason`，缓存写入期间不应因空闲计时进入 RTC。
4. 缓存校验完成后应正常进入“开始 CAN 升级 BMS”，不再出现 BMS 已睡眠导致的 0% 超时/错误。

已验证：

- `py -3.9 tools\comm_tool_upgrade_ui.py --self-test` 通过。
- 已执行 `tools\build_comm_tool_upgrade_ui_exe.ps1 -Clean`，覆盖生成 `dist\BMS_CommTool_Upgrade_UI.exe`。

## D009 RTC blockReason=8 修复补充验证

状态：部分验证

参考源码：

- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep.h`
- `103 + 309/Project/Source/main.c`

验证步骤：

1. 空闲、无充放电、无外部通信时，确认 `g_stLowPowerRtcStatus.blockReason` 不再长期停留在 `LOW_POWER_RTC_BLOCK_FRAMEWORK(8)`。
2. 若仍被低功耗框架阻塞，同时读取 `g_stLowPowerRtcStatus.frameworkBlockReason`：`0x00000004` 表示通信忙，`0x00000020` 表示 Flash 忙，`0x00000040` 表示升级写入，`0x00000080` 表示系统故障/加热，`0x00000200` 表示 IWDG 周期不安全。
3. 保持 `g_stCellInfoReport.unMdlFault_Third` 中非 AFE 硬件类历史/软故障置位，确认 RTC 轻休眠入口仍可按空闲计时进入；若 AFE `BSTATUS` 仍有硬件异常，应保持 `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` 阻塞。

已验证：

- `FD_Release` Keil 编译通过：0 error / 1 warning。剩余 warning 为 `MainLoop_EnterIdleSleep` 未引用，和本修复无关。

## D009 RTC 休眠 CAN 1s 通信

状态：部分验证

参考源码：

- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c`
- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/bsp_rtc.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/LedBar.c`

已验证：

- `FD_Release` Keil 编译通过：0 error / 8 warning。
- 编译产物：`103 + 309/Project/Users/Objects/FD_Release.axf`，`103 + 309/Project/Users/Objects/FD_Release.bin`。
- 手工 ARMCC 单文件编译检查覆盖 CAN、RTC、低功耗拆分模块，无错误。

验证步骤：

1. 烧录 D009 BMS App Release，烧录必须使用安全脚本，确认 App 地址仍为 `0x08004800`。
2. 接入可 ACK 的 CAN 对端，进入 RTC STOP 休眠后，用示波器观察 `GPIO_CMNT_EN`、CAN_TX、CANH/CANL。
3. 有 CAN ACK 对端时，确认 RTC 休眠中约 1s 唤醒一次并发送 1s 周期业务帧；5s 周期帧按累计周期发送。
4. 断开 CAN 对端或无 ACK 时，确认不会持续 1s 空发，进入低频探测窗口。
5. 确认普通 SOC LED 和 `PA6 socKey` 的短按显示、长按休眠、休眠唤醒行为未被 CAN/RTC 迁移破坏。
6. 确认 `GPIO_CMNT_EN` 在 STOP 前断电，RTC 唤醒 CAN 服务窗口能重新配置输出并上电。
7. 确认 CAN App `0x60/0x61`、老化时间广播 `0x14F80208`、进入 IAP 命令行为保持兼容。

状态：部分验证

## 老化模式提前结束

参考源码：

- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `tools/comm_tool_upgrade_ui.py`
- `tools/can_bms_host.py`

验证步骤：

1. 编译 D009 BMS App Release，确认无错误。
2. 编译 `dist/BMS_CommTool_Upgrade_UI.exe`，确认输出文件名保持 `BMS_CommTool_Upgrade_UI.exe`。
3. 使用 CAN 用户上位机开启老化模式，读取老化时间，应显示状态为运行且剩余时间大于 0。
4. 点击 `关闭老化模式`，确认提示中说明会提前结束本轮老化时间。
5. 关闭命令完成后读取老化时间，应显示状态为完成、剩余时间为 0。
6. 断电重启后再次读取老化时间，应保持完成状态且不会自动恢复老化。
7. 再次点击 `开启老化模式`，应直接清零累计时间并开启新一轮，状态为运行且剩余时间从总时长重新开始计算。
