# 应用层宏审查与配置收敛建议

文档状态：已按源码部分验证
生成日期：2026-06-04
范围：`103 + 309/Project/Source` 应用层 `.c/.h`，不包含 STM32 标准外设库和用户未提交的 `todo.md`
主要参考源码：`Project_Config.h`、`conf.h`、`Project_BuildGuard.h`、`SocEnhance.c/h`、`LedBar.c/h`、`Sci_Upper.c/h`、`DataDeal.h`、`Fault.h`、`SH367309_Func.h`、`Flash.h`、`FactoryAging.c/h`、`LogRecord.c`、`rtc_sleep.c/h`、`Can_HDX.c/h`

## 1. 结论

当前应用层宏的主要问题不是单个宏值错误，而是层次混乱：

- `Project_Config.h` 同时承载产品配置、调试开关、模块调参、升级策略和历史预留开关。
- `conf.h` 把 `PROJECT_CFG_*` 再派生成旧宏，如 `BAT_TYPE`、`AFE_TYPE`、`TERNARYLI`、`_IAP`、`__FUNC_RTC__`、`__VIRTURE_CURRENT__`、`_COMMOM_UPPER_SCI1`，读代码需要多层跳转。
- 许多模块内部常量只是算法、寄存器、地址或表默认值，不应该继续上升为全局配置宏。
- 部分宏只定义不被有效消费，或者只是默认关闭的历史测试/预留开关，应该删除或移出当前配置视图。

建议原则：

- `Project_Config.h` 只保留产品差异、现场体验调参、必须编译期隔离的安全/调试开关。
- 模块内部固定常量留在模块 `.c/.h`，不要做成 `PROJECT_CFG_*`。
- 纯转发宏逐步减少，尤其是 `PROJECT_CFG_* -> conf.h old alias -> module alias` 这种三层转发。
- 默认关闭且当前无消费者的宏直接删除；默认关闭但仍有测试价值的宏移到明确的测试构建入口，不放在量产配置主视图。
- 改名不和删宏混在同一批做；先删无效宏，再减少转发层，最后处理命名。

## 2. 扫描摘要

本次扫描到：

| 项目 | 数量/事实 |
|---|---:|
| 含 `#define` 的应用层文件 | 58 |
| 应用层 `#define` 总数 | 约 917 |
| `Project_Config.h` 当前 `PROJECT_CFG_*` | 57 |
| 高密度宏文件 | `Fault.h` 97、`SH367309_Func.h` 81、`Project_Config.h` 63、`SocEnhance.c` 53、`I2C_AFE1.h` 53、`conf_gpio.h` 53 |
| Keil target 覆盖宏 | `FD_Debug` 定义 `PROJECT_CFG_BUILD_PROFILE=1`、`PROJECT_CFG_DEBUG_WATCH_ENABLE=1`、`_DEBUG_` |
| 当前未提交用户文件 | `103 + 309/Project/Source/todo.md`，本轮不引用其内容作为源码事实 |

## 3. 宏分层规则

| 层级 | 应该放什么 | 不应该放什么 | 示例 |
|---|---|---|---|
| 产品配置层 | 电池类型、化学体系、AFE 类型、容量/显示体验、量产可调策略 | 模块私有常量、寄存器 bit 拼装、历史空开关 | `PROJECT_CFG_BAT_CHEMISTRY`、`PROJECT_CFG_SOC_REST_OCV_SECONDS` |
| 构建/调试层 | Release/Debug/Test 隔离，Keil target override | 业务算法参数 | `PROJECT_CFG_DEBUG_WATCH_ENABLE`、`_DEBUG_` |
| 模块内部常量 | 时间换算、状态枚举、bit mask、magic、寄存器地址 | 跨模块产品开关 | `SOC_TICK_MS`、`FACTORY_AGING_BKP_MAGIC` |
| 硬件映射层 | GPIO、Flash 地址、AFE 寄存器地址、协议地址 | 普通算法调参 | `GPIO_MCU_WK`、`FLASH_ADDR_APP_START`、`MTP_CELL1` |
| 旧兼容别名层 | 仅作为过渡兼容 | 新增宏、长期配置入口 | `BAT_TYPE`、`AFE_TYPE`、`TERNARYLI` |

