# 删除或保留清单

文档状态：部分验证  
验证方式：只读源码审查，未修改源码。  
硬性边界：本清单只是候选决策表，不代表已经允许删除。所有固件源码删除必须等用户逐项确认后再执行。

## 1. 判断规则

删除优先级按风险从低到高排列：

1. 注释调用、无实现、无有效引用的死代码。
2. `#if 0` 包住的旧实现，且当前协议行为不依赖旧实现。
3. 空函数、伪实现、只返回默认值的兼容函数。
4. 默认关闭、只用于测试的功能，迁移到 `tests_or_tools/` 或单独测试固件。
5. 客户历史逻辑、保留协议位、特殊硬件逻辑，只能在确认产品不需要后删除。

默认保留原则：

- 保护、MOS、AFE、低功耗、Flash 地址、Modbus/CAN 协议、老化、SN/版本、IAP 默认保留。
- 即使源码显示疑似无用，只要可能影响协议布局或客户可见行为，先保留并提问。
- 优先删除包装层、死宏、历史注释和重复状态；不优先改算法。

## 2. 建议保留清单

| ID | 功能 | 建议 | 依据 | 后续处理 |
|---|---|---|---|---|
| K-001 | Boot/App 地址与 IAP | 必须保留 | `Flash.h` 固定 `APP_ADDR=0x08004800`，`APP_To_IAP_Jump()` 依赖 SRAM mailbox | 重构期间只允许封装，不允许改地址。 |
| K-002 | Modbus/SCI1 协议 | 必须保留 | `Sci_Upper.c/h`、寄存器 `0xD000/0xD300/0xC002` 等 | 可重构寄存器表，但地址、长度、错误码不变。 |
| K-003 | 飞道 CAN 协议 | 必须保留 | `Can_HDX.c`、`CanFeidaoFrames.c` | CAN ID、周期、数据含义保持不变。 |
| K-004 | AFE SH367309 驱动 | 必须保留 | `I2C_AFE1.c`、`SH367309_*` | 可拆 driver/config/monitor，但保持 bitbang 时序和 MTP 行为。 |
| K-005 | 保护参数和故障位 | 必须保留 | `Fault.h/c`、`System_Monitor.h/c` | 可整理命名，不可改协议位含义。 |
| K-006 | SOC 算法 | 必须保留 | `SOC.c`、`SocEnhance.c` | 先收口输入/输出，后续单独测试算法。 |
| K-007 | Flash 持久化布局 | 必须保留 | `Flash.h` storage address、`Flash.c` typed load/save | 地址和结构版本不能随意改。 |
| K-008 | RTC/低功耗唤醒 | 必须保留 | `rtc_sleep.c`、`SleepDeal.c`、`app_lowpower.c` | 高风险重构，先拆边界后验证。 |
| K-009 | LED 数码管和睡眠快显 | 必须保留 | `LedBar.c`、BKP DR4/DR5 | 与按键唤醒和 SOC 显示体验相关。 |
| K-010 | 老化流程 | 必须保留 | `FactoryAging.c`、CAN/Modbus 老化命令 | 当前产品要求原上位机可见老化剩余时间。 |
| K-011 | 事件日志 | 必须保留 | `LogRecord.c`、`Flash.c`、`Sci_Upper.c` | 保护事件、启动、休眠记录依赖。 |
| K-012 | SN/硬件/软件版本 | 必须保留 | `ProductionID.c`、`Sci_Upper.h` `0xC002` | 当前上位机要求实时监控底部显示。 |

## 3. 建议保留但可简化

