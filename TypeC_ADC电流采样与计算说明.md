# Type-C ADC 电流采样与计算说明

## 1. 背景与边界

当前工程里存在两类电流，不能混用：

| 电流来源 | 代码入口 | 物理含义 | 当前用途 |
| --- | --- | --- | --- |
| SH367309 AFE CADC | `DataDeal_Current()`、`g_stCellInfoReport.u16Ichg/u16IDischg` | 电池包主回路充放电电流 | 已参与保护、CAN、SOC |
| MCU ADC PA2 | `ADC_CUR_AMP`、`ADC_Current_Smooth()` | Type-C 输出支路电流 | 本次先完成独立计算，暂未接入 SOC |

本次处理目标是先把 PA2 ADC 采样的 Type-C 输出电流计算出来，形成可调试、可标定的稳定值；SOC 如何扣减留到后续独立设计。

## 2. 当前 ADC 采样链路

当前 ADC 规则组启用 3 路：

| DMA 下标 | 枚举 | 引脚 | ADC 通道 | 用途 |
| --- | --- | --- | --- | --- |
| 0 | `ADC_TEMP_MOS1` | PB1 | `ADC_Channel_9` | NMOS 温度 |
| 1 | `ADC_CUR_AMP` | PA2 | `ADC_Channel_2` | Type-C 输出电流采样 |
| 2 | `ADC_VBC` | PA1 | `ADC_Channel_1` | 母线/总压采样 |

硬件采样路径：

```text
TIM2_CC2 周期触发
-> ADC1 扫描 PB1 / PA2 / PA1
-> DMA1_Channel1 循环搬运到 g_u16ADCValFilter[]
-> App_AnlogCal() 按 10ms tick 调用计算函数
```

Type-C 电流计算入口仍沿用原函数名：

```c
ADC_Current_Smooth();
```

但它当前的语义已经是 Type-C 输出电流计算，不再代表电池主回路充放电电流。

## 3. Type-C 电流采样原理

已知 Type-C 输出电流采样电阻：

```c
Rsense = 10mohm
```

当前确认的前端链路：

```text
Type-C 输出电流 Iout
-> 10mohm 采样电阻
-> 产生采样压降 Vshunt
-> MCU PA2 ADC 直接采样分流器两端压降
```

物理关系：

```text
Vshunt_mV = Iout_A * Rsense_mohm
Vadc_delta_mV = Vshunt_mV
Iout_A = Vadc_delta_mV / Rsense_mohm
Iout_mA = Vadc_delta_mV * 1000 / Rsense_mohm
```

当前代码整数公式为：

```c
Iout_mA = Vadc_delta_mV * 1000 / TYPEC_CUR_RSENSE_MOHM;
```

当 `Rsense = 10mohm` 时：

```text
Iout_A = Vadc_delta_mV / 10
Iout_mA = Vadc_delta_mV * 100
Iout_A10 = Vadc_delta_mV
```

示例：

```
10mV -> 1000mA -> 1A
20mV -> 2000mA -> 2A
50mV -> 5000mA -> 5A
```

## 4. 当前参数如何调整

当前只保留两个硬件相关参数：

```c
#define TYPEC_CUR_RSENSE_MOHM       10U
#define TYPEC_CUR_VDDA_MV           3300U
```

调整原则：

| 参数 | 含义 | 什么时候改 |
| --- | --- | --- |
| `TYPEC_CUR_RSENSE_MOHM` | Type-C 输出支路分流器阻值，单位 mohm | 更换采样电阻时修改 |
| `TYPEC_CUR_VDDA_MV` | MCU ADC 参考电压，单位 mV | VDDA/VREF+ 不是 3.3V 或需要标定时修改 |

如果后续电路改成“分流器压降经过运放/电流检测芯片后再进 PA2”，那时需要重新引入前端比例系数：

```text
Vadc_delta_mV = Vshunt_mV * G
Iout_mA = Vadc_delta_mV * 1000 / (Rsense_mohm * G)
```

但这不是当前硬件状态。当前 PA2 直接采分流器两端压降，因此代码不配置放大倍数。

### 4.1 用已知负载验证

建议用电子负载验证直接采样关系：

1. Type-C 无负载时确认 PA2 ADC 接近 0。
2. 接入稳定负载，例如 1A、2A、3A。
3. 读取 `g_u16TypeCOutDelta_mV` 和 `g_u16TypeCOutCurrent_mA`。
4. 对 10mohm 分流器，理论上 `g_u16TypeCOutDelta_mV` 应约等于 `I_A * 10`。

