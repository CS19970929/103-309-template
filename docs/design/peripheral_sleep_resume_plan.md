# PeripheralAgent 外设休眠与恢复方案

## 设计目标

第一版目标是稳定实现 `Stop + RTC 周期唤醒`，不追求最低电流，不改 Modbus/CAN 协议，不重构 AFE/SOC/Flash/LED 业务。外设处理策略以当前工程已有入口为基础，把“能不能睡”和“睡前/唤醒后做什么”显式化。

本文件是设计方案，不修改源码。

## 推荐外设状态机切入点

后续最小实现可把外设处理挂到用户建议的低功耗接口中：

- `LP_CanSleep()`：只做准入判断，返回阻塞原因位图。
- `LP_BeforeSleep()`：执行一次性睡前外设收口。
- `LP_EnterStop(seconds)`：配置 RTC 周期并进入 Stop。
- `LP_AfterWakeup()`：按固定顺序恢复时钟、GPIO、外设、业务同步。

当前源码可映射为：

- `LP_BeforeSleep()` 对应当前 `LowPowerSleep_SaveCoreState()`、`IOstatus_RTCMode()`、`Conf_PrepareStopEntry()`。
- `LP_EnterStop()` 对应当前 `Sys_StopMode()`。
- `LP_AfterWakeup()` 对应当前 `cpu_frequency_conf()` 和 `InitRunAfterStopWakeup()`。

依据：

- `LowPowerSleep_SaveCoreState()` 当前保存 CAN/SOC/老化状态，见 `103 + 309/Project/Source/LowPowerSleep.c:5` 到 `LowPowerSleep.c:10`。
- `IOstatus_RTCMode()` 当前处理 IO/ADC/LED，见 `103 + 309/Project/Source/conf/conf.c:297` 到 `conf.c:324`。
- `Sys_StopMode()` 当前关闭 TIM3 并进入 Stop，见 `conf.c:374` 到 `conf.c:384`。
- `InitRunAfterStopWakeup()` 当前恢复外设，见 `conf.c:392` 到 `conf.c:421`。

## 外设动作矩阵

| 外设/模块 | 睡前动作 | Stop 中保持 | 唤醒后动作 | 当前依据 | 第一版建议 |
|---|---|---|---|---|---|
| 系统时钟 | 无需在外设层处理 | HSI/RTC 域 | 先 `cpu_frequency_conf()` | `rtc_sleep_port.c:207` 到 `rtc_sleep_port.c:211`，`conf.c:382` 到 `conf.c:384` | 保持当前顺序，后续抽成 `BspClock_RestoreAfterStop()` |
| SysTick 延时 | 不需要关闭长期 tick | 不作为唤醒源 | `InitDelay()` | `System_Init.c:132` 到 `System_Init.c:172` | 保持，只在系统时钟恢复后调用 |
| TIM3 系统 10ms | 关闭 TIM3、清 pending、关时钟 | 不运行 | `InitTimer()` | `conf.c:376` 到 `conf.c:380`，`System_Init.c:100` 到 `System_Init.c:127` | 保持；休眠时间由 RTC 记录，不补跑 10ms 任务 |
| TIM2/ADC/DMA | `ADC_StopForLowPower()` | 不运行 | `InitADC()` | `ADC.c:268` 到 `ADC.c:283`，`ADC.c:463` 到 `ADC.c:484` | 保持；增加“采样稳定窗口”测试 |
| UART/Modbus | 睡前必须确认不 busy；第一版不做 Stop 唤醒 | 不保持协议会话 | `USART_DeInit()` 后 `InitUSART_CommonUpper()` | `Sci_Upper.c:1577` 到 `Sci_Upper.c:1595`，`conf.c:409` 到 `conf.c:412` | 新增 `LP_BLOCK_COMM`，使用 `Sci_IsAnyPortBusy()` 和通信静默窗口 |
| CAN | 睡前必须确认不 busy，再 `Can_PrepareSleep()` | 收发器/通信电源关闭 | `InitCan()`；RTC 周期服务再短暂打开 | `Can_HDX.c:865` 到 `Can_HDX.c:893`，`conf.c:412` 到 `conf.c:414`，`Can_HDX.c:906` 到 `Can_HDX.c:933` | 新增 CAN quiet window；不要用粘性 `Can_IsBusActive()` 单独阻塞 |
| LED/LedBar | `LedBar_SetSleep(1)`、`LedBar_PrepareForStop()` | 不扫描 | `APP_LedBar()` 自恢复显示状态 | `LedBar.c:817` 到 `LedBar.c:833`，`LedBar.c:982` 到 `LedBar.c:990` | 显示窗口内可用 `LP_BLOCK_LED_ACTIVE` 延迟睡眠 |
| AFE/IIC | 判断 AFE 是否允许睡；复位式睡眠才 `AFE_Sleep()` | AFE 自身保护保持 | 普通 Stop 后先 `initAFE1_IIC()`，再运行态同步 | `rtc_sleep_afe_sh367309.c:11` 到 `rtc_sleep_afe_sh367309.c:28`，`I2C_AFE1.c:648` 到 `I2C_AFE1.c:667`，`conf.c:420` 到 `conf.c:421` | 不无条件完整 `InitAFE1()`；增加 `LP_BLOCK_AFE_BUSY` |
| GPIO/电源轨 | 进入低功耗 IO 状态，关闭不需要的电源 | 保持唤醒源 GPIO | `InitIO_rtc()` | `conf.c:297` 到 `conf.c:324`，`conf.c:168` 到 `conf.c:173` | 保持；通信唤醒源第一版先不作为主策略 |

