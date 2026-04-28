# ADC 总压分压计算说明

本文说明当前工程如何通过 PA1 / `ADC_VBC` 测量电池包总压，以及后续更换分压电阻时需要修改的位置。

注意：`ADC_VBC` 计算出的总压只作为独立调试、分压电阻验证和校准参考。`DataDeal.c` 中对外上报的 `g_stCellInfoReport.u16VCellTotle` 使用 AFE 单体电压采样累加值，不使用 ADC 总压。

## 1. 硬件关系

总压采样按普通电阻分压处理：

```text
Vbat+ -- Rtop -- PA1(ADC_VBC) -- Rbottom -- GND
```

其中：

```text
Vadc = Vbat * Rbottom / (Rtop + Rbottom)
Vbat = Vadc * (Rtop + Rbottom) / Rbottom
```

PA1 是 MCU ADC 输入脚，必须保证最大电池包电压下 `Vadc < VDDA/VREF+`，当前按 3.3V 计算。

## 2. 当前代码参数

参数在 `103 + 309/Project/Source/ADC.h`：

```c
#define VBC_ADC_VDDA_MV           3300U
#define VBC_DIVIDER_RTOP_KOHM     300U
#define VBC_DIVIDER_RBOTTOM_KOHM  10U
```

默认 `300K / 10K` 的分压比例是：

```text
(300 + 10) / 10 = 31
```

这等价于原来的 `Vbc_scale = 31`，只是现在改成了可直接按电阻值调整。

## 3. 软件计算流程

`ADC_Vbc()` 当前流程：

1. DMA 更新 `g_u16ADCValFilter[ADC_VBC]`，来源是 PA1 / `ADC_Channel_1`。
2. 软件累计 8 次后取平均，得到 `g_u16VbcStableAD`。
3. ADC 原始值换算 PA1 引脚电压：

```text
g_u16VbcAdc_mV = ADC_AD * VBC_ADC_VDDA_MV / 4096
```

4. PA1 引脚电压按分压电阻还原电池包总压：

```text
Vbat_mV = g_u16VbcAdc_mV * (Rtop + Rbottom) / Rbottom
```

5. `g_i32ADCResult[ADC_VBC]` 和 `g_u32Vbat_mV` 保存滤波后的 ADC 分压还原总压，单位 mV。
6. `ADC_GetVbatMilliVolt()` 返回 `g_u32Vbat_mV`，供调试或验证分压电阻使用。
7. `DataLoad_CellVoltMaxMinFind()` 不使用 ADC 总压；它累加 AFE 单体采样值生成 `g_stCellInfoReport.u16VCellTotle`。

## 4. 调试总压与上报总压单位

ADC 调试变量：

```c
g_u16VbcStableAD     // PA1 总压通道 8 次平均后的 ADC 值
g_u16VbcAdc_mV       // PA1 分压点电压，单位 mV
g_u32Vbat_mV         // 电池包总压，单位 mV，已做 1/8 IIR
g_i32ADCResult[ADC_VBC] // 同样表示电池包总压，单位 mV，已做 1/8 IIR
```

对外上报变量，来源是 AFE 单体电压累加：

```c
g_stCellInfoReport.u16VCellTotle // 电池包总压，单位 10mV
```

例子：

```text
AFE 单体采样累加值 42000mV
g_stCellInfoReport.u16VCellTotle ≈ 4200

ADC 分压测得总压 42000mV
g_u32Vbat_mV ≈ 42000
```

## 5. 更换分压电阻时怎么改

只需要修改 `ADC.h` 中这两个宏，单位保持一致即可：

```c
#define VBC_DIVIDER_RTOP_KOHM     300U
#define VBC_DIVIDER_RBOTTOM_KOHM  10U
```

如果实际是 `680K / 33K`：

```c
#define VBC_DIVIDER_RTOP_KOHM     680U
#define VBC_DIVIDER_RBOTTOM_KOHM  33U
```

如果使用欧姆也可以，但两个宏必须使用同一种单位：

```c
#define VBC_DIVIDER_RTOP_KOHM     680000U
#define VBC_DIVIDER_RBOTTOM_KOHM  33000U
```

建议优先用 KΩ 级整数，避免数值过大。

## 6. 校准关系

分压电阻只影响 `ADC_VBC` 调试总压，不影响 `g_stCellInfoReport.u16VCellTotle` 的来源。通信校准系数仍保留在 AFE 累加总压上：

```c
g_u16CalibCoefK[VOLT_VBUS] // 默认 1024，表示 1.0 倍
g_i16CalibCoefB[VOLT_VBUS] // 默认 0，单位 V，代码里换算成 mV
```

上报总压计算顺序是：

```text
AFE 单体采样累加出的总压 mV
-> 乘 VOLT_VBUS K 系数
-> 加 VOLT_VBUS B 偏移
-> 转为 10mV 写入 u16VCellTotle
```

所以分压电阻宏只按实际硬件设置，用于看 `g_u32Vbat_mV` 是否符合万用表测量值；`u16VCellTotle` 的小比例误差仍通过 `VOLT_VBUS` 的 K/B 校准修正。

## 7. 上板检查

1. 无论如何，最大包压下 PA1 不能超过 `VBC_ADC_VDDA_MV`。
2. 用万用表测 PA1 分压点电压，确认和 `g_u16VbcAdc_mV` 接近。
3. 用万用表测电池包总压，确认和 `g_u32Vbat_mV` 接近。
4. 如果 PA1 正确但总压不准，优先检查 `Rtop/Rbottom` 宏。
5. 如果总压只差少量比例，再调整 `VOLT_VBUS` 的 K/B 校准。
