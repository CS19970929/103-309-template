# 风险点与确认问题

文档状态：部分验证  
用途：重构开始前的风险审查和需求确认入口。  
结论：当前项目可以重构，但不能直接大规模搬目录或改算法；必须先确认删除项和客户可见行为。

## 1. 高风险区域

| 风险 ID | 区域 | 风险描述 | 代码依据 | 风险等级 | 建议 |
|---|---|---|---|---|---|
| R-001 | 保护/MOS/charger 混合逻辑 | `new_todo_logi()` 同时处理 charger、MOS、认证、RF_EN、AFE error、保护阈值，任意误删都可能影响安全行为 | `DataDeal.c:new_todo_logi` | 高 | 必须拆分，但先保持判断顺序和输出时机。 |
| R-002 | AFE MTP 写入 | AFE 参数刷新会写 MTP、切电、reset、重新配置，失败会影响采样和保护 | `SH367309_DataDeal.c`、`I2C_AFE1.c` | 高 | 先封装，不改时序。 |
| R-003 | 低功耗 STOP/RTC 唤醒 | RTC sleep 期间会更新 AFE/SOC、执行 CAN 唤醒广播、判断异常唤醒 | `rtc_sleep.c`、`rtc_sleep_port.c` | 高 | 低功耗单独阶段验证。 |
| R-004 | reset sleep boot flag | 休眠模式通过 BKP DR2/DR3 标记，启动时进入不同 IO/STOP 路径 | `SleepDeal.c` | 高 | 不改 flag 值和 BKP 寄存器。 |
| R-005 | CAN 低功耗保活 | `Can_RtcWakeService()` 在 RTC 唤醒窗口同步发帧并等待队列清空 | `Can_HDX.c` | 高 | CAN 与低功耗不能分开盲改。 |
| R-006 | Modbus/CAN 协议兼容 | 上位机、CAN App 命令、老化、IAP 都依赖现有地址、ID、ACK/错误码 | `Sci_Upper.c/h`、`Can_HDX.c` | 高 | 重构只改内部结构，不改协议。 |
| R-007 | Flash 持久化布局 | SOC/AFE/RW/log/aging 数据存在固定 Flash page 和版本结构 | `Flash.c/h` | 高 | 地址、magic、CRC、结构版本保持不变。 |
| R-008 | IAP/Bootloader 地址 | App 必须从 `0x08004800` 烧录，写错会覆盖 IAP | `Flash.h`、安全脚本文档 | 高 | 所有脚本必须保留 dry-run 和地址检查。 |
| R-009 | SOC 算法复杂 | SOC 包含 OCV、库仑计、full/empty anchor、静置补偿、显示平滑 | `SocEnhance.c` | 中高 | 第一轮只收口接口，不改算法。 |
| R-010 | 老化流程 | 老化状态存 BKP 和 Flash，控制 MOS，且要求上位机可见剩余时间 | `FactoryAging.c`、`CanFeidaoFrames.c` | 中高 | 保留功能，后续只拆边界。 |
| R-011 | LED 与低功耗耦合 | LED 显示、睡眠 SOC 快显、长按关机都在 `LedBar.c` | `LedBar.c` | 中高 | 扫描和策略拆开，但先不改用户体验。 |
| R-012 | 系统错误位布局 | `System_ERROR_UserCallback()` 用 enum offset 映射 union 位，协议可能依赖布局 | `System_Monitor.c/h` | 中高 | 不确认协议前不删保留位。 |
| R-013 | Debug/Test 宏散落 | `_DEBUG_CODE`、`FLASH64K_*`、`SOC_TEST_MODE_ENABLE`、`__VIRTURE_CURRENT__` 等散落 | `conf.h`、`Project_Config.h` | 中 | 先集中配置，Release guard 保留。 |
| R-014 | Keil 工程和中文编码 | 文件包含中文注释和 Keil Configuration Wizard 内容，误格式化可能乱码 | `Project_Config.h` 等 | 中 | 不做全文件格式化，局部小改。 |

## 2. 必须由用户确认的删除项