## 4. `Project_Config.h` 当前配置宏审查

### 4.1 建议保留在配置层

这些宏确实体现产品差异、协议/量产策略或现场体验，暂时不建议删。

| 宏 | 当前值 | 用途注释 | 建议 |
|---|---:|---|---|
| `PROJECT_CFG_BAT_TYPE` | 1 | 主/从板类型，影响 CAN/数据默认值分支 | 保留，但后续可用更清晰枚举名替代 `BAT_TYPE` |
| `PROJECT_CFG_BAT_CHEMISTRY` | 0 | 三元/铁锂，影响 SOC 表、保护默认值、上位机读表 | 保留 |
| `PROJECT_CFG_HOST_WRITE_ENABLE` | 1 | 上位机写寄存器入口开关 | 保留，Release 当前也需要为 1 |
| `PROJECT_CFG_AFE_TYPE` | 1 | AFE 型号选择，影响 `ShortFunc.c`、`Sci_Upper.c` 分支 | 保留，后续减少 `AFE_TYPE` 别名 |
| `PROJECT_CFG_FACTORY_AGING_ENABLE` | 1 | 工厂老化模块编译/运行入口 | 保留 |
| `PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS` | 259200 | 老化默认 3 天 | 保留 |
| `PROJECT_CFG_DEBUG_MONITOR_ENABLE` | 0 | `g_dbg` 系统监控导出 | 已与 Debug target 统一，`FD_Debug` 显式打开，Release 默认关闭 |
| `PROJECT_CFG_IRQ_DEBUG_ENABLE` | 0 | 中断计数调试 | 已与 Debug target 统一，`FD_Debug` 显式打开，Release 默认关闭 |
| `PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE` | 0 | IRQ 事件 ring | 默认关闭，高频 IRQ 事件环仅按需临时打开 |
| `PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` | 3600 | Flash 日志重复事件抑制 | 保留 |
| `PROJECT_CFG_SOC_*` 核心体验参数 | 多个 | SOC 满电、静置、tail、显示平滑、自耗 | 保留，但不要继续扩展无必要宏 |
| `PROJECT_CFG_LEDBAR_SLEEP_ENABLE` | 1 | 休眠 LED 行为 | 保留 |
| `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS` | 500 | SOC 显示时长 | 保留，可考虑下沉到 LedBar 模块常量 |
| `PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS` | 1000 | 唤醒显示时长 | 保留，可考虑下沉到 LedBar 模块常量 |
| `PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE` | 1 | 升级参数策略总开关 | 保留 |
| `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` | 0x0603 | 升级策略版本 | 保留 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_*` | 0/1 | 升级时是否重置对应参数区 | 保留，但只保留真实存在的数据区 |

### 4.2 可以优先删除或收口的配置宏

这些宏不是安全边界，或者当前没有有效消费者，适合下一批处理。

| 宏 | 当前值 | 证据 | 建议 |
|---|---:|---|---|
| `PROJECT_CFG_SCI2_ROLE` | 已删除 | 只在 `Project_Config.h` 定义，未在 `conf.h` 派生 `_COMMOM_UPPER_SCI2` | 已从配置层删除；如需 SCI2，未来用明确目标分支实现 |
| `PROJECT_CFG_SCI3_ROLE` | 已删除 | 同上，未派生 `_COMMOM_UPPER_SCI3` | 已从配置层删除 |
| `PROJECT_CFG_RTC_ENABLE` | 1 | 只派生 `__FUNC_RTC__`；当前产品需要 RTC | 改为固定启用，删除配置开关，直接保留 RTC 代码 |
| `PROJECT_CFG_IAP_ENABLE` | 1 | 只派生 `_IAP`；App/IAP 是当前固定能力 | 改为固定启用，删除配置开关或改为构建层安全检查 |
| `PROJECT_CFG_UART1_WAKEUP_ENABLE` | 1 | 只派生 `UART1_WAKEUP_ENABLE` | 若硬件固定，删除配置开关，保留宏或直接改源码判断 |
| `PROJECT_CFG_RS485_WAKEUP_ENABLE` | 1 | 只派生 `RS485_WAKEUP_ENABLE`，当前引用少 | 若硬件固定，删除配置开关 |
| `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE` | 1 | 派生 `__VIRTURE_CURRENT__`，拼写错误且只做旧路径开关 | 需先确认虚拟电流是否量产真实需求；若固定启用，删除开关并重命名 |
| `PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE` | 1 | 派生 `_DI_SWITCH_longKEY_ONOFF` | 若按键开关为固定硬件能力，删除配置开关 |
| `PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS` | 已删除 | 原先只被 `LedBar.c` 转成本地宏 | 本轮连同 LedBar 内部 MCU_WK 软件滤波计数一起删除 |
| `PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS` | 已删除 | 同上 | 本轮连同 LedBar 内部 MCU_WK 软件滤波计数一起删除 |
| `PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS` | 已删除 | 只被 `LedBar.c` 转发 | 已下沉为 `LedBar.c` 内部常量 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_AFE` 等 reset 开关 | 1 | 只在 `UpgradeParamPolicy.h` 纯转发 | 保留策略，但可把纯转发宏去掉，业务直接读 `PROJECT_CFG_*` |

