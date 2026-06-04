# BMS 项目宏配置完整参考

> 基于 Project_Config.h 和 conf.h
> 生成日期: 2026-06-01

---

## 1. 构建配置状态

当前 `Project_Config.h` 不再提供 `PROJECT_CFG_BUILD_PROFILE` 作为应用层配置宏。Keil `FD_Debug` target 仍定义 `PROJECT_CFG_BUILD_PROFILE=1`、`PROJECT_CFG_DEBUG_WATCH_ENABLE=1`、`_DEBUG_`，主要用于调试目标识别和 Keil Watch；当前应用源码没有活动的 SOC 注入式测试模式宏。

量产判断以 Keil target 和检查脚本为准：

| 项目 | 当前事实 |
|----|----|
| `FD_Release` defines | `STM32F10X_MD,USE_STDPERIPH_DRIVER` |
| `FD_Debug` 额外 defines | `PROJECT_CFG_BUILD_PROFILE=1,PROJECT_CFG_DEBUG_WATCH_ENABLE=1,_DEBUG_` |
| 当前 `Project_BuildGuard.h` 职责 | 检查仍在配置层的 SOC、Host write、老化、日志参数范围 |
| 当前 SOC 测试入口 | 未启用；`0xD300` 保留兼容占位 |

## 2. 硬件产品配置

| 宏 | 值 | 派生宏 | 说明 |
|----|-----|--------|------|
| `PROJECT_CFG_BAT_TYPE` | 1 | `BAT_TYPE=1` (SLAVE) | 0=MASTER(20A), 1=SLAVE(40A) |
| `PROJECT_CFG_BAT_CHEMISTRY` | 0 | `TERNARYLI` | 0=三元锂, 1=磷酸铁锂(LIFEPO) |
| `PROJECT_CFG_AFE_TYPE` | 1 | `AFE_TYPE=1` (sh36xx) | 0=bq76xx, 1=sh36xx |
| `PROJECT_CFG_LEVEL_CURR` | 2 | `LEVEL_CURR=2` (150A) | 0=80A,1=100A,2=150A,3=200A,4=250A |
| `PROJECT_CFG_FD_YEAR` | 26 | `FD_YEAR=26` | 固件年份 |
| `PROJECT_CFG_FD_MONTH` | 5 | `FD_MONTH=5` | 固件月份 |
| `PROJECT_CFG_FD_DAY` | 9 | `FD_DAY=9` | 固件日期 |
| `PROJECT_CFG_VERSION` | 5 | `VERSION=5` | 固件版本号 |
| `PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG` | 0x2445 | `EEPROM_VALUE_BEGIN_FLAG=0x2445` | 参数初始化标志 |

## 3. 功能启用开关

| 宏 | 默认值 | 条件编译宏 | 功能 |
|----|--------|-----------|------|
| `PROJECT_CFG_WDOG_ENABLE` | 1 | `wdog_enable` | IWDG 看门狗 |
| `PROJECT_CFG_RTC_ENABLE` | 1 | `__FUNC_RTC__` | RTC 时钟 |
| `PROJECT_CFG_IAP_ENABLE` | 1 | `_IAP` | IAP 固件升级 |
| `PROJECT_CFG_FACTORY_AGING_ENABLE` | 1 | - | 工厂老化模式 |
| `PROJECT_CFG_DEBUG_MONITOR_ENABLE` | 1 | - | 系统调试快照导出 |
| `PROJECT_CFG_IRQ_DEBUG_ENABLE` | 1 | - | IRQ 计数 |
| `PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE` | 1 | - | IRQ 事件 ring |
| `PROJECT_CFG_HOST_WRITE_ENABLE` | 1 | - | 上位机写权限 |
| `PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE` | 1 | - | 升级参数策略 |

说明：`PROJECT_CFG_DEBUG_WATCH_ENABLE` 当前不是 `Project_Config.h` 配置项，只由 Keil `FD_Debug` target 定义为 1。

## 4. 唤醒源配置

