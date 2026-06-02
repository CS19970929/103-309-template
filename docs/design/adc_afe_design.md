# ADC / AFE 数据流设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`ADC.c`, `ADC.h`, `I2C_AFE1.c`, `I2C_AFE1.h`, `SH367309_Func.c`, `SH367309_DataDeal.c`, `DataDeal.c`, `rtc_sleep_afe_sh367309.c`
最后更新时间：2026-05-26
未确认事项：真实电流路径、均衡需求、AFE 参数写权限、温度探头数量。

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

## 5. 当前问题

1. P0：量产主路径调用虚拟电流。
2. 主动均衡入口未确认。
3. 温度 ENV2/ENV3 无效值需要确认。
4. AFE 抽象层尚未形成。

## 6. 后续建议

1. 先恢复真实电流路径并做硬件验证。
2. 再设计 AFE interface，当前 SH367309 作为第一个实现。
3. 均衡是否落地必须先由用户确认。
