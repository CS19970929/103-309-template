# BMS 系统初始化与运行时调度模块分析

---

## 第一部分：系统初始化与运行时调度

### 1. main.c 入口点

**文件路径**: `103 + 309/Project/Source/main.c`

```c
int main(void)
{
    Runtime_Boot();
    while (1)
    {
        Runtime_RunOnce();
    }
}
```

`main.c` 是整个固件的入口，仅有 12 行代码，体现了极简的"初始化 + 主循环"经典嵌入式架构。整个程序的生命周期只有两个阶段：

1. **启动阶段** — 调用 `Runtime_Boot()` 完成所有硬件和软件初始化，此阶段仅执行一次。
2. **运行阶段** — `while(1)` 无限循环中反复调用 `Runtime_RunOnce()`，完成周期性任务调度。

没有任何 RTOS 或调度器框架，整个系统采用**协作式单任务轮询**模型。每个外设模块的周期性逻辑都挂在 `Runtime_RunOnce()` 中按顺序执行，通过时间标志位（Time Flag）机制实现不同周期的分频调度。

---

### 2. Runtime_Boot() 启动初始化

**文件路径**: `103 + 309/Project/Source/Runtime.c` 第 11-53 行

`Runtime_Boot()` 是系统上电后的完整初始化序列，按严格顺序执行以下步骤：

| 序号 | 调用 | 功能说明 |
|------|------|----------|
| 1 | `DebugWatch_BindAll()` | 绑定所有调试观测变量到调试窗口 |
| 2 | `IrqDebug_SetPhase(IRQDBG_PHASE_BOOT)` | 标记当前为 BOOT 阶段（用于 IRQ 调试计数归零） |
| 3 | `SystemInit()` | STM32 标准库系统初始化（时钟树配置） |
| 4 | `InitDelay()` | 配置 SysTick 微秒/毫秒延时（基于 HCLK/8） |
| 5 | `SleepDeal_HandleBootSleepStartup()` | 处理启动时的休眠唤醒逻辑 |
| 6 | `jtag_disableAndConfIO()` | 禁用 JTAG 并释放引脚为普通 GPIO |
| 7 | `InitNVIC()` | 配置 NVIC 优先级分组（Group_1） |
| 8 | `InitIO()` | 初始化所有 GPIO 引脚 |
| 9 | `InitUSART_CommonUpper()` | 初始化上位机通信串口 |
| 10 | `InitE2PROM()` | 初始化 EEPROM |
| 11 | `InitAFE1()` | 初始化 AFE（模拟前端芯片，如 SH367309） |
| 12 | `InitCan()` | 初始化 CAN 总线 |
| 13 | `InitADC()` | 初始化 ADC + DMA + TIM2 触发 |
| 14 | `InitData_SOC()` | 初始化 SOC 数据 |
| 15 | `InitTimer()` | 配置 TIM3 产生 10ms 周期中断 |
| 16 | `__enable_irq()` | 开放全局中断 |
| 17 | `IrqDebug_SetPhase(IRQDBG_PHASE_RUN)` | 切换 IRQ 调试阶段为 RUN |
| 18 | `EnableLowPowerDebug()` | 配置低功耗调试模式（DBGMCU 寄存器） |
| 19 | `Init_IWDG()` | 初始化独立看门狗 |
| 20 | `InitSystemMonitorData_EEPROM()` | 从 EEPROM 加载系统监控数据 |
| 21 | 计算采样电阻 | `g_u32CS_Res_AFE = (u16Sys_CS_Res_Num * 1000) / u16Sys_CS_Res` |
| 22 | `SystemRuntime_MarkBootReady()` | 标记系统启动完成 |
| 23 | `SystemRuntime_SetProjectVersion(1U)` | 设置项目版本号 |
| 24 | `LedBar_Init()` | 初始化 LED 灯条 |
| 25 | `InitProID()` | 初始化产品 ID（硬件/软件版本、序列号） |
| 26 | `LogRecord_RequestStartup()` | 请求记录启动事件日志 |
| 27 | `Init_RTC()` | 初始化 RTC 实时时钟 |
| 28 | `DBG_Init()` | 初始化调试子系统 |

**关键设计要点**：

- **中断开启时机**：`InitTimer()` 在 `__enable_irq()` 之前执行，确保 TIM3 中断在全局中断开放后立即生效，但在此之前已完成定时器配置，避免产生意外中断。
- **看门狗位置**：IWDG 在大部分初始化完成后才开启，避免初始化耗时过长导致看门狗复位。
- **ADC 校准**：`InitADC()` 内部包含 ADC 复位校准和启动校准，带超时保护（100000 次循环），校准失败会调用 `System_ERROR_UserCallback(ERROR_ADC)` 上报。
- **调试阶段标记**：`IrqDebug_SetPhase()` 用于区分 BOOT 和 RUN 阶段的 IRQ 计数行为。

