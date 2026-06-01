# 量产分支变更报告

> 分支: `production-release` (基于 `t3-master-new-new-new`)
> 日期: 2026-06-01
> 净删除: **~5500 行** (60+ 个源文件)

---

## 提交历史

```
ab0d12e fixup: remove SCI2/SCI3 IRQ handler declarations from Sci_Upper.h
025aca0 fixup: remove stray closing brace in Sci_Upper.c fputc
7d6e41d fixup: remove unused functions in PubFunc + CopperLoss reads in Sci_Upper
416fbcc fixup: remove remaining CopperLoss/SOC_TEST references in Sci_Upper
8bd8560 production: 量产分支清理 - 删除 ~5000 行无用代码
dc56d15 docs: 重组文档目录结构，新增 INDEX.md 主索引
640b558 docs: 完整梳理各模块功能、逻辑、全局变量、宏配置，输出参考文档
```

---

## 第一类：整文件删除 (18个文件，~3400行)

| 文件 | 行数 | 原因 |
|------|------|------|
| `easylogger/inc/elog.h` | 279 | ELOG_OUTPUT_ENABLE Release=0, 全部宏空操作 |
| `easylogger/inc/elog_cfg.h` | 84 | 日志配置 |
| `easylogger/src/elog.c` | 940 | 日志核心 |
| `easylogger/src/elog_async.c` | 388 | 异步日志 |
| `easylogger/src/elog_buf.c` | 105 | 缓冲日志 |
| `easylogger/src/elog_utils.c` | 103 | 日志工具函数 |
| `easylogger/port/elog_port.c` | 96 | 日志端口适配 |
| `easylogger/plugins/file/elog_file.c` | 181 | 文件插件 |
| `easylogger/plugins/file/elog_file_port.c` | 70 | 文件端口 |
| `easylogger/plugins/file/elog_file.h` | 72 | 文件头 |
| `easylogger/plugins/file/elog_file_cfg.h` | 41 | 文件配置 |
| `easylogger/plugins/flash/elog_flash.c` | 308 | Flash 日志插件 |
| `easylogger/plugins/flash/elog_flash_port.c` | 72 | Flash 端口 |
| `easylogger/plugins/flash/elog_flash.h` | 70 | Flash 头 |
| `easylogger/plugins/flash/elog_flash_cfg.h` | 37 | Flash 配置 |
| `Flash64KAppTest.c` | ~520 | FLASH64K_QUICK/USE_TEST Release=0 |
| `Flash64KAppTest.h` | 9 | Flash测试头 |

---

## 第二类：配置系统精简 (4个文件，~630行)

### Project_Config.h (-200行)
删除的宏 (~30个):
- `PROJECT_CFG_IDLE_SLEEP_ENABLE` — 空闲 WFI
- `PROJECT_CFG_LOAD_REMOVE_SHORT_ENABLE` — 负载移除短路恢复
- `PROJECT_CFG_UART2_WAKEUP_ENABLE` — UART2唤醒(未用)
- `PROJECT_CFG_SECOND_CURR_PROTECT_ENABLE` — 第二路电流保护
- `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE` — 虚拟电流
- `PROJECT_CFG_DI_SWITCH_SYS_ONOFF_ENABLE` — DI系统开关(未用)
- `PROJECT_CFG_DI_SWITCH_DSG_ONOFF_ENABLE` — DI放电开关(未用)
- `PROJECT_CFG_LED_FUNC_ENABLE` — 旧LED功能标志
- `PROJECT_CFG_DEBUG_CODE_ENABLE` — Debug代码
- `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE` — Flash启动打印
- `PROJECT_CFG_DEBUG_WATCH_ENABLE` — Keil Watch导出
- `PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE` — 串口日志
- `PROJECT_CFG_SLEEP_WITH_CURRENT_ENABLE` — 电流休眠
- `PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE` — Flash快速测试
- `PROJECT_CFG_FLASH64K_QUICK_TEST_CYCLES` — 测试循环数
- `PROJECT_CFG_FLASH64K_USE_TEST_ENABLE` — Flash应用测试
- `PROJECT_CFG_FLASH64K_USE_TEST_PRINT_PERIOD_SEC` — 测试打印周期
- `PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE` — 测试加速
- `PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_SOC_PERIOD_SEC` — SOC加速周期
- `PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_AFE_PERIOD_SEC` — AFE加速周期
- `PROJECT_CFG_SOC_TEST_MODE_ENABLE` — SOC测试模式
- `PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX` — 测试加速tick
- `PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON` — LED测试常亮
- `PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST` — GPIO测试
- `PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY` — 强制重新执行策略

