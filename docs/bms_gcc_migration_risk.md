# BMS GCC 迁移功能风险清单

记录日期：2026-05-31

本清单只记录迁移风险，不修改 BMS 保护逻辑、SOC 逻辑、通信协议、低功耗逻辑。

| 风险项 | 涉及文件 | 风险等级 | 迁移处理 |
|---|---|---|---|
| 软件 I2C/AFE 时序 | `I2C_AFE1.c`、`SH367309_Func.c`、`DataDeal.c` | 高 | GCC 构建成功后用硬件读写 AFE 验证；必要时单文件降优化 |
| UART/上位机/Modbus 中断 | `Sci_Upper.c`、`stm32f10x_it.c` | 高 | ISR 名称必须与 startup 一致；printf retarget 需验证 |
| CAN 中断和调度 | `Can_HDX.c`、`CanFeidaoFrames.c` | 高 | `USB_LP_CAN1_RX0_IRQHandler` 必须进入向量表；验证 1000ms/5000ms 帧 |
| Flash 参数结构体/CRC | `Flash.c`、`EEPROM.c`、`Flash.h` | 高 | linker 预留 `0x0801C000+`；验证 CRC 和双槽选择 |
| SOC 定时积分 tick | `SOC.c`、`SocEnhance.c`、`Runtime.c` | 高 | 验证 10ms/200ms tick、RTC 补偿、休眠恢复 |
| 低功耗唤醒和 SystemInit | `rtc_sleep.c`、`LowPowerSleep.c`、`system_stm32f10x.c` | 高 | 验证 STOP/RTC 唤醒后时钟和 VTOR |
| 看门狗喂狗 | `System_Init.h`、各长循环模块 | 中 | 编译优化变化后验证长延时、Flash 写、串口发送中喂狗 |
| LED 查理复用扫描 | `LedBar.c` | 中 | Keil 单文件 O0，GCC 保留 `-O0` |
| C 库 printf 行为 | `Sci_Upper.c`、easylogger | 中 | `nano.specs/nosys.specs` 下验证 `_write/fputc` |
| 结构体对齐/packed | 通信帧和存储结构体 | 中 | 当前未发现 `__packed`，仍需用协议实测验证字节布局 |

## 必须验证的板端点

- 上电后读 `0xD000` 和 `0xD300`。
- 量产固件 `0xD300 supported=0` 应保持正常。
- CAN 周期帧和低功耗 RTC 周期帧。
- AFE 采样、电流零点、SOC 保存/恢复。
- 参数区、日志区、IAP 跳转标志不被烧录擦除。
