# 103-309 BMS 项目总览

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`main.c`, `AppInit.c`, `Runtime.c`, `Project_Config.h`, `DataDeal.c`, `SOC.c`, `Sci_Upper.c`, `Can_HDX.c`, `Flash.c`, `rtc_sleep.c`, `LedBar.c`
最后更新时间：2026-05-26
未确认事项：真实电流路径、均衡、老化、Host 写权限、Flash 容量、低功耗 CAN 广播。

## 1. 项目背景

当前项目是一个 STM32F1 标准外设库 BMS 保护板 App 工程，核心目标是支持 103 + 309 硬件组合，并逐步整理成可维护、可复用、可向 STM32F0/F1 和不同 AFE/客户协议扩展的模板。

当前主工程是：

`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`

## 2. 硬件平台

| 项目 | 当前源码体现 | 备注 |
|---|---|---|
| MCU | Keil 工程显示 STM32F103C8 / Cortex-M3 / SPL | 实际 Flash 容量必须硬件确认 |
| AFE | SH367309 | 当前未形成通用 AFE 抽象 |
| 通信 | USART1 Modbus RTU, CAN1 飞道/服务协议 | SCI2/SCI3 当前关闭 |
| ADC | ADC1 + DMA1_Channel1 + TIM2_CC2 | PA1 VBC、PA2 Type-C 电流、PB1 MOS 温度 |
| 低功耗 | RTC + STOP + BKP + IWDG | RTC 周期受 IWDG 10s 限制 |
| 显示 | 5 GPIO Charlieplexing LedBar | TIM4 扫描 |
| 存储 | 内部 Flash 后 64K 区域 | 地址依赖实际 Flash >= 128KB |

## 3. 软件目标

1. 保持上位机协议兼容。
2. 保持 BMS 安全逻辑和硬件行为稳定。
3. 将旧 EEPROM 依赖收敛为内部 Flash。
4. 支持 SOC 快照、低功耗休眠补偿和显示平滑。
5. 保留 IAP/Bootloader 隔离，禁止 App 覆盖 IAP。
6. 后续逐步抽象 AFE 和客户协议。

## 4. 当前主要功能

| 功能 | 当前状态 |
|---|---|
| 启动/主循环 | `AppInit_Boot()` 初始化，`Runtime_RunOnce()` 裸机轮询 |
| 采样 | AFE 200ms 链路 + ADC 10ms 触发链路 |
| 保护 | AFE status 映射 fault，保护阈值可写入 AFE |
| SOC | 库仑积分、满电锚点、低压尾段、静置 OCV、显示平滑、snapshot |
| 通信 | Modbus RTU `0x03/0x06/0x10`，CAN 周期广播和 App 服务 |
| 存储 | SOC/Afe/RW/log/aging 使用内部 Flash |
| 低功耗 | HICCUP STOP、NORMAL/DEEP reset sleep、RTC 补偿 |
| IWDG | 默认开启，运行态和阻塞等待喂狗 |
| IAP | App 地址 `0x08004800`，IAP 地址 `0x08000000` |
| 老化 | 默认开启，3 天运行态计时，可 CAN 控制 |

## 5. 当前状态与关键风险

最关键风险：

1. 旧文档曾记录量产 profile 下 `App_AFEGet()` 调用 `test_Autocurrent_cycle()`；当前源码已恢复调用真实 `DataLoad_Current()`，剩余风险是虚拟电流测试入口是否删除或隔离。
2. 均衡参数存在，但主动均衡任务入口未确认。
3. `SH367309_DataDeal.c` 中均衡开压硬编码 `4160`。
4. Host 写权限在量产开启。
5. Flash 后 64K 地址和 App/IAP 链接地址必须用真实硬件和 map 验证。

后续不应直接重构代码，应先按 `docs/review/requirement_questions.md` 确认需求。
