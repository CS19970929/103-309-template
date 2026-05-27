# BMS App IO 与 RTC 低功耗配置对比确认

状态：部分验证

日期：2026-05-27

对比基准：`5d5564e0d706085e6d50a442588febdb8eaed21a`

对比对象：当前工作区 `6108ebda2ce46cdbcfce00365381d74fc6ebf542` 加未提交改动。

说明：本文只记录静态源码审查和已有构建日志确认结果。未修改源码，未做上板电流、波形、串口或 CAN 实测。

## 参考源码

- `103 + 309/Project/Source/main.c`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf_gpio.h`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c`
- `103 + 309/Project/Source/app_lowpower.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/System_Init.c`
- `103 + 309/Project/Source/I2C_AFE1.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/LowPowerSleep.c`
- `103 + 309/Project/Source/MosStartup.c`
- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- `103 + 309/Project/Users/codex_rtc_low_power_build.log`

## 总体结论

当前版本相对 `5d5564e0d706085e6d50a442588febdb8eaed21a`，BMS App 的正常模式、RTC 低功耗模式、低功耗唤醒后恢复路径没有发现明显 IO 映射错配。主要变化不是管脚定义变化，而是启动链路、低功耗框架、RTC/AFE 适配层和唤醒恢复路径被拆分成更清晰的模块。

关键变化：

1. `main.c` 只保留 `AppInit_Boot()` 和主循环，原 `InitDevice/InitVar/MosStartup` 逻辑移到 `AppInit.c`、`MosStartup.c`。
2. `Runtime_RunOnce()` 从直接调用 `App_LowPowerProcess()` 改为调用 `LP_Task()`，再由 `LP_Task()` 调用 `rtc_sleep()`。
3. 新增 `app_lowpower.c` 的 `LP_BLOCK_*` 低功耗阻塞原因，通信、Flash、升级、故障、灯板活动、IWDG 不安全都会阻止进入 STOP。
4. RTC 唤醒后恢复路径统一为 `InitRunAfterStopWakeup()`，会恢复 RTC 秒中断、运行态 IO、ADC、USART、CAN、TIM3、AFE I2C。
5. `RTC.c` 增加 RTC alarm pending 清理和 IWDG 安全唤醒周期检查。当前看门狗开启时 RTC 唤醒周期上限为 10 秒。

## 正常模式 IO 与外设

启动链路：

`main()` -> `AppInit_Boot()` -> `AppInit_InitDevice()` -> `Runtime_RunOnce()`

正常模式初始化内容：

- `SystemInit()`
- `InitDelay()`
- `IsSleepStartUp()`
- `jtag_disableAndConfIO()`
- `InitNVIC()`
- `InitIO()`
- `AppInit_InitSci()`
- `InitE2PROM()`
- `InitAFE1()`
- `InitCan()`
- `InitADC()`
- `InitData_SOC()`
- `InitTimer()`
- `Init_RTC()`
- `LP_Init()`
- `Init_IWDG()`

正常运行 IO 确认：

