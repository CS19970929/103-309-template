# RTC 低功耗测试矩阵

本文档由 TestAgent 在第一阶段生成。范围是测试设计和验证清单，只读分析源码，未修改源码。适用工程目录：`E:\TODO\103 + 309 - 副本`。

## 1. 测试边界

- 当前目标 MCU：`STM32F103C8`，标准外设库工程。依据：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` 中 `Device=STM32F103C8`、编译宏 `STM32F10X_MD,USE_STDPERIPH_DRIVER`。
- App 安全烧录地址固定 `0x08004800`，IAP/Bootloader 地址固定 `0x08000000`。测试固件必须通过 `tools/soc_flash_app_safe.ps1`，禁止把 `FD_Release.bin` 裸写到 `0x08000000`。
- 第一版低功耗验证目标是稳定的 `Stop + RTC Alarm 周期唤醒`，不验证 CAN/USART 作为 Stop 唤醒源。
- 本矩阵不要求改协议，不要求新增寄存器；但部分项目目前缺少上位机可读的低功耗内部计数，需用电流表、示波器、Keil Watch、调试日志或后续诊断寄存器补足观察。

## 2. 当前项目测试依据

| 模块 | 当前源码依据 | 测试关注点 |
|---|---|---|
| 主循环 | `103 + 309/Project/Source/main.c:5-12`、`Runtime.c:14-42` | 低功耗任务在 `Runtime_RunIoAndPowerTasks()` 中运行，喂狗在后台任务末尾。 |
| 初始化 | `AppInit.c:7-72` | 启动顺序包含 `SystemInit/InitDelay/IsSleepStartUp/InitIO/SCI/Flash/AFE/CAN/ADC/SOC/TIM3/IWDG/RTC`。 |
| RTC | `RTC.c:26-55`、`:305-317`、`:366-418`、`:494-535` | 有安全等待，Stop 唤醒走 F1 `RTC Alarm + EXTI17 + RTCAlarm_IRQn`；当前 IWDG 安全裁剪被注释。 |
| Stop | `conf.c:374-385`、`:392-421` | Stop 前关 TIM3，Stop 后 `cpu_frequency_conf()` 恢复时钟，再恢复 Delay/RTC/IO/ADC/SCI/CAN/TIM3/AFE IIC。 |
| RTC sleep 策略 | `rtc_sleep.c:130-229`、`:303-327` | 阻塞原因现有电流、MCU_WAKE、老化、外部通信计数、AFE 不空闲；HICCUP 模式循环 Stop 并在 RTC 唤醒后服务 CAN/SOC。 |
| Port 层 | `rtc_sleep_port.c:108-177` | 入睡前保存核心状态，Stop 前配置 RTC/IO/唤醒源，醒后恢复，SOC 根据 RTC 休眠秒数补偿。 |
| IWDG | `System_Init.c:33-48`、`:288-290` | `__FUNC_RTC__` 下 IWDG 使用 `Prescaler_256 + Reload 0x0FFF`；Stop 前后有喂狗，CAN RTC 服务循环内喂狗。 |
| TIM/SysTick | `System_Init.c:100-127`、`:132-172`、`:246-299` | TIM3 产生 10ms/50ms/100ms/200ms/1000ms 标志；Stop 后必须重新 `InitTimer()`。 |
| ADC | `ADC.c:149-181`、`:215-260`、`:268-285` | ADC 由 TIM2 触发，Stop 前显式关闭 ADC/TIM2/DMA，醒后重新 `InitADC()`。 |
| CAN | `Can_HDX.c:847-933` | `Can_PrepareSleep()` 取消队列并关闭 `GPIO_CMNT_EN`；RTC 唤醒服务上电 CAN，最多服务 150 个 10ms tick。 |
| Modbus/RS485 | `Sci_Upper.c:1342-1593`、`:1678-1690`、`:2246-2256` | `Sci_IsAnyPortBusy()` 可作为通信忙阻塞依据；RX 字节递增 `RTC_ExtComCnt`，外部通信会延迟睡眠。 |
| Flash | `Flash.c:259-335`、`:456-565`、`:760-840`、`:867-940`、`:1078-1092` | SOC/AFE/参数/日志/老化都可能擦写 Flash；当前低功耗阻塞未显式覆盖 Flash busy。 |
| LED | `LedBar.c:817-843`、`:937-990`、`:1024-1115` | 入睡前保存显示 SOC 并准备 GPIO；唤醒、按键、充电显示需回归。 |
| AFE/保护 | `rtc_sleep_afe_sh367309.c:11-108`、`SleepDeal.c:83-114` | AFE 状态/故障/MOS 关闭会阻止或退出 RTC 睡眠；Reset 式睡眠前会 `AFE_Sleep()`。 |
| 充电唤醒 | `conf.c:192-213`、`:245-251`、`SleepDeal.c:11-80`、`rtc_sleep_port.c:192-205` | `GPIO_CHG_IN/PIN_CHG_IN` 低电平作为充电唤醒/有效唤醒来源。 |
| 过放深休眠 | `rtc_sleep.c:154-179`、`SleepDeal.c:220-230` | `VCellMin <= 2800mV` 连续 60s 或低于配置低压延时后进入 `DEEP_MODE`。 |

## 3. 工具和脚本约束

| 目的 | 固定方式 |
|---|---|
| 安全烧录 dry-run | `.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin"` |
| 安全烧录执行 | `.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash` |
| 直接 Modbus 状态读取 | `py -3.9 tools\soc_online_monitor.py --port COM4 --baud 19200 --slave 1 --samples 30 --interval 1` |
| SOC 测试 UI 正确启动 | `.\tools\start_soc_test_ui.ps1`，不要直接双击 `tools\soc_test_ui.py` |
| SOC 演示监控 | `.\tools\start_soc_test_ui.ps1 -Demo -Port COM4 -Baud 19200 -Slave 1 -Samples 10 -Interval 0.5` |
| 量产状态确认 | `COM4/19200/slave=1` 读取 `0xD000` 和 `0xD300`；量产 `0xD300 supported=0` 是正常结果。 |
| CAN/升级上位机 | `.\tools\start_comm_tool_upgrade_ui.ps1 -Port COM4 -Baud 115200` |
| comm tool 读 BMS | `py -3.9 tools\comm_tool_host.py bms-read --port COM4 --baud 115200 --address 0xD000 --count 63 --long-timeout 90` |
| CAN 诊断 | `py -3.9 tools\comm_tool_host.py can-diag --port COM4 --baud 115200` |
| 老化剩余时间 | UI 路径 `其它功能 -> 常用功能 -> 读取老化时间`，底层为 `0x13 BMS_AGING_STATUS` 解析 `0x14F80208` 广播。 |