## `LP_CanSleep()` 准入建议

第一版建议把外设阻塞原因至少拆成以下几类：

- `LP_BLOCK_COMM`：任一通信端口正在收发、协议帧待处理、CAN 队列未清、最近通信静默时间不足。
- `LP_BLOCK_AFE_BUSY`：AFE 状态异常、AFE 正在 EEPROM/配置写入、保护状态未同步。
- `LP_BLOCK_LED_ACTIVE`：按键/SOC 显示窗口仍在显示。
- `LP_BLOCK_IWDG_UNSAFE`：RTC 周期超过 IWDG 安全窗口。该项由 IWDG 设计文档统一约束。

当前可直接复用的判断：

- 串口 busy：`Sci_IsAnyPortBusy()`，见 `103 + 309/Project/Source/Sci_Upper.c:1679` 到 `Sci_Upper.c:1691`。
- 串口最近 RX：`RTC_ExtComCnt`，由 `USART_SR_RXNE` 中断递增，见 `Sci_Upper.c:1474` 到 `Sci_Upper.c:1480`。
- CAN busy：`Can_IsBusy()`，见 `103 + 309/Project/Source/Can_HDX.c:865` 到 `Can_HDX.c:880`。
- AFE 阻塞：`RtcSleep_AfePortIsSleepBlocked()`，见 `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:11` 到 `rtc_sleep_afe_sh367309.c:28`。
- MCU_WK/电流/老化/外部通信现有逻辑：`low_power_get_rtc_block_reason()` 和后续判断，见 `103 + 309/Project/Source/rtc_sleep.c:130` 到 `rtc_sleep.c:216`。

不建议直接复用为准入的判断：

- `Can_IsBusActive()` 不适合作为“当前活跃”唯一条件。它返回 `s_u8BusActive`，而该变量在 CAN 收发成功后置 1，见 `Can_HDX.c:289` 到 `Can_HDX.c:296`、`Can_HDX.c:758` 到 `Can_HDX.c:770`，直到 `InitCan()` 才清零，见 `Can_HDX.c:847` 到 `Can_HDX.c:853`。如果直接作为阻塞，会导致一旦 CAN 活跃过就长时间不睡。

建议新增抽象时按如下顺序判断：

1. 保护/故障/升级/Flash 忙等业务硬阻塞。
2. IWDG 周期安全。
3. 通信忙：`Sci_IsAnyPortBusy()`、`Can_IsBusy()`。
4. 通信静默窗口：最近 RS485/CAN RX/TX 至少静默 N 秒。N 初值建议 3 秒，后续按上位机轮询周期调整。
5. AFE busy：复用 `RtcSleep_AfePortIsSleepBlocked()`，后续扩展 AFE EEPROM 写状态。
6. LED active：按键/SOC 显示窗口未结束时延迟，不作为长期硬阻塞。

## `LP_BeforeSleep()` 推荐顺序

推荐顺序：

1. 再次执行 `LP_CanSleep()`，避免检查后到睡前之间出现新通信或 AFE 状态变化。
2. 保存状态：
   - `Can_PrepareSleep()`。
   - `SOC_SaveSnapshotBeforeSleep()`。
   - `FactoryAging_SaveProgressBeforeSleep()`。
   - 当前对应 `LowPowerSleep_SaveCoreState()`，见 `103 + 309/Project/Source/LowPowerSleep.c:5` 到 `LowPowerSleep.c:10`。
3. LED 收口：
   - `LedBar_SetSleep(1u)`。
   - `LedBar_PrepareForStop()`。
   - 当前依据见 `103 + 309/Project/Source/conf/conf.c:114` 到 `conf.c:118`、`conf.c:321` 到 `conf.c:324`。
4. ADC 采样链路关闭：
   - `ADC_StopForLowPower()`，当前会关 TIM2、ADC 外触发、DMA、ADC1、相关时钟，见 `103 + 309/Project/Source/ADC.c:268` 到 `ADC.c:283`。