| 宏 | 默认值 | 条件编译宏 |
|----|--------|-----------|
| `PROJECT_CFG_UART1_WAKEUP_ENABLE` | 1 | `UART1_WAKEUP_ENABLE` |
| `PROJECT_CFG_RS485_WAKEUP_ENABLE` | 1 | `RS485_WAKEUP_ENABLE` |
| `PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE` | 1 | `_DI_SWITCH_longKEY_ONOFF` |
| `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE` | 1 | `__VIRTURE_CURRENT__` |

## 5. 串口角色配置

| 宏 | 默认值 | 条件编译宏 |
|----|--------|-----------|
| `PROJECT_CFG_SCI1_ROLE` | 1 | `_COMMOM_UPPER_SCI1` (上位机) |

说明：SCI2/SCI3 角色配置已从当前配置层删除；源码仍有 `_COMMOM_UPPER_SCI2/3` 历史条件路径，后续单独清理。

## 6. CAN 配置

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `CAN_ADRESS_STD_ID` | 0x00 | CAN 标准 ID 偏移 |
| `FEIDAO_CAN_TX_QUEUE_SIZE` | 32 | 发送队列大小 |
| `FEIDAO_CAN_APP_CMD_QUEUE_SIZE` | 4 | 应用命令队列 |
| `FEIDAO_CAN_TX_TIMEOUT_TICKS` | 20 | 发送超时 (200ms) |

## 7. SOC 配置

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `SOC_TABLE_SIZE` | 42 | OCV 表大小 |
| `SOC_DEFAULT_STARTUP_PERCENT` | 60 | 默认启动 SOC |
| `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` | 30 | 板子自耗电 mA |
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | 1800 | 静置等待 OCV 时间 (30分钟) |
| `PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS` | 1800 | 长静置下修步长 (30分钟/1%) |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | 1 | 每步校准 % |
| `PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV` | 2000 | 有效最低电压 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV` | 5000 | 有效最高电压 |
| `PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV` | 1000 | 有效最大压差 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS` | 15 | 满电确认时间 (正常) |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS` | 5 | 满电确认时间 (快速) |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT` | 95 | 满电确认最低 SOC |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV` | 80 | 满电确认电压余量 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV` | 120 | 满电确认最大压差 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV` | 30 | 快速确认余量 |
| `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` | 30 | 电压跌落抑制时间 |
| `PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV` | 50 | 跌落允许偏移 |
| `PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV` | 400 | 低压尾部开始 |
| `PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS` | 5 | 正常显示秒/1% |
| `PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS` | 5 | 充电显示秒/1% |
| `PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS` | 1 | 低压显示秒/1% |
| `PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV` | 50 | 低压边界偏移 |
| `PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV` | 50 | 空电快速下降偏移 |

## 8. LED/LedBar 配置

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `LEDBAR_PIN_COUNT` | 5 | Charlieplexing 引脚数 |
| `LEDBAR_ROUTE_COUNT` | 18 | LED 段数 |
| `PROJECT_CFG_LEDBAR_SLEEP_ENABLE` | 1 | 休眠时关 LED |
| `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS` | 500 | SOC 显示 5 秒 |
| `PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS` | 1000 | 唤醒显示 10 秒 |

说明：LedBar 扫描周期和 MCU_WK 滤波已下沉为 `LedBar.c` 内部常量，不再作为全局配置宏。

## 9. 升级参数策略

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` | 0x0603 | 策略版本号 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_AFE` | 1 | 升级复位 AFE 参数 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_PROTECT` | 1 | 升级复位保护参数 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE` | 1 | 升级复位均衡电压 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG` | 1 | 升级复位 SOC 配置 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT` | 1 | 升级复位 SOC 快照 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD` | 1 | 升级清除事件记录 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_FACTORY_AGING_TIME` | 0 | 升级不重置老化时间 |

说明：SOC runtime table 已删除，升级策略不再提供 SOC 表复位开关。

## 10. 历史测试宏

以下宏曾在历史测试/文档中出现，但当前 `Project_Config.h` 未定义，不作为当前配置项：

- `PROJECT_CFG_SOC_TEST_MODE_ENABLE`
- `PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX`
- `PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE`
- `PROJECT_CFG_FLASH64K_USE_TEST_ENABLE`
- `PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE`
- `PROJECT_CFG_DEBUG_CODE_ENABLE`
- `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE`
- `PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE`
- `PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST`
- `PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON`
- `PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY`

## 11. 日志配置

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` | 3600 | 重复事件抑制 (1小时) |