### 4.3 暂不从配置层删除，但应改写/注释

| 宏 | 问题 | 建议 |
|---|---|---|
| `PROJECT_CFG_FD_YEAR/MONTH/DAY/VERSION` | 只是派生 `FD_YEAR/MONTH/DAY/VERSION`，配置入口仍有价值 | 保留配置，逐步删除 `conf.h` 中纯转发别名 |
| `PROJECT_CFG_LEVEL_CURR` | 目前 `LEVEL_CURR` 使用不明显，但额定电流是产品参数 | 保留，后续确认是否仍参与保护/显示 |
| `PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG` | 初始化标志是存储兼容边界 | 保留，补充注释说明改动会触发参数初始化 |
| `PROJECT_CFG_WDOG_ENABLE` | 当前量产必须开；关闭只用于调试 | 保留为构建/调试层，Release guard 强制为 1 |
| `PROJECT_CFG_DEBUG_MONITOR_ENABLE` | 已从 Release 默认打开改为默认 0 | `FD_Debug` 显式打开，并通过 `g_dbg_watch.system.snapshot` 观察 |
| `PROJECT_CFG_IRQ_DEBUG_*` | 已从 Release 默认打开改为默认 0 | `FD_Debug` 显式打开轻量计数，事件环保持 0 |

## 5. `conf.h` 派生宏审查

`conf.h` 是当前阅读困难的核心来源。它把新配置宏变成旧命名，导致代码里看不到真实配置来源。

| 派生宏 | 来源 | 当前用途 | 建议 |
|---|---|---|---|
| `BAT_TYPE` | `PROJECT_CFG_BAT_TYPE` | `CanFeidaoFrames.c`、`DataDeal.h` 主/从分支 | 后续直接使用 `PROJECT_CFG_BAT_TYPE` 或新枚举名 |
| `TERNARYLI` / `LIFEPO` | `PROJECT_CFG_BAT_CHEMISTRY` | `Fault.h`、`DataDeal.h`、`SH367309_*` | 后续统一改为 `PROJECT_CFG_BAT_CHEMISTRY == ...` |
| `AFE_TYPE` | `PROJECT_CFG_AFE_TYPE` | `ShortFunc.c`、`Sci_Upper.c` | 后续直接用 `PROJECT_CFG_AFE_TYPE` |
| `wdog_enable` | `PROJECT_CFG_WDOG_ENABLE` | `RTC.c` watchdog 喂狗 | 改名为 `PROJECT_CFG_WDOG_ENABLE` 条件 |
| `__FUNC_RTC__` | `PROJECT_CFG_RTC_ENABLE` | `System_Init.c` | 删除配置后可删该派生 |
| `_IAP` | `PROJECT_CFG_IAP_ENABLE` | `Flash.c` | IAP 固定启用时删除派生 |
| `__VIRTURE_CURRENT__` | `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE` | `DataDeal.c` | 拼写错误，优先改名或删除开关 |
| `_DI_SWITCH_longKEY_ONOFF` | `PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE` | 主开关路径 | 改为直接条件或固定启用 |
| `_COMMOM_UPPER_SCI1` | `PROJECT_CFG_SCI1_ROLE` | `Sci_Upper.c` | 拼写错误，SCI1 固定启用时删除派生 |
| `_COMMOM_UPPER_SCI2/3` | 无当前派生 | `Sci_Upper.c` 中有条件路径，但配置未真正接入 | 删除配置项前先确认是否永久不支持 SCI2/3 |
| `FD_YEAR/MONTH/DAY/VERSION` | `PROJECT_CFG_*` | 版本信息 | 可后续直接引用 `PROJECT_CFG_*` |

