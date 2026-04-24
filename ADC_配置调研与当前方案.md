# STM32F103 ADC 配置调研与当前工程方案

本文面向当前 `103 + 309` 工程，说明 STM32F103 ADC 的常见使用方式、配置项差异、适用场景，以及当前工程里 `GPIO_ADC_VBUS`、`GPIO_ADC_NMOS`、`GPIO_ADC_CUR` 应如何接入。

## 1. 本工程结论

当前工程使用的是 STM32F10x 标准外设库，不是 HAL/CubeMX 生成结构。Keil 工程目标声明为 `STM32F103C8`，预定义宏包含 `STM32F10X_MD, USE_STDPERIPH_DRIVER`。

本次已按三路 ADC 需求完成代码配置：`GPIO_ADC_VBUS` 测总压，`GPIO_ADC_NMOS` 测 NMOS 温度，`GPIO_ADC_CUR` 测电流。

建议当前需求采用：

| 信号 | 当前宏 | 引脚 | STM32F103 ADC 通道 | 推荐缓存枚举 | 用途 |
|---|---|---:|---:|---|---|
| 总压 | `GPIO_ADC_VBUS` / `PIN_ADC_VBUS` | PA1 | `ADC_Channel_1` | `ADC_VBC` | 总压分压采样 |
| NMOS 温度 | `GPIO_ADC_NMOS` / `PIN_ADC_NMOS` | PB1 | `ADC_Channel_9` | `ADC_TEMP_MOS1` | 10K NTC 温度 |
| 电流 | `GPIO_ADC_CUR` / `PIN_ADC_CUR` | PA2 | `ADC_Channel_2` | `ADC_CUR_AMP` | 电流采样电压 |

推荐整体模式：`ADC1` 独立模式 + 规则组扫描 3 个通道 + `TIM2_CC2` 定时触发 + `DMA1_Channel1` 循环搬运 + 软件滤波/平均。

理由：总压、温度、电流都属于 BMS 低速模拟量，不需要 CPU 在主循环里逐个阻塞等待；定时器触发能保证采样节拍稳定，DMA 循环能把 CPU 占用降到很低，当前代码也已经按这个方向搭好了框架。

## 2. ADC 基础概念

STM32F103 的 ADC 是逐次逼近型 12 位 ADC，原始值范围通常按 `0..4095` 理解。当前代码按 4096 做换算：

```c
输入电压约等于 raw * Vref / 4096
```

其中 `Vref` 通常来自 `VREF+ / VDDA`。如果板子使用 3.3V 供电且 `VREF+` 接 3.3V，则满量程约为 3.3V。ADC 输入脚不能超过模拟参考范围；总压、电流必须先通过分压、放大、限幅等模拟电路变成 ADC 可接受范围。

ADC 正常使用至少要配置：

| 配置项 | 含义 | 影响 |
|---|---|---|
| GPIO 模式 | ADC 引脚必须配置为 `GPIO_Mode_AIN` | 否则数字输入/输出电路会影响采样，甚至把模拟信号拉坏 |
| ADC 时钟 | 当前代码 `RCC_PCLK2_Div8`，72MHz PCLK2 时 ADC 为 9MHz | STM32F103 ADC 时钟不能过高，时钟越高转换越快但误差风险越高 |
| 采样时间 | 当前使用 `ADC_SampleTime_55Cycles5` | 信号源阻抗越高，采样时间应越长 |
| 通道顺序 | `ADC_RegularChannelConfig(..., Rank, ...)` | DMA 缓冲区下标就是扫描顺序 |
| 触发源 | 软件触发、定时器触发、外部事件触发 | 决定什么时候开始一次转换序列 |
| 取数方式 | 轮询、中断、DMA | 决定 CPU 占用和代码复杂度 |
| 校准 | `ADC_ResetCalibration` + `ADC_StartCalibration` | F1 ADC 上电后建议做一次校准 |

转换总时间近似为：

```text
单通道转换时间 = 采样周期 + 12.5 个 ADC 周期
```

当前 `55.5 + 12.5 = 68` 个 ADC 周期。若 ADC 时钟为 9MHz，单通道约 `7.56us`，3 个通道约 `22.7us`。即使按 1kHz 采样，裕量也很大。

## 3. 常见 ADC 配置方式对比

### 3.1 单通道单次转换

做法：配置一个通道，软件启动一次，轮询 EOC，读 `ADC_GetConversionValue()`。

适合：按键电压、偶尔读取的电池包识别电阻、生产测试点。

优点：最简单。缺点：阻塞 CPU，不适合多通道周期采样。

### 3.2 单通道连续转换