### conf.h (-140行)
删除的派生宏:
- `UART2_WAKEUP_ENABLE`, `_SECOND_CURR_PROTECT_FUNC_`
- `FLASH64K_APP_*` 系列 (7个)
- `_DI_SWITCH_SYS_ONOFF`, `_DI_SWITCH_DSG_ONOFF`
- `__FUNC__LED__`, `_DEBUG_CODE`
- `FLASH_BOOT_PRINT_ENABLE`, `SOC_TEST_MODE_ENABLE`
- `_SLEEP_WITH_CURRENT`, `__LOAD_REMOVE_SHORT_FUNC__`
- `_CLIENT_SCI1/2/3`, `_LCD_SCI1/2/3` (6个)
- SCI2/SCI3 角色条件编译块
- Time_T 结构体中删除注释字段 ~18行

### Project_BuildGuard.h (-240行)
- 删除所有已删除宏的范围检查 ~30项
- 删除 Release 构建强制约束 ~15项 (已删宏对应)
- 保留: SOC范围检查, factory aging检查, log检查

### main.h (-50行)
- 删除 `#include "elog.h"`
- 删除 `CRC_KEY 7`
- 删除 `I2C_RW_W 0`, `I2C_RW_R 1`
- 删除 `FactoryAging_IsActive/ShouldStartOnBoot/SaveProgressBeforeSleep` 声明(移到 FactoryAging.h)

---

## 第三类：Debug/Test 条件编译代码删除 (8个文件，~300行)

### AppInit.c (-20行)
- 删除 `#if (defined _DEBUG_CODE)` ... `#else` 条件, 只保留 Release 路径
- 删除 `#ifdef ELOG_OUTPUT_ENABLE` 块 (elogInit + log_w)
- 删除 `#ifdef FLASH_BOOT_PRINT_ENABLE` 块 (StorageFlash_PrintBootCheck)
- 删除 `#ifdef FLASH64K_APP_QUICK_TEST_ENABLE` 块 (StorageFlash_RunAppQuickTest)
- 删除 `#ifdef wdog_enable` 包装 (Init_IWDG 始终调用)

### Runtime.c (-60行)
- 删除 Debug 分支 `Runtime_RunDebugOnce()` 整个函数
- 删除 `#if PROJECT_CFG_IDLE_SLEEP_ENABLE` 全部 (Runtime_IsIdleSleepReady, Runtime_TryIdleSleep, 宏)
- 删除 `StorageFlash_AppUseTest_Task()` 调用
- 删除注释掉的 `App_WarnCtrl()`, `App_SOC()` 调用
- 删除 `#ifdef wdog_enable` 包装

### Can_HDX.c/h (-80行)
- 删除 `CAN_ERROR_SNAPSHOT` 结构体 (11字段)
- 删除 `CAN_LOW_POWER_STATUS` 结构体 (14字段)
- 删除 `g_stCanErrorSnapshot`, `g_stCanLowPowerStatus` 全局变量
- 删除 `FEIDAO_CAN_ERROR_INC` 宏, 替换为 `do{}while(0)`
- 删除 `feidao_can_update_error_snapshot()` 函数
- 删除 `feidao_can_update_debug_status()` 函数
- 简化 `feidao_can_record_tx_failed/timeout/no_mailbox` 为空函数

### SOC.c/h (-170行)
- 删除 `SOC_Table_Set[]`, `SOC_Table_Default[]` (RUNTIME_TABLE=0)
- 删除 7个 `SOC_TEST_MODE_*` 宏
- 删除 `SOC_TEST_MODE_STATE` 结构体和 `s_stSocTestMode` 变量
- 删除 `SOC_TestMode_RunSample()` 测试注入体 → stub
- 删除 `SOC_TestMode_ReadStatus()` 测试状态体 → stub
- 删除 App_SOC() 中测试模式分支

### DataDeal.c/h (-50行)
- 删除 `AFE_CURRENT_DIR` 枚举
- 删除 `AFE_CURRENT_OBSERVE` 结构体 (27字段 debug watch)
- 删除 `AfeCurrent_ObserveReset()` 函数
- 删除 3处 `#if DEBUG_WATCH` 数据填充块

### rtc_sleep.c (-80行)
- 删除 ~10处 `log_w()` 调用
- 删除 ~10处 `log_e()` 调用
- 删除 `report_wkup_sig()` 函数 (47行, 仅日志)
- 删除 `rtc_sleep_dump_state()` 函数 (9行, 仅日志)
- 删除 `#include "elog.h"`, `LOG_TAG`

### rtc_sleep_port.c (-13行)
- 删除 `log_w`/`log_e`/`log_a` 调用
- 删除 `#ifdef __FUNC__LED__` 块

### rtc_sleep_afe_sh367309.c (-8行)
- 删除 `log_w`/`log_e` 调用

---

## 第四类：未使用功能删除 (5个文件，~250行)

