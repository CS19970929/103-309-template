# ADC / AFE 数据流设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`ADC.c`, `ADC.h`, `I2C_AFE1.c`, `I2C_AFE1.h`, `SH367309_Func.c`, `SH367309_DataDeal.c`, `DataDeal.c`, `rtc_sleep_afe_sh367309.c`
最后更新时间：2026-06-04
未确认事项：真实电流路径、均衡需求、AFE 参数写权限、温度探头数量、Type-C 电流计入 SOC 的产品语义。

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
| Type-C 电流 | ADC PA2 当前 raw 直接折算电池侧等效电流，保留死区/限幅 |

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

## 7. RTC 唤醒后 ADC 直接采样简化（2026-06-04）

文档状态：部分验证

专项文档：`docs/review/adc_rtc_wakeup_simplification_2026-06-04.md`

参考源码：

- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/ADC.h`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/SOC.c`

当前源码事实：

- STOP 前 `ADC_StopForLowPower()` 会关闭 TIM2、ADC1 和 DMA1。
- STOP 唤醒恢复中 `InitRunAfterStopWakeup()` 会再次停止 ADC 后调用 `InitADC()`。
- `InitADC()` 会清零 `s_adc.raw[]`、`s_adc.result[]`、`s_adc.vbat` 和 Type-C 电流，并通过 `discard` 丢弃 1 个 10ms tick。
- VBC 当前由最新 `ADC_VBC` raw 直接换算 ADC mV 和电池侧 mV，不再做 8 点平均和 1/8 IIR。
- MOS 温度当前由最新 `ADC_TEMP_MOS1` raw 查表后直接写入结果，不再做 IIR。
- Type-C 当前由最新 `ADC_CUR_AMP` raw 直接换算 delta_mV/mA，保留 `AD_CurZeroDeadband` 和最大值限幅，不再做 32 点平均。
- `App_AnlogCal()` 当前为 latest-sample 模式，不再按历史 10ms tick 补跑滤波。
- 主业务总压仍由 AFE 单体累加得到，ADC VBC 不应覆盖主总压路径。

验证重点：

1. 冷启动和 RTC STOP 唤醒后，VBC/MOS/Type-C 最终值不再长时间从 0 慢收敛。
2. Type-C 小电流、插拔和断开清零边界需上板确认，避免直接采样噪声影响 SOC 附加放电。
3. AFE 单体累加总压、AFE 电流 sample seq、SOC 主积分和协议字段含义必须保持不变。
