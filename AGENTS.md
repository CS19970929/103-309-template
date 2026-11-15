# 仓库协作规则

## 当前项目事实

- 本仓库主 BMS 固件位于 `103 + 309/Project`，是裸机 STM32F1 工程。
- 主工程文件是 `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`，有效 Keil 目标为 `FD_Release` 和 `FD_Debug`。
- 主 BMS 实物 MCU 已由用户于 2026-10-26 确认为 `STM32F103CBT6`：128 KB Flash、20 KB SRAM、1 KB Flash 擦除页。两个 Keil 目标使用 `STM32F103CB`，保留 `STM32F10X_MD` 和 `USE_STDPERIPH_DRIVER`。
- MCU 外设实现沿用仓库内 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0` 的头文件、驱动源码和现有体系；硬件限制以 STM32F103CBT6 对应官方手册和 Errata 为准，发现现有实现冲突时先核实。禁止凭 STM32F0、其他 STM32F1 型号或 HAL 经验猜测。
- 当前 AFE 路径以 `SH367309` 为主，真实行为以 `I2C_AFE1.*`、`SH367309_DataDeal.*`、`SH367309_Func.*` 和相关端口实现为准。
- `firmware/comm_tool_f103ret6` 是独立的 STM32F103RET6 通信工具固件，使用其自己的 Keil 工程、地址布局和 STM32F10x StdPeriph 配置；不得与主 BMS 固件参数混用。
- 主工程名中的 `103RCT6` 是历史命名，不代表实物型号；后续器件或容量变更须核对实际硬件、容量寄存器和工程配置，不根据文件名推断。
- 产品型号、板号和发布版本必须从当前配置、工程文件或用户输入确认；不要仅凭目录名推断。

## 默认工作方式

- 全部使用中文回答；函数名、寄存器名、协议字段名保持英文原文。
- 当前源码和工具输出证明软件现状，用户要求和产品合同确定预期行为，官方手册和 Errata 确定硬件限制；旧文档、历史记录和记忆仅作参考。
- 日常开发以解决当前明确问题为目标，优先小步净删减、少改文件、少加变量、少加封装。
- 按本次行为影响维护最相关的必要说明；不默认新增全量审查、需求表、长篇文档或更新 `change_log`、`test_plan`。协议、安全边界和发布流程变化必须记录。
- 验证采用目标化验证：改哪个模块就跑相关脚本、host test、静态检查或最小编译检查；不要默认跑全仓噪声很大的 `project_check.py --quiet`。
- 用户要求每条修改单独提交时，一个逻辑变更一个 commit；只白名单 stage 本次相关文件（包括本次新增文件），不带入 `todo.md`、原有无关未跟踪文件或脏文件。

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

## 同口 BMS 的 MOS 仲裁

- 本项目遵循全局同口 BMS 反向临时导通规则，所有单路软件关断统一进入 `Fault_ApplyMosPermits`；维护软件保护、客户协议或其他关断入口时均须检查反向放行和原请求恢复。
- 当前反向门限为 1 A（既有 A*10 电流值为 10），RUN 按 200 ms 电流采样节拍更新仲裁。读取无效、STOP 暂停测量或超过 500 ms 未更新时，撤销反向放行资格；门限和时间属于本项目配置，不作为其他项目默认值。
- MOS 过温、客户 `xx=3` 和显式双路关断保持两路关闭。AFE 参数等待应用或失败时禁止新增许可；许可写入通过 `SH367309_SetMosPermits` 读取、按需修改并回读核对。保护回归入口为 `tools/test_protection_refactor.py`，实际 MOS 动作仍需板上验证。

## 参数与持久化

- IAP 起始地址为 `0x08000000`，主 BMS App 起始地址为 `0x08004800`；地址定义以 `Flash.h`、Keil 工程和 scatter 文件三者一致为准。
- APP 链接范围为 `[0x08004800, 0x0801C000)`，最大长度 `0x17800`，保留后续持久化区；物理 Flash 范围为 `[0x08000000, 0x08020000)`。应用 SRAM 范围为 `[0x20000000, 0x20004FE0)`，末尾 32 字节保留给 IAP 邮箱，不能因同步器件型号而放开这些边界。
- 当前配置、状态、日志、统计各占两个 1 KB 页，位于 `0x0801E000`～`0x0801FFFF`。修改持久化、升级清空或事件计数前，阅读 `docs/flash-storage.md` 并运行 `tools/test_storage.py`；校验提交标记、CRC、序号、切页和掉电恢复。
- 本项目按用户明确授权不迁移旧参数格式，属于全局旧数据兼容规则的例外，不授权改变存储布局或清空统计。`PROJECT_CFG_STORAGE_UPDATE_GENERATION` 与 `PROJECT_CFG_STORAGE_UPDATE_MASK` 控制显式按组参数更新；默认 0/0 仅保留数据，不按软件版本字符串清空。状态/日志清除先提交意图、执行后再清标记，重启必须完成已提交事务。
- 统计页 `0x0801F800`、`0x0801FC00` 独立保存客户命令4计数；参数初始化、日志覆盖和日志清空均须保留统计。客户命令5可清零前 10 项故障次数，保留重启次数和当前故障掩码；统计损坏须返回错误，不能静默清零。
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

- 主固件默认无镜像编译检查命令：
  `powershell -ExecutionPolicy Bypass -File .\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release`
- 默认 `build` 复用 `tools/check_bms_no_bin.py`，在用户临时区关闭 BIN/HEX 及构建钩子，编译 AXF/MAP 并校验 App 链接边界；不修改原 Keil 工程。
- 只有用户明确要求固件镜像时，才给上述命令增加 `-GenerateImage`，由原 Keil 工程在既定输出目录生成镜像。`flash` 或 `full -Flash` 明确请求生成镜像并走安全 App 烧录；普通编译、测试或文档列出的命令不构成镜像或烧录授权。
- 需要调试目标时使用同一脚本的 `-Target FD_Debug`；不要把 Debug 产物当作发布或量产烧录文件。
- SOC 相关改动可使用：
  `powershell -ExecutionPolicy Bypass -File .\tools\bms_dev_workflow.ps1 -Mode quick`
- `quick` 只运行 SOC 和电流校准 host C 回归；显式加 `-ProjectCheck` 才做全仓一致性检查，`-StrictProjectCheck` 同时要求该检查成功。`full` 显式扩大为全仓检查、host 回归和默认无镜像编译，不隐含生成 BIN 或烧录。
- 修改工作流入口时运行 `py -B tools/test_bms_dev_workflow.py`；该回归在用户临时区使用工具替身，不生成镜像或连接硬件。`full -Probe` 使用本次无镜像编译的临时 AXF，单独 `probe` 仍使用工程既有 AXF。
- 只在改动确实跨越多个模块或用户明确要求时扩大全量检查；已有 `c073.cppcheck` 可作为相关静态分析入口，发现工具自身配置错误时要与代码问题区分报告。
- Codex 日志和临时分析目录写入用户临时区；允许工程既定输出目录中的正常编译中间产物和已授权正式固件产物，不在项目内另建 Codex 临时目录。
- 未连接硬件或未执行烧录时必须明确说明，不得把编译、host test 或 dry-run 描述成板上验证。

## 上位机与显示要求

- 本节 CAN 工具独立于通用串口/BLE 客户端，继续维护本仓库原源码和固定 EXE；串口/BLE Windows、Android、CLI 的开发入口仍是 `C:\Users\Administrator\source\bms-client` 的 `client/main`，不因新板型另起产品分支。
- 修改用户上位机代码后，必须立即编译最新 exe。
- CAN 用户上位机固定在 `BMS_CommTool_Upgrade_UI.exe` 基础上修改，生成文件必须覆盖 `dist\BMS_CommTool_Upgrade_UI.exe`，不要另起新的 exe 名称。
- 老化剩余时间必须在原上位机界面里单独可见，入口是 `其它功能 -> 常用功能 -> 读取老化时间`，通过 `0x13 BMS_AGING_STATUS` 解析 `0x14F80208` 广播。
- BMS 序列号、硬件版本、软件版本必须在原上位机 `实时监控` 界面最底部边栏显示，读取来源固定为 `0xC002` 的 48 个寄存器。

全局规则的版本快照见 `docs/codex-global-rules.md`，仅用于追溯；实际全局文件为 `C:\Users\Administrator\.codex\AGENTS.md`。