| IO/外设 | 当前配置 | 对比旧 commit | 结论 |
|---|---|---|---|
| `PA0 / CHG_IN` | 输入浮空，低电平表示 5V 充电接入，低功耗下降沿唤醒 | 基本一致 | 保持 |
| `PA1 / ADC_VBUS` | ADC 模拟输入，ADC1 channel 1 | 一致 | 保持 |
| `PA2 / ADC_CUR` | ADC 模拟输入，ADC1 channel 2 | 一致 | 保持 |
| `PB1 / ADC_NMOS` | ADC 模拟输入，ADC1 channel 9 | 一致 | 保持 |
| `PA3 / 2737_EN` | 正常模式输出置高 | 一致 | 保持 |
| `PA4/PA5/PA6/PB10/PB11` | LedBar Charlieplexing 管脚 | 当前 LedBar 模块化后仍由 `LedBar.c` 接管 | 保持，需实测显示 |
| `PA7 / RF_EN` | 输出 | 一致 | 保持 |
| `PA8 / MCC_C` | 输出，默认低；MOS 启动逻辑按充电/老化状态更新 | 逻辑从 `main.c` 移到 `MosStartup.c` | 行为保持 |
| `PA9 / SW` | 输入浮空，低功耗下降沿唤醒 | 一致 | 保持 |
| `PA10 / DC_EN` | 正常模式输出置高 | 一致 | 保持 |
| `PA11/PA12 / CAN RX/TX` | CAN RX 输入上拉、TX 复用推挽，CAN1 `Prescaler=4` | CAN 模块重构，管脚配置保持 | 保持 |
| `PA15 / M_STB` | JTAG 禁用后作为普通输出，正常模式置高 | 一致 | 保持 |
| `PB0 / AFE1_PRO_EN` | `InitIO()` 中输出；RTC 唤醒恢复的 `InitIO_rtc()` 未恢复该脚 | 旧 commit 同样未在 `InitIO_rtc()` 恢复 | 需硬件确认 |
| `PB3 / AD_EN` | JTAG 禁用后作为普通输出，正常模式置高 | 一致 | 保持 |
| `PB4 / CMNT_EN` | 正常模式初始化为 CAN 电源关闭电平，CAN 模块按需打开 | 当前 CAN 低功耗策略更完整 | 保持，需实测 CAN |
| `PB5 / ADC_BUS_EN` | 正常模式输出置高 | 一致 | 保持 |
| `PB6/PB7 / USART1 remap` | `19200 8N1`，USART1 remap 到 PB6/PB7 | 一致 | 保持 |
| `PB8/PB9 / AFE I2C` | SH367309 软件 I2C | 接口层拆分，但管脚保持 | 保持 |
| `PB12 / INT_WK_CMNT` | 低功耗上升沿唤醒 | 一致 | 保持 |
| `PB13 / MCU_WK` | 低功耗上升沿唤醒 | 一致 | 保持 |
| `PB14 / AFE1_CTL` | `InitIO()` 中输出低；RTC 模式排除模拟输入 | 一致 | 保持，需确认低功耗硬件意图 |
| `PB15 / DBG_LED` | 输出 | 一致 | 保持 |

外设确认：

| 外设 | 当前正常模式 | 对比结论 |
|---|---|---|
| ADC1 + DMA1_Channel1 | TIM2_CC2 触发，3 路扫描，顺序为 `PB1 -> PA2 -> PA1` | 通道和触发方式保持 |
| TIM2 | 作为 ADC 触发源 | 保持 |
| TIM3 | 10ms 运行态节拍 | 唤醒后统一恢复 |
| TIM4 | LedBar 扫描 | STOP 前关闭，唤醒后随定时器恢复 |
| USART1 | Modbus RTU，PB6/PB7 remap，19200 8N1 | 保持 |
| USART2 | `Sci2` 宏关闭时不启用，唤醒路径仍统一 DeInit | 保持兼容 |
| CAN1 | 500k 典型配置，当前 `CAN_ABOM` 由 DISABLE 改为 ENABLE | bus-off 自动恢复能力增强 |
| RTC | alarm 唤醒 + 秒中断运行态恢复 | 当前 pending 清理更完整 |
| IWDG | 运行态开启，低功耗 RTC 周期受 10 秒上限约束 | 需结合功耗目标确认 |
| SH367309 AFE | 启动初始化，低功耗前 sleep，唤醒后恢复 I2C/配置 | 当前有独立适配层 |

## RTC 低功耗路径

运行态进入低功耗链路：

`Runtime_RunOnce()` -> `Runtime_RunIoAndPowerTasks()` -> `LP_Task()` -> `rtc_sleep()`

进入 STOP 前主要动作：

1. `LowPowerSleep_SaveCoreState()` 保存核心状态。
2. `Can_PrepareSleep()` 关闭 CAN 发送和收发器供电。
3. `SOC_SaveSnapshotBeforeSleep()` 保存 SOC 快照。
4. `FactoryAging_SaveProgressBeforeSleep()` 保存老化进度。
5. `Init_RTC()`、`IOstatus_RTCMode()`、`InitWakeUp_RTCMode()`、`RTC_WKTimeConfig()` 配置 RTC 和 EXTI。
6. `Sys_StopMode()` 进入 STOP。

STOP 前 IO/外设状态：

| 项目 | RTC 低功耗前状态 | 结论 |
|---|---|---|
| ADC/TIM2/DMA | `ADC_StopForLowPower()` 停止 | 正确 |
| GPIOA | 全模拟，排除 `PA3`；`PA10 / DC_EN` 明确拉低 | 需确认 `PA3` 保持输出是否为硬件要求 |
| GPIOB | 全模拟，排除 `PB14` | 需确认 `PB14 / AFE1_CTL` 保持非模拟是否为硬件要求 |
| GPIOC/GPIOD/GPIOE | 模拟输入 | 正确 |
| LedBar | `LedBar_StopForSleep()` | 正确 |
| CAN | `Can_PrepareSleep()`，关闭发送和 CMNT 电源 | 正确，需实测无设备/有设备策略 |
| TIM3 | STOP 前关闭 | 正确 |
| AFE | `SleepDeal.c` / `rtc_sleep_afe_sh367309.c` 处理 sleep 或恢复 | 正确，需实测 AFE 唤醒采样 |