## 6. 各模块宏整理

### 6.1 SOC 模块

主要文件：`SocEnhance.c/h`、`SOC.c/h`

| 宏组 | 示例 | 类型 | 建议 |
|---|---|---|---|
| SOC 全局配置转发 | `SOC_FULL_SECONDS`、`SOC_REST_OCV_SECONDS` | `PROJECT_CFG_SOC_*` 转本地别名 | 可以保留，目的是在算法里缩短名字；但不要再新增无必要 `PROJECT_CFG_*` |
| 算法固定常量 | `SOC_TICK_MS`、`SOC_TICKS_PER_SECOND`、`SOC_EMPTY_MV` | 模块内部常量 | 保留在 `SocEnhance.c`，不做全局配置 |
| 状态枚举宏 | `SOC_MODE_RELAX/CHG/DSG`、`SOC_EMPTY_BAND_*` | 模块内部状态值 | 可后续改成 `enum`，但不是配置宏 |
| 调试开关 | `PROJECT_CFG_DEBUG_WATCH_ENABLE`、`SOC_DEBUG_USED` | Debug watch 导出 | SOC 测试期暂保留 |

已处理事实：runtime table、manual OCV、mid-tail、empty-tail soft tuning、校准故障阻断预留宏已删除。

### 6.2 LedBar 模块

主要文件：`LedBar.c/h`

| 宏组 | 示例 | 类型 | 建议 |
|---|---|---|---|
| 硬件引脚映射 | `LEDBAR_GPIO_P1`、`LEDBAR_PIN_P1` | 板级映射 | 保留，不做产品配置宏 |
| 显示时长配置 | `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS`、`PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS` | 体验参数 | 可保留；若长期不调，可下沉为模块常量 |
| 扫描/滤波参数 | `PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS`、`PROJECT_CFG_LEDBAR_MCU_WK_*` | 模块内部时序 | 优先下沉为 `LedBar.c` 常量 |
| 调试测试宏 | `PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON` | 测试开关 | 已删除 `LedBar.h` 旧引用和 `LedBar.c` 常亮分支 |
| 内部 bit/magic | `LEDBAR_SLEEP_SOC_MAGIC`、`LEDBAR_DIGIT_BIT_A` | 模块内部常量 | 保留，不上升配置层 |

### 6.3 通信模块

主要文件：`Sci_Upper.c/h`、`Can_HDX.c/h`、`CanFeidaoFrames.c/h`

| 宏组 | 示例 | 类型 | 建议 |
|---|---|---|---|
| Modbus 地址/长度 | `RS485_ADDR_*`、`RS485_CMD_ADDR_*` | 协议边界 | 必须保留，不随意改名/删 |
| Host 写权限 | `PROJECT_CFG_HOST_WRITE_ENABLE` | 产品/安全策略 | 保留 |
| SCI 口条件编译 | `_COMMOM_UPPER_SCI1/2/3` | 历史串口角色 | SCI1 固定启用时去条件化；SCI2/3 若不用，删除死路径 |
| CAN 命令 | `FEIDAO_CAN_APP_CMD_ENTER_IAP` | 协议命令 | 保留 |
| Debug UART | `debug_uart` | 本地调试宏 | 后续改成静态函数或明确 `SCI_DEBUG_UART` 常量 |