做法：`ADC_ContinuousConvMode = ENABLE`，ADC 一直转换同一个通道。

适合：只关心一个模拟量，例如单个电位器或单路传感器的快速刷新。

优点：配置简单，数据更新快。缺点：采样时刻不容易和系统节拍对齐，多通道时不如扫描模式清晰。

### 3.3 多通道扫描

做法：`ADC_ScanConvMode = ENABLE`，配置多个规则通道及 Rank，ADC 按 Rank 依次转换。

适合：总压、温度、电流这类周期性采集的多路低速模拟量。

关键点：Rank 顺序必须和 DMA 缓冲区下标、枚举定义保持一致。例如 Rank1 的结果会落到 DMA 缓冲区下标 0。

### 3.4 软件触发

做法：`ADC_ExternalTrigConv = ADC_ExternalTrigConv_None`，用 `ADC_SoftwareStartConvCmd()` 启动。

适合：主循环控制采样时机、采样频率低、实时性不敏感。

优点：理解简单。缺点：采样间隔受主循环任务影响。

### 3.5 定时器触发

做法：选择 `ADC_ExternalTrigConv_Tx_xxx`，由定时器比较事件或 TRGO 触发 ADC。

适合：需要固定采样频率、后续做滤波、积分、功率估算的场景。

当前工程已经采用 `TIM2_CC2` 触发。注意这里不需要把 PA1 配成 TIM2_CH2 复用输出；ADC 使用的是定时器内部触发事件，PA1 仍应作为模拟输入使用。

### 3.6 轮询、中断、DMA 取数

| 方式 | 适合场景 | 优点 | 缺点 |
|---|---|---|---|
| 轮询 | 偶尔采一次、初始化检测 | 简单直观 | 阻塞 CPU |
| ADC EOC 中断 | 采样频率较低、需要事件通知 | 不阻塞主循环 | 多通道下中断频繁，处理复杂 |
| DMA 循环 | 多通道周期采样 | CPU 占用低，天然适合扫描序列 | 必须保证 DMA 长度、ADC 通道数、缓存下标一致 |

当前需求推荐 DMA 循环。

### 3.7 规则组与注入组

规则组用于常规周期采样，最多 16 个通道序列。注入组用于插队采样，最多 4 个通道，常用于电机控制、电源控制中某个 PWM 相位点的精确采样。

当前总压、温度、电流没有强 PWM 相位要求，规则组足够。若后续电流需要在特定开关节点采样，才考虑注入组。

### 3.8 ADC 独立、双 ADC 同步、交错模式

`ADC_Mode_Independent` 是最常用模式，一个 ADC 独立工作。双 ADC 同步/交错用于更高采样率或同一时刻采两路信号。

当前工程使用 `ADC_Mode_Independent` 是合理的。BMS 总压、温度、电流采样速度要求不高，不需要双 ADC 复杂模式。

### 3.9 模拟看门狗

ADC 模拟看门狗可对某个通道设置上下阈值，超限触发中断。

适合：需要 MCU 侧快速发现过压、过温、过流的辅助保护。BMS 真正安全保护仍应优先依赖 AFE、硬件比较器或专用保护链路，ADC 看门狗只能作为补充。

## 4. 当前工程 ADC 配置状态

### 4.1 已定义的目标引脚

在 `103 + 309/Project/Source/conf/conf_gpio.h` 中：

```c
#define GPIO_ADC_VBUS        GPIOA
#define PIN_ADC_VBUS         GPIO_Pin_1

#define GPIO_ADC_NMOS        GPIOB
#define PIN_ADC_NMOS         GPIO_Pin_1

#define GPIO_ADC_CUR         GPIOA
#define PIN_ADC_CUR          GPIO_Pin_2
```

这些宏已经存在，并已纳入 ADC1 规则组扫描序列。

### 4.2 GPIO 初始化状态

`103 + 309/Project/Source/conf/conf.c` 的 `InitIO()` 已把三个 ADC 引脚配置为模拟输入：

```c
GPIO_InitStructure.GPIO_Pin = PIN_ADC_VBUS | PIN_ADC_CUR;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
GPIO_Init(GPIOA, &GPIO_InitStructure);

GPIO_InitStructure.GPIO_Pin = PIN_ADC_NMOS;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
GPIO_Init(GPIOB, &GPIO_InitStructure);
```

`ADC.c` 的 `InitADC_GPIO()` 也做了同样的模拟输入配置，避免后续单独初始化 ADC 时漏配 GPIO。

### 4.3 当前 ADC 扫描序列

`103 + 309/Project/Source/ADC.h`：

```c
#define AD_Used_amount 3
```