| Question ID | 需要确认的问题 | 当前源码证据 | 建议决策 | 用户决策占位 |
|---|---|---|---|---|
| Q-001 | `ShortFunc.c/h` 和 `InitShortCur()` 是否可以删除？ | 只有 `SH367309_DataDeal.c` 中注释调用，`main.h` 仍 include | 如果当前产品不用负载移除短路恢复，删除 | 待确认 |
| Q-002 | `SH367309_SC_DelayT_Set()` 是否可以删除？ | 有定义和声明，未见有效调用 | 若不再调 AFE 短路延时，删除 | 待确认 |
| Q-003 | `test_Autocurrent_cycle()` 是否可以删除？ | 函数存在，调用处注释 | 删除 | 待确认 |
| Q-004 | 伪 EEPROM API 是否可以删除？ | `ReadEEPROM_Byte/Word` 等无真实存储，未见有效调用 | 删除或迁移到 legacy archive | 待确认 |
| Q-005 | `App_WarnCtrl` 残留是否可以删除？ | 仅声明和注释调用，未见实现 | 删除 | 待确认 |
| Q-006 | `Sci_Upper.c` 中 `#if 0` 旧校准写入代码是否可以清理？ | 当前写函数主体被禁用或为空 | 清理旧实现，保留当前协议返回行为 | 待确认 |
| Q-007 | `Sci_WrRegs_0x10_CopperLoss()` 和 `Sci_WrRegs_0x10_RTC()` 空实现应删除、拒绝还是补全？ | 函数存在但无实际动作 | 若上位机不依赖，改成明确不支持 | 待确认 |
| Q-008 | Flash64K 测试是否移出主业务工程？ | 默认关闭且 Release guard 禁止 | 移到 `tests_or_tools/` 或单独测试固件 | 待确认 |
| Q-009 | AFE I2C `#if 0` 旧包装路径是否删除？ | 当前使用直接 bitbang 宏 | 删除旧路径 | 待确认 |
| Q-010 | `AFE_SHIP()` 空函数是否删除？ | 函数体为空 | 删除或标注未支持 | 待确认 |

## 3. 必须由用户确认的功能边界

| Question ID | Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|---|
| Q-101 | REQ-PROTECT-001 | 保护、MOS、charger、认证逻辑必须保留 | `DataDeal.c:new_todo_logi`、`MosStartup.c`、`SH367309_Func.c` | 200ms 采样后执行，直接改 AFE MOS 和 GPIO | 高 | 必须重构但不能删 | `_UL_RENZHENG_ENABLE_` 认证/熔丝逻辑当前产品是否必须保留？ | 默认保留，拆到 `mos_ctrl` | 待确认 |
| Q-102 | REQ-CELL-001 | 13 串特殊 cell 映射是否保留 | `DataDeal.c:DataLoad_CellVolt` | `series_num == 13 && i == 8` 时读 AFE index 9 | 中 | 可能是硬件布线适配 | 当前 103+309 是否存在 13 串版本？ | 默认保留 | 待确认 |
| Q-103 | REQ-TEMP-001 | ENV2/ENV3 温度通道是否实际未贴 | `DataDeal.c:DataLoad_Temperature` | 两个环境温度通道强制 -40 | 中 | 建议改成显式 not-fitted | ENV2/ENV3 是否永远不用？ | 若不用，删采样路径但保持协议占位 | 待确认 |
| Q-104 | REQ-CURRENT-001 | `__VIRTURE_CURRENT__` 是否保留 | `conf.h`、`DataDeal.c` | 调试开关可覆盖电流 | 中高 | 量产建议关闭或隔离 | 现场是否还需要虚拟电流调试？ | 默认迁移到 Factory/Test | 待确认 |
| Q-105 | REQ-SCI-001 | SCI2/SCI3 是否保留 | `Project_Config.h` role=0，`Sci_Upper.c` 条件路径 | 当前默认禁用 | 中 | 可裁剪 | 当前硬件/客户是否只用 SCI1？ | 若只用 SCI1，删除 SCI2/3 协议路径 | 待确认 |
| Q-106 | REQ-MON-001 | System Monitor 保留位是否必须保持协议可见 | `System_Monitor.h` heat/cool/client/screen/wifi/bluetooth/app 等 | 多数可能为历史预留 | 中高 | 不确认不删位 | 上位机是否显示这些状态位？ | 协议位保留，内部逻辑裁剪 | 待确认 |
| Q-107 | REQ-LP-001 | 低压强制深睡策略是否已验证 | `rtc_sleep.c` `2800mV/60s` 标注待测试 | 满足低压和小充电电流后请求 deep sleep | 高 | 必须实机确认 | 是否保留 2800mV/60s 策略？ | 默认保留并写测试计划 | 待确认 |
| Q-108 | REQ-CAN-001 | CAN RTC 唤醒广播周期是否固定 1s | `Project_Config.h`、`Can_HDX.c` | active/idle 根据 bus 状态决定 | 高 | 客户可见行为 | 客户是否要求休眠中仍按当前周期发 CAN？ | 默认保留 | 待确认 |
| Q-109 | REQ-AGING-001 | 老化是否量产默认启用 | `PROJECT_CFG_FACTORY_AGING_ENABLE=1`、`FactoryAging.c` | 启动后自动按存储状态运行 | 高 | 当前需求要求 UI 可见，默认保留 | 后续量产是否仍默认启用老化？ | 默认保留 | 待确认 |
| Q-110 | REQ-SN-001 | SN/硬件/软件版本来源是否固定 `0xC002` | `Sci_Upper.h`、`ProductionID.c` | 默认值初始化，协议读取 | 高 | 必须保留 | 是否后续要支持写入 Flash 持久化？ | 当前保留读取，写入另立任务 | 待确认 |
| Q-111 | REQ-SOC-001 | SOC 注入式测试是否作为长期测试资产保留 | `SOC.c`、`Project_Config.h` | Release 关闭，Factory/Test 可启用 | 中 | 可迁移到 tools | 是否保留 `0xD300` 测试状态入口？ | 默认保留协议查询，测试固件启用 | 待确认 |
| Q-112 | REQ-FLASH-001 | 升级参数策略是否继续重置 SOC/log/aging | `EEPROM.c:UpgradeParamPolicy_ApplyOnce` | policy version 变化时重置多类数据 | 高 | 会影响现场数据 | 这些 reset flag 是否都是当前量产要求？ | 默认保留，逐项确认后再改 | 待确认 |

