# 项目文档入口

本文是当前工程的文档导航入口，用于快速定位架构、模块、低功耗、通信、存储、测试等说明文档。后续新增长期有效的技术文档时，优先补充到本文对应分类。

## 推荐阅读路径

### 快速熟悉整机架构

1. [项目级代码与文档审查记录](PROJECT_REVIEW_2026-05-09.md)
2. [MCU 资源分布与架构优化评估](MCU资源分布与架构优化评估.md)
3. [系统时钟系统梳理](系统时钟系统梳理.md)
4. [System_Monitor 模块梳理](System_Monitor模块梳理.md)
5. [项目宏定义梳理](项目宏定义梳理.md)
6. [休眠低功耗逻辑梳理与优化建议](休眠低功耗逻辑梳理与优化建议.md)
7. [LED 软件框架与时序梳理](LED软件框架与时序梳理.md)

### 修改 LED / 数码管显示

1. [LED 软件框架与时序梳理](LED软件框架与时序梳理.md)
2. [LedBar GPIO Charlieplexing 显示方案](LEDBAR_GPIO_CHARLIE_DISPLAY_PLAN.md)
3. [Charlie + 74HC595 显示框架审查与移植说明](CHARLIE_595_DISPLAY_PORT_REVIEW.md)
4. [LedBar SOC 显示策略状况分析](LEDBAR_SOC_DISPLAY_STRATEGY_ANALYSIS.md)
5. [LedBar SOC 1~100 显示完美度逐项分析](LEDBAR_SOC_1_100_DISPLAY_ANALYSIS.md)

### 修改低功耗 / RTC / CAN 唤醒

1. [休眠低功耗逻辑梳理与优化建议](休眠低功耗逻辑梳理与优化建议.md)
2. [RTC CAN 自适应休眠说明](RTC_CAN自适应休眠说明.md)
3. [CAN 低功耗发送调度说明](CAN低功耗发送调度说明.md)
4. [CAN 通信逻辑与低功耗策略分析](CAN通信逻辑与低功耗策略分析.md)
5. [CAN 通信回退到 9ec9f26 说明](CAN_RTC低功耗广播修复说明.md)

### 修改通信地址 / EEPROM / 参数升级

1. [通信逻辑与地址整理文档](COMMUNICATION_LAYOUT_REPORT.md)
2. [通信完整地址索引](COMMUNICATION_ADDRESS_INDEX.md)
3. [0x10 子地址完整清单](COMMUNICATION_WRITE_DETAIL.md)
4. [EEPROM 地址与读写逻辑梳理](EEPROM_LAYOUT_OPTIMIZATION.md)
5. [存储布局说明](STORAGE_LAYOUT_REPORT.md)
6. [升级参数策略说明](升级参数策略说明.md)

### 修改 SOC / ADC / AFE 监控

1. [主流 BMS SOC 策略对比与本工程取舍](BMS_SOC_STRATEGY_COMPARISON.md)
2. [SOC 模块完整逻辑说明与首次烧录默认值](SOC_MODULE_LOGIC.md)
3. [STM32F103 ADC 配置调研与当前工程方案](ADC_配置调研与当前方案.md)
4. [Type-C ADC 电流采样与计算说明](TypeC_ADC电流采样与计算说明.md)
5. [ADC 总压分压计算说明](ADC总压分压计算说明.md)
6. [App_AnlogCal 时基修改影响说明](App_AnlogCal时基修改影响说明.md)
7. [MonitorAFE 逻辑优化说明](MONITOR_AFE_LOGIC_OPTIMIZATION.md)
8. [后 64K SOC/AFE 参数快速测试说明](后64K_SOC_AFE快速测试说明.md)

## 文档分类索引

### 系统架构与基础设施

| 文档 | 内容定位 |
| --- | --- |
| [项目级代码与文档审查记录](PROJECT_REVIEW_2026-05-09.md) | 本轮项目级 bug 修复、架构风险、验证结果和后续优先级 |
| [MCU 资源分布与架构优化评估](MCU资源分布与架构优化评估.md) | MCU 外设、资源占用、架构优化方向 |
| [系统时钟系统梳理](系统时钟系统梳理.md) | TIM3 系统节拍、任务 flag、时基关系 |
| [System_Monitor 模块梳理](System_Monitor模块梳理.md) | 系统状态位、功能开关、错误标志使用关系 |
| [项目宏定义梳理](项目宏定义梳理.md) | 编译宏、产品配置、硬件映射、参数地址、第三方库宏使用边界 |
| [TODO](TODO.md) | 根目录待办事项 |
| [Source/todo](<103 + 309/Project/Source/todo.md>) | 源码目录内临时待办记录 |

### LED / 数码管显示

| 文档 | 内容定位 |
| --- | --- |
| [LED 软件框架与时序梳理](LED软件框架与时序梳理.md) | 当前 LED 软件架构、刷新时序、休眠交互、`GPIO_MCU_WK` 持续显示逻辑 |
| [LedBar GPIO Charlieplexing 显示方案](LEDBAR_GPIO_CHARLIE_DISPLAY_PLAN.md) | GPIO Charlieplexing 方案设计 |
| [LedBar 74HC595 + 5Pin Charlieplexing 最优显示方案](LEDBAR_74HC595_CHARLIEPLEX_OPTIMAL_PLAN.md) | 74HC595 分支的显示优化方案 |
| [Charlie + 74HC595 显示框架审查与移植说明](CHARLIE_595_DISPLAY_PORT_REVIEW.md) | 显示端口和移植风险审查 |
| [LedBar SOC 显示策略状况分析](LEDBAR_SOC_DISPLAY_STRATEGY_ANALYSIS.md) | SOC 显示策略、串亮情况分析 |
| [LedBar SOC 1~100 显示完美度逐项分析](LEDBAR_SOC_1_100_DISPLAY_ANALYSIS.md) | 1~100 每个 SOC 显示质量逐项分析 |