`103 + 309/Project/Source/ADC.c`：

```c
ADC_InitStruct.ADC_Mode = ADC_Mode_Independent;
ADC_InitStruct.ADC_ScanConvMode = ENABLE;
ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
ADC_InitStruct.ADC_NbrOfChannel = AD_Used_amount;

ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 1, ADC_SampleTime_239Cycles5);
ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_55Cycles5);
ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 3, ADC_SampleTime_239Cycles5);
```

DMA 缓冲区下标与枚举保持一致：`g_u16ADCValFilter[ADC_TEMP_MOS1]` 为 PB1 温度，`g_u16ADCValFilter[ADC_CUR_AMP]` 为 PA2 电流采样，`g_u16ADCValFilter[ADC_VBC]` 为 PA1 总压采样。

### 4.4 当前处理函数接入状态

`App_AnlogCal()` 已在 1ms 节拍中调用：

```c
ADC_TTC();
ADC_Vbc();
ADC_Current_Smooth();
```

`ADC_Current_Smooth()` 已改为单端电流采样逻辑：启动后先对 `GPIO_ADC_CUR` 做 16 组均值作为零点 `g_u16IoutOffsetAD`，后续按当前采样值相对零点的偏差计算充/放电电流。它不再读取未配置的 `AD_VREF_AD`。

## 5. 推荐落地配置

### 5.1 推荐 Rank 顺序

为了尽量复用现有枚举和换算函数，推荐让 ADC 扫描顺序匹配现有数组下标：

| DMA 下标 | 枚举 | 信号 | ADC 通道 | 引脚 |
|---:|---|---|---|---|
| 0 | `ADC_TEMP_MOS1` | NMOS 温度 | `ADC_Channel_9` | PB1 |
| 1 | `ADC_CUR_AMP` | 电流采样 | `ADC_Channel_2` | PA2 |
| 2 | `ADC_VBC` | 总压 | `ADC_Channel_1` | PA1 |

对应配置思路：

```c
#define AD_Used_amount 3

ADC_InitStruct.ADC_NbrOfChannel = AD_Used_amount;

ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 1, ADC_SampleTime_239Cycles5); // PB1, NMOS temp
ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_55Cycles5); // PA2, current
ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 3, ADC_SampleTime_239Cycles5); // PA1, VBUS
```

如果总压分压或 NTC 分压的等效源阻抗较高，建议把总压和温度通道采样时间提高到 `ADC_SampleTime_71Cycles5` 或 `ADC_SampleTime_239Cycles5`。电流放大器输出若阻抗低，可以继续用 `55Cycles5`；若有 RC 滤波，也应按实际源阻抗评估。

### 5.2 GPIO 初始化建议

`GPIO_ADC_VBUS`、`GPIO_ADC_NMOS`、`GPIO_ADC_CUR` 必须改为模拟输入：

```c
GPIO_InitStructure.GPIO_Pin = PIN_ADC_VBUS | PIN_ADC_CUR;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
GPIO_Init(GPIOA, &GPIO_InitStructure);

GPIO_InitStructure.GPIO_Pin = PIN_ADC_NMOS;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
GPIO_Init(GPIOB, &GPIO_InitStructure);
```

同时应删除或屏蔽 `InitIO()` 中把这三个引脚配置成 `GPIO_Mode_Out_PP` 的代码，否则后初始化的一方会覆盖前面的配置。

### 5.3 DMA 配置建议

当前 DMA 框架基本合理：

```c
DMA_InitStruct.DMA_PeripheralBaseAddr = (UINT32)(&(ADC1->DR));
DMA_InitStruct.DMA_MemoryBaseAddr = (UINT32)(&g_u16ADCValFilter[0]);
DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralSRC;
DMA_InitStruct.DMA_BufferSize = AD_Used_amount;
DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;
```

需要保证 `AD_Used_amount` 与实际 Rank 数一致。三通道时就是 3。

### 5.4 处理函数建议

采到三路 ADC 后，`App_AnlogCal()` 应至少考虑：

```c
ADC_TTC();              // NMOS 温度
ADC_Vbc();              // 总压
ADC_Current_Smooth();   // 电流，单端采样加启动零点
```

如果后续确认硬件电流输出方式不同，需要按实际电路调整：

| 硬件电流输出方式 | 建议 |
|---|---|
| 单端输出，0A 对应固定偏置 | 上电无电流时采集 `g_u16IoutOffsetAD`，用 `ADC_CUR_AMP - offset` 换算 |
| 双向电流，另有参考电压输出 | 增加 `AD_VREF_AD` 的 ADC 通道，`AD_Used_amount` 变为 4 |
| AFE 已经提供电流，MCU ADC 只做辅助 | MCU ADC 电流值只参与显示/诊断，不直接做保护决策 |