---

### 3. Runtime_RunOnce() 运行时调度

**文件路径**: `103 + 309/Project/Source/Runtime.c` 第 55-109 行

`Runtime_RunOnce()` 是主循环的核心，每次被调用完成一轮所有周期性任务的调度。它将任务分为三个段（Section），每段前后有调试钩子用于测量执行时间：

#### 第一段：时间敏感任务

| 任务 | 说明 |
|------|------|
| `SysTime_LatchTaskFlags()` | 从中断上下文锁存时间标志到主循环 |
| `FactoryAging_Task()` | 工厂老化计时任务 |
| `APP_LedBar()` | LED 灯条显示刷新 |
| `App_AFEGet()` | 读取 AFE 芯片数据（电芯电压、温度等） |

#### 第二段：通信与采集任务

| 任务 | 说明 |
|------|------|
| `App_CommonUpper()` | 上位机串口通信协议处理 |
| `App_AnlogCal()` | ADC 模拟量采集与校准（MOS温度、VBAT、TypeC电流） |
| `rtc_sleep()` | 低功耗休眠管理 |
| `App_Can()` | CAN 总线通信 |

#### 第三段：后台任务

| 任务 | 说明 |
|------|------|
| `App_FlashUpdate()` | Flash 存储更新 |
| `App_LogRecord()` | 日志记录 |
| `App_ProID_Deal()` | 产品 ID 处理（当前为空函数占位） |
| `Feed_IWatchDog` | 喂狗（宏展开为 `IWDG_Feed()`） |
| `DBG_Task()` | 调试任务 |

**设计特点**：

- **调试时间测量**：每个任务前后都有 `DebugHooks_Runtime*()` 调用，用于精确测量各任务的执行时间，便于性能优化。
- **喂狗位置**：看门狗喂狗放在最后一段的末尾，确保整个主循环在看门狗超时周期内完成。
- **任务排列顺序**：时间敏感任务（时间锁存、AFE读取）在最前面，通信任务在中间，后台非实时任务在最后。

---

### 4. System_Init.c：时钟、定时器、IWDG

**文件路径**: `103 + 309/Project/Source/System_Init.c`

#### 4.1 NVIC 优先级配置

```c
void InitNVIC(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);  // 1位抢占 + 3位子优先级
}
```

使用 **NVIC_PriorityGroup_1**：最高 1 位用于抢占式优先级，最低 3 位用于响应优先级。这意味着系统最多支持 2 级抢占中断嵌套和每级 8 个子优先级。

#### 4.2 SysTick 延时系统

```c
void InitDelay(void)
{
    SysTick->CTRL &= ~(1 << 2);         // 使用外部时钟源（HCLK/8）
    fac_us = SystemCoreClock / 8000000;  // 每微秒的计数值
    fac_ms = fac_us * 1000;             // 每毫秒的计数值
}
```

- **时钟源**：选择外部时钟源 STCLK = HCLK/8。在 72MHz 主频下，STCLK = 9MHz。
- **延时精度**：`fac_us = 72000000 / 8000000 = 9`，即每个微秒需要 9 个 SysTick 时钟周期。
- **最大延时**：SysTick 是 24 位寄存器，在 72MHz 下最大延时约 1864ms。
- `__delay_us()` 用于微秒级延时，不喂狗；`__delay_ms()` 用于毫秒级延时，循环中喂狗防止延时期间看门狗复位。

#### 4.3 IWDG 看门狗

```c
void Init_IWDG(void)
{
    // 使能 PWR 外设时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    
    #ifndef __FUNC_RTC__
    // 无 RTC 时：Prescaler=64, Reload=800 → 超时约 1.28s
    IWDG_SetPrescaler(IWDG_Prescaler_64);
    IWDG_SetReload(800);
    #else
    // 有 RTC 时：Prescaler=256, Reload=0x0FFF → 超时约 16.38s
    IWDG_SetPrescaler(IWDG_Prescaler_256);
    IWDG_SetReload(0x0FFF);
    #endif
    
    IWDG_ReloadCounter();
    IWDG_Enable();
}
```

**看门狗超时计算**：

- **无 RTC 模式**：LSI=40kHz, 分频 64 → 计数时钟 = 40000/64 = 625Hz, 重载值 800 → 超时 = 800/625 = 1.28s
- **有 RTC 模式**：LSI=40kHz, 分频 256 → 计数时钟 = 40000/256 ≈ 156.25Hz, 重载值 0x0FFF(4095) → 超时 = 4095/156.25 ≈ 26.2s