## 4. 测试矩阵

| ID | 覆盖项 | 前置条件 | 步骤摘要 | 通过标准 | 失败判据 |
|---|---|---|---|---|---|
| LP-T001 | 烧录安全 | 已生成 `FD_Release.bin` | 先执行 safe flash dry-run，再用 `-Flash` 烧录；确认输出地址。 | 输出明确 `address: 0x08004800`，提示 IAP 保持 `0x08000000`，烧录后板可运行。 | 任何脚本或工具显示写入 `0x08000000`，立即停止测试。 |
| LP-T002 | 基线运行状态 | 板端量产配置，COM4 未被占用 | 用 `soc_online_monitor.py` 读取 `0xD000` 30 个样本；读取 `0xD300`。 | `0xD000` 电压、电流、SOC、故障字持续更新；量产 `0xD300 supported=0`。 | Modbus 超时、CRC 错、SOC 测试入口在量产中打开。 |
| LP-T003 | RTC 初始化 | 正常上电 | 用 Keil Watch 或诊断日志观察 `sys_time.rtc_sec_cnt`、`RTC_time`；若只能上位机，读 `0xD100` 附近时间字段。 | 秒中断正常，`RTC_WaitForSynchroSafe()` 不死等，系统继续跑主循环。 | 上电卡死、RTC 秒不动、调试器连接后卡在 RTC 同步。 |
| LP-T004 | Stop 进入 | 空闲、无充放电、无通信、老化未运行 | 等待 `sys_time.time_enter_rtc` 到达；用电流表观察进入 Stop；Watch `g_stLowPowerRtcStatus.readyToSleep/mode/blockReason`。 | 进入 HICCUP/RTC 模式，工作电流下降；TIM3 停止，RTC Alarm 保持。 | 空闲仍不进 Stop 且 `blockReason` 不明确；或进入后立即异常复位。 |
| LP-T005 | RTC 周期唤醒 | 已进入 Stop | 观察每 1s 左右唤醒一次；Watch `sys_time.rtc_alm_cnt`、`is_rtc_wakekup`、`s_u32RtcWakeCycles`。 | `RTCAlarm_IRQHandler()` 后 `is_rtc_wakekup=true`，`rtc_alm_cnt` 递增，醒后能再次入睡。 | Alarm 不触发、EXTI17 pending 未清、醒后留在高功耗运行态。 |
| LP-T006 | Stop 退出恢复 | 已 RTC 唤醒 | 醒后读取 `0xD000`，并观察 ADC/CAN/SCI/TIM3 是否恢复。 | `InitRunAfterStopWakeup()` 后 Modbus 可读、CAN 可发、ADC 数值刷新、TIM3 周期任务恢复。 | 唤醒后 Modbus/CAN 波特率错、ADC 恒定、任务标志不再推进。 |
| LP-T007 | IWDG 安全 | IWDG 使能，RTC 周期默认 1s | 连续执行 10 分钟 RTC Stop 循环；必要时 Watch 复位标志/启动日志。 | 不发生 IWDG 误复位；每次 Stop 前后喂狗正常。 | 周期性复位、启动标志显示非预期复位。 |
| LP-T008 | IWDG 边界 | 后续设计阶段需要扩大 RTC 周期时执行 | 仅在测试固件中把 RTC 周期调到接近 IWDG 窗口，确认框架阻止入睡或裁剪周期。 | 启用 IWDG 时 RTC 周期被限制在安全窗口内，或返回 `LP_BLOCK_IWDG_UNSAFE`。 | RTC 周期大于 IWDG 最短超时仍允许非复位 Stop。 |
| LP-T009 | SysTick 延时恢复 | Stop 前后执行需要 `__delay_ms()` 的路径 | 唤醒后执行 Modbus 读、CAN RTC 服务、ADC 校准等待。 | 延时不明显失准，`__delay_ms()` 不死等，TIM3 10ms 节拍继续。 | 唤醒后延时过长/过短，喂狗异常，TIM3 标志不再产生。 |
| LP-T010 | CAN 周期报文 | 连接 comm tool/CAN 设备 | 用 UI 或 `can-diag` 观察 1s/5s 报文；空闲进入 RTC 后继续观察。 | RTC 唤醒服务中 `Can_RtcWakeService()` 可发送到期 1s 报文，CAN 诊断无持续超时。 | RTC 期间 CAN 永久不发、`u8LastRtcWakeTimeout` 长期为 1、BusOff 持续。 |
| LP-T011 | CAN 无设备/有设备切换 | 可断开外部 CAN 设备 | 有设备、断开设备、再接入设备，各观察 3 分钟。 | 无设备时不应导致异常复位；再接入后能恢复通信或探测；不影响 Modbus。 | 断开后 CAN 队列卡死、再接入不恢复、Stop 唤醒后 CAN 供电未恢复。 |
| LP-T012 | 通信活跃禁止休眠 | COM4/19200 或 comm tool 连续读 | 以 0.5s 到 1s 间隔连续读 `0xD000`；观察 `RTC_ExtComCnt` 和 `blockReason`。 | `RTC_ExtComCnt` 变化导致 `LOW_POWER_RTC_BLOCK_EXT_COMM`，通信窗口内不进 Stop。 | 正在收发 Modbus/CAN 时进入 Stop，导致响应丢包或串口半包。 |
| LP-T013 | Modbus 读恢复 | RTC 周期睡眠后 | 醒前、醒后分别读 `0xD000` 63 或 88 个寄存器。 | CRC 正确，实时数据无明显冻结，故障字/状态字保持协议兼容。 | 醒后首帧持续超时或寄存器布局变化。 |
| LP-T014 | Modbus 写与 Flash 请求 | 用非破坏性写项，避免改关键保护参数 | 使用 UI 写一次 SOC `0x1005` 或允许的参数项；同时观察是否延迟睡眠。 | 写响应完成后再允许睡眠；`u8FlashUpdateFlag` 相关保存流程不被中断。 | 写响应尚未完成即入睡，或写后 Flash 保存/ACK 丢失。 |
| LP-T015 | ADC 关闭和恢复 | 有稳定输入电压/电流源 | Stop 前确认 ADC 正常；Stop 后醒来读 `0xD000` 多样本。 | `u16VCellMin/Max`、总压、充放电电流继续刷新，无 ADC 错误标志。 | ADC 数据全 0、恒定、明显跳变，或 `ERROR_ADC` 置位。 |
| LP-T016 | AFE 忙/故障阻塞 | 可模拟 AFE 故障或断 I2C | 触发 AFE 通信异常或状态位异常，观察 `RtcSleep_AfePortIsSleepBlocked()` 结果。 | AFE 不空闲时阻塞 RTC 睡眠；已睡眠时 AFE/MOS/故障能退出 RTC 循环。 | AFE 故障仍进入 Stop，或醒后 MOS/故障状态不同步。 |
| LP-T017 | SOC RTC 静置补偿 | 无充放电、稳定电压 | 记录睡前 SOC/电压；经历多次 RTC 唤醒后读 SOC；观察 `rtc rest` 日志或 Watch。 | `SOC_ApplyRtcRelaxationCompensation(rest_seconds, vmin, vmax)` 被调用，SOC 变化符合策略，不丢静置时间。 | 休眠时间未计入 SOC，SOC 跳变异常或 Flash 快照损坏。 |
| LP-T018 | Flash 忙禁止休眠 | 后续实现 busy 位后执行；当前先作为缺口验证 | 触发 SOC/AFE/参数/日志/老化保存路径，观察低功耗是否等待 Flash 完成。 | Flash 擦写/编程/验证期间有显式阻塞或测试确认不会入睡。 | Flash 写入期间进入 Stop，导致保存失败、参数丢失或协议响应丢。 |
| LP-T019 | LED 睡前处理 | LED 功能启用板型 | 按键显示 SOC 后等待入睡；观察 `LedBar_SaveSleepSoc()`、`LedBar_PrepareForStop()` 行为。 | 睡前显示关闭/GPIO 低功耗，唤醒按键显示睡前 SOC，充电图标不误亮。 | LED 扫描常开导致电流异常，或唤醒显示错乱。 |
| LP-T020 | 充电唤醒 | 可控充电输入，`GPIO_CHG_IN` 低有效 | RTC/DEEP/NORMAL 睡眠中接入充电，随后读取状态。 | `GPIO_CHG_IN` 低电平触发有效唤醒，`SleepDeal_IsBootFromSleepChargerWakeup()` 可识别，系统恢复通信。 | 插充无唤醒、唤醒后协议不可用、误判为按键唤醒。 |
| LP-T021 | 过放深休眠 | 使用电池模拟器/限流电源，不直接伤害电芯 | 模拟 `VCellMin <= 2800mV` 且充电电流小于阈值持续 60s；再接充电唤醒。 | 进入 `DEEP_MODE` 路径，保护/MOS/AFE 状态安全；接充电后可恢复。 | 过放不休眠、过放被通信/LED永久阻塞、接充电无法恢复。 |
| LP-T022 | 老化模式禁止普通睡眠 | 老化模式可由 UI/comm tool 控制 | 开启老化后观察 `FactoryAging_IsActive()`；读老化剩余时间。 | 老化运行时 `LOW_POWER_RTC_BLOCK_FACTORY_AGING` 阻塞普通 RTC 睡眠；剩余时间仍在 UI 单独可见。 | 老化时误入 Stop 导致剩余时间/广播/显示异常。 |
| LP-T023 | 升级/IAP 不被低功耗打断 | 使用 comm tool 缓存和升级流程 | 升级前后监控低功耗状态；升级中保持通信活跃。 | 升级/缓存/写 App 期间不进入 Stop；App 地址仍 `0x08004800`。 | 升级中睡眠、通信断开、App/IAP 地址混淆。 |
| LP-T024 | 长时间稳定性 | 基础项通过后 | 空闲 RTC Stop 循环 8h，期间每 10min 读一次 `0xD000` 和 CAN 诊断。 | 无误复位、无通信永久失效、无电流台阶异常、SOC/AFE/Flash/LED 状态稳定。 | 任一模块卡死、复位、功耗持续升高或协议失败。 |

