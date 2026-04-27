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

典型前端链路：

```text
Type-C 输出电流 Iout
-> 10mohm 采样电阻
-> 产生采样压降 Vshunt
-> 电流检测放大器/运放放大
-> 可能叠加零点偏置 Voffset
-> MCU PA2 ADC 采样
```

物理关系：

```text
Vshunt_mV = Iout_A * Rsense_mohm
Vadc_delta_mV = Vshunt_mV * G
Iout_A = Vadc_delta_mV / (Rsense_mohm * G)
Iout_mA = Vadc_delta_mV * 1000 / (Rsense_mohm * G)
```

其中 `G` 是从采样电阻两端压降到 MCU ADC 输入端的总电压增益。

当前代码用 `TYPEC_CUR_AMP_GAIN_X10` 表示 `G * 10`，所以整数公式为：

```c
Iout_mA = Vadc_delta_mV * 10000 / (TYPEC_CUR_RSENSE_MOHM * TYPEC_CUR_AMP_GAIN_X10);
```

当 `Rsense = 10mohm` 时：

```text
Iout_A = Vadc_delta_mV / (10 * G)
Iout_A10 = Vadc_delta_mV / G
```

## 4. 放大倍数如何根据电路调整

`TYPEC_CUR_AMP_GAIN_X10` 不是采样电阻值，而是总链路增益 `G` 的 10 倍。

### 4.1 使用电流检测芯片

如果 PA2 前面是专用电流检测芯片，优先查芯片手册的固定增益。例如：

| 芯片/档位示例 | 增益 G | 宏配置 |
| --- | --- | --- |
| 20 V/V | 20 | `TYPEC_CUR_AMP_GAIN_X10 200U` |
| 50 V/V | 50 | `TYPEC_CUR_AMP_GAIN_X10 500U` |
| 100 V/V | 100 | `TYPEC_CUR_AMP_GAIN_X10 1000U` |

如果芯片输出后还有分压进入 MCU ADC，需要把分压衰减也乘进去：

```text
G_total = G_chip * K_divider
K_divider = R_down / (R_up + R_down)
```

最终：

```c
#define TYPEC_CUR_AMP_GAIN_X10  ((UINT16)(G_total * 10))
```

### 4.2 使用运放搭建放大电路

如果是普通运放电路，需要按实际拓扑计算：

| 拓扑 | 理想增益 |
| --- | --- |
| 同相放大 | `G = 1 + Rf / Rg` |
| 反相放大 | `G = Rf / Rin`，符号由硬件极性决定 |
| 差分放大 | 匹配电阻条件下 `G = Rf / Rin` |

如果运放后面还有 RC 滤波、电阻分压、限幅网络，且会改变直流幅值，也必须计入总增益。

### 4.3 用已知负载反标定

如果原理图参数不确定，可以用电子负载反推。

步骤：

1. Type-C 无负载上电，等待零点建立。
2. 接入一个稳定已知负载，例如 5V/2A。
3. 读取 `g_u16TypeCOutDelta_mV`。
4. 用下面公式反推：

```text
TYPEC_CUR_AMP_GAIN_X10 = g_u16TypeCOutDelta_mV * 10000 / (10 * I_known_mA)
```

示例：采样电阻 10mohm，已知输出电流 2000mA，调试读到 `g_u16TypeCOutDelta_mV = 400mV`：

```text
TYPEC_CUR_AMP_GAIN_X10 = 400 * 10000 / (10 * 2000) = 200
G = 20
```

建议至少用 0.5A、1A、2A 三个点检查线性。如果不同电流点反推出来的增益差异较大，优先检查采样电阻精度、放大器饱和、输出滤波、ADC 参考电压、Type-C 负载是否稳定。

## 5. 当前新增调试变量

本次新增以下对外变量：

```c
g_u16TypeCOutCurrent_mA    // Type-C 输出电流，单位 mA，已滤波
g_u16TypeCOutCurrent_A10   // Type-C 输出电流，单位 A*10，已滤波
g_u16TypeCOutOffsetAD      // Type-C 电流零点 ADC
g_u16TypeCOutStableAD      // 32 次平均后的 PA2 ADC 值
g_u16TypeCOutDelta_mV      // PA2 相对零点的 ADC 端电压差，mV
```

兼容保留：

```c
g_i32ADCResult[ADC_CURR]   // 当前镜像为 Type-C 输出电流 A*10
gu16_BusCurr_DSG           // 当前镜像为 Type-C 输出电流 A*10
gu16_BusCurr_CHG           // 当前固定为 0
g_u16IoutOffsetAD          // 当前镜像为 g_u16TypeCOutOffsetAD
```

后续新代码应优先使用 `g_u16TypeCOutCurrent_mA` 或 `g_u16TypeCOutCurrent_A10`，不要再用 `gu16_BusCurr_*` 表达 Type-C 电流。

## 6. 当前计算流程

`ADC_Current_Smooth()` 当前流程：

1. 累加 `ADC_CUR_AMP` 32 次。
2. 得到 `g_u16TypeCOutStableAD`。
3. 若 `g_u16TypeCOutOffsetAD == 0`，累计 16 组稳定值作为零点。
4. 后续计算当前稳定 AD 与零点差值。
5. 差值在 `AD_CurZeroDeadband` 内时，输出电流清零。
6. 差值超过死区时，换算 `g_u16TypeCOutDelta_mV`。
7. 按 `Rsense` 和 `G` 换算 `g_u16TypeCOutCurrent_mA`。
8. 做 1/8 IIR 滤波。
9. 输出 `g_u16TypeCOutCurrent_A10`。

当前不区分正负方向，因为 Type-C 输出支路对 SOC 来说最终只会是负载消耗。硬件极性只影响当前 AD 高于零点还是低于零点，软件取绝对差值计算电流。

## 7. 调试注意事项

1. 首次零点建立要求 Type-C 输出支路无电流，否则会把带载电流当成零点。
2. 若产品可能带载启动，应增加“确认无 Type-C 输出后再校零”或“强制重新校零”的状态机。
3. `TYPEC_CUR_VDDA_MV` 当前按 3300mV 处理。如果 VDDA 偏差较大，电流会按比例偏差。
4. `TYPEC_CUR_AMP_GAIN_X10` 当前默认是 `10U`，即暂按 `G = 1` 计算，只是占位值，不代表真实电路。
5. 若 PA2 前端放大器在大电流下接近 0V 或 VDDA 饱和，ADC 结果会失真，必须降低增益或调整偏置。

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
4. 电流单位不能只根据旧变量注释推断，必须基于采样电阻、放大倍数、ADC 参考电压计算。
5. 放大倍数当前未知，因此代码使用 `TYPEC_CUR_AMP_GAIN_X10` 参数化，等待根据原理图或实测标定。