有 RTC 时看门狗超时更长，因为 RTC 进入 STOP 模式后看门狗也暂停（取决于 `wdog_enable` 宏），需要更大的超时窗口。

#### 4.4 低功耗调试

```c
void EnableLowPowerDebug(void)
{
    #ifdef __EnableLowPowerDebug__
    DBGMCU->CR |= LOW_POWER_DEBUG_MASK;  // 使能低功耗调试
    #else
    DBGMCU->CR &= ~LOW_POWER_DEBUG_MASK; // 禁用低功耗调试
    #endif
}
```

通过 `DBGMCU->CR` 寄存器控制 STOP/STANDBY 模式下的调试行为。`__EnableLowPowerDebug__` 默认未定义（在 conf.h 中被注释掉），因此 Release 版本会清除这些调试位以降低功耗。

---

### 5. TIM3 10ms 定时器系统

**文件路径**: `103 + 309/Project/Source/System_Init.c` 第 87-154 行, 第 324-332 行

#### 5.1 定时器配置

```c
void InitTimer(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_Cmd(TIM3, DISABLE);
    
    timer_init.TIM_Prescaler = Timer_GetPrescalerFor100kHz();  // 分频到 100kHz
    timer_init.TIM_Period = 999U;                              // 周期 = 1000 计数
    timer_init.TIM_ClockDivision = TIM_CKD_DIV1;
    timer_init.TIM_CounterMode = TIM_CounterMode_Up;
    timer_init.TIM_RepetitionCounter = 0x00;
    TIM_TimeBaseInit(TIM3, &timer_init);
    ...
    NVIC_Init(&nvic_init);  // 抢占优先级 0, 子优先级 3
    SysTime_ResetCounters();
    TIM_Cmd(TIM3, ENABLE);
}
```

**定时参数**：
- TIM3 挂载在 APB1 总线上，APB1 最大 36MHz，但定时器时钟为 72MHz（APB1 预分频不为1时自动倍频）。
- 预分频 = `72000000 / 100000 - 1 = 719`，得到 100kHz 计数频率。
- 周期 = 999 + 1 = 1000 个计数 → **中断频率 = 100kHz / 1000 = 100Hz → 10ms 周期**。

#### 5.2 TIM3 中断处理

```c
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        IrqDebug_CountFast(IRQDBG_TIM3_10MS);
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        SysTime_Post10msTick();
    }
}
```

TIM3 更新中断中调用 `SysTime_Post10msTick()`，这是整个时间调度的心跳源。

---

### 6. 时间标志位机制

**文件路径**: `103 + 309/Project/Source/System_Init.c` 第 285-314 行, 第 202-218 行

#### 6.1 SYS_TIME 位域结构

```c
union SYS_TIME {
    UINT16 all;
    struct StatusSysTimeFlagBit {
        UINT16 b1Sys10msFlag   : 1;
        UINT16 b1Sys50msFlag   : 1;
        UINT16 b1Sys100msFlag  : 1;
        UINT16 b1Sys200msFlag  : 1;
        UINT16 b1Sys1000msFlag : 1;
        UINT16 reserved        : 11;
    } bits;
};
```

使用位域联合体实现多个布尔时间标志的原子操作。`all` 字段允许一次性读取或清除所有标志。

#### 6.2 双缓冲时间标志机制

系统采用**双缓冲（Pending + Latch）**机制，避免主循环和中断之间的竞态：

```
中断上下文 (TIM3_IRQHandler)          主循环 (Runtime_RunOnce)
─────────────────────────────         ─────────────────────────────
SysTime_Post10msTick()                SysTime_LatchTaskFlags()
  ↓                                     ↓
写入 s_st_SysTimePending              读取 s_st_SysTimePending
(原子操作, 关中断)                      复制到 g_st_SysTimeFlag
                                       清零 s_st_SysTimePending
```

**数据流**：
1. TIM3 中断中 `SysTime_Post10msTick()` 在 `s_st_SysTimePending` 中设置标志位。
2. 主循环开头调用 `SysTime_LatchTaskFlags()`，关中断后将 pending 复制到 `g_st_SysTimeFlag`，同时清零 pending。
3. 各任务模块检查 `g_st_SysTimeFlag.bits.b1Sys*msFlag` 决定是否执行。

#### 6.3 分频计数器

