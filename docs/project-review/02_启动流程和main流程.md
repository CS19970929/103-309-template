# 02 — 启动流程和 main 流程

## 总体架构

项目采用**裸机大循环**架构，无 RTOS：

```c
// main.c (12 行)
int main(void) {
    Runtime_Boot();    // 一次性初始化序列
    while (1) {
        Runtime_RunOnce();  // 每轮主循环执行固定顺序任务
    }
}
```

## 启动序列 (`Runtime_Boot()`)

**源文件**: `Runtime.c:11-52`

启动序列按**严格顺序**执行，不可重排：

| 步骤 | 函数调用 | 作用 | 源文件 |
|------|---------|------|--------|
| 0 | `DebugWatch_BindAll()` | 绑定调试观察点到各模块全局变量 | `DebugWatch.c:33` |
| 0 | `IrqDebug_SetPhase(BOOT)` | 标记当前为启动阶段 | `IrqDebug.c` |
| 1 | `SystemInit()` | STM32 系统时钟初始化（72MHz, PLL） | `system_stm32f10x.c` |
| 2 | `InitDelay()` | 初始化 SysTick 延时 | `System_Init.c` |
| 3 | `SleepDeal_HandleBootSleepStartup()` | **关键**: 检测启动原因（正常上电 / 休眠唤醒 / 充电器唤醒），进入对应状态 | `SleepDeal.c:217` |
| 4 | `jtag_disableAndConfIO()` | 禁用 JTAG，释放 PA15/PB3/PB4 为 GPIO | `conf.c` |
| 5 | `InitNVIC()` | NVIC 优先级分组配置 (2:2) | `System_Init.c` |
| 6 | `InitIO()` | 全部 GPIO 初始化为默认状态 | `conf.c` |
| 7 | `InitUSART_CommonUpper()` | RS485 SCI1 (USART1) 初始化 | `Sci_Upper.c` |
| 8 | `InitE2PROM()` | **核心**: 从 Flash 加载所有参数（保护/AFE/SOC/日志/老化），执行升级策略 | `EEPROM.c:318` |
| 9 | `InitAFE1()` | SH367309 AFE 初始化：I2C → 复位 → 等待就绪 → 写配置 → MOS 启动 → 电流归零 | `I2C_AFE1.c:678` |
| 10 | `InitCan()` | CAN 控制器初始化为 250 kbit/s | `Can_HDX.c:830` |
| 11 | `InitADC()` | ADC1 + DMA1 + TIM2 触发采样初始化 | `ADC.c` |
| 12 | `InitData_SOC()` | SOC 状态机初始化 | `SOC.c:59` |
| 13 | `InitTimer()` | TIM3 10ms 系统时基初始化 | `System_Init.c` |
| 14 | `__enable_irq()` | **关键**: 全局中断使能（此后 ISR 开始运行） | CMSIS |
| 15 | `IrqDebug_SetPhase(RUN)` | 启动完成，切换到运行阶段 | `IrqDebug.c` |
| 16 | `InitSystemMonitorData_EEPROM()` | 加载系统错误/状态数据 | `System_Monitor.c` |
| 17 | 计算 `g_u32CS_Res_AFE` | 电流采样电阻计算: `(Num * 1000) / Res` | `Runtime.c:39` |
| 18 | `SystemRuntime_MarkBootReady()` | 标记系统启动完成 | `System_Monitor.c` |
| 19 | `SystemRuntime_SetProjectVersion(1)` | 设置项目版本号 | `System_Monitor.c` |
| 20 | `LedBar_Init()` | LED 数码管初始化 (TIM4) | `LedBar.c` |
| 21 | `InitProID()` | 初始化产品序列号 | `ProductionID.c` |
| 22 | `LogRecord_RequestStartup()` | 记录启动事件到 Flash 日志 | `LogRecord.c` |
| 23 | `Init_RTC()` | RTC 初始化（LSE 优先，失败回退 LSI） | `RTC.c:426` |
| 24 | `DBG_Init()` | 调试系统初始化 | `debug_hub.c` |
| 25 | `EnableLowPowerDebug()` | 低功耗调试配置（DBGMCU 停止定时器） | `System_Init.c` |
| 26 | `Init_IWDG()` | 独立看门狗初始化 | `System_Init.c` |