5. CAN 收发器/通信电源关闭：
   - 继续使用 `Can_PrepareSleep()` 写 `GPIO_CMNT_EN` 到关闭电平，见 `103 + 309/Project/Source/Can_HDX.c:882` 到 `Can_HDX.c:893`。
6. GPIO 进入低功耗态：
   - 可复用 `IOstatus_RTCMode()`，但应在文档/代码中明确哪些唤醒源保留，见 `103 + 309/Project/Source/conf/conf.c:297` 到 `conf.c:324`。
7. 清 EXTI/NVIC pending：
   - `LowPower_ClearWakeupPending()`，见 `conf.c:129` 到 `conf.c:149`。

注意：

- 不在普通 RTC Stop 前调用完整 `InitAFE1()` 或重写 AFE 配置。`InitAFE1_Sleep()` 注释说明重新初始化参数可能导致 MOS 开关反复，见 `103 + 309/Project/Source/I2C_AFE1.c:659` 到 `I2C_AFE1.c:667`。
- 不在通信 busy 时调用 `Can_PrepareSleep()` 来强行清队列。应先由 `LP_CanSleep()` 阻止进入 Stop。

## `LP_EnterStop()` 推荐顺序

推荐顺序：

1. 配置 RTC Alarm 周期唤醒。
2. 配置允许的外部唤醒源。
3. 关闭 TIM3。
4. 清 pending。
5. `PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI)`。
6. Stop 返回后立即恢复系统时钟。

当前实现中第 3 到 6 步在 `Sys_StopMode()`，见 `103 + 309/Project/Source/conf/conf.c:374` 到 `conf.c:384`。

第一版外部唤醒源建议：

- 保留：RTC Alarm、充电输入、按键/MCU_WK。
- 暂缓：CAN/USART Stop 唤醒。

当前 `InitWakeUp_RTCMode()` 会复用 `InitWakeUp_NormalMode()`，因此会带上 UART1 RX EXTI7、通信 PB12、MCU_WK PB13 等外部唤醒源，见 `103 + 309/Project/Source/conf/conf.c:215` 到 `conf.c:272`。后续实现框架时应把“允许哪些唤醒源”改成显式配置，避免第一版调试时通信唤醒和 RTC 周期唤醒混在一起。

## `LP_AfterWakeup()` 推荐顺序

推荐顺序：

1. 恢复系统时钟。
   - 当前由 `Sys_StopMode()` 返回后调用 `cpu_frequency_conf()`，见 `103 + 309/Project/Source/conf/conf.c:382` 到 `conf.c:384`。
2. 恢复 SysTick 延时参数。
   - 当前 `cpu_frequency_conf()` 和 `InitRunAfterStopWakeup()` 都会调用 `InitDelay()`，见 `103 + 309/Project/Source/rtc_sleep_port.c:207` 到 `rtc_sleep_port.c:211`、`conf.c:392` 到 `conf.c:397`。
3. 恢复 RTC 运行态中断。
   - `RTC_RestoreRunInterrupts()`，见 `conf.c:396` 到 `conf.c:397`。
4. 恢复运行态 GPIO/电源轨。
   - `InitIO_rtc()`，见 `conf.c:168` 到 `conf.c:173`。
5. 恢复 ADC 采样链路。
   - 当前先 `ADC_StopForLowPower()` 再 `InitADC()`，见 `conf.c:401` 到 `conf.c:402`。
6. 恢复 USART/Modbus。
   - 当前 `USART_DeInit(USART1/2)` 后 `AppInit_InitSci()`，见 `conf.c:409` 到 `conf.c:412`。
7. 恢复 CAN。
   - `InitCan()`，见 `conf.c:412` 到 `conf.c:414`。
8. 恢复 TIM3 系统 10ms。
   - `InitTimer()`，见 `conf.c:412` 到 `conf.c:414`。
9. 恢复 AFE IIC 引脚并触发后续业务同步。
   - 当前 `initAFE1_IIC()`，见 `conf.c:420` 到 `conf.c:421`。

建议调整点：

- 将恢复顺序固化到 `LP_AfterWakeup()` 文档和代码注释中，避免后续维护者在外设恢复前启动依赖 10ms tick 的任务。
- 唤醒后先标记一个 `sample_warmup` 窗口，允许 ADC/AFE 重新稳定后再认为遥测数据有效。
- AFE 唤醒后不要立即完整 `InitAFE1()`；优先读状态并同步 `SystemRuntime_SetMosStatus()`、`Fault_ChangeToMCU()`，当前 `RtcSleep_AfePortHasAfeWake()` 已体现这一做法，见 `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c:74` 到 `rtc_sleep_afe_sh367309.c:91`。

## 通信活跃禁止 Stop 方案

### RS485/Modbus

当前可用依据：