```c
static void SysTime_Post10msTick(void)
{
    s_u32Sys10msTickCount++;
    s_st_SysTimePending.bits.b1Sys10msFlag = 1U;

    if (++s_u8Cnt50ms >= 5U)    { s_u8Cnt50ms = 0;  s_st_SysTimePending.bits.b1Sys50msFlag = 1U; }
    if (++s_u8Cnt100ms >= 10U)  { s_u8Cnt100ms = 0; s_st_SysTimePending.bits.b1Sys100msFlag = 1U; }
    if (++s_u8Cnt200ms >= 20U)  { s_u8Cnt200ms = 0; s_st_SysTimePending.bits.b1Sys200msFlag = 1U; 
                                   SysTime_Post200msTaskPeriod(); }
    if (++s_u8Cnt1000ms >= 100U){ s_u8Cnt1000ms = 0; s_st_SysTimePending.bits.b1Sys1000msFlag = 1U; }
}
```

所有分频均基于 10ms 基准：
- **10ms** — 每次中断直接设置
- **50ms** — 每 5 次 10ms 设置
- **100ms** — 每 10 次 10ms 设置
- **200ms** — 每 20 次 10ms 设置
- **1000ms** — 每 100 次 10ms 设置

#### 6.4 200ms 任务溢出保护

```c
#define SYS_TIME_200MS_PENDING_LIMIT ((UINT8)5U)

static void SysTime_Post200msTaskPeriod(void)
{
    if (s_u8Sys200msPendingPeriods < SYS_TIME_200MS_PENDING_LIMIT)
    {
        s_u8Sys200msPendingPeriods++;
    }
    else if (s_u16Sys200msOverflowCnt < 0xFFFFU)
    {
        s_u16Sys200msOverflowCnt++;
    }
}
```

200ms 任务有专门的"待处理周期计数器"，最大缓存 5 个周期。如果主循环处理太慢导致积压超过 5 个周期，溢出计数器会递增。这为监控系统调度实时性提供了诊断手段。消费端通过 `SysTime_Take200msTaskPeriod()` 原子地递减并返回。

---

## 第二部分：外设模块分析

### 1. ADC 模块

**文件路径**: `103 + 309/Project/Source/ADC.c`

#### 1.1 运行时数据结构

```c
typedef struct ADC_RUNTIME_TAG
{
    __IO UINT16 raw[ADC_NUM];      // DMA 原始采样值
    INT32 result[ADC_NUM];         // 转换后的工程值
    UINT32 last;                   // 上次处理时的 10ms tick
    UINT32 vbat;                   // 电池电压 (mV)
    UINT16 typec;                  // TypeC 输出电流 (mA)
    UINT8 discard;                 // 启动丢弃计数
    UINT8 ready;                   // 数据就绪标志
} ADC_RUNTIME;
```

#### 1.2 三通道 ADC 采集

ADC 采集 3 路模拟信号：

| 通道 | 引脚 | ADC Channel | 采样时间 | 用途 |
|------|------|-------------|----------|------|
| ADC_TEMP_MOS1 | PB1 | Channel 9 | 239.5 cycles | MOS 管 NTC 温度 |
| ADC_CUR_AMP | PA2 | Channel 2 | 55.5 cycles | TypeC 输出电流 |
| ADC_VBC | PA1 | Channel 1 | 239.5 cycles | 电池电压分压采样 |

#### 1.3 触发与 DMA

- **TIM2 CC2 触发**：TIM2 配置为 PWM1 模式，周期 100kHz/1000 = 100Hz（10ms），每次 CC2 比较匹配触发一次 ADC 转换。
- **DMA1 Channel1 循环传输**：ADC 转换结果通过 DMA 自动搬运到 `s_adc.raw[]` 数组，无需 CPU 干预。
- **扫描模式**：`ADC_ScanConvMode = ENABLE`，每次触发依次采集 3 个通道。

#### 1.4 数据处理 (App_AnlogCal)

```c
void App_AnlogCal(void)
{
    // 每 10ms 检查一次
    if (0U == u32Elapsed10msTick) return;
    
    // 启动后丢弃前 1 个周期的数据
    if (s_adc.discard != 0U) { --s_adc.discard; return; }
    
    ADC_UpdateMosTemp();       // NTC 温度查表
    ADC_UpdateVbc();           // 电池电压计算
    ADC_UpdateTypeCCurrent();  // TypeC 电流计算
    s_adc.ready = 1U;
}
```

#### 1.5 NTC 温度查表

`iSheldTemp_10K[]` 是 10K NTC 热敏电阻的 AD-温度对照表，覆盖 -30°C 到 +105°C（步进 5°C），每项包含 AD 值和对应的温度值（(Temp+40)*10 编码）。使用 `GetEndValue()` 函数进行线性插值。

#### 1.6 电池电压计算链

```
AD 原始值 → ADC_VbcAdToMilliVolt() → ADC_mV = (AD * VDDA_MV + 2048) / 4096
         → ADC_VbcAdcMvToBatteryMV() → Battery_mV = ADC_mV * (RTOP + RBOT) / RBOT
```

