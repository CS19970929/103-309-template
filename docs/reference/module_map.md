# 103-309 BMS 模块地图

文档状态: CURRENT
源码验证: VERIFIED - 完整源码分析
最后更新: 2026-06-01

## 1. 模块到文件映射

| 模块 | 主要文件 | 代码行数(估) | 职责 |
|------|----------|-------------|------|
| 启动与调度 | main.c, AppInit.c/h, Runtime.c/h | ~120 | main→AppInit_Boot→Runtime_RunOnce |
| 配置与IO | conf/Project_Config.h, conf.h, conf_gpio.h, conf.c | ~800+ | 100+编译宏, GPIO, 电源轨, STOP IO, 唤醒 |
| 时基系统 | System_Init.c/h | ~200 | TIM3 10ms, SysTick, IWDG, NVIC |
| ADC | ADC.c/h | ~500 | TIM2触发, DMA, VBUS/TypeC电流/MOS温度 |
| AFE | I2C_AFE1.c/h, SH367309_Func.c/h, SH367309_DataDeal.c/h | ~2000+ | GPIO模拟I2C, 寄存器解析, MOS控制, 均衡 |
| 数据处理 | DataDeal.c/h | ~300 | AFE数据加载, 零电流校准, 校准系数 |
| SOC | SOC.c/h, SocEnhance.c/h | ~2000+ | 安时积分, OCV校准, 满电锚定, 尾端, 静置补偿, 显示平滑, 快照 |
| Modbus | Sci_Upper.c/h | ~2000+ | 0x03/0x06/0x10, 地址映射, CRC16 |
| CAN | Can_HDX.c/h, CanFeidaoFrames.c/h | ~1100+ | TX队列32, 运行态周期广播, 睡前CMNT关闭, 应用命令处理 |
| 存储 | Flash.c/h, EEPROM.c/h (废弃) | ~1000+ | A/B双槽, SOC/AFE/参数/日志/老化 |
| 升级策略 | UpgradeParamPolicy.h | ~200 | 升级时参数重置策略 |
| 低功耗核心 | rtc_sleep.c/h | ~480 | 三层架构: core→port→afe |
| 低功耗调度 | app_lowpower.c/h | ~160 | 阻塞检查, STOP进出 |
| 低功耗底层 | rtc_sleep_port.c/h, rtc_sleep_afe_sh367309.c/h | ~500+ | 硬件抽象层实现 |
| 休眠处理 | SleepDeal.c/h, LowPowerSleep.c/h | ~200 | 深度休眠, 唤醒检查 |
| RTC驱动 | RTC.c/h, bsp_rtc.c/h | ~300 | RTC时钟/闹钟, 备份域 |
| 时钟/电源 | bsp_clock.c/h, bsp_power.c/h | ~100 | HSE/HSE/LSE配置, STOP进出 |
| LED显示 | LedBar.c/h | ~1400 | Charlieplexing 5pin→18段, 扫描帧贪心优化, 按键滤波, 休眠SOC |
| 工厂老化 | FactoryAging.c/h | ~600 | 状态机, BKP+Flash双存, CAN控制 |
| 故障管理 | Fault.c/h, FaultSnapshot.h | ~400 | 三级保护参数(65字), 故障标志, 故障记录 |
| 日志 | LogRecord.c/h | ~200 | 20种事件, 重复抑制(3600s) |
| 系统监控 | System_Monitor.c/h | ~175 | 23个错误标志, 功能开关, MOS状态 |
| MOS控制 | MosStartup.c/h | ~100 | 工厂模式, 5V充电检测 |
| 产品信息 | ProductionID.c/h | ~50 | 序列号, 硬/软件版本 |
| 公共函数 | PubFunc.c/h | ~300 | CRC16, 查表, 排序, 校验 |
| 调试日志 | easylogger/ | ~500 | 日志框架 |

## 2. 关键全局数据流

```
SH367309 AFE (I2C, 硬件保护)
  │
  ├─ I2C_AFE1/MTPRead → SH367309_Read_AFE1
  ├─ SH367309_DataDeal → Register解析 → 电压/电流/温度
  ├─ DataDeal/App_AFEGet → 零电流校准 → g_stCellInfoReport
  │
  ├─ SOC.c/SocEnhance.c
  │   ├─ SOC_UpdateSampleData → 安时积分
  │   ├─ SOC_IntEnhance_Ctrl → OCV校准/满电/尾端/静置
  │   └─ SOC_PublishReportData → g_stCellInfoReport.SocElement
  │
  ├─ Sci_Upper.c → Modbus RTU 0x03/0x06/0x10 应答
  ├─ Can_HDX.c/CanFeidaoFrames.c → CAN 周期广播(1s/5s)
  └─ LedBar.c → 数码管 SOC 显示
```

## 3. 模块间依赖

```
              ┌──────────────┐
              │  g_stCellInfo│  ← 所有数据汇聚点
              │  Report      │
              └──────┬───────┘
        ┌────────────┼────────────┐
        ▼            ▼            ▼
    Sci_Upper    Can_HDX      LedBar
    (串口)       (CAN)        (显示)
        │            │
        ▼            ▼
    Flash.c      CanFeidaoFrames
    (存储)       (周期帧)
        │
   ┌────┴────┐
   ▼         ▼
 SOC数据   参数/日志
```

## 4. 中断使用一览

| 中断 | 用途 | 优先级(Pre,Sub) |
|------|------|-----------------|
| TIM3 | 10ms 时基 | 0,0 |
| TIM2 | ADC 触发 | - |
| TIM4 | LED 扫描 | 1,3 |
| EXTI0 | PA0 充电检测 | 1,1 |
| EXTI9_5 | PA9按键, PB7 UART1 RX | 1,1 |
| EXTI15_10 | PB12通讯唤醒, PB13 MCU_WK | 1,1 |
| USB_LP_CAN1_RX0 | CAN 接收 | 1,1 |
| USART1 | 串口1 | 1,1 |
| RTC | 秒中断/闹钟 | 1,1 |
| IWDG | 独立看门狗 | 硬件 |

## 5. Flash 地址分配

| 地址 | 内容 |
|------|------|
| 0x08000000-0x080047FF | IAP Bootloader (18KB) |
| 0x08004800-0x0801BFFF | APP (~94KB) |
| 0x0801C000-0x0801C3FF | AFE参数 A |
| 0x0801C400-0x0801C7FF | RW参数 A |
| 0x0801C800-0x0801CBFF | AFE参数 B |
| 0x0801CC00-0x0801CFFF | RW参数 B |
| 0x0801D000-0x0801D7FF | 日志 A |
| 0x0801D800-0x0801DFFF | 日志 B |
| 0x0801E000-0x0801E7FF | SOC数据 A |
| 0x0801E800-0x0801EFFF | SOC数据 B |
| 0x0801F000-0x0801F3FF | 升级策略 |
| 0x0801F400-0x0801F7FF | 老化数据 |
| 0x0801F800-0x0801FBFF | IAP跳转标志 |
| 0x0801FC00-0x0801FFFF | 休眠标志 |