### 低功耗 / RTC / CAN

| 文档 | 内容定位 |
| --- | --- |
| [休眠低功耗逻辑梳理与优化建议](休眠低功耗逻辑梳理与优化建议.md) | 主低功耗框架、RTC STOP、普通/深度休眠路径 |
| [RTC CAN 自适应休眠说明](RTC_CAN自适应休眠说明.md) | RTC 周期与 CAN 对端检测自适应策略 |
| [CAN 低功耗发送调度说明](CAN低功耗发送调度说明.md) | 低功耗唤醒后的 CAN 发送调度 |
| [CAN 通信逻辑与低功耗策略分析](CAN通信逻辑与低功耗策略分析.md) | CAN 通信与休眠策略整体分析 |
| [CAN 通信回退到 9ec9f26 说明](CAN_RTC低功耗广播修复说明.md) | CAN/RTC 低功耗回退与修复记录 |

### 通信 / 地址 / EEPROM

| 文档 | 内容定位 |
| --- | --- |
| [通信逻辑与地址整理文档](COMMUNICATION_LAYOUT_REPORT.md) | 通信寄存器、读写逻辑、地址布局总览 |
| [通信完整地址索引](COMMUNICATION_ADDRESS_INDEX.md) | 通信地址索引表 |
| [0x10 子地址完整清单](COMMUNICATION_WRITE_DETAIL.md) | 0x10 写寄存器子地址明细 |
| [参数修改方式与可修改性梳理](参数修改方式与可修改性梳理.md) | 当前可修改参数、修改方式、持久化状态与不可修改边界 |
| [通信写 EEPROM 标志位映射表](COMMUNICATION_EEPROM_FLAG_MAPPING.md) | 写 EEPROM 标志位与参数区映射 |
| [通信写 EEPROM 标志位收敛与 Keil 调试方案](COMMUNICATION_EEPROM_FLAG_REFACTOR_DEBUG.md) | EEPROM 写标志收敛和调试方法 |
| [EEPROM 地址与读写逻辑梳理](EEPROM_LAYOUT_OPTIMIZATION.md) | EEPROM 地址规划和读写流程 |
| [EEPROM 写标志清理说明](EEPROM_WRITEFLAG_CLEANUP.md) | EEPROM 写标志清理记录 |

### 存储 / 参数 / 升级

| 文档 | 内容定位 |
| --- | --- |
| [存储布局说明](STORAGE_LAYOUT_REPORT.md) | EEPROM、Flash、BKP 等存储布局 |
| [可读写运行参数持久化说明](RW_PARAMETER_FLASH_STORAGE.md) | 运行参数 Flash 持久化策略 |
| [升级参数策略说明](升级参数策略说明.md) | 升级参数策略和兼容处理 |

### SOC / ADC / AFE / 监控

| 文档 | 内容定位 |
| --- | --- |
| [主流 BMS SOC 策略对比与本工程取舍](BMS_SOC_STRATEGY_COMPARISON.md) | 对比 OCV、安时积分、IR 修正、高级 fuel gauge 思路，并说明本工程用户体验优先的中等增强方案 |
| [SOC 模块完整逻辑说明与首次烧录默认值](SOC_MODULE_LOGIC.md) | SOC 入口、状态机、积分、静置/RTC OCV 小步校正、快照、通信配置和回放测试 |
| [SOC 校准策略与参数调优说明](SOC_CALIBRATION_STRATEGY.md) | SOC 校准策略、异常不校准门控、低压表、可配置参数和调优建议 |
| [STM32F103 ADC 配置调研与当前工程方案](ADC_配置调研与当前方案.md) | ADC 配置、采样、低功耗关联 |
| [Type-C ADC 电流采样与计算说明](TypeC_ADC电流采样与计算说明.md) | PA2 直接采 10mΩ 分流器压降、Type-C 输出电流稳定值与标定方法 |
| [ADC 总压分压计算说明](ADC总压分压计算说明.md) | PA1 总压分压采样、电阻参数调整、AFE 上报总压来源说明 |
| [App_AnlogCal 时基修改影响说明](App_AnlogCal时基修改影响说明.md) | 模拟校准任务时基调整影响 |
| [MonitorAFE 逻辑优化说明](MONITOR_AFE_LOGIC_OPTIMIZATION.md) | AFE 监控和恢复逻辑优化 |
| [后 64K SOC/AFE 参数快速测试说明](后64K_SOC_AFE快速测试说明.md) | 后 64K 参数区快速测试 |

### 测试与待办

| 文档 | 内容定位 |
| --- | --- |
| [待测试清单](TEST_PENDING.md) | 当前主要待测试项 |
| [待测试清单 copy](TEST_PENDING%20copy.md) | 历史/副本待测试项，建议后续合并后删除 |
| [TODO](TODO.md) | 根目录 TODO |
| [Source/todo](<103 + 309/Project/Source/todo.md>) | 源码目录局部 TODO |

## 文档维护约定

- 长期有效的模块说明、设计决策、调试结论放在根目录 Markdown 文档中，并补充到本文索引。
- 临时问题、短期任务、未验证猜想放入 `TODO.md` 或 `TEST_PENDING.md`，验证完成后再沉淀为模块文档。
- 新增文档优先使用中文标题和中文说明，函数名、寄存器名、协议名保持英文原文。
- 避免继续新增 `copy` 类副本文档；需要保留历史时，在原文档中新增“历史记录”或“变更记录”章节。