通过分压电阻比例还原实际电池电压。

#### 1.7 TypeC 电流计算链

```
AD 原始值 → ADC_TypeCAdToMilliVolt() → delta_mV = (AD * VDDA_MV + 2048) / 4096
         → ADC_TypeCDeltaMvToMilliAmp() → current_mA = delta_mV * 1000 / RSENSE_MOHM
```

支持零电流死区判断（`AD_CurZeroDeadband`），低于死区值时直接返回 0。

#### 1.8 低功耗停止

```c
void ADC_StopForLowPower(void)
{
    // 依次关闭 TIM2 → ADC → DMA，最后关闭时钟
    // 反向于 InitADC 的初始化顺序
}
```

进入低功耗模式前完全关闭 ADC 外设及其时钟，降低漏电流。

---

### 2. RTC 模块

**文件路径**: `103 + 309/Project/Source/RTC.c`

#### 2.1 运行时数据结构

```c
typedef struct RTC_RUNTIME_TAG
{
    __IO UINT8 disp;              // 时间更新标志（秒中断设置）
    volatile bool wake;           // 闹钟唤醒标志
    struct RTC_ELEMENT time;      // 当前时间（年月日时分秒）
    UINT32 last;                  // 上次设置的唤醒周期（秒）
    UINT32 wake_override;         // 自定义唤醒周期
} RTC_RUNTIME;
```

#### 2.2 时钟源策略

`RTC_ClockConfig()` 实现了完整的时钟源选择和降级策略：

```
第一次启动 (BKP_DR1 != RTC_BKP_DATA)?
├── 是 → 完整初始化
│   ├── 尝试 LSE (32.768kHz 外部晶振)
│   │   ├── 成功 → LSE 时钟，分频 32767+1 = 1Hz
│   │   └── 超时 → 降级到 LSI，分频 40000-1 = 1Hz
│   └── 写入初始时间 2018-12-31 23:59:30
└── 否 → 恢复已有配置
    ├── 检查当前 RTCSEL 寄存器
    │   ├── LSE → 等待 LSIRDY
    │   └── LSI → 使能 LSI
    └── 如果恢复失败 → 重新完整初始化
```

**关键设计**：
- 使用 BKP_DR1 作为"RTC 已初始化"标志（写入 `RTC_BKP_DATA`）。
- LSE 失败时自动降级到 LSI，保证 RTC 始终可用。
- 每个 RTC 寄存器操作后都有 `RTC_WaitForLastTaskSafe()` 超时保护。
- 默认初始时间为 2018-12-31 23:59:30（Unix Time 纪元附近），确保有合理的时间起点。

#### 2.3 时间转换

- **Unix Time**：以 1970-01-01 00:00:00 为纪元的秒计数。
- `TIME_ZOOM` 是北京时间偏移量（UTC+8 = 28800 秒）。
- `Second_To_RTCtime()` 将 Unix Time 转为年月日时分秒。
- `Seccond_Cal()` 反向计算，将日期时间转为 Unix Time。

#### 2.4 中断处理

| 中断源 | 处理 |
|--------|------|
| `RTC_IRQHandler` (秒中断) | 递增 `sys_time.rtc_sec_cnt`，设置 `s_rtc.disp = 1` |
| `RTCAlarm_IRQHandler` (闹钟中断) | 递增 `sys_time.rtc_alm_cnt`，设置 `s_rtc.wake = true` |

秒中断通过 `s_rtc.disp` 标志通知主循环更新时间显示，闹钟中断用于从 STOP 模式唤醒。

#### 2.5 低功耗唤醒

```c
void RTC_WKTimeConfig(void)
{
    RTC_EnableBackupAccess();
    RTC_DisableSecondInterrupt();     // 关闭秒中断
    RTC_DisableAlarmInterrupt();      // 关闭闹钟中断
    wake_seconds = RTC_GetWakeupPeriodSeconds();
    RTC_EnableAlarmAfterSeconds(wake_seconds);  // 设置新的闹钟
}
```

进入 STOP 模式前配置 RTC 闹钟唤醒时间。默认 10 秒，有看门狗时限制最大 10 秒。唤醒后通过 `RTC_RestoreRunInterrupts()` 恢复秒中断。

---

### 3. Fault 模块

**文件路径**: `103 + 309/Project/Source/Fault.c`

Fault 模块相对简洁，主要定义全局故障标志和记录结构：