## 6. 本次修改清单

本次已完成：

1. `AD_Used_amount` 修改为 3。
2. PA1、PA2、PB1 配置为 `GPIO_Mode_AIN`。
3. ADC1 规则组 Rank 配置为 PB1 温度、PA2 电流、PA1 总压。
4. `App_AnlogCal()` 接入温度、总压、电流处理函数。
5. 电流处理改为单端采样加启动零点偏置。
6. `GPIO_AD_TTC_MOS1` 改为兼容别名，指向 `GPIO_ADC_VBUS`，避免 PA1 旧命名继续独立漂移。

## 7. 当前最需要注意的风险

1. 电流方向当前按“采样值低于启动零点为放电、高于启动零点为充电”处理；如果硬件极性相反，需要交换 `gu16_BusCurr_CHG` / `gu16_BusCurr_DSG` 的赋值分支。
2. 启动零点要求上电初期电流接近 0；如果上电时一定带载，需要改为 EEPROM 校准零点或上位机校准零点。
3. 总压比例 `Vbc_scale`、NTC 表、电流采样电阻/放大倍数仍需要上板实测校准。
4. 总压分压必须确认最大包压下 ADC 输入不超过 `VDDA/VREF+`，否则有烧毁 MCU 或读数饱和风险。

## 8. STOP/RTC 唤醒与 ADC 恢复

本次补充了 STOP 低功耗下的 ADC 处理策略：

1. 进入 STOP 前调用 `ADC_StopForLowPower()`，显式关闭 `TIM2`、`ADC1`、`DMA1_Channel1`，再把 GPIO 切到低功耗状态，避免唤醒后残留触发源或 DMA 状态。
2. `Sys_StopMode()` 从 STOP 返回后无条件调用 `cpu_frequency_conf()`，不再依赖未定义的 `_HSE_8M_PLL_48M` / `_HSE_12M_PLL_48M` 宏恢复 HSE/PLL。
3. RTC 周期唤醒时只走 `InitRtcWakeupCheck()`，恢复延时、串口日志、AFE IIC 和 EEPROM IIC，用于读取 AFE 状态。
4. 若 RTC 唤醒后 AFE 判断无充电、无放电、无异常，`rtc_sleep_run_hiccup_cycle()` 直接返回继续下一轮 RTC STOP，不启动 ADC。
5. 若 RTC 唤醒发现电流或异常，或者由外部中断唤醒，则走 `InitRunAfterStopWakeup()`，完整恢复 `InitIO()`、`InitADC()`、CAN、TIM3 等运行外设。

ADC 电流零点 `g_u16IoutOffsetAD` 在 `InitADC()` 中不再每次清零，STOP 唤醒后会尽量沿用休眠前的零点，避免“带电流唤醒后重新把当前电流当零点”的风险。首次上电时该变量仍由 BSS 初始化为 0，后续会按原逻辑自动采集零点。

## 9. 构建验证

2026-04-24 使用 Keil MDK 命令行全量重编译：

```text
UV4.exe -r 103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx -t "Target 1"
```

结果：

```text
0 Error(s), 37 Warning(s)
Program Size: Code=45356 RO-data=4020 RW-data=1256 ZI-data=6176
```

主要产物：

```text
103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.axf
103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.bin
```

这些 warning 为工程已有的未使用变量、缺少 return、文件末尾换行等问题；本次 STOP/ADC 恢复相关文件没有编译错误。

## 10. 参考资料

1. ST 官方参考手册 RM0008：`STM32F101xx, STM32F102xx, STM32F103xx ... advanced ARM-based 32-bit MCUs`，ADC 章节包含扫描模式、外部触发、DMA、规则组/注入组、多 ADC 模式说明。  
   https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf
2. ST 官方 STM32F103x8/xB 数据手册：引脚复用表包含 PA1/PA2/PB1 的 ADC 通道映射。  
   https://www.st.com/resource/en/datasheet/stm32f103c8.pdf
3. 本工程标准外设库头文件：`103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/inc/stm32f10x_adc.h`，包含 `ADC_Mode_*`、`ADC_ExternalTrigConv_*`、`ADC_Channel_*`、`ADC_SampleTime_*` 定义。
4. 本工程当前 ADC 代码：`103 + 309/Project/Source/ADC.c`、`103 + 309/Project/Source/ADC.h`。
5. 本工程当前 GPIO 配置：`103 + 309/Project/Source/conf/conf.c`、`103 + 309/Project/Source/conf/conf_gpio.h`。