## 低功耗唤醒后恢复

唤醒恢复链路：

`Sys_StopMode()` 返回 -> `RtcSleep_PortRestoreAfterStop()` -> `InitRunAfterStopWakeup()`

恢复内容：

- `SystemInit()` / 时钟恢复
- `InitDelay()`
- `RTC_RestoreRunInterrupts()`
- `InitIO_rtc()`
- `ADC_StopForLowPower()`
- `InitADC()`
- `USART_DeInit(USART1/USART2)`
- `AppInit_InitSci()`
- `InitCan()`
- `InitTimer()`
- `initAFE1_IIC()`

唤醒后结论：

- ADC、USART、CAN、TIM3、AFE I2C 均有恢复路径。
- RTC alarm pending 和 NVIC pending 清理比旧 commit 更完整。
- `PB0 / AFE1_PRO_EN` 仍没有在 `InitIO_rtc()` 中显式恢复，旧 commit 也存在同样行为。若该脚对 AFE 供电或保护使能有实际约束，需要硬件确认后再决定是否修改。

## 与基准 commit 的主要差异

| 模块 | 当前变化 | 风险判断 |
|---|---|---|
| `main.c` | 初始化逻辑迁移到 `AppInit.c` | 结构变化，行为需以 AppInit 链路确认 |
| `Runtime.c` | 低功耗任务由 `LP_Task()` 接管 | 正向变化，阻塞原因更清楚 |
| `app_lowpower.c` | 新增低功耗阻塞原因 | 正向变化，但需现场确认 fault/通信阻塞策略 |
| `rtc_sleep_port.c` | 新增 STOP 前后端口适配层 | 正向变化，便于审查 |
| `rtc_sleep_afe_sh367309.c` | 新增 AFE 低功耗适配层 | 正向变化，需实测 AFE 采样恢复 |
| `RTC.c` | pending 清理和 IWDG 周期约束增强 | 正向变化，但 10 秒唤醒上限影响低功耗目标 |
| `Can_HDX.c` | CAN 低功耗服务和 bus-off 自动恢复增强 | 正向变化，需 CAN 总线实测 |
| `stm32f10x_it.c` | `EXTI0_IRQHandler` 不再设置旧 `ChargerLoad_Func` 标志 | 旧模块移除后的清理，需确认充电器唤醒路径仍完整 |
| Keil 工程 | 加入新分层文件，移除旧 `IODrivers.c`、`IO_Control.c`、`Heat_Cool.c`、`ChargerLoadFunc.c` | 结构变化，需要持续保持工程文件和源码同步 |

## 构建确认

已有构建日志：

- `103 + 309/Project/Users/codex_rtc_low_power_build.log`

日志结论：

- `FD_Release` rebuild：`0 Error(s), 0 Warning(s)`
- Program Size：`Code=51524 RO-data=3000 RW-data=1336 ZI-data=5496`

本次仅引用已有日志，未重新执行编译。

## 需求确认表