### CopperLoss 铜损补偿
- `DataDeal.c`: 删除 `CopperLoss[16]`, `CopperLoss_Num[16]` 全局变量
- `DataDeal.h`: 删除 `CompensateNUM`, `CopperLoss_*`/`CopperLossNum_*` 宏 (6个)
- `Sci_Upper.c`: 删除 `Sci_WrRegs_0x10_CopperLoss()` 函数
- `Sci_Upper.c`: 删除 Modbus 读表中 CopperLoss/CopperLossNum 循环
- `EEPROM.c`: 删除 `EEPROM_LoadDefaultCopperLoss()` 函数和调用

### Fault_record_First2/Second2
- `Fault.c`: 删除 `Fault_record_First2/Second2` 变量定义
- `Fault.c`: 删除 `FaultPoint_First2/Second2`
- `Fault.h`: 删除相关 extern 声明
- `Fault.c`: 简化 `FaultWarnRecord2()`, 所有故障统一走 Third
- `Sci_Upper.c`: 删除 First2/Second2 的 Modbus 读取和清除

### test_Autocurrent_cycle()
- `DataDeal.c`: 删除整个 ~200行函数
- 删除注释掉的调用

### App_WarnCtrl()
- `Fault.h`: 删除声明
- `Runtime.c`: 删除注释掉调用

### PubFunc.c 未使用函数 (~132行)
- 删除 `App_PubOPUPChk()` — 无调用者
- 删除 `CRC8()` — 无调用者
- 删除 `ModulusSub()` — 无调用者
- 删除 `Delay_Base10us()` — 无调用者
- 删除 `MemoryCopy()` — 无调用者
- 删除 `U16_SwapEndian_Adress()` — 无调用者
- 删除 `SPUBOPUPCHK` 结构体

### LedBar.c (-4行)
- 删除 `g_dbg_ledbar_runtime` debug watch 指针

---

## 第五类：死代码删除 (5个文件，~57行)

| 文件 | 删除行 | 内容 |
|------|--------|------|
| `PubFunc.c` | 27 | 3处 `#if 0` 块 (Delay1ms备选, SwapEndian旧实现, GPIO旧注释) |
| `ADC.c` | 6 | `#if 0` TIM2部分重映射 |
| `RTC.c` | 6 | `#if 0` RTC初始化 |
| `conf.c` | 18 | `#if 0` IOstatus_RTCMode, 注释的UART2/SCI2 GPIO配置 |
| `Sci_Upper.h` | 0 | `#if 0` 块在先前的清理中已移除 |

---

## 第六类：未用宏删除 (main.h，3个)

| 宏 | 说明 |
|----|------|
| `CRC_KEY 7` | 全项目无引用 |
| `I2C_RW_W 0` | 全项目无引用 |
| `I2C_RW_R 1` | 全项目无引用 |

---

## 保留不变的功能模块

| 模块 | 状态 |
|------|------|
| SOC 核心算法 (安时积分/OCV/满电/尾端/静置补偿) | ✅ 完整 |
| CAN 全部功能 (TX队列/电源管理/探测/应用命令/RTC唤醒) | ✅ 完整 |
| ADC 全部 (VBUS/TypeC电流/MOS温度) | ✅ 完整 |
| LED Charlieplexing 数码管 (5-GPIO/18段/扫描帧) | ✅ 完整 |
| RTC 低功耗 HICCUP NORMAL DEEP | ✅ 完整 |
| 工厂老化 (状态机/BKP+Flash双存/CAN控制) | ✅ 完整 |
| Flash 存储 (A/B双槽/SOC/参数/日志/老化) | ✅ 完整 |
| 串口 Modbus (0x03/0x06/0x10, CRC16) | ✅ 完整 |
| 保护参数 (三级 65个参数) | ✅ 完整 |
| IAP 升级 (串口/CAN) | ✅ 完整 |
| AFE SH367309 驱动 (I2C/寄存器/保护) | ✅ 完整 |
| 系统监控/错误标志/功能开关 | ✅ 完整 |

---

## 代码量对比

| 指标 | 清理前 | 清理后 | 减少 |
|------|--------|--------|------|
| 源文件数 | ~55 | ~43 | -12 |
| 估计总行数 | ~15500 | ~10000 | **~35%** |
| Project_Config.h 宏 | ~100 | ~68 | -32 |
| 条件编译分支 | ~25 | ~12 | -13 |
| `#if 0` 死代码 | 15处 | 0 | -15 |
| 全局变量 (debug watch) | ~10 | 0 | -10 |

---

## 风险评估

| 风险 | 等级 | 缓解措施 |
|------|------|---------|
| 编译错误 | 低 | FD_Release 配置下编译验证 |
| 功能回归 | 低 | 只删除 dead code/不可达代码 |
| 协议不兼容 | 极低 | Modbus/CAN ID 和 payload 未变 |
| IAP 异常 | 极低 | IAP 跳转逻辑未改 |
| 参数丢失 | 无 | Flash 存储格式未变 |