- `Sci_IsAnyPortBusy()` 能覆盖帧 pending、TX length 和协议 busy，见 `103 + 309/Project/Source/Sci_Upper.c:1577` 到 `Sci_Upper.c:1595`、`Sci_Upper.c:1679` 到 `Sci_Upper.c:1691`。
- RX 字节会递增 `RTC_ExtComCnt`，见 `Sci_Upper.c:1474` 到 `Sci_Upper.c:1480`。

建议：

- `LP_CanSleep()` 中直接判断 `Sci_IsAnyPortBusy()`。
- 增加 `last_sci_activity_tick` 或复用 `RTC_ExtComCnt` 形成静默窗口。仅比较 1 秒 tick 下的计数变化不够，应保存最后变化时刻。
- 在静默窗口不足时返回 `LP_BLOCK_COMM`，不进入 Stop。

### CAN

当前可用依据：

- `Can_IsBusy()` 能判断 TX 队列、mailbox、读块流和硬件 TSR，见 `103 + 309/Project/Source/Can_HDX.c:865` 到 `Can_HDX.c:880`。
- `Can_PrepareSleep()` 会取消发送和清队列，见 `Can_HDX.c:882` 到 `Can_HDX.c:893`。

建议：

- `LP_CanSleep()` 中直接判断 `Can_IsBusy()`。
- 新增 CAN 最近活动时间，不直接使用粘性 `Can_IsBusActive()`。
- 只有在 `Can_IsBusy() == 0` 且最近 CAN 活动超过静默窗口后，才允许 `Can_PrepareSleep()`。

## 不建议第一版做的内容

1. 不做 CAN Stop 唤醒。当前 CAN RTC 服务已经能在周期唤醒后短暂打开 CAN 并发送，见 `103 + 309/Project/Source/Can_HDX.c:906` 到 `Can_HDX.c:933`。
2. 不做 USART Stop 唤醒。当前 UART1 EXTI 唤醒已经随 `InitWakeUp_NormalMode()` 进入 RTC 模式，见 `103 + 309/Project/Source/conf/conf.c:220` 到 `conf.c:228`、`conf.c:268` 到 `conf.c:272`，但第一版应降低变量数量。
3. 不在唤醒后完整重置 AFE。`InitAFE1_Sleep()` 已注明重新初始化参数会导致 MOS 开关反复风险，见 `103 + 309/Project/Source/I2C_AFE1.c:659` 到 `I2C_AFE1.c:667`。
4. 不补跑所有休眠期间 10ms/200ms 任务。休眠秒数应走 RTC/SOC 补偿路径，当前 `RtcSleep_PortApplySocRtcRest()` 已接入 SOC 休眠补偿，见 `103 + 309/Project/Source/rtc_sleep_port.c:166` 到 `rtc_sleep_port.c:178`。
5. 不改 Modbus 地址映射、CAN 帧 ID、现有 AFE 参数写入协议。

## 建议后续新增的外设接口

这些接口可先做薄封装，不重构原驱动：

```c
uint8_t BspAdc_IsReadyAfterWake(void);
void BspAdc_BeforeStop(void);
void BspAdc_AfterStopWake(void);

uint8_t BspSci_IsCommBusy(void);
uint8_t BspSci_IsQuiet(uint32_t quiet_ms);

uint8_t BspCan_IsCommBusy(void);
uint8_t BspCan_IsQuiet(uint32_t quiet_ms);
void BspCan_BeforeStop(void);
void BspCan_AfterStopWake(void);

uint8_t BspLedBar_IsActive(void);
void BspLedBar_BeforeStop(void);
void BspLedBar_AfterStopWake(void);

uint8_t BspAfe_CanSleep(void);
void BspAfe_BeforeStop(void);
void BspAfe_AfterStopWake(void);
```

第一版可以不一次性新增全部接口，但 `LP_CanSleep()` 至少应统一调用通信 busy、AFE blocked、LED active、IWDG unsafe 判断，避免继续把低功耗准入分散在多个模块。

## 最小验证点

实现前后必须验证：

1. 空闲无通信：能进入 Stop，RTC 周期唤醒后恢复 TIM3/ADC/USART/CAN/AFE IIC。
2. RS485 上位机轮询中：`Sci_IsAnyPortBusy()` 或通信静默窗口阻止 Stop。
3. CAN 发送队列未空：`Can_IsBusy()` 阻止 Stop，不调用 `Can_PrepareSleep()` 清队列。
4. LED SOC 显示窗口中：不立即黑屏进入 Stop，或有明确设计说明。
5. AFE 状态异常：`RtcSleep_AfePortIsSleepBlocked()` 阻止 RTC Stop。
6. 唤醒后 ADC 首批数据：确认不会把清零滤波缓存误报为有效保护/遥测数据。
7. 唤醒后通信：Modbus 和 CAN 不丢首帧，不改变现有协议。