### 6.4 AFE/I2C 模块

主要文件：`SH367309_Func.h`、`SH367309_DataDeal.h`、`I2C_AFE1.h`

| 宏组 | 示例 | 类型 | 建议 |
|---|---|---|---|
| AFE 寄存器地址 | `MTP_CELL1`、`MTP_BSTATUS1` | 芯片寄存器映射 | 保留在 AFE 驱动，不做项目配置 |
| AFE bit 拼装 | `BIT_ENMOS`、`BYTE_00H_SCONF1` | 芯片配置字 | 保留，但建议补中文注释/表格化，不上升配置层 |
| 保护默认值 | `VAL_CELL_OVP`、`AFE_COV_recover` | AFE 默认参数 | 保留，后续和 Flash 参数默认值统一梳理 |
| I2C GPIO 操作宏 | `TWI_CLK_HIGH`、`TWI_DAT_LOW` | 低层 IO 操作 | 可以保留，后续如重构 I2C 再改 inline 函数 |

### 6.5 保护/参数模块

主要文件：`Fault.h`、`DataDeal.h`

| 宏组 | 示例 | 类型 | 建议 |
|---|---|---|---|
| 保护默认阈值 | `COV_1`、`CUV_1`、`OCC_1` | 默认参数 | 保留，不要简单删除；后续迁移为结构化默认表更好 |
| filter/recover 宏 | `COV_filter1`、`mos_recover` | 默认参数 | 保留但命名混乱，后续统一大小写 |
| 产品基础参数 | `SNum`、`CS_Res`、`BMS_CAPCITY` | 产品参数 | 可能应该由 `Project_Config.h` 管理，但当前和默认表耦合，先不动 |
| 版本/SN 默认 | `BMS_HARDWARE_VERDION_DEFAULT` | 出厂默认信息 | 保留；拼写 `VERDION` 后续修正需同步所有引用 |
| `OtherElement_*` 表 | 默认/范围表 | 参数默认值 | 保留，后续改为 `static const` 表更清晰 |

### 6.6 Flash/存储模块

主要文件：`Flash.h`、`UpgradeParamPolicy.h`、`EEPROM.h`

| 宏组 | 示例 | 类型 | 建议 |
|---|---|---|---|
| Flash 地址 | `FLASH_ADDR_APP_START`、`FLASH_ADDR_STORAGE_*` | 烧录/存储安全边界 | 必须保留，且文档/脚本同步 |
| 存储 magic | `FLASH_*_VALUE`、`BOOT_FLAG_RESET_VALUE` | 持久化兼容边界 | 保留 |
| 升级策略转发 | `UPGRADE_PARAM_RESET_*` | 纯转发 | 后续可以减少一层，让实现直接读 `PROJECT_CFG_*` |
| EEPROM 地址/长度 | `E2P_*` | 历史/协议兼容 | 先保留，待 EEPROM 旧路径整体梳理 |

### 6.7 低功耗/RTC/启动模块

主要文件：`rtc_sleep.c/h`、`RTC.h`、`System_Init.h`、`LowPowerSleep.c/h`

| 宏组 | 示例 | 类型 | 建议 |
|---|---|---|---|
| 低功耗阈值 | `LOW_POWER_FORCE_DEEP_SLEEP_MV`、`LOW_POWER_FORCE_DEEP_SLEEP_SECONDS` | 产品策略 | 保留在模块内，不做全局配置 |
| RTC 常量 | `RTC_BKP_DATA`、`LSE_START_TIMEOUT`、`SEC_DAY` | RTC 内部常量 | 保留 |
| 时间换算 | `DELAYB10MS_*` | 全局时间单位常量 | 保留，但后续可收敛到一个公共时间头文件 |
| GPIO bitband | `PAout/PAin` 相关宏 | 寄存器级 IO | 保留，除非重构 GPIO 层 |
| `__RTC_SLEEP__` | 旧条件/标识 | 命名不规范 | 后续确认无必要后删除或改名 |

