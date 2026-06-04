# 代码执行流程分析 README

状态：部分验证。本文档集只分析代码并生成Markdown，不修改业务源码，不提交git。

## 本次分析范围

- 主目标：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` 的 BMS 固件。
- 主入口：`103 + 309/Project/Source/main.c`。
- 重点模块：启动、初始化、主循环、TIM/USART/CAN/RTC/EXTI中断、AFE采样、ADC、保护/MOS、SOC、SCI/Modbus、CAN、Flash/参数/日志、低功耗、LED。
- 非主目标：`firmware/comm_tool_f103ret6` 和 `tools/*.c` 有独立 `main()`，已在索引中列出，但未纳入BMS主运行时序。

## 阅读顺序

| 顺序 | 文档 | 作用 |
| --- | --- | --- |
| 1 | `00_project_symbol_index.md` | 先看文件、函数、变量、宏、类型和中断索引，定位代码。 |
| 2 | `01_main_startup_flow.md` | 从Reset/main理解真实入口和启动调用顺序。 |
| 3 | `02_init_flow.md` | 看进入while前每个初始化函数的硬件和状态副作用。 |
| 4 | `03_main_loop_flow.md` | 看主循环每圈的真实任务顺序和核心业务链。 |
| 5 | `04_interrupt_async_flow.md` | 看ISR入口、共享变量和竞态风险。 |
| 6 | `05_global_variables_data_flow.md` | 按模块看关键全局变量和数据流。 |
| 7 | `06_module_relationship.md` | 看模块边界、依赖和耦合点。 |
| 8 | `07_full_call_tree.md` | 从main递归看调用树和隐藏宏/函数指针。 |
| 9 | `08_runtime_sequence.md` | 按运行阶段看上电、采样、通信、低功耗和异常。 |
| 10 | `09_refactor_reference.md` | 只读重构参考，不包含源码修改。 |


## 重要结论摘要

- 真实主骨架是 `Reset_Handler -> SystemInit -> __main -> main -> AppInit_Boot -> while Runtime_RunOnce`。
- `Runtime_RunOnce()` 固定分为前段任务、IO/低功耗任务、后台任务。
- `TIM3_IRQHandler()` 是10ms节拍源；200ms AFE/SOC任务通过 `SysTime_Take200msTaskPeriod()` 消费pending周期。
- `App_AFEGet()` 是采样/保护/MOS/SOC的主入口，顺序不能随便改。
- 低功耗有运行期 RTC STOP/HICCUP 和 reset-sleep 两条路径，必须分开理解。
- SCI和CAN都能触发参数/SOC/老化/IAP副作用，协议边界是高风险重构点。
- 当前工作区有用户未提交改动，文档以当前文件内容为准。

## 后续人工检查清单

- 用Keil/实际预处理结果确认 `PROJECT_CFG_*` 宏最终值，特别是 release/debug profile 与 debug guard。
- 确认 `Init_IWDG()` 注释状态是否符合量产要求。
- 确认 `__EnableLowPowerDebug__` 是否应在真实低功耗测试前关闭。
- 对 `g_stCellInfoReport`、`OtherElement`、`System_ErrFlag` 做字段级协议/单位确认。
- 对 `new_todo_logi()`、`Sci_Upper.c` 写寄存器副作用、低功耗blocker 做重构前需求确认。
