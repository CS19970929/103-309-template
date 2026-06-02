# 103-309 BMS 项目总览

文档状态: CURRENT (2026-06-01 完整更新)
源码验证: VERIFIED - 基于完整源码分析
主要参考源码: 全部 Source/ 目录下的 .c/.h 文件
最后更新: 2026-06-01

## 1. 项目定位

STM32F103C8 + SH367309 BMS 保护板固件。面向三元锂/磷酸铁锂 10串电池包。
- 硬件保护: 由 AFE (SH367309) 完成
- 软件功能: SOC 估算、通讯、显示、低功耗管理、老化测试
- 通讯: 串口 Modbus RTU (上位机) + CAN 飞道协议 (Comm Tool/BMS 互联)
- 固件升级: IAP Bootloader + CAN 远程升级 + 串口升级

## 2. 硬件平台

| 项目 | 配置 |
|------|------|
| MCU | STM32F103C8T6, 64KB Flash, 20KB RAM |
| AFE | SH367309 (I2C 地址 0x34, GPIO 模拟 I2C: PB8/PB9) |
| 串口 | USART1 (PB6/PB7) - 上位机通讯 |
| CAN | CAN1 (PA11/PA12), 250kbps, GPIO_CMNT_EN(PB4) 控制收发器供电 |
| ADC | ADC1+DMA1_CH1+TIM2_CC2 触发: PA1(VBUS), PA2(TypeC电流), PB1(MOS温度) |
| RTC | LSE 32.768kHz, STOP 模式唤醒 |
| 看门狗 | IWDG, 约10秒超时 |
| 显示 | 5-GPIO Charlieplexing 数码管 (PA4/PA5/PA6/PB10/PB11), TIM4 扫描 |
| 按键 | PA9 (SW), 长按2秒关机 |
| 充电检测 | PA0 (CHG_IN), 中断唤醒 |
| 唤醒源 | PB13(MCU_WK), PB12(通讯唤醒), PB7(串口RX), PA0(充电) |

## 3. 软件架构 (三层)

```
main() → AppInit_Boot() → while(1) Runtime_RunOnce()

Runtime_RunOnce() 分为四个阶段:
  1. Front Tasks:  时基锁存 → 老化 → LED → AFE数据获取
  2. IO/Power Tasks: 串口服务 → ADC计算 → 低功耗调度 → CAN服务
  3. Background:     Flash测试 → 参数更新 → 日志 → 产品ID → 喂狗
  4. Idle:           WFI (可配置)

低功耗:
  空闲 N 秒 → HICCUP (RTC STOP 周期唤醒) → SOC补偿/状态刷新
  过放 → DEEP (MCU复位) 或 NORMAL (MCU复位)
```

## 4. 核心模块一览

| 模块 | 文件 | 行数(估) | 功能 |
|------|------|----------|------|
| 配置系统 | conf/Project_Config.h, conf.h | 450+ | 100+编译宏, Keil可视化 |
| 主循环 | main.c, AppInit.c, Runtime.c | ~120 | 启动+调度 |
| SOC 算法 | SOC.c, SocEnhance.c | ~2000+ | 安时积分, OCV校准, 静置补偿, 显示平滑 |
| CAN 通讯 | Can_HDX.c, CanFeidaoFrames.c | ~1500+ | TX队列, 运行态周期广播, 睡前CMNT关闭, 应用命令 |
| 串口 Modbus | Sci_Upper.c | ~2000+ | 0x03/0x06/0x10, 地址映射, CRC16 |
| ADC 采样 | ADC.c | ~500 | TIM2触发, DMA, 总压/电流/温度 |
| AFE 驱动 | I2C_AFE1.c, SH367309_*.c | ~1000+ | GPIO模拟I2C, 寄存器解析 |
| LED 显示 | LedBar.c | ~1400 | Charlieplexing, 扫描帧优化, 按键滤波 |
| RTC 低功耗 | rtc_sleep.c, rtc_sleep_port.c | ~500+ | 三层架构, HICCUP循环, 唤醒源管理 |
| 工厂老化 | FactoryAging.c | ~600 | 定时老化, BKP+Flash双存, CAN控制 |
| Flash 存储 | Flash.c | ~1000+ | A/B双槽, SOC/参数/日志/老化 |
| 日志 | LogRecord.c | ~200 | 20种事件, 重复抑制 |
| 系统监控 | System_Monitor.c | ~175 | 错误标志, 功能开关, MOS状态 |
| 故障管理 | Fault.c/h | ~400 | 三级保护参数, 故障标志 |

## 5. 关键数据流

```
SH367309 AFE (I2C)
  └─ DataDeal.c/App_AFEGet() → g_stCellInfoReport (电压/电流/温度)
      ├─ SOC.c/SocEnhance.c → SOC 计算 → g_stCellInfoReport.SocElement
      ├─ Sci_Upper.c → 串口 Modbus 应答
      ├─ CanFeidaoFrames.c → CAN 周期广播
      └─ LedBar.c → 数码管显示
```

## 6. 当前状态

- 核心功能: SOC、保护映射、通讯、低功耗 均已实现
- 已知待优化: SOC校准策略、CAN功耗、LED闪烁、代码简化
- IAP: 支持串口和CAN两种升级路径
- Comm Tool: 独立工程 (firmware/comm_tool_f103ret6/), 实现CAN中继升级
- PC 上位机: Python (tools/), 支持串口通讯/升级/监控

## 7. 文档索引

| 文档 | 路径 | 说明 |
|------|------|------|
| 模块完整参考 | docs/module_reference.md | 所有模块详细功能/逻辑/变量/函数 |
| 宏配置参考 | docs/macro_config_reference.md | 所有宏及派生关系 |
| 模块地图 | docs/module_map.md | 模块→文件映射 |
| 通讯地址索引 | COMMUNICATION_ADDRESS_INDEX.md | Modbus 寄存器映射 |
| CAN 协议 | docs/protocol/can_protocol.md | 飞道 CAN 协议 |
| 串口协议 | docs/protocol/uart_protocol.md | Modbus 协议 |
| 架构 | docs/architecture.md | 系统架构 |
| 低功耗设计 | docs/design/low_power_design.md | 低功耗方案 |
| SOC 设计 | docs/design/soc_design.md | SOC 算法 |
| LED 设计 | docs/design/led_display_design.md | 数码管方案 |
| IAP 设计 | docs/design/bootloader_iap_design.md | 升级方案 |
| 变更日志 | docs/change_log.md | 变更记录 |