### 启动时序注意事项

1. **中断使能时机**: 在第 14 步才使能，之前的所有初始化都在关中断环境下完成
2. **EEPROM 加载**: 在第 8 步加载参数，因此 `SystemInit()` 到 `InitE2PROM()` 之间不能依赖任何 EEPROM 参数
3. **AFE 初始化**: 包括硬件复位（~1s 等待）、配置写入和电流归零校准，是启动中最耗时的步骤
4. **休眠唤醒路径**: 第 3 步的 `SleepDeal_HandleBootSleepStartup()` 是特殊路径——如果检测到是从休眠唤醒，会进入 STOP 模式等待有效唤醒源（充电器或长按键）

## 主循环 (`Runtime_RunOnce()`)

**源文件**: `Runtime.c:54-109`

主循环无阻塞等待，每轮快速执行所有任务。任务间有 `DebugHooks` 性能标记点（Release 构建时编译为空）。

| # | 函数 | 频率 | 功能 | 源文件 |
|---|------|------|------|--------|
| 1 | `SysTime_LatchTaskFlags()` | 每轮 | 将 ISR 中设置的时间标志锁存到全局变量 | `System_Init.c` |
| 2 | `FactoryAging_Task()` | 每轮 | 老化测试状态机（非老化时快速返回） | `FactoryAging.c:579` |
| 3 | `APP_LedBar()` | 每轮 | LED 数码管刷新 | `LedBar.c` |
| 4 | `App_AFEGet()` | 每轮 | 通过 I2C 读取 AFE 电池数据（电压/温度/电流） | `DataDeal.c:1192` |
| 5 | `App_CommonUpper()` | 每轮 | RS485 上位机帧处理 | `Sci_Upper.c` |
| 6 | `App_AnlogCal()` | 每轮 | ADC 数据处理（总压/电流/温度计算） | `ADC.c` |
| 7 | `rtc_sleep()` | 每轮 | 低功耗休眠状态机（判断/进入/唤醒处理） | `rtc_sleep.c` |
| 8 | `App_Can()` | 每轮 | CAN 总线：接收处理 + 发送周期帧 | `Can_HDX.c:925` |
| 9 | `App_FlashUpdate()` | 每轮 | 检查 Flash 更新标志，必要时复位 | `Flash.c` |
| 10 | `App_LogRecord()` | 每轮 | 事件日志写入（延迟批量写入） | `LogRecord.c` |
| 11 | `App_ProID_Deal()` | 每轮 | 产品 ID 处理 | `ProductionID.c` |
| 12 | `Feed_IWatchDog` | 每轮 | 喂狗（宏展开为 `IWDG_Feed()`） | `System_Init.c` |
| 13 | `DBG_Task()` | 每轮 | 调试系统轮询（Release 构建时为空） | `debug_hub.c` |

### 调度特点

1. **无优先级**: 所有任务按固定顺序执行，无抢占
2. **无延迟**: 任务内部不应有阻塞等待（HICCUP 休眠是个反例，见风险点说明）
3. **时间触发**: 各任务内部通过 `SysTime_LatchTaskFlags()` 锁存的 `g_st_SysTimeFlag` 自行判断是否执行
4. **任务顺序依赖**: 例如 `App_AFEGet()` 必须在 `App_CommonUpper()` 之前执行，因为 RS485 响应需要最新的 AFE 数据

## 时间基准系统

**源文件**: `System_Init.c` (TIM3_IRQHandler), `System_Init.h` (SYS_TIME)

```
TIM3 → 10ms 中断 → SysTime_Post10msTick()
  ├── 累加 s_u32Sys10msTickCount
  ├── 每 10ms:   设置 g_st_SysTimeFlag.bits.b1Sys10msFlag
  ├── 每 50ms:   设置 b1Sys50msFlag
  ├── 每 100ms:  设置 b1Sys100msFlag
  ├── 每 200ms:  设置 b1Sys200msFlag
  └── 每 1000ms: 设置 b1Sys1000msFlag
```

主循环中 `SysTime_LatchTaskFlags()` 读取并保存这些标志。每个任务模块通过检查对应标志决定是否在本轮执行实际工作。