```c
struct PRT_E2ROM_PARAS PRT_E2ROMParas;          // EEPROM 中的保护参数

union FAULT_FLAG_FIRST  Fault_Flag_Fisrt;       // 一级故障标志（快断）
union FAULT_FLAG_SECOND Fault_Flag_Second;      // 二级故障标志
union FAULT_FLAG_THIRD  Fault_Flag_Third;       // 三级故障标志

UINT16 Fault_record_Third[Record_len];          // 三级故障历史记录环形缓冲区
UINT16 Fault_record_Third2[Record_len];         // 三级故障历史记录备份
UINT8 FaultPoint_Third;                          // 三级记录写指针
UINT8 FaultPoint_Third2;                         // 三级备份记录写指针
```

#### 三级故障体系

| 级别 | 说明 |
|------|------|
| **一级故障 (First)** | 最严重的即时故障，需要立即响应 |
| **二级故障 (Second)** | 中等严重故障 |
| **三级故障 (Third)** | 一般告警，带历史记录功能 |

`FaultWarnRecord2()` 实现环形缓冲区写入，当写指针达到 `Record_len` 时自动回绕到 0。

---

### 4. FactoryAging 模块

**文件路径**: `103 + 309/Project/Source/FactoryAging.c`

#### 4.1 功能概述

工厂老化测试模块，用于在生产线上对 BMS 进行持续老化测试。支持：
- 通过上位机启动/停止/重置老化
- 自定义老化时长（1-168 小时）
- 掉电保存进度（BKP 寄存器 + Flash 双重备份）
- 老化期间自动开启 MOS 管

#### 4.2 状态机

```
                    ┌──────────────┐
                    │   UNINIT     │ ← 上电初始状态
                    └──────┬───────┘
                           │ FactoryAging_Start()
                    ┌──────▼───────┐
              ┌─────│   RUNNING    │←───┐
              │     └──────┬───────┘    │
              │            │ 老化完成    │ Host 重置时间
              │     ┌──────▼───────┐    │
              │     │    DONE      │    │
              │     └──────────────┘    │
              │                          │
    Host 停止 │     ┌──────────────┐    │
              └────→│   STOPPED    │────┘
                    └──────────────┘
```

- **UNINIT** — 上电后首次进入，加载持久化存储恢复进度
- **RUNNING** — 老化进行中，MOS 管开启
- **DONE** — 老化完成，MOS 管关闭
- **STOPPED** — 上位机主动停止，MOS 管关闭

#### 4.3 持久化存储策略

采用 **BKP + Flash 双重备份**：

| 存储介质 | 保存间隔 | 内容 |
|----------|----------|------|
| BKP_DR6-DR10 | 每 1 秒 | elapsed10ms + magic + CRC |
| Flash | 每 7200 秒 (2小时) | elapsed10ms + state + durationHours |

- **BKP 优势**：写入速度快，STOP 模式下保持，适合频繁更新
- **BKP 校验**：magic (0xA91E) + inverse_magic + CRC 三重校验
- **Flash 优势**：掉电不丢失（BKP 在某些复位情况下可能丢失）
- **恢复策略**：上电时同时读取 BKP 和 Flash，取较大值作为恢复起点

#### 4.4 MOS 管控制

```c
static void FactoryAging_ApplyRunningMos(void)
{
    UINT8 next_mode = MosStartup_Is5vChargeActive() ? 
        FACTORY_AGING_MOS_MODE_5V_CHARGE : FACTORY_AGING_MOS_MODE_FACTORY;
    
    if (s_factory_aging.mosMode == next_mode) return;  // 缓存避免重复操作
    
    enter_fac_mode(true);
    s_factory_aging.mosMode = next_mode;
}
```

老化期间 MOS 管通过 `enter_fac_mode(true)` 开启，支持区分 5V 充电模式和普通工厂模式。使用 `mosMode` 缓存避免重复切换。

#### 4.5 看门狗安全

老化完成时如果 Flash 写入失败，会在 1 秒后重试（`FACTORY_AGING_FINISH_RETRY_10MS`），而不是死循环重试。

#### 4.6 睡眠前保存

```c
UINT8 FactoryAging_SaveProgressBeforeSleep(void)
```

在进入 STOP 模式前被调用，确保老化进度被保存。如果此时恰好完成老化，直接调用 `FactoryAging_Finish()`。

---

### 5. ProductionID 模块

**文件路径**: `103 + 309/Project/Source/ProductionID.c`

#### 5.1 数据结构

```c
PRODUCTION_ID_INFO ProductionInfor;
```

包含：
- `BMS_HardWareVersion[]` — 硬件版本字符串
- `BMS_SoftWareVersion[]` — 软件版本字符串
- `BMS_SerialNumber[]` — 序列号字符串
- 各字段的长度字段

#### 5.2 初始化

```c
void InitProID(void)
{
    InitProID_DefaultData();  // 从编译时宏填充默认值
}
```