| ID | 项目 | 当前证据 | 简化方向 | 风险 |
|---|---|---|---|---|
| S-001 | `main.h` 伞状 include | `main.h` include 大量业务头文件 | 分阶段让每个 C 文件 include 自己需要的头文件 | 改 include 可能暴露隐藏依赖，需小步编译。 |
| S-002 | BSP 薄包装 | `bsp_power.c`、`bsp_clock.c`、`bsp_rtc.c` 多数是转调 | 在新架构中保留 BSP 边界，但减少无意义一层转发 | 低功耗阶段不要一次合并。 |
| S-003 | `System_ERROR_UserCallback` 偏移表 | `System_Monitor.c` 用 enum 差值映射 union 位 | 改成显式 switch/table，并保留协议位布局 | 位含义不能变。 |
| S-004 | `sys_time` 大杂烩 | `conf.h:Time_T` 包含 CAN、SCI、RTC、调试、Type-C 等字段 | 拆成 `system_time_runtime`、`debug_counters`、`low_power_counters` | 先只读写等价迁移。 |
| S-005 | `DataDeal.c` 采样和业务混杂 | `App_AFEGet` 串起采样、保护、SOC、MOS | 拆 `bms_core`、`protection`、`mos_ctrl`，保留调用顺序 | 保护/MOS 行为高风险。 |
| S-006 | `Sci_Upper.c` 寄存器映射 | 读写地址判断分散在大函数 | 迁移成只读/读写寄存器表和 handler 表 | 地址兼容必须自动对照测试。 |
| S-007 | `Can_HDX.c` 多职责 | CAN 硬件、队列、周期调度、App 命令混合 | 拆 `can_hw`、`comm_can`、`can_app_cmd`、`can_feidao_frames` | CAN 低功耗行为需实机验证。 |
| S-008 | `Flash.c` 多职责 | driver、storage、IAP、upgrade 都在一个文件 | 拆 `flash_hw`、`storage_flash`、`iap_service` | 地址和擦写顺序不能变。 |
| S-009 | `LedBar.c` 多职责 | 扫描、按键、低功耗、显示策略同文件 | 拆 `led_scan_gpio` 与 `led_display` | TIM4 扫描不能抖动。 |
| S-010 | `FactoryAging.c` 状态和存储耦合 | BKP、Flash、MOS、host 命令混杂 | 保留状态机，抽出 storage/mos 接口 | 老化剩余时间要保持 UI 可见。 |

## 4. 疑似无用，需要用户确认

| ID | 候选项 | 代码证据 | 建议动作 | 删除前必须确认 |
|---|---|---|---|---|
| C-001 | `ShortFunc.c/h` 和 `InitShortCur()` | `SH367309_DataDeal.c` 中只有 `// InitShortCur();` 注释调用，`main.h` 仍 include `ShortFunc.h` | 若负载移除短路恢复不再使用，删除源码和 include | 当前产品是否需要负载移除短路恢复？ |
| C-002 | `SH367309_SC_DelayT_Set()` | `SH367309_Func.c/h` 有定义/声明，未见有效调用 | 删除或归档 | 是否曾用于 AFE 短路延时参数现场调试？ |
| C-003 | `test_Autocurrent_cycle()` | `DataDeal.c` 有函数，调用处被注释 | 删除 | 是否仍需要作为人工电流循环测试入口？ |
| C-004 | `ReadEEPROM_Byte/Word`、`WriteEEPROM_Byte/Word` | `EEPROM.c/h` 提供伪 EEPROM API，搜索未见有效调用 | 删除或改名为 legacy stub | 是否有外部工程或历史分支还依赖这些函数名？ |
| C-005 | SCI2/SCI3 代码路径 | `PROJECT_CFG_SCI2_ROLE=0`、`PROJECT_CFG_SCI3_ROLE=0`，代码条件编译 | 若硬件无 SCI2/SCI3 协议需求，可删除角色路径 | 当前板子是否只使用 SCI1？ |
| C-006 | `__VIRTURE_CURRENT__` 调试电流 | `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE=1`，`DataDeal.c` 用 `sys_time.isdebugenable` 覆盖电流 | 量产建议关闭或隔离到 Debug/Factory | 是否现场仍用该功能模拟电流？ |
| C-007 | 环境温度 ENV2/ENV3 强制 -40 | `DataDeal.c:DataLoad_Temperature` 中硬编码 | 改成明确的 not-fitted 通道或删除协议上报 | 该硬件是否实际未贴 ENV2/ENV3？ |
| C-008 | System Monitor 预留错误位 | `System_Monitor.h` 包含 client/screen/wifi/bluetooth/app/heat/cool/relay 等 | 协议保留位可保留，内部业务路径可裁剪 | 上位机是否显示或依赖这些位？ |
| C-009 | Flash64K app test | `Flash64KAppTest.c` 默认关闭，Release guard 禁止打开 | 迁移到 `tests_or_tools/` 或单独测试固件 | 是否还需要在主 Keil 工程保留？ |
| C-010 | EasyLogger | Release 关闭，Debug/Factory 可启用 | 保留为 Debug 资产或移动到 debug 目录 | 后续调试是否继续用串口日志？ |
| C-011 | 旧校准写寄存器实现 | `Sci_Upper.c` 中 `Sci_WrRegs_0x10_CalibCoef` 和 reset body 被 `#if 0` 包住 | 删除旧实现，保留协议拒绝/空行为 | 上位机是否仍认为这些写入应生效？ |
| C-012 | `Sci_WrRegs_0x10_CopperLoss()`、`Sci_WrRegs_0x10_RTC()` 空实现 | `Sci_Upper.c` 有函数但无实际动作 | 改成明确不支持或实现真实写入 | 上位机是否会调用并期待保存成功？ |
| C-013 | `FLASH_ADDR_SLEEP_FLAG` | `Flash.h` 仍定义，当前 `SleepDeal.c` 使用 BKP DR2/DR3 保存 boot flag | 若确认已迁移 BKP，可删除旧地址宏 | 是否有旧版本 Bootloader 或外部工具读取该 Flash 地址？ |
| C-014 | `AFE_SHIP()` 空函数 | `SH367309_Func.c` 中函数体为空 | 删除或明确未支持 | 是否计划支持 ship mode？ |
| C-015 | 低功耗强制深睡 2800mV/60s | `rtc_sleep.c` 中 `LOW_POWER_FORCE_DEEP_SLEEP_*` 标注待测试 | 保留但写入配置和测试计划 | 该策略是否已经实机确认？ |