| 字段 | 说明 |
|---|---|
| Requirement ID | 需求 ID |
| Requirement description | 需求描述 |
| Evidence from code | 代码证据 |
| Current behavior | 当前行为 |
| Risk | 风险 |
| Codex judgment | Codex 判断 |
| Question for user | 需要用户确认的问题 |
| Suggested decision | 建议决策 |
| User decision placeholder | 用户决策占位 |

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| REQ-IO-RTC-001 | 正常模式 IO 映射必须保持现有硬件定义 | `conf_gpio.h`, `conf.c:InitIO()` | 当前相对基准 commit 未发现明显 IO 映射错配 | 改错会导致 ADC、CAN、AFE、LedBar、唤醒源异常 | MUST_KEEP | 当前 IO 表是否与最新原理图完全一致？ | 保持现有 IO 映射，后续用原理图逐项复核 | 待确认 |
| REQ-IO-RTC-002 | RTC STOP 前应关闭 ADC/TIM2/DMA、CAN、LedBar、TIM3 并设置 GPIO 低漏电状态 | `ADC.c`, `conf.c:IOstatus_RTCMode()`, `Can_HDX.c`, `LedBar.c` | 已有 STOP 前关闭路径 | 若关闭不完整，会导致低功耗电流偏高 | MUST_KEEP | STOP 前是否还需要额外关闭 2737、AFE 保护或外部负载？ | 保持当前策略，追加上板电流测试 | 待确认 |
| REQ-IO-RTC-003 | `PA3 / 2737_EN` 在 RTC 模式下排除模拟输入 | `conf.c:IOstatus_RTCMode()` | STOP 前 GPIOA 模拟化时排除 PA3 | 如果 PA3 应关闭，可能增加低功耗电流；如果必须保持，改错会影响硬件 | UNKNOWN | PA3 在休眠时应保持输出还是进入模拟？ | 硬件确认前不改 | 待确认 |
| REQ-IO-RTC-004 | `PB14 / AFE1_CTL` 在 RTC 模式下排除模拟输入 | `conf.c:IOstatus_RTCMode()` | STOP 前 GPIOB 模拟化时排除 PB14 | 可能影响 AFE 控制或漏电 | UNKNOWN | PB14 在休眠时应保持当前状态还是进入模拟？ | 硬件确认前不改 | 待确认 |
| REQ-IO-RTC-005 | `PB0 / AFE1_PRO_EN` 唤醒后是否需要显式恢复 | `conf.c:InitIO()`, `conf.c:InitIO_rtc()` | 正常初始化有 PB0，RTC 唤醒恢复未显式恢复，旧 commit 同样如此 | 如果 PB0 控制 AFE 保护/供电，唤醒后可能状态不确定 | UNKNOWN | PB0 的硬件功能是否要求 STOP 唤醒后重新配置和置位？ | 核对原理图；确认后再决定是否补 `InitIO_rtc()` | 待确认 |
| REQ-IO-RTC-006 | RTC 唤醒后必须恢复 ADC、USART、CAN、TIM3、AFE I2C | `conf.c:InitRunAfterStopWakeup()` | 当前统一恢复这些外设 | 恢复缺项会导致唤醒后采样或通信异常 | MUST_KEEP | 这些恢复动作是否覆盖所有需要运行的外设？ | 保持当前恢复链路并上板验证 | 待确认 |
| REQ-IO-RTC-007 | IWDG 开启时 RTC 唤醒周期不得超过 10 秒 | `RTC.c` | 当前限制为 10 秒 | 与极低功耗目标可能冲突 | CONFLICT | 低功耗目标优先，还是 IWDG 持续开启优先？ | 先保持安全策略，后续结合实测功耗确认 | 待确认 |
| REQ-IO-RTC-008 | RTC 唤醒后 CAN 可短时上电服务广播 | `Can_HDX.c`, `rtc_sleep.c` | 当前有 RTC wake CAN 服务策略 | 会增加周期唤醒电流，但可提高通信可见性 | UNKNOWN | 休眠中是否必须周期 CAN 广播？ | 暂保留，按客户通信需求确认 | 待确认 |

## 上板验证清单

1. 正常模式测量 `PA3`、`PA10`、`PB0`、`PB4`、`PB5`、`PB14` 默认电平。
2. 正常模式通过 Modbus 读取 ADC/VBUS/电流/温度，确认 ADC 通道顺序未错。
3. 正常模式确认 CAN 周期帧和 `0x14F80208` 老化广播。
4. 进入 RTC STOP，测整板低功耗电流，记录 10 秒周期唤醒波形。
5. RTC STOP 前后分别测 `PA3`、`PA10`、`PB14`、`PB4` 电平。
6. 通过 `PA0 / CHG_IN`、`PA9 / SW`、`PB12`、`PB13` 分别唤醒，确认唤醒源有效。
7. 唤醒后读取 Modbus `0xD000`、`0xD300`，确认 App 运行状态。
8. 唤醒后确认 ADC 采样、AFE 电流/电压、CAN 周期帧、LedBar 显示恢复。
9. CAN 无设备场景确认 CMNT 电源不会长期打开。
10. CAN 有设备场景确认 RTC 唤醒短时广播可被接收。

## 暂不修改源码原因

本次审查没有发现必须立即修正的 IO 映射错误。剩余问题都依赖硬件原理图或上板实测确认。按照仓库规则，在用户确认前不修改源码。