使用 `memcpy` 从 `BMS_HARDWARE_VERDION_DEFAULT`、`BMS_SOFTWARE_VERDION_DEFAULT`、`BMS_SERIAL_NUMBER_DEFAULT` 宏拷贝默认值。每个字段长度限制在 `PRODUCT_ID_LENGTH_MAX` 以内。

#### 5.3 运行时

```c
void App_ProID_Deal(void)
{
    /* Production ID is initialized during boot; keep this hook for runtime heartbeat. */
}
```

当前为空函数占位。产品 ID 在启动时一次性初始化，运行时不需要动态更新。这个函数保留在 `Runtime_RunOnce()` 中作为占位，未来可以扩展（如从 EEPROM 读取、或响应上位机修改）。

---

## 第三部分：配置体系

### 1. Project_Config.h 宏定义

**文件路径**: `103 + 309/Project/Source/conf/Project_Config.h`

这是整个项目的核心配置文件，使用 Keil Configuration Wizard 格式，可通过 IDE 图形界面编辑。分为以下配置组：

#### 1.1 基础配置 (Project Basic Configuration)

| 宏 | 值 | 说明 |
|----|-----|------|
| `PROJECT_CFG_BAT_TYPE` | 1 (Slave) | 电池类型：从机 40A |
| `PROJECT_CFG_BAT_CHEMISTRY` | 0 (三元锂) | 电池化学体系 |
| `PROJECT_CFG_HOST_WRITE_ENABLE` | 1 | 允许上位机写寄存器 |
| `PROJECT_CFG_FD_YEAR/MONTH/DAY` | 26/5/9 | 固件日期 2026-05-09 |
| `PROJECT_CFG_VERSION` | 5 | 固件版本号 |
| `PROJECT_CFG_LEVEL_CURR` | 2 (150A) | 额定电流档位 |
| `PROJECT_CFG_AFE_TYPE` | 1 (sh36xx) | AFE 芯片类型 |
| `PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG` | 0x2445 | EEPROM 初始化标志 |

#### 1.2 功能开关 (Feature Switches)

| 宏 | 值 | 说明 |
|----|-----|------|
| `PROJECT_CFG_WDOG_ENABLE` | 1 | 使能独立看门狗 |
| `PROJECT_CFG_RTC_ENABLE` | 1 | 使能 RTC |
| `PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS` | 259200 (72h) | 默认老化时长 |
| `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE` | 1 | 使能虚拟电流 |
| `PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE` | 1 | 使能长按键开关机 |
| `PROJECT_CFG_DEBUG_MONITOR_ENABLE` | 0 | 禁用调试监控 |
| `PROJECT_CFG_IRQ_DEBUG_ENABLE` | 0 | 禁用 IRQ 调试计数 |

#### 1.3 唤醒源 (Wakeup Sources)

| 宏 | 值 | 说明 |
|----|-----|------|
| `PROJECT_CFG_UART1_WAKEUP_ENABLE` | 1 | UART1 唤醒 |
| `PROJECT_CFG_RS485_WAKEUP_ENABLE` | 1 | RS485 唤醒 |

#### 1.4 串口角色 (Serial Port Roles)

| 宏 | 值 | 说明 |
|----|-----|------|
| `PROJECT_CFG_SCI1_ROLE` | 1 (Host) | SCI1 作为上位机通信口 |

#### 1.5 Flash 日志磨损保护

| 宏 | 值 | 说明 |
|----|-----|------|
| `PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` | 3600 | 同事件最小记录间隔 1 小时 |

#### 1.6 SOC 校准参数

一组用于 SOC（荷电状态）算法的产品调优参数，包括满电确认电压裕度、OCV 等待时间、板载自耗电等。

#### 1.7 升级参数策略 (Upgrade Parameter Policy)

定义固件升级时的参数处理策略，包括是否重置 AFE 参数、保护参数、均衡参数等，以及 32 个 OtherElement 寄存器的默认值。

---

### 2. conf.h 衍生宏定义

**文件路径**: `103 + 309/Project/Source/conf/conf.h`

`conf.h` 是连接 `Project_Config.h` 和源代码的**适配层**，将 `PROJECT_CFG_*` 数字宏转换为源代码中使用的功能宏：