### 6.8 工厂老化/日志/调试模块

主要文件：`FactoryAging.c/h`、`LogRecord.c/h`、`SystemDebug.c/h`、`IrqDebug.c/h`

| 宏组 | 示例 | 类型 | 建议 |
|---|---|---|---|
| 老化状态/间隔 | `FACTORY_AGING_STATE_*`、`FACTORY_AGING_BKP_*` | 模块内部状态和 BKP 映射 | 保留 |
| 老化使能/时长 | `PROJECT_CFG_FACTORY_AGING_*` | 产品/量产策略 | 保留 |
| 日志重复间隔 | `PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` | Flash 寿命保护 | 保留 |
| IRQ debug ring | `IRQ_DEBUG_EVENT_RING_SIZE` | 调试模块内部常量 | 保留 |
| SystemDebug 导出开关 | `PROJECT_CFG_DEBUG_MONITOR_ENABLE` | 调试/观测 | Release 默认关闭，`FD_Debug` 显式打开 |

## 7. 删除/下沉候选清单

### 7.1 第一批：低风险删除候选

这些候选更偏“无消费者或当前配置无效”，适合先做。

| ID | 宏/路径 | 建议动作 | 风险 |
|---|---|---|---|
| MACRO-DEL-01 | `PROJECT_CFG_SCI2_ROLE`、`PROJECT_CFG_SCI3_ROLE` | 已从 `Project_Config.h` 删除 | 低；当前没有有效派生 |
| MACRO-DEL-02 | 旧文档中的 `PROJECT_CFG_SOC_TEST_MODE_ENABLE`、`PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX` | 已从当前宏参考移到历史测试宏说明 | 低；当前源码未定义 |
| MACRO-DEL-03 | 旧文档/脚本期待的 `PROJECT_CFG_FLASH64K_*`、`PROJECT_CFG_DEBUG_CODE_ENABLE` 等不存在宏 | 已从当前宏参考和 `project_check.py` 当前配置检查中移除 | 中；历史文档仍可保留上下文 |
| MACRO-DEL-04 | `PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON` 引用链 | 已删除 `LedBar.h` 引用和 `LedBar.c` 分支 | 中；测试常亮如需恢复应走独立测试构建 |

### 7.2 第二批：下沉为模块内部常量

这些宏不应该作为全局配置暴露，但删除前需要同步模块源码和文档。

| ID | 宏 | 建议动作 | 说明 |
|---|---|---|---|
| MACRO-INTERNAL-01 | `PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS` | 已下沉到 `LedBar.c` | 扫描定时器参数是模块实现细节 |
| MACRO-INTERNAL-02 | `PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS` | 已删除 | MCU_WK 软件滤波计数已按试验要求删除 |
| MACRO-INTERNAL-03 | `PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS` | 已删除 | 同上 |
| MACRO-INTERNAL-04 | `PROJECT_CFG_RTC_ENABLE` | 固定启用 RTC，删除派生 `__FUNC_RTC__` | 当前产品 RTC 是必需能力 |
| MACRO-INTERNAL-05 | `PROJECT_CFG_IAP_ENABLE` | 固定启用 IAP，删除派生 `_IAP` | 需保持烧录/IAP 安全文档 |
| MACRO-INTERNAL-06 | `PROJECT_CFG_UART1_WAKEUP_ENABLE`、`PROJECT_CFG_RS485_WAKEUP_ENABLE` | 若硬件固定，改为模块内部常量或直接保留代码路径 | 需确认唤醒策略 |

### 7.3 第三批：命名和别名收口

这些不建议和功能删除混在一起，应单独小批次改名。