## 4. 推荐第一轮确认决策

建议第一轮只确认低风险净删减，不进入高风险业务逻辑：

| 优先级 | 决策项 | 建议 |
|---|---|---|
| P1 | 删除 `App_WarnCtrl` 残留 | 同意删除。 |
| P1 | 删除 `test_Autocurrent_cycle()` | 同意删除。 |
| P1 | 删除无效 `#if 0` 旧实现 | 同意删除，但不改函数当前行为。 |
| P2 | 删除 `ShortFunc.c/h` | 需要确认负载移除短路恢复是否不用。 |
| P2 | 删除伪 EEPROM API | 需要确认外部工程不依赖函数名。 |
| P2 | Flash64K 测试移出主业务工程 | 建议迁移，不直接丢失测试代码。 |
| P3 | 裁剪 SCI2/SCI3 | 需要确认硬件和客户协议。 |
| P3 | 关闭/隔离虚拟电流 | 需要确认现场调试习惯。 |

## 5. 验证缺口

当前只读分析无法证明以下内容：

- Keil Release 是否仍可编译。
- 当前二进制大小和 map 是否符合预期。
- AFE 实机读写是否稳定。
- CAN 周期帧和 App 命令是否与客户文档完全一致。
- Modbus 上位机 UI 是否覆盖所有寄存器。
- 低功耗深睡、RTC hiccup sleep、按键/充电器/电流/AFE fault 唤醒是否全部正常。
- SOC 在充电、放电、静置、休眠补偿场景是否符合期望。
- 老化剩余时间在原上位机界面是否仍单独可见。

这些验证缺口应在进入源码重构前补齐，或至少作为每个迁移阶段的验收条件。

## 6. 下一步建议

下一步不建议直接大规模搬目录。建议按以下顺序推进：

1. 用户确认第一批删除项。
2. 建立编译和实机 baseline。
3. 执行第一批死代码净删减。
4. 先拆 `system_time` 和 `storage` 这类边界清晰模块。
5. 再拆 AFE、BMS core、MOS、低功耗、CAN 等高风险模块。

在用户确认前，本仓库源码应保持不变。
