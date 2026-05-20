# AFE 文档索引

本目录整理 `103 + 309` 主项目中 SH3673520/SH36735XX AFE 相关资料、源码路径、SPI 协议、寄存器配置和整改计划。文档面向后续代码清理、Keil 调试、ST-Link 验证和硬件问题排查。

## 当前结论

当前主工程的 AFE 通信链路已经不是最初那种“只发 SPI、不完整校验”的状态。底层 `sh36735_spi_proto.c` 已按官方例程补齐写回显、读回显、ACK、CRC8 和重试。软件 SPI 的时序也满足 SH36735XX 文档对 Mode 3、1 MHz 上限和 SCK 高低电平时间的要求。

2026-05-18 本轮已处理的 AFE 配置闭环和架构边界问题：

- `AFEDATA` 结构体里 `BALANCEM/BALANCEL` 顺序已修正。
- `SNum`、`SeriesNum`、EEPROM 串数和 AFE `SCONF4.CN` 已通过启动顺序和规范化函数统一。
- AFE 初始化写寄存器后会逐项读回校验，不再只读回 `SCONF4`。
- SPI 读写 API 已增加寄存器地址和长度边界检查。
- `FLAG2` 的 `VADC/CADC` 自清标志已由采样模块锁存到 `AFE1_LastFlag2ConversionFlags`。

仍需继续整理的是旧 SH367309/I2C 命名与新 SH3673520/SPI 路径混用的问题。

## 文档结构

| 文档 | 内容 |
| --- | --- |
| [sh36735xx-spi-protocol.md](sh36735xx-spi-protocol.md) | SPI Mode、帧格式、CRC、官方例程和主工程实现对照 |
| [sh36735xx-register-config.md](sh36735xx-register-config.md) | SH36735XX 关键寄存器、当前主工程初始化值、配置风险 |
| [main-project-afe-integration.md](main-project-afe-integration.md) | 主工程 AFE 入口、采样链路、保护清除、均衡、休眠关联 |
| [afe-risk-roadmap.md](afe-risk-roadmap.md) | 风险分级、建议修改顺序、验证清单 |
| [afe-risk-fixes-20260518.md](afe-risk-fixes-20260518.md) | 五项 AFE 风险的具体解释和本轮修复记录 |
| [sh36735xx-ai-reference-20260520.md](sh36735xx-ai-reference-20260520.md) | 基于 `SH36735XX CV0.2C.pdf` 的 AI 开发参考，覆盖工作模式、SPI 协议、完整寄存器表和换算规则 |
| [current-project-afe-code-audit-20260520.md](current-project-afe-code-audit-20260520.md) | 当前主工程 AFE 代码拓扑、已确认正确点、Bug 和优化清单 |
| [source-cross-check-20260520.md](source-cross-check-20260520.md) | PDF、提取文档、官方 Demo 和主工程四源交叉对比，明确一致项、偏差和后续开发准则 |
| [afe-p1-p2-fixes-20260520.md](afe-p1-p2-fixes-20260520.md) | 本轮 P1/P2 AFE 代码整改记录，包含串数、采样电阻、电流/温度/总压换算、寄存器地址和参数工具修复 |

已有上层文档：

- [../sh36735xx-afe-datasheet-code-review.md](../sh36735xx-afe-datasheet-code-review.md)：结合 PDF、官方例程和主工程的完整 code review。
- [../afe-spi-communication-review.md](../afe-spi-communication-review.md)：上一轮 SPI 专项梳理。
- [../main-project-logic.md](../main-project-logic.md)：主项目整体逻辑梳理。

## 代码入口速查

主工程：

- SPI 协议层：`103 + 309/Project/Source/sh3520 driver/sh36735_spi_proto.c`
- 软件 SPI：`103 + 309/Project/Source/sh3520 driver/sh36735_spi_sw.c`
- 硬件 SPI：`103 + 309/Project/Source/sh3520 driver/sh36735_spi_hw.c`，当前 Keil 工程排除编译
- SPI GPIO：`103 + 309/Project/Source/bsp_spi_bus.c`
- AFE 寄存器宏：`103 + 309/Project/Source/SH36735_reg.h`
- AFE 初始化：`103 + 309/Project/Source/main.c`
- AFE 采样：`103 + 309/Project/Source/I2C_AFE1.c`
- AFE 数据结构：`103 + 309/Project/Source/I2C_AFE1.h`
- AFE 保护清除/MOS 控制：`103 + 309/Project/Source/SH367309_Func.c`
- AFE 均衡控制：`103 + 309/Project/Source/Cell_balance.c`

官方例程：

- SPI 协议：`SH3673520+STM32F072CBT6 DemoCode V1.2_20241227/BMS_Drivers/Src/SPIApp.c`
- AFE 初始化/寄存器检查：`SH3673520+STM32F072CBT6 DemoCode V1.2_20241227/BMS_Drivers/Src/AFE.c`
- 官方 SPI 外设配置：`SH3673520+STM32F072CBT6 DemoCode V1.2_20241227/Core/Src/spi.c`
- 官方 AFE 寄存器结构：`SH3673520+STM32F072CBT6 DemoCode V1.2_20241227/BMS_Drivers/Inc/Common.h`