| ID | 当前宏 | 问题 | 建议新写法 |
|---|---|---|---|
| MACRO-NAME-01 | `__VIRTURE_CURRENT__` | 拼写错误、双下划线保留命名风险 | `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE` 或固定路径 |
| MACRO-NAME-02 | `_COMMOM_UPPER_SCI1` | 拼写错误、前导下划线 | `SCI1_HOST_ENABLE` 或直接固定启用 |
| MACRO-NAME-03 | `wdog_enable` | 小写宏 | `PROJECT_CFG_WDOG_ENABLE` |
| MACRO-NAME-04 | `bq76xx_afe`、`sh36xx` | 小写宏 | `AFE_TYPE_BQ76XX`、`AFE_TYPE_SH36XX` |
| MACRO-NAME-05 | `BMS_HARDWARE_VERDION_DEFAULT` | 拼写错误 | `BMS_HARDWARE_VERSION_DEFAULT` |
| MACRO-NAME-06 | `TERNARYLI`、`LIFEPO` | 旧式条件宏 | `PROJECT_CFG_BAT_CHEMISTRY == BAT_CHEMISTRY_*` |

## 8. 不建议删除的宏

以下宏虽然数量多，但承担硬件、协议或存储兼容边界，不应作为“宏太多”直接删除：

- Flash/IAP 地址：`FLASH_ADDR_IAP_START`、`FLASH_ADDR_APP_START`、`FLASH_ADDR_STORAGE_*`。
- Modbus/CAN 地址和命令：`RS485_ADDR_*`、`RS485_CMD_ADDR_*`、`FEIDAO_CAN_APP_CMD_*`。
- GPIO 映射：`GPIO_*`、`PIN_*`、LedBar Charlieplexing 引脚。
- AFE 寄存器地址和 bit 拼装：`MTP_*`、`BYTE_*`、`BIT_*`。
- 保护默认参数：`COV_*`、`CUV_*`、`OCC_*`、`ODC_*`、温度保护默认值。
- 持久化 magic/version：`FLASH_*_VALUE`、`FACTORY_AGING_BKP_*`、`FLASH_STORAGE_SOC_DATA_VERSION_V2`。

这些可以后续整理命名、表格化或改成 `static const`，但不应在本轮作为可删配置宏处理。

## 9. 建议执行顺序

1. 已更新 `docs/reference/macro_config_reference.md`，删除当前源码不存在的历史宏，避免文档继续误导。
2. 已删除 `PROJECT_CFG_SCI2_ROLE`、`PROJECT_CFG_SCI3_ROLE`，同时清理 `project_check.py` 中对它们的检查。
3. 已完成 LedBar 收敛：扫描周期、MCU_WK 防抖值下沉到 `LedBar.c`，显示体验宏保留在配置层。
4. 下一步建议固定 RTC/IAP 路径：确认 RTC/IAP 当前产品永远启用后，删除 `PROJECT_CFG_RTC_ENABLE`、`PROJECT_CFG_IAP_ENABLE` 和旧派生宏。
5. 后续串口角色收敛：SCI1 固定启用，SCI2/SCI3 死路径单独评估删除。
6. 后续命名清理：逐步替换 `__VIRTURE_CURRENT__`、`_COMMOM_UPPER_SCI1`、`wdog_enable`、`bq76xx_afe/sh36xx` 等旧宏。
7. 最后处理保护/AFE/Fault 宏的结构化重写，不能和前几步混做，因为它们直接影响保护默认值。

## 10. 待用户确认

| 问题 | 建议 |
|---|---|
| SCI2/SCI3 死路径是否删除 | 配置宏已删除；下一批可只删 `_COMMOM_UPPER_SCI2/3` 对应死路径 |
| RTC/IAP 是否固定启用 | 若是，删除配置开关，保留功能本体 |
| Virtual current 是否量产真实需要 | 若固定需要，去掉开关并修正拼写；若不需要，删除路径 |
| LedBar 扫描/滤波是否还需要现场调 | 已下沉为模块内部常量；如需现场调参应另走明确测试构建 |
| `PROJECT_CFG_DEBUG_MONITOR_ENABLE` 是否允许 Release 默认开启 | 已决定纳入 Debug target，Release 默认关闭并由 BuildGuard 阻止误开 |
