# ADC / AFE 数据流设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`ADC.c`, `ADC.h`, `I2C_AFE1.c`, `I2C_AFE1.h`, `SH367309_Func.c`, `SH367309_DataDeal.c`, `DataDeal.c`, `rtc_sleep_afe_sh367309.c`
最后更新时间：2026-05-31
未确认事项：虚拟电流调试入口归属、均衡需求、AFE 参数写权限、温度探头数量、AFE fail-safe 策略、AFE watchdog 策略。

## 1. ADC 数据流

```text
TIM2_CC2
  -> ADC1 scan
  -> DMA1_Channel1
  -> g_u16ADCVal / g_u16ADCValFilter
  -> App_AnlogCal()
```

当前三路：

| 通道 | GPIO | 用途 |
|---|---|---|
| ADC_Channel_9 | PB1 | MOS/NMOS 温度 |
| ADC_Channel_2 | PA2 | Type-C 输出电流 |
| ADC_Channel_1 | PA1 | VBC/总压分压 |

低功耗前会停止 TIM2/ADC/DMA，唤醒后重新 `InitADC()`。

## 2. AFE 数据流

当前 AFE 是 SH367309，软件 I2C 读写。

```text
InitAFE1()
  InitIIC_AFE()
  InitAFE1_Sleep(0)
  SH367309_UpdataAfeConfig()
  MosStartup_ApplyInitialState()

App_AFEGet()
  UpdateVoltageFromBqMaximo()
  DataLoad_CellVolt()
  DataLoad_Temperature()
  DataLoad_Current()
  App_SH367309()
  new_todo_logi()
  App_SOC()
```

## 3. 采样、滤波、校准

| 数据 | 当前处理 |
|---|---|
| 电芯电压 | AFE raw -> mV，按串数映射到 `u16VCell[]` |
| 总压 | 单体求和 + ADC VBC 另一路 |
| 温度 | AFE 两路温度 + ADC MOS 温度，ENV2/ENV3 强制 -40 |
| 主回路电流 | 理论 AFE CADC + startup zero + deadband |
| Type-C 电流 | ADC PA2 平滑后折算电池侧等效电流 |

## 4. AFE 参数

`PRT_E2ROMParas` 和 `OtherElement` 会转换为 AFE ROM/寄存器字段。参数差异时写入并 reset AFE。

当前注意点：

- `u16Balance_OpenVoltage` 当前没有真正参与计算，源码硬编码 4160。
- AFE MTP/ROM 写入是高风险动作，需要工装权限确认。
- 主机写 `0x2400..0x2417` AFE 参数时，当前写入口会直接更新 `curValue` 并保存；加载侧有 `min/max` 校验，但写前实时校验仍需补齐。
- `MTPRead()` 失败会置 `ERROR_AFE1`，但连续失败阈值、是否断 MOS/CTLC、是否阻塞低功耗还未确认。
- `SH367309_Enable_AFE_Wdt_Cadc_Drivers()` 当前未启用 AFE watchdog；是否启用需要产品安全策略确认。

## 5. 当前问题

1. P0：AFE 参数写入口缺实时范围校验。
2. P0：AFE 通信失败后的 fail-safe 动作未定义。
3. P0/P1：AFE watchdog 是否启用未确认。
4. P1：`PROJECT_CFG_VIRTUAL_CURRENT_ENABLE=1` 时，`DataLoad_Current()` 内仍可在 `sys_time.isdebugenable==1` 下覆盖真实电流；`test_Autocurrent_cycle()` 当前已注释，但残留测试入口归属仍需确认。
5. 主动均衡入口未确认。
6. 温度 ENV2/ENV3 无效值需要确认。
7. AFE 抽象层尚未形成。

## 6. 后续建议

1. 先确认 AFE 参数写权限和非法值错误码，再补写前校验。
2. 再确认 AFE 连续通信失败阈值和 fail-safe 动作。
3. AFE watchdog 单独评估，不和写参校验混在同一阶段。
4. 虚拟电流入口迁移到 Factory/Test profile 或关闭 Release 宏。
5. AFE interface 设计排在安全边界确认之后，当前 SH367309 作为第一个实现。
6. 均衡是否落地必须先由用户确认。