## 5. 可删除候选

这些项目源码证据较明确，但仍必须等用户确认后进入源码修改阶段。

| ID | 候选项 | 证据 | 删除方式 | 验证 |
|---|---|---|---|---|
| D-001 | `App_WarnCtrl` 残留 | `Fault.h` 仅声明，`Runtime.c` 仅注释调用，未见实现 | 删除声明和注释调用 | Keil 编译无缺符号。 |
| D-002 | `I2C_AFE1.c/h` 中 `#if 0` 旧 I2C 包装路径 | 当前使用直接 PB8/PB9 bitbang 宏 | 删除旧分支和注释 | AFE 读写时序编译一致。 |
| D-003 | `ADC.c` 中无效 TIM2 remap `#if 0` | 注释标注“无效” | 删除无效分支 | ADC 初始化编译通过。 |
| D-004 | `PubFunc.c`、`RTC.c`、`Sci_Upper.c` 中大段 `#if 0` 旧实现 | 当前不参与编译 | 删除或迁移到 `docs/archive` 摘要 | 保持当前函数签名和行为。 |
| D-005 | `Runtime.c` Debug 专用 `_DEBUG_CODE` 简化路径 | Release guard 禁止 `_DEBUG_CODE` | 若 Debug profile 不再需要，可删除 | 需确认 Debug profile 是否仍使用。 |

## 6. 高风险必须重构但不能直接删

| ID | 区域 | 当前问题 | 重构目标 | 禁止事项 |
|---|---|---|---|---|
| R-001 | `DataDeal.c:new_todo_logi()` | MOS、charger、认证、RF_EN、保护、AFE error 混杂 | 拆成 `bms_core` 调度、`protection` 判断、`mos_ctrl` 执行 | 禁止直接删 UL/charger/低压/MOS 条件。 |
| R-002 | `App_AFEGet()` | 200ms 采样、保护、SOC 的主链路过重 | 保留顺序，拆为清晰阶段函数 | 禁止改变 200ms 更新节奏。 |
| R-003 | `rtc_sleep.c` + `rtc_sleep_port.c` | STOP 周期唤醒触达 AFE/SOC/CAN/日志/LED | 拆 `low_power` 状态机和端口 API | 禁止改变唤醒源判断和 BKP flag 含义。 |
| R-004 | `Can_HDX.c` | 低功耗保活、TX queue、App 命令、IAP、老化命令混杂 | 先拆纯 CAN 队列和协议服务 | 禁止改 CAN ID、帧周期、ACK 状态。 |
| R-005 | `Sci_Upper.c` | 协议映射和业务写入混杂 | 提取 register map，保留 handler | 禁止改地址、长度、读写权限、错误码。 |
| R-006 | `Flash.c` | Flash driver 和 typed storage 混杂 | 拆 driver/storage/IAP | 禁止改擦写地址、magic/version、双槽策略。 |
| R-007 | `System_Monitor.c/h` | enum 差值映射 union 位，保留位多 | 明确命令到状态位映射 | 禁止改协议位序。 |
| R-008 | `SocEnhance.c` | 算法复杂、状态多 | 先加输入样本结构和输出发布边界 | 禁止在无测试时改校准逻辑。 |

## 7. 第一批建议确认项

建议用户优先确认以下删除/保留问题，用于启动第一批低风险净删减：

1. 是否删除 `App_WarnCtrl` 残留声明和注释调用？
2. 是否删除 `ShortFunc.c/h` 和 `InitShortCur()`？
3. 是否删除 `SH367309_SC_DelayT_Set()`？
4. 是否删除 `test_Autocurrent_cycle()`？
5. 是否删除伪 EEPROM API？
6. 是否把 Flash64K 测试移出主业务工程？
7. 是否清理 `Sci_Upper.c` 中 `#if 0` 旧校准写入代码？
8. 是否确认 ENV2/ENV3 温度通道未贴片，可改为明确 not-fitted？
