# App_AnlogCal 时基修改影响说明

日期：2026-04-25

## 1. 结论

不建议把 `App_AnlogCal()` 的计算时基直接改到 1s。

当前工程中，ADC 原始采样和 ADC 结果计算是两层逻辑：

1. ADC 原始采样由 `TIM2_CC2` 触发，DMA 写入 `g_u16ADCValFilter[]`。
2. `App_AnlogCal()` 在系统 10ms 标志到来时，把 DMA 中的最新原始值转换为温度、总压、电流等工程量，并做均值/IIR 滤波。

如果只把 `App_AnlogCal()` 改成 1s 调用，ADC 硬件仍会按 `TIM2` 周期采样，但计算结果只会 1s 更新一次，动态响应和滤波时间都会被成比例拉长。稳态直流值通常不会因为时基本身改变而发生比例性错误，但结果更新会明显滞后，电流零点建立尤其慢。

如果同时把 ADC 触发 `TIM2` 也改成 1s，则 ADC 原始采样频率会降到 1Hz，会进一步降低对电流、电压突变的感知能力。

## 2. 当前实现

`InitADC_TIMER()` 中把 `TIM2` 配成约 10ms 周期：

```c
timer_div = SystemCoreClock / 100000U;
TIM_TimeBaseStructure.TIM_Prescaler = (UINT16)(timer_div - 1U);
TIM_TimeBaseStructure.TIM_Period = 999U;
```

`InitADC_ADC1()` 中 ADC 使用 `TIM2_CC2` 外部触发：

```c
ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
ADC_ExternalTrigConvCmd(ADC1, ENABLE);
```

`App_AnlogCal()` 本身在主循环中调用，但函数内部只在 `b1Sys10msFlag` 有效时执行：

```c
if (0 == g_st_SysTimeFlag.bits.b1Sys10msFlag)
{
    return;
}

ADC_TTC();
ADC_Vbc();
ADC_Current_Smooth();
```

系统 10ms 标志来自 `TIM3_IRQHandler()` 中的 `SysTime_Post10msTick()`。

## 3. 改到 1s 后的影响

### 3.1 温度结果

`ADC_TTC()` 每次调用读取一次 `g_u16ADCValFilter[ADC_TEMP_MOS1]`，查表后做一阶滤波：

```c
g_u32ADCValFilter2[ADC_TEMP_MOS1] =
    (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_MOS1]) >> 3)
    + g_u32ADCValFilter2[ADC_TEMP_MOS1];
```

该滤波系数固定为 `1/8`。调用周期从 10ms 改到 1s 后，滤波步进不变，但实际时间常数约放大 100 倍。温度本来变化较慢，功能上可能还能接受，但显示和保护相关温度会明显滞后。

### 3.2 总压结果

`ADC_Vbc()` 使用 `AD_CalNum = 8` 做累加均值，再做 `1/8` IIR 滤波。

由于代码写法是 `if (s8ADcnt++ < AD_CalNum)`，从初始值开始会先累计 8 次，第 9 次调用才计算一次均值。因此：

| `App_AnlogCal()` 时基 | 均值计算间隔 | 后续 IIR 响应 |
| --- | --- | --- |
| 10ms | 约 90ms 出一次均值 | 正常 |
| 1s | 约 9s 出一次均值 | 进一步变慢 |

所以把计算时基改成 1s 后，总压结果不是立即每秒更新一次，而是约 9s 才进入一次新的均值结果，然后还要经过 `1/8` 滤波。

### 3.3 电流结果

`ADC_Current_Smooth()` 使用 `AD_CalNum_Cur = 32` 做电流采样均值，再按零点偏差计算充/放电电流。

同样由于 `if (su8_ADcnt++ < AD_CalNum_Cur)` 的写法，初始阶段会累计 32 次，第 33 次调用才计算一次均值：

| `App_AnlogCal()` 时基 | 电流均值计算间隔 |
| --- | --- |
| 10ms | 约 330ms |
| 1s | 约 33s |

更关键的是电流零点 `g_u16IoutOffsetAD` 需要 `AD_CurOffsetCalNum = 16` 组电流均值才能建立。若 `App_AnlogCal()` 改成 1s，首次零点建立可能需要约 `33s * 16 = 528s`，接近 9 分钟。

这会导致上电后电流结果长期为 0，或在零点未建立时不能及时反映真实充放电电流。

## 4. 是否会改变计算值本身

对于稳定不变的模拟量，单次换算公式本身不依赖调用周期，例如：

1. 总压换算仍然按 ADC 原始值、分压比例 `Vbc_scale` 计算。
2. 温度仍然按 NTC 查表结果计算。
3. 电流仍然按 ADC 与零点偏差、电流采样电阻参数计算。

因此，稳定输入下最终结果大体不会因为 10ms 改为 1s 而产生固定比例错误。

但滤波器和均值器是按“调用次数”设计的，不是按真实时间设计的。调用周期变慢后，结果更新时间、零点建立时间、动态跟踪速度都会按比例变慢，这会影响显示、通信上报、SOC 估算和依赖 MCU ADC 的辅助判断。

## 5. 建议方案

如果目标是降低计算负载或低功耗，不建议整体把 `App_AnlogCal()` 改成 1s。更稳妥的方式是拆分不同信号的处理周期：

| 信号 | 建议周期 | 原因 |
| --- | --- | --- |
| 电流 | 10ms 或 20ms | 动态变化快，零点和滤波依赖调用频率 |
| 总压 | 100ms 到 200ms | 变化中等，通信显示和判断需要较及时 |
| MOS 温度 | 500ms 到 1s | 热变化慢，可降低处理频率 |

如果确实要把某一路改为 1s，需要同步重新设计对应的均值次数、滤波系数和初始化零点策略，不能只改调用标志。

## 6. 建议结论

保持 `App_AnlogCal()` 10ms 主节拍更安全。若后续需要优化，可以改成如下结构：

1. 保持 ADC 硬件采样 `TIM2` 周期不低于 10ms 到 20ms。
2. 电流计算保持较快周期。
3. 总压和温度按独立软计数降频计算。
4. 对降频后的滤波参数重新按真实时间常数标定。

