# App_AnlogCal 时基修改影响说明

日期：2026-04-25

## 1. 结论

本次已完成架构优化：`App_AnlogCal()` 不再把“函数调用一次”当作一次 10ms 计算节拍，而是读取系统累计 10ms tick，根据真实经过的 tick 数补齐 ADC 计算步数。

当前工程中，ADC 原始采样和 ADC 结果计算是两层逻辑：

1. ADC 原始采样由 `TIM2_CC2` 触发，DMA 写入 `g_u16ADCValFilter[]`。
2. `App_AnlogCal()` 按系统 10ms tick 把 DMA 中的最新原始值转换为温度、总压、电流等工程量，并做均值/IIR 滤波。

优化后，如果外层以后把 `App_AnlogCal()` 放到 1s 调用，ADC 硬件仍按 `TIM2` 周期采样，ADC 计算也会在每次调用时按约 100 个 10ms 步长补算，均值次数、IIR 滤波次数、电流零点建立时间不会再被外层调用周期放大 100 倍。

仍需注意：不要把 ADC 触发 `TIM2` 也改成 1s。若 `TIM2` 改成 1s，ADC 原始采样频率会降到 1Hz，这会直接降低对电流、电压突变的感知能力。

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

系统 10ms tick 在 `SysTime_Post10msTick()` 中累计：

```c
s_u32Sys10msTickCount++;
```

`App_AnlogCal()` 本身仍可在主循环中调用，但函数内部改为消费累计 tick：

```c
u32Now10msTick = SysTime_Get10msTickCount();
u32Elapsed10msTick = u32Now10msTick - s_u32AnlogCalLast10msTick;
if (0U == u32Elapsed10msTick)
{
    return;
}
s_u32AnlogCalLast10msTick = u32Now10msTick;

while (u32Elapsed10msTick > 0U)
{
    ADC_TTC();
    ADC_Vbc();
    ADC_Current_Smooth();
    u32Elapsed10msTick--;
}
```

这样外层调用周期只影响“什么时候补算”，不再改变 ADC 计算内部的 10ms 步数尺度。

## 3. 优化前改到 1s 的影响

### 3.1 温度结果

`ADC_TTC()` 每次调用读取一次 `g_u16ADCValFilter[ADC_TEMP_MOS1]`，查表后做一阶滤波：

```c
g_u32ADCValFilter2[ADC_TEMP_MOS1] =
    (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_MOS1]) >> 3)
    + g_u32ADCValFilter2[ADC_TEMP_MOS1];
```

该滤波系数固定为 `1/8`。优化前若调用周期从 10ms 改到 1s，滤波步进不变，但实际时间常数会约放大 100 倍。优化后会按经过的 10ms tick 补算，滤波步数不会被 1s 外层调用周期拉长。

### 3.2 总压结果

`ADC_Vbc()` 使用 `AD_CalNum = 8` 做累加均值，再做 `1/8` IIR 滤波。

由于代码写法是 `if (s8ADcnt++ < AD_CalNum)`，从初始值开始会先累计 8 次，第 9 次调用才计算一次均值。因此：

| `App_AnlogCal()` 时基 | 均值计算间隔 | 后续 IIR 响应 |
| --- | --- | --- |
| 10ms | 约 90ms 出一次均值 | 正常 |
| 1s | 约 9s 出一次均值 | 进一步变慢 |

优化前把计算时基改成 1s 后，总压结果不是立即每秒更新一次，而是约 9s 才进入一次新的均值结果，然后还要经过 `1/8` 滤波。优化后 1s 调用会一次补约 100 个 10ms 步，总压均值和 IIR 的内部步数仍按原设计推进。

### 3.3 电流结果

`ADC_Current_Smooth()` 使用 `AD_CalNum_Cur = 32` 做电流采样均值，再按零点偏差计算充/放电电流。

同样由于 `if (su8_ADcnt++ < AD_CalNum_Cur)` 的写法，初始阶段会累计 32 次，第 33 次调用才计算一次均值：

| `App_AnlogCal()` 时基 | 电流均值计算间隔 |
| --- | --- |
| 10ms | 约 330ms |
| 1s | 约 33s |

更关键的是电流零点 `g_u16IoutOffsetAD` 需要 `AD_CurOffsetCalNum = 16` 组电流均值才能建立。优化前若 `App_AnlogCal()` 改成 1s，首次零点建立可能需要约 `33s * 16 = 528s`，接近 9 分钟。

优化后外层 1s 调用时会补齐 10ms 步数，电流均值和零点建立仍按原 10ms 设计推进，不会变成 528s 量级。

## 4. 是否会改变计算值本身

对于稳定不变的模拟量，单次换算公式本身不依赖调用周期，例如：

1. 总压换算仍然按 ADC 原始值、分压比例 `Vbc_scale` 计算。
2. 温度仍然按 NTC 查表结果计算。
3. 电流仍然按 ADC 与零点偏差、电流采样电阻参数计算。

因此，稳定输入下最终结果不会因为外层调用周期从 10ms 改为 1s 而产生固定比例错误。

优化前滤波器和均值器是按“调用次数”设计的，不是按真实时间设计的。优化后 `App_AnlogCal()` 已把调用次数改为真实 10ms tick 的补算次数，后续修改外层调用时基时不需要同步修改 `AD_CalNum`、`AD_CalNum_Cur`、`AD_CurOffsetCalNum`。

## 5. 后续修改建议

如果目标是降低主循环调用频率或低功耗，可以在外层调度中降低 `App_AnlogCal()` 的调用频率，但应保持 ADC 硬件触发 `TIM2` 不低于当前 10ms 量级。不同信号的推荐策略如下：

| 信号 | 建议周期 | 原因 |
| --- | --- | --- |
| 电流 | 内部保持 10ms tick 补算 | 动态变化快，零点和滤波依赖 10ms 步数 |
| 总压 | 100ms 到 200ms | 变化中等，通信显示和判断需要较及时 |
| MOS 温度 | 500ms 到 1s | 热变化慢，可降低处理频率 |

若后续希望进一步节省计算量，可以在 `ADC_TTC()`、`ADC_Vbc()`、`ADC_Current_Smooth()` 内部分别做独立降频，但需要按真实时间常数重新标定滤波系数。

## 6. 建议结论

当前改造后的结构：

1. 保持 ADC 硬件采样 `TIM2` 周期不低于 10ms 到 20ms。
2. `TIM3` 维护系统累计 10ms tick。
3. `App_AnlogCal()` 按 elapsed tick 补齐 ADC 计算。
4. 后续外层调用周期可调整到 100ms、200ms 或 1s，但调用间隔内的通信上报仍只能看到上一次补算后的结果。
5. 如需完整保留调用间隔内的 ADC 瞬态变化，需要再增加 ADC DMA 采样历史缓冲；当前方案优先解决滤波和零点建立时间不被外层调用周期放大的问题。
