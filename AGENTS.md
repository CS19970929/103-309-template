# 仓库协作规则

## 当前项目事实

- 本仓库主 BMS 固件位于 `103 + 309/Project`，是裸机 STM32F1 工程。
- 主工程文件是 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`，有效 Keil 目标为 `FD_Release` 和 `FD_Debug`。
- Keil `Device` 当前为 `STM32F103C8`，编译宏包含 `STM32F10X_MD` 和 `USE_STDPERIPH_DRIVER`；这描述的是当前工程配置，不等同于已经确认实物型号。
- MCU 外设实现必须以仓库内 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0` 的头文件、驱动源码和当前工程已有实现为依据；禁止凭 STM32F0、其他 STM32F1 型号或 HAL 经验猜测。
- 当前 AFE 路径以 `SH367309` 为主，真实行为以 `I2C_AFE1.*`、`SH367309_DataDeal.*`、`SH367309_Func.*` 和相关端口实现为准。
- `firmware/comm_tool_f103ret6` 是独立的 STM32F103RET6 通信工具固件，使用其自己的 Keil 工程、地址布局和 STM32F10x StdPeriph 配置；不得与主 BMS 固件参数混用。
- 已知需要核对的硬件差异：主工程名含 `103RCT6`，Keil `Device` 为 `STM32F103C8`，而持久化地址位于 `0x08010000` 以上且代码检查 Flash 容量至少为 128 KB。修改器件、Flash 容量、链接布局或擦除页大小前，必须核实实物丝印、容量寄存器和量产硬件，不得根据文件名或 Keil 选项单独下结论。
- 产品型号、板号和发布版本必须从当前配置、工程文件或用户输入确认；不要仅凭目录名推断。

## 默认工作方式

- 全部使用中文回答；函数名、寄存器名、协议字段名保持英文原文。
- 当前源码是第一可信来源；旧文档、历史记录和记忆只能作为参考。
- 日常开发以解决当前明确问题为目标，优先小步净删减、少改文件、少加变量、少加封装。
- 不要默认做全量项目审查、需求确认表、长篇文档、`change_log` 或 `test_plan` 更新；只有用户明确要求、发布收口或安全边界变化时才做。
- 验证采用目标化验证：改哪个模块就跑相关脚本、host test、静态检查或最小编译检查；不要默认跑全仓噪声很大的 `project_check.py --quiet`。
- 用户要求每条修改单独提交时，一个逻辑变更一个 commit；只白名单 stage 本次相关文件，不带入 `todo.md`、未跟踪文件或无关脏文件。

## 代码原则

- 不要引入 HAL、RTOS、`malloc`，除非用户明确批准。
- 不要破坏现有 Modbus、CAN、上位机协议兼容性；不要修改客户可见寄存器、帧格式、CAN ID 或数据含义，除非用户明确批准。
- 优先使用 STM32 标准外设库或寄存器级实现。
- 优先写简单、清晰、可维护的 C 代码；避免过度设计和一次性大规模重构。
- 新增抽象必须有明确收益：减少真实重复、隔离明确边界或降低调用方复杂度；不要为了结构完整新增中间层、状态缓存、影子字段或未来预留接口。
- 对 SOC、低功耗、保护、协议、烧录相关改动，优先保持现有调度顺序、硬件时序、保护条件、协议字段和安全边界不变。
- 对新增资源受限 MCU 代码，默认禁止递归、动态内存、无界等待和无必要浮点；运行态延时优先使用现有 tick、时间戳或状态机。
- ISR 只做必要寄存器操作、数据搬运、计数和事件置位；不得在 ISR 中做 Flash 擦写、完整协议解析、复杂保护业务、长循环或格式化输出。
- 模块内部函数和变量优先 `static`，重要状态保持明确所有者和修改入口；不要扩散新的 `extern` 可写全局变量。
- 物理量命名尽量带单位，例如 `_mv`、`_ma`、`_mah`、`_ms`、`_decic`、`_permille`；改动旧代码时先确认现有缩放比例，不为统一命名进行无关批量重构。
- 时间差、计数、缩放和协议转换必须检查 signed/unsigned、截断、溢出及 tick 回绕。
- 注释解释硬件约束、时序、兼容原因和异常行为，不逐句翻译代码。

## 参数与持久化