示例：2A 负载时，分流器压降约 20mV，软件输出约 2000mA。如果偏差明显，优先检查采样电阻精度、PA2 接线、ADC 参考电压和负载稳定性。

## 5. 当前新增调试变量

本次新增以下对外变量：

```c
g_u16TypeCOutCurrent_mA    // Type-C 输出电流，单位 mA，由 g_u16TypeCOutDelta_mV 直接换算
g_u16TypeCOutCurrent_A10   // Type-C 输出电流，单位 A*10，由 mA 值直接换算
g_u16TypeCOutOffsetAD      // 当前不参与 Type-C 电流计算，固定清 0
g_u16TypeCOutStableAD      // 32 次平均后的 PA2 ADC 值
g_u16TypeCOutDelta_mV      // PA2 ADC 端电压，mV
```

兼容保留：

```c
g_i32ADCResult[ADC_CURR]   // 当前镜像为 Type-C 输出电流 A*10
gu16_BusCurr_DSG           // 当前镜像为 Type-C 输出电流 A*10
gu16_BusCurr_CHG           // 当前固定为 0
g_u16IoutOffsetAD          // 当前不参与 Type-C 电流计算，固定清 0
```

后续新代码应优先使用 `g_u16TypeCOutCurrent_mA` 或 `g_u16TypeCOutCurrent_A10`，不要再用 `gu16_BusCurr_*` 表达 Type-C 电流。

## 6. 当前计算流程

`ADC_Current_Smooth()` 当前流程：

1. 实时读取 `ADC_CUR_AMP`，连续 3 次小于等于 `AD_CurZeroDeadband` 时立即清零输出。
2. 非零电流时累加 `ADC_CUR_AMP` 32 次。
3. 得到 `g_u16TypeCOutStableAD`。
4. 若稳定 AD 小于等于 `AD_CurZeroDeadband`，输出电流清零。
5. 将稳定 AD 直接换算为 `g_u16TypeCOutDelta_mV`。
6. 按 `Rsense` 由 `g_u16TypeCOutDelta_mV` 直接换算 `g_u16TypeCOutCurrent_mA`。
7. 输出 `g_u16TypeCOutCurrent_A10`。

当前按 PA2 直接采 Type-C 分流器压降处理，不再建立运行时零点，也不再使用 `g_u16TypeCOutOffsetAD` 参与计算。

## 7. 调试注意事项

1. PA2 无负载时应接近 0；如果无负载仍有明显 AD 值，软件会按真实采样值换算出电流。
2. `TYPEC_CUR_VDDA_MV` 当前按 3300mV 处理。如果 VDDA 偏差较大，电流会按比例偏差。
3. 当前 PA2 直接采分流器两端压降；若后续增加运放/电流检测芯片，需要重新设计换算公式。
4. 10mohm 直接采样时电压很小，1A 只有 10mV；低电流分辨率和噪声需要上板验证。

## 8. 后续接入 SOC 的边界

本次没有把 Type-C ADC 电流接入 SOC。

后续接入前必须先确认 Type-C 输出电流是否已经被 AFE 主回路电流覆盖：

| 情况 | SOC 处理 |
| --- | --- |
| Type-C 输出电流已经流过 AFE 采样电阻 | 不应重复扣减 SOC |
| Type-C 输出支路绕过 AFE 采样电阻 | 需要换算为电池侧等效放电电流后加到 SOC 放电电流 |

如果需要换算电池侧等效电流，应考虑 Type-C 输出电压、电池包电压和 DC/DC 效率：

```text
Ibat_equiv = I_typec_out * V_typec_out / V_bat / efficiency
```

这部分应作为 SOC 设计的独立改动处理。

## 9. 本次对话重点记录

1. 用户确认 PA2 ADC 电流不是 `DataDeal/SciUpper` 中的 AFE 电池电流，而是 Type-C 输出电流。
2. 用户确认 Type-C 电流采样电阻为 10mohm。
3. 当前先完成 Type-C ADC 电流计算，暂不接入 SOC。
4. 用户进一步确认 PA2 ADC 当前采到的就是 10mohm 分流器两端压降，不经过前端放大。
5. 当前代码按直接采样配置，不再使用放大倍数参数；后续电路改版时再按新前端重新调整参数和公式。