## 5. P0 必测组合

| 组合 | 目的 | 通过标准 |
|---|---|---|
| 空闲 Stop + RTC + IWDG | 证明第一版低功耗主链路稳定 | 10 分钟以上无误复位，RTC 每秒唤醒，醒后可再入睡。 |
| Stop 唤醒 + 时钟恢复 + Modbus | 验证 HSE/HSI/SysTick/TIM3/USART 恢复 | 醒后 `COM4/19200/slave=1` 连续读 `0xD000` 成功。 |
| Stop 唤醒 + CAN 服务 | 验证 `Can_RtcWakeService()` 不破坏 CAN 节奏 | CAN 周期报文继续，`can-diag` 无持续 timeout/busoff。 |
| 通信活跃禁止休眠 | 避免协议被低功耗打断 | 连续 Modbus/CAN 访问期间不进 Stop。 |
| Flash 保存禁止休眠 | 避免参数/日志/SOC 快照损坏 | 写入请求期间不进 Stop，重启后数据一致。当前源码缺显式 busy，属于后续实现重点。 |
| 过放深休眠 + 充电唤醒 | BMS 安全底线 | 过放可进入安全低功耗，接充电能恢复通信和保护采样。 |

## 6. 当前测试观察缺口

- `g_stLowPowerRtcStatus`、`sys_time.rtc_alm_cnt`、`g_stCanLowPowerStatus` 目前主要适合 Keil Watch 或调试构建观察，未形成稳定量产只读寄存器。
- Flash busy 没有统一 `StorageFlash_IsBusy()` 或 busy 计数，无法仅靠现有协议精准断言“擦写期间禁止休眠”。
- RTC 唤醒周期当前固定经 `Can_GetIdleRtcPeriodSeconds()` 返回 1s，不能在不改源码的前提下覆盖 IWDG 边界周期。
- 过放和充电唤醒测试需要硬件模拟条件，不能只靠 PC 上位机完成。