- IAP 起始地址为 `0x08000000`，主 BMS App 起始地址为 `0x08004800`；地址定义以 `Flash.h`、Keil 工程和 scatter 文件三者一致为准。
- 当前参数与状态存储集中在 `0x0801C000`～`0x0801FC00` 区域，具体 slot、页面大小和兼容逻辑以 `103 + 309/Project/Source/Flash.h`、`Flash.c`、`EEPROM.c` 为准。
- 持久化记录包含 magic、版本、长度、sequence 和 CRC；修改结构时必须检查旧版本读取、双 slot/日志页切换、写后校验和掉电一致性。
- 参数升级策略版本由 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` 控制，落盘标志地址为 `FLASH_ADDR_UPGRADE_PARAM_FLAG`。不得因未知或旧版本标志而无条件覆盖全部历史参数；只执行当前发布明确要求的迁移动作。
- SOC 快照当前格式版本为 `FLASH_STORAGE_SOC_DATA_VERSION_V2`，已有 V1 兼容读取路径；修改时必须保留可验证的迁移行为。
- 修改 Flash 地址、页大小、参数数量、CRC、版本字段或升级策略属于安全边界变化，必须同步最相关文档或脚本并完成针对性验证。

## 低功耗与唤醒

- 当前工程已有 `HICCUP_MODE`、`NORMAL_MODE`、`DEEP_MODE` 及 STOP/复位睡眠流程；不得仅因空闲新增入口、改写模式含义或绕过现有状态保存顺序。
- 低功耗改动必须沿 `rtc_sleep.*`、`SleepDeal.*`、`LowPowerSleep.*` 和对应 port/AFE 实现核对进入条件、AFE 睡眠、SOC/运行时保存、CAN/串口状态及唤醒恢复。
- 已配置的唤醒相关路径包括 RTC、按键/MCU_WAKE、充电器/电流、AFE 以及 UART1/RS485；具体启用条件必须以编译配置和当前代码为准。
- 必须验证无效唤醒回睡、充电唤醒、通信唤醒、低电压强制休眠、watchdog、外设恢复和复位后标志恢复。

## 需求与文档

- 用户说“先看、先分析、先给方案、等待确认”时，只做只读分析和方案，不改源码。
- 只有用户明确要求完整审查、需求确认或文档整理时，才创建需求确认表、风险表、全量 review 文档。
- 日常代码修改不需要每次更新 `docs/change_log.md` 或 `docs/test_plan.md`。
- 如果代码行为变化会让当前权威文档误导维护者，只更新最相关的一处文档，简短写清楚当前事实。
- 涉及烧录地址、测试模式、量产隔离、上位机启动方式、安全脚本、协议对外行为时，必须把长期规则写入脚本或文档。

## 烧录安全

- IAP/Bootloader 地址是 `0x08000000`。
- 正常 App 地址是 `0x08004800`。
- App scatter 文件是 `103 + 309/Project/Users/Objects/FD_Release.sct`。
- 禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`，否则会覆盖 IAP。
- App 烧录优先使用：
  `.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash`
- 修改或新增烧录脚本时，必须保留 `0x08004800` 地址检查和 dry-run 输出。

## 构建与目标化验证

- 主固件最小发布构建命令：
  `powershell -ExecutionPolicy Bypass -File .\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release`
- 需要调试目标时使用同一脚本的 `-Target FD_Debug`；不要把 Debug 产物当作发布或量产烧录文件。
- SOC 相关改动可使用：
  `powershell -ExecutionPolicy Bypass -File .\tools\bms_dev_workflow.ps1 -Mode quick`
- 只在改动确实跨越多个模块或用户明确要求时扩大全量检查；已有 `c073.cppcheck` 可作为相关静态分析入口，发现工具自身配置错误时要与代码问题区分报告。
- 构建日志和临时分析目录必须写入用户临时区；项目约定的正式固件产物继续使用 Keil 目标的既有输出目录。
- 未连接硬件或未执行烧录时必须明确说明，不得把编译、host test 或 dry-run 描述成板上验证。

## 上位机与显示要求

- 修改用户上位机代码后，必须立即编译最新 exe。
- CAN 用户上位机固定在 `BMS_CommTool_Upgrade_UI.exe` 基础上修改，生成文件必须覆盖 `dist\BMS_CommTool_Upgrade_UI.exe`，不要另起新的 exe 名称。
- 老化剩余时间必须在原上位机界面里单独可见，入口是 `其它功能 -> 常用功能 -> 读取老化时间`，通过 `0x13 BMS_AGING_STATUS` 解析 `0x14F80208` 广播。
- BMS 序列号、硬件版本、软件版本必须在原上位机 `实时监控` 界面最底部边栏显示，读取来源固定为 `0xC002` 的 48 个寄存器。