## 12. 工厂老化配置

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS` | 259200 | 老化 3 天 |
| `FACTORY_AGING_DURATION_HOURS_MIN` | 1 | 最小时长 (1小时) |
| `FACTORY_AGING_DURATION_HOURS_MAX` | 168 | 最大时长 (168小时=7天) |
| `FACTORY_AGING_FLASH_SAVE_INTERVAL_SECONDS` | 7200 | Flash 保存间隔 (2小时) |
| `FACTORY_AGING_BKP_SAVE_INTERVAL_10MS` | 100 | 备份域保存间隔 (1秒) |

## 13. ADC 硬件配置

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `AD_Used_amount` | 3 | ADC 通道数 |
| `VBC_DIVIDER_RTOP_KOHM` | 470 | 分压电阻上臂 (kΩ) |
| `VBC_DIVIDER_RBOTTOM_KOHM` | 15 | 分压电阻下臂 (kΩ) |
| `VBC_ADC_VDDA_MV` | 3300 | ADC 参考电压 (mV) |
| `TYPEC_CUR_RSENSE_MOHM` | 10 | TypeC 检流电阻 (mΩ) |
| `TYPEC_CUR_VDDA_MV` | 3300 | TypeC ADC 参考电压 |
| `TYPEC_OUT_VOLTAGE_MV` | 9000 | TypeC 输出电压 |
| `TYPEC_DCDC_EFFICIENCY_PERMILLE` | 900 | DC/DC 效率 90% |
| `AD_CalNum` | 8 | 电压滤波深度 (3bit移位) |
| `AD_CalNum_Cur` | 32 | 电流滤波深度 (5bit移位) |
| `AD_CurZeroDeadband` | 4 | 零电流死区 |

## 14. AFE/I2C 配置

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `AFE_ID` | 0x34 | AFE I2C 地址 |

## 15. Release 构建检查边界

- `tools/project_check.py` 检查 Keil `FD_Release` target 不得定义 `_DEBUG_`、`_DEBUG_CODE`、`FLASH64K_APP_*`、`ELOG_OUTPUT_ENABLE` 等测试/调试符号，也不得把 `PROJECT_CFG_BUILD_PROFILE` 覆盖成非 0。
- `Project_BuildGuard.h` 继续检查当前仍在配置层的 SOC、Host write、老化和日志参数范围；它当前不负责 SOC 测试档位隔离。
- 当前源码未启用 `PROJECT_CFG_SOC_TEST_MODE_ENABLE` 注入式测试入口，`0xD300` 兼容区返回 16 word 0。

## 16. 宏派生关系速查表

| 源宏 (Project_Config.h) | 派生宏 (conf.h) |
|--------------------------|-----------------|
| `PROJECT_CFG_WDOG_ENABLE=1` | `wdog_enable` |
| `PROJECT_CFG_RTC_ENABLE=1` | `__FUNC_RTC__` |
| `PROJECT_CFG_IAP_ENABLE=1` | `_IAP` |
| `PROJECT_CFG_BAT_CHEMISTRY=0` | `TERNARYLI` |
| `PROJECT_CFG_BAT_CHEMISTRY=1` | `LIFEPO` |
| `PROJECT_CFG_SCI1_ROLE=1` | `_COMMOM_UPPER_SCI1` |
| `PROJECT_CFG_UART1_WAKEUP_ENABLE=1` | `UART1_WAKEUP_ENABLE` |
| `PROJECT_CFG_RS485_WAKEUP_ENABLE=1` | `RS485_WAKEUP_ENABLE` |
| `PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE=1` | `_DI_SWITCH_longKEY_ONOFF` |
| `PROJECT_CFG_VIRTUAL_CURRENT_ENABLE=1` | `__VIRTURE_CURRENT__` |