| conf.h 宏 | 源自 | 说明 |
|-----------|------|------|
| `TERNARYLI` | `PROJECT_CFG_BAT_CHEMISTRY == 0` | 三元锂电池化学体系 |
| `LIFEPO` | `PROJECT_CFG_BAT_CHEMISTRY == 1` | 磷酸铁锂化学体系 |
| `wdog_enable` | `PROJECT_CFG_WDOG_ENABLE` | 看门狗使能 |
| `__FUNC_RTC__` | `PROJECT_CFG_RTC_ENABLE` | RTC 功能使能 |
| `UART1_WAKEUP_ENABLE` | `PROJECT_CFG_UART1_WAKEUP_ENABLE` | UART1 唤醒 |
| `RS485_WAKEUP_ENABLE` | `PROJECT_CFG_RS485_WAKEUP_ENABLE` | RS485 唤醒 |
| `__VIRTURE_CURRENT__` | `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE` | 虚拟电流 |
| `_DI_SWITCH_longKEY_ONOFF` | `PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE` | 长按键开关机 |
| `_COMMOM_UPPER_SCI1` | `PROJECT_CFG_SCI1_ROLE == 1` | SCI1 上位机通信 |
| `VERSION` | `PROJECT_CFG_VERSION` | 固件版本 |
| `LEVEL_CURR` | `PROJECT_CFG_LEVEL_CURR` | 电流档位 |
| `AFE_TYPE` | `PROJECT_CFG_AFE_TYPE` | AFE 类型 |
| `EEPROM_VALUE_BEGIN_FLAG` | `PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG` | EEPROM 标志 |

**`conf.h` 还定义了**：

- `Time_T sys_time` — 全局运行时状态结构体（CAN 计数器、SCI 中断计数、唤醒原因、充放电状态、RTC 计数等）
- GPIO 模式枚举 `GPIO_Type`（预充/充电/放电/主回路）
- 大量 IO 操作函数声明（`InitIO`, `InitWakeUp_*`, `IOstatus_*`, `IORecover_*`）

**包含链**：
```
conf.h
├── stdio.h, stdint.h, stdbool.h
├── stm32f10x.h
├── conf_gpio.h
│   └── System_Init.h  (位带映射, GPIO 宏)
├── Project_Config.h   (产品配置宏)
└── Project_BuildGuard.h (编译时范围检查)
```

---

### 3. 编译配置 (Project_BuildGuard.h)

**文件路径**: `103 + 309/Project/Source/conf/Project_BuildGuard.h`

`Project_BuildGuard.h` 是**编译时安全网**，通过 `#if ... #error` 在编译阶段捕获非法配置：

#### 3.1 默认值保护

为可能未定义的宏提供安全默认值：
- `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` → 30
- `PROJECT_CFG_HOST_WRITE_ENABLE` → 1
- `PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` → 3600
- 所有 debug 开关 → 0

#### 3.2 必需功能检查

以下功能被标记为"不得禁用"：
- `PROJECT_CFG_IAP_ENABLE` — IAP（In-Application Programming）
- `PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE` — 升级参数策略
- `PROJECT_CFG_FACTORY_AGING_ENABLE` — 工厂老化

#### 3.3 范围校验

对所有关键参数进行范围检查：
- `PROJECT_CFG_BUILD_PROFILE` — 必须为 0/1/2
- `PROJECT_CFG_BAT_CHEMISTRY` — 必须为 0 或 1
- `PROJECT_CFG_SOC_*` — 各项 SOC 参数范围检查
- `PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS` — 1 到 604800（7天）
- `PROJECT_CFG_UPGRADE_OTHER_*` — 所有 OtherElement 值范围检查

#### 3.4 Release 构建保护

```c
#if (PROJECT_CFG_BUILD_PROFILE == 0) && \
    ((PROJECT_CFG_DEBUG_WATCH_ENABLE != 0) || \
     (PROJECT_CFG_DEBUG_MONITOR_ENABLE != 0) || ...)
#error "Debug watch, system debug and IRQ debug must stay disabled for release build profile"
#endif
```

**Build Profile 0（Release）时，所有调试功能必须关闭**，防止调试代码进入生产固件。

#### 3.5 逻辑依赖检查

```c
#if (PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE != 0) && (PROJECT_CFG_IRQ_DEBUG_ENABLE == 0)
#error "IRQ debug event requires IRQ debug"
#endif
```

确保功能依赖关系在编译时得到验证。

---

## 总结

### 系统架构特征

1. **极简主循环**：无 RTOS，`main.c` 仅 12 行，"初始化 + 轮询"经典架构。
2. **时间驱动调度**：TIM3 产生 10ms 心跳，通过双缓冲位域机制分频到 50/100/200/1000ms 周期。
3. **配置与代码分离**：`Project_Config.h` 集中管理所有可调参数，`conf.h` 适配转换，`Project_BuildGuard.h` 编译时校验。
4. **防御性编程**：所有硬件操作带超时保护，看门狗贯穿始终，ADC 校准失败有回调上报。
5. **低功耗设计**：RTC 闹钟唤醒 + STOP 模式，多级唤醒源（UART1/RS485/RTC），唤醒前保存状态。
6. **工厂友好**：老化测试模块支持掉电恢复、上位机控制、自定义时长，双重持久化存储。
