# 量产出货分支 - 可删除项分析清单

> 日期: 2026-06-01
> 分支: 待创建 (基于 t3-master-new-new-new)
> 目的: 梳理可删除的测试代码、无用宏、死代码、未使用变量，为量产出货分支做准备
> 状态: 待用户确认

---

## 总览

| 类别 | 数量 | 预计删除行数 |
|------|------|-------------|
| 整文件删除 | 2-3个 | ~2500行 |
| 功能宏及代码块 | 12个 | ~500行 |
| 死代码 (#if 0) | 12处 | ~200行 |
| 未使用函数/变量 | 5处 | ~250行 |
| Heat/Cool 残留 | 5个位域 | ~10行 |
| 可简化结构 | 2处 | ~30行 |
| Project_Config.h 可删宏 | ~20个 | ~80行 |
| **合计** | | **~3500-4000行** |

---

## 第一类：整文件删除（功能=0 且 Release 编译检查强制为0）

### 1. Flash64KAppTest.c/h - 后64K Flash 测试模块

**当前状态**: 历史 Flash64K 测试配置宏已不在当前 `Project_Config.h` 中；若仍保留相关测试文件，应通过独立测试构建入口管理，不放入量产配置主视图。

**影响**:
- 删除 `Flash64KAppTest.c` (~500行)
- 删除 `Flash64KAppTest.h` (~20行)
- 删除 AppInit.c 中 `#include "Flash64KAppTest.h"` 和条件编译块
- 删除 Runtime.c 中 `StorageFlash_AppUseTest_Task()` 调用
- 删除 conf.h 中 FLASH64K 相关宏定义 (82-97行)
- 删除 Project_Config.h 中 FLASH64K 相关配置 (8个宏)

### 2. easylogger/ 整个日志框架目录

**原因**:
- 当前 `FD_Release` target 不定义 `ELOG_OUTPUT_ENABLE`，`tools/project_check.py` 将其列为 Release 禁止符号。
- 所有 `log_w(...)`/`log_e(...)` 宏展开为 `((void)0)` - 空操作
- 目录包含: `elog.c`, `elog_async.c`, `elog_buf.c`, `elog_utils.c`, `elog_port.c` 及 flash/file 插件

**影响**:
- 删除 `easylogger/` 整个目录 (~2000行)
- 删除所有源文件中 `log_w(...)` 和 `log_e(...)` 调用行 (~50处)
- 删除 `#include "elog.h"` 和 `#define LOG_TAG` 宏
- 删除 AppInit.c 中 `elogInit()` 调用
- 删除 elog_cfg.h 中 `ELOG_OUTPUT_ENABLE` 宏定义

### 3. PubFunc.c/h - 需审查

**原因**: 大部分函数为辅助工具函数，需逐函数确认是否有调用

**需要保留的**: `Sci_CRC16RTU()` (Sci_Upper.c 使用)
**可能无用的**: 需要逐函数检查调用链

---

## 第二类：功能宏（默认值=0，可在 Release 中删除对应代码块）

### 4. 历史 debug code 宏

**影响范围**:
| 文件 | 行号 | 内容 |
|------|------|------|
| AppInit.c | 12-53 | `#if (defined _DEBUG_CODE)` 分支 - Debug 设备初始化 |
| AppInit.c | 14 | `#else` 分支才是 Release 路径 |
| Runtime.c | 8-14 | `Runtime_RunDebugOnce()` - 只跑 AFE+串口 |
| Runtime.c | 94-98 | 主循环选择 `_DEBUG_CODE` vs 正常路径 |

**操作**: 删除 `#if (defined _DEBUG_CODE)` ... `#else` 条件，保留 Release 路径代码

### 5. PROJECT_CFG_DEBUG_WATCH_ENABLE=0 → Keil Watch 调试导出

**影响范围**:
| 文件 | 内容 |
|------|------|
| Can_HDX.h:7-42 | `CAN_ERROR_SNAPSHOT` 结构体(11字段) + `CAN_LOW_POWER_STATUS` 结构体(14字段) |
| Can_HDX.h:43-44 | 2个全局变量声明 |
| Can_HDX.c:7-12 | 2个全局变量定义 + `FEIDAO_CAN_ERROR_INC` 宏 |
| Can_HDX.c:281-290 | `feidao_can_update_error_snapshot()` 函数体 |
| Can_HDX.c:333-351 | `feidao_can_update_debug_status()` 函数体 |
| DataDeal.h:97-130 | `AFE_CURRENT_OBSERVE` 结构体(27字段) |
| DataDeal.h:251-252 | 全局变量声明 |
| DataDeal.c:46-51 | 调试变量初始化 (~80行) |
| SocEnhance.c:136 | `g_dbg_soc_watch` 指针定义 |
| SocEnhance.c:1016-1760 | 4处调试数据填充 |
| LedBar.c:168-170 | `g_dbg_ledbar_runtime` 指针定义 |

**操作**: 删除所有 `#if PROJECT_CFG_DEBUG_WATCH_ENABLE` 保护的代码块

### 6. SOC 测试模式历史残留

> 2026-06-02 SOC 复核：当前源码未看到活动的 `PROJECT_CFG_SOC_TEST_MODE_ENABLE` 注入式测试状态机；`SOC.c` 仅保留 `#if 0` 包裹的 `SOC_TestMode_RunSample()` / `SOC_TestMode_ReadStatus()` 空壳。`Sci_Upper.c` 中 `0xD300` 兼容区当前填充 16 个 0，保留协议长度，不再调用 SOC 测试状态函数。

**影响范围**:
| 文件 | 行号 | 内容 |
|------|------|------|
| SOC.c | 144-162 | `#if 0` 包裹的 `SOC_TestMode_RunSample()` / `SOC_TestMode_ReadStatus()` 空壳 |
| Sci_Upper.c | 828-829 | `SOC_TEST status padding`，16 word 置 0，用于保持协议长度 |
| Sci_Upper.c | `Sci_WrRegs_0x10_SocTable()` | SOC 表写入入口；runtime table 已删除，写入固定返回错误 |

**操作**: 可作为低风险候选进一步确认是否删除 `SOC.c` 中 `#if 0` 空壳；不要删除 `Sci_Upper.c` 的 padding 或改变协议长度。

### 7. 历史 Flash boot print 宏

**影响**: 仅 AppInit.c 中 `StorageFlash_PrintBootCheck()` 一处调用。删除即可。

---

## 第三类：默认=0 的唤醒/功能开关（从未启用，可清理宏和相关代码）

| # | 宏 | 默认值 | 使用文件 | 说明 |
|---|------|--------|---------|------|
| 8 | `PROJECT_CFG_UART2_WAKEUP_ENABLE` | 0 | conf.c, conf.h | conf.c 中有3处条件编译 (139/146/161行) |
| 9 | `PROJECT_CFG_DI_SWITCH_SYS_ONOFF_ENABLE` | 0 | conf.h:99-101 | `_DI_SWITCH_SYS_ONOFF` 未使用 |
| 10 | `PROJECT_CFG_DI_SWITCH_DSG_ONOFF_ENABLE` | 0 | conf.h:103-105 | `_DI_SWITCH_DSG_ONOFF` 未使用 |
| 11 | `PROJECT_CFG_SECOND_CURR_PROTECT_ENABLE` | 0 | conf.h:73-75 | `_SECOND_CURR_PROTECT_FUNC_` 未使用 |
| 12 | `PROJECT_CFG_SLEEP_WITH_CURRENT_ENABLE` | 0 | conf.h:127-129 | `_SLEEP_WITH_CURRENT` 未使用 |
| 13 | `PROJECT_CFG_LOAD_REMOVE_SHORT_ENABLE` | 0 | conf.h:57-59 | `__LOAD_REMOVE_SHORT_FUNC__` 未使用 |
| 14 | `PROJECT_CFG_IDLE_SLEEP_ENABLE` | 0 | Runtime.c:15-51 | WFI idle sleep 代码块 |
| 15 | `PROJECT_CFG_LED_FUNC_ENABLE` | 0 | conf.h:111-113, conf.c:402, rtc_sleep_port.c:103 | `__FUNC__LED__` |

---

## 第四类：SCI2/SCI3 角色禁用

### 16-18. SCI2/SCI3 及客户端/LCD 角色

**当前状态**: `PROJECT_CFG_SCI2_ROLE`、`PROJECT_CFG_SCI3_ROLE` 已从当前 `Project_Config.h` 删除；`Sci_Upper.c` 仍有 `_COMMOM_UPPER_SCI2/3` 历史条件路径，后续可单独删除死路径。

**操作**: 删除 conf.h 中 138-163行的客户端/LCD角色条件编译

---

## 第五类：死代码（`#if 0` 块，共12处）

| # | 文件 | 行号 | 内容 |
|---|------|------|------|
| 19 | Sci_Upper.h | 130-137 | 注释掉的 RO 地址定义 |
| 20 | Sci_Upper.h | 181-192 | 注释掉的系统功能开关地址 |
| 21 | Sci_Upper.c | 965 | 死代码块 |
| 22 | Sci_Upper.c | 1760 | 死代码块 |
| 23 | Sci_Upper.c | 2017 | 死代码块 |
| 24 | Sci_Upper.c | 2272 | 死代码块 - printf 通过中断 |
| 25 | I2C_AFE1.h | 91-104 | 函数宏替代写法(备选) |
| 26 | I2C_AFE1.c | 108 | 死代码块 |
| 27 | I2C_AFE1.c | 519 | 死代码块 |
| 28 | ADC.c | 129-135 | TIM2 部分重映射注释 |
| 29 | DataDeal.c | 245 | 死代码块 |
| 30 | conf.c | 326 | 死代码块 |
| 31 | PubFunc.c | 254 | 死代码块 |
| 32 | PubFunc.c | 297 | 死代码块 |
| 33 | PubFunc.c | 382 | 死代码块 |
| 34 | RTC.c | 340 | 死代码块 |

---

## 第六类：未使用/仅调试用的函数和变量

### 35. test_Autocurrent_cycle() - DataDeal.c:1040-1237

**原因**: 约200行的测试函数，调用处(DataDeal.c:1239)已被注释掉
**操作**: 删除整个函数

### 36. App_WarnCtrl() - Fault.h 声明，Runtime.c 调用已注释

**原因**: Runtime.c:58 已注释 `/* App_WarnCtrl(); */`
**操作**: 删除 Fault.c 中的实现和 Fault.h 中的声明

### 37. CRC_KEY, I2C_RW_W, I2C_RW_R - main.h:43-45

**原因**: 搜索全项目无任何实际使用
**操作**: 删除这3个宏

### 38. CopperLoss[16] + CopperLoss_Num[16] - 铜损补偿

**原因**: DataDeal.c 中定义，Sci_Upper.c 中有读写协议。**需确认客户是否使用**
**操作**: 若不用，删除变量、读写协议、EEPROM 默认值加载

### 39. Fault_record_First2/Second2/FaultPoint_First2/Second2

**原因**: Fault.c 中记录二级故障，与 Fault_record_Third 功能重叠
**操作**: 若只保留 Third，删除 First2/Second2 相关变量和记录逻辑

---

## 第七类：Heat/Cool 残留（代码审查已确认无用）

### 40. System_Monitor.h - Heat/Cool 位域

| 位置 | 字段 |
|------|------|
| 第166行 | `b1Status_ReservedHeat` |
| 第167行 | `b1Status_ReservedCool` |
| 第174行 | `b1Status_ReservedHeatCloseIO` |
| 第199行 | `b1OnOFF_ReservedHeat` |
| 第200行 | `b1OnOFF_ReservedCool` |

**all**: 这些位域名为 "Reserved" 但占用结构体空间。确认无使用后可清理。

---

## 第八类：注释掉的大段代码

### 41. conf.h 中被注释的 sys_time 字段 (220-240行)

```c
// uint16_t    cov1_cnt;
// uint16_t    cov2_cnt;
// ...
// uint16_t    test_current_cnt;
// uint16_t    test_sci2_err_cnt;
```
约20行注释掉的字段声明。

### 42. conf.c 中 InitWakeUp_NormalMode 注释掉的 UART2 配置 (230-243行)

---

## 第九类：可简化的 Project_Config.h 宏

以下宏在量产中确认不需要后可从 Project_Config.h 删除:

```
PROJECT_CFG_DEBUG_CODE_ENABLE (历史 debug 宏，当前 Project_Config.h 未定义)
PROJECT_CFG_DEBUG_WATCH_ENABLE (仅debug)
PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE (历史 debug serial log 宏，当前 Project_Config.h 未定义)
PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE (历史 boot print 宏，当前 Project_Config.h 未定义)
PROJECT_CFG_FACTORY_AGING_ENABLE (看是否需要保留)
SOC_TEST_MODE_ENABLE 旧宏/旧路径（当前 Project_Config/conf 未见定义，仅 SOC.c 保留 #if 0 空壳）
PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE (历史 Flash 测试宏，当前 Project_Config.h 未定义)
PROJECT_CFG_FLASH64K_USE_TEST_ENABLE (历史 Flash 测试宏，当前 Project_Config.h 未定义)
PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE (历史 Flash 测试宏，当前 Project_Config.h 未定义)
PROJECT_CFG_UART2_WAKEUP_ENABLE (未使用)
PROJECT_CFG_DI_SWITCH_SYS_ONOFF_ENABLE (未使用)
PROJECT_CFG_DI_SWITCH_DSG_ONOFF_ENABLE (未使用)
PROJECT_CFG_SECOND_CURR_PROTECT_ENABLE (未使用)
PROJECT_CFG_SLEEP_WITH_CURRENT_ENABLE (未使用)
PROJECT_CFG_LOAD_REMOVE_SHORT_ENABLE (未使用)
PROJECT_CFG_IDLE_SLEEP_ENABLE (未使用)
PROJECT_CFG_LED_FUNC_ENABLE (未使用, 旧LED标志)
PROJECT_CFG_VIRTUAL_CURRENT_ENABLE (需确认)
PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS (需确认)
```

---

## 预计最终效果

| 指标 | 当前 | 清理后预计 |
|------|------|-----------|
| 源文件数 | ~55个 | ~45个 |
| 代码总行数 | ~15000+ | ~11000-12000 |
| Project_Config.h 宏 | ~100个 | ~70个 |
| 全局变量 | ~80个 | ~60个 |
| 条件编译分支 | ~30个 | ~10个 |

---

## 确认清单（请逐项回复）

请对以下需要确认的项回复 **Y(删除)/N(保留)**:

1. [ ] easylogger 整个目录及所有 `log_w()`/`log_e()` 调用行 → 删除?
2. [ ] Flash64KAppTest.c/h → 删除?
3. [ ] CopperLoss 铜损补偿功能（变量+DATAFLASH读写+Modbus地址）→ 删除?
4. [ ] Fault_record_First2/Second2（二级故障记录）→ 只保留 Third?
5. [ ] `SOC.c` 中 `#if 0` 的 `SOC_TestMode_*` 空壳 → 删除?（不要删除 `Sci_Upper.c` 的 SOC_TEST padding）
6. [ ] DEBUG_WATCH 全部 struct 和全局变量 → 删除?
7. [ ] DEBUG_CODE 分支 → 删除，只保留 Release 路径?
8. [ ] test_Autocurrent_cycle() 函数 → 删除?
9. [ ] App_WarnCtrl() → 删除?
10. [ ] 所有 `#if 0` 死代码块(12处) → 删除?
11. [ ] 8个默认=0的唤醒/功能宏 → 全部删除?
12. [ ] SCI2/SCI3 及 _CLIENT/_LCD 角色宏 → 全部删除?
13. [ ] Heat/Cool 残留位域 → 删除?
14. [ ] 注释掉的 sys_time 字段 → 删除?
15. [ ] FLASH_BOOT_PRINT → 删除?
16. [ ] PROJECT_CFG_IDLE_SLEEP_ENABLE → 删除?
17. [ ] CRC_KEY, I2C_RW_W, I2C_RW_R → 删除?
18. [ ] PubFunc.c/h 逐函数审查（待定）
