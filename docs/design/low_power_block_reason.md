# 低功耗禁止休眠原因位图设计

本文是 BmsLogicAgent 第一阶段只读分析结果。目标是把当前 BMS 业务约束映射为后续可复用低功耗框架的禁止休眠原因位图；本阶段未修改源码。

## 1. 当前阻塞原因现状

当前项目已有一个枚举式阻塞原因，定义在 `103 + 309/Project/Source/rtc_sleep.h:39-48`：

| 当前枚举 | 当前含义 |
| --- | --- |
| `LOW_POWER_RTC_BLOCK_NONE` | 无阻塞 |
| `LOW_POWER_RTC_BLOCK_CURRENT` | 充/放电电流阻塞 |
| `LOW_POWER_RTC_BLOCK_MOS_OFF` | 预留或历史 MOS 阻塞 |
| `LOW_POWER_RTC_BLOCK_MCU_WAKE` | MCU_WAKE 或按键/显示相关唤醒阻塞 |
| `LOW_POWER_RTC_BLOCK_FACTORY_AGING` | 工厂老化运行阻塞 |
| `LOW_POWER_RTC_BLOCK_EXT_COMM` | 外部通信计数变化阻塞 |
| `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` | AFE 状态非空闲阻塞 |

当前枚举只能表达单一原因。后续低功耗框架建议内部使用位图，保留旧 `blockReason` 作为对上位机兼容的摘要字段。

## 2. 建议位图

建议第一版内部使用 `uint32_t` 位图，位定义如下：

| 建议位 | 触发来源 | 当前项目依据 | 处理策略 |
| --- | --- | --- | --- |
| `LP_BLOCK_CHARGE` | 充电器插入、充电电流存在 | `MosStartup_Is5vChargeActive()` 读 `GPIO_CHG_IN`，见 `103 + 309/Project/Source/MosStartup.c:14-17`；`RtcSleep_PortGetChargeCurrentMa()` 返回 `u16Ichg`，见 `rtc_sleep_port.c:26-29` | 禁止普通 Stop；充电输入有效时禁止 deep，除非另有明确硬件策略 |
| `LP_BLOCK_DISCHARGE` | 放电电流存在或负载活动 | `RtcSleep_PortGetDischargeCurrentMa()` 返回 `u16IDischg`，见 `rtc_sleep_port.c:31-34`；当前 `rtc_sleep.c:132-136` 用充/放电电流阻塞 | 禁止普通 Stop，避免带载时关外设和 AFE 状态不同步 |
| `LP_BLOCK_COMM` | 串口、Modbus、CAN 活跃 | `Sci_IsAnyPortBusy()` 见 `Sci_Upper.c:1678-1690`；串口接收递增 `RTC_ExtComCnt` 见 `Sci_Upper.c:1479`；CAN bus active/RTC 服务见 `Can_HDX.c:896-929` | 第一版通信活跃禁止休眠；不做 USART/CAN Stop 唤醒 |
| `LP_BLOCK_KEY` | MCU_WAKE、按键、用户交互 | `RtcSleep_PortIsMcuWakeActive()` 读 `GPIO_MCU_WK`，见 `rtc_sleep_port.c:46-49`；`SleepDeal.c:22-79` 处理按键预览和充电唤醒 | 禁止 idle Stop；低压 deep 可以覆盖，但要保存 SOC 显示 |
| `LP_BLOCK_AFE_BUSY` | AFE BSTATUS 非空、AFE 通信失败、AFE 写状态 | `RtcSleep_AfePortIsSleepBlocked()` 见 `rtc_sleep_afe_sh367309.c:11-27`；`RtcSleep_AfePortUpdateRtcData()` 失败见 `rtc_sleep_afe_sh367309.c:30-42`；`App_SH367309_Monitor()` 检查 `EEPR_WR/PF/WDT/L0V` 见 `SH367309_Func.c:349-383` | 禁止普通 Stop；唤醒后异常回 RUN |
| `LP_BLOCK_FLASH_BUSY` | Flash 擦写/参数保存/日志保存/SOC 快照/老化进度保存 | `EEPROM_SaveRWParametersToFlash()` 见 `EEPROM.c:161-173`；`StorageFlash_SavePair()` 见 `Flash.c:280-342`；`StorageFlash_SaveJournalPair()` 见 `Flash.c:456-565`；`StorageFlash_SaveLogData()` 见 `Flash.c:867-897` | 禁止所有可恢复 sleep，直到同步写完成或失败处理完成 |
| `LP_BLOCK_UPGRADE` | IAP/升级请求待处理 | `Sci_WrRegs_0x10_FlashConnect()` 置 `u8FlashUpdateE2PROM`，见 `Sci_Upper.c:1948-1963`；`App_FlashUpdate()` 处理 `u8FlashUpdateFlag`，关闭 MOS 后复位，见 `Flash.c:1078-1089` | 禁止低功耗，优先走升级流程 |
| `LP_BLOCK_FAULT` | 保护故障、系统故障未处理 | `Fault_ChangeToMCU()` 映射 AFE 故障，见 `SH367309_Func.c:228-305`；`LogRecord.c:192-209` 记录故障 | 禁止普通 Stop；过放/低压转 deep |
| `LP_BLOCK_LED_ACTIVE` | LED/数码管显示窗口、睡眠预览、用户可见状态 | `LowPowerSleep_SaveResetState()` 调 `LedBar_SaveSleepSoc()`，见 `LowPowerSleep.c:12-16`；`LedBar.c:1040-1043` 在待睡眠且 MCU_WAKE 非活跃时保存并熄屏 | 禁止 idle Stop，直到显示窗口结束或显式进入睡眠 |
| `LP_BLOCK_IWDG_UNSAFE` | RTC 周期不满足 IWDG 安全窗口 | 当前源码未统一实现，IwdgAgent 文档已指出 `RTC_GetWakeupPeriodSeconds()` 的 IWDG 裁剪被注释 | 禁止非复位 Stop；除非周期小于 IWDG 最短超时并留恢复余量 |

## 3. 旧枚举到新位图的兼容映射

为了不破坏现有诊断和上位机读取，建议后续新增内部位图，同时保留 `g_stLowPowerRtcStatus.blockReason`。可按优先级把位图折叠成旧枚举：

| 新位图 | 旧摘要字段 |
| --- | --- |
| `LP_BLOCK_CHARGE` 或 `LP_BLOCK_DISCHARGE` | `LOW_POWER_RTC_BLOCK_CURRENT` |
| `LP_BLOCK_KEY` 或 `LP_BLOCK_LED_ACTIVE` | `LOW_POWER_RTC_BLOCK_MCU_WAKE` |
| `LP_BLOCK_COMM` | `LOW_POWER_RTC_BLOCK_EXT_COMM` |
| `LP_BLOCK_AFE_BUSY` 或 `LP_BLOCK_FAULT` | `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` |
| `LP_BLOCK_FLASH_BUSY` | 当前无对应项，可暂映射到 `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` 并新增日志字段 |
| `LP_BLOCK_UPGRADE` | 当前无对应项，建议新增旧枚举值前先只内部使用 |
| `LP_BLOCK_IWDG_UNSAFE` | 当前无对应项，建议新增旧枚举值前先只内部使用 |

折叠策略必须只用于兼容展示，实际睡眠判断必须使用完整位图。

## 4. 当前可直接接入的信号

### 4.1 电流与充电输入

当前 `rtc_sleep.c:132-136` 已用 `RtcSleep_PortGetChargeCurrentMa() > 10` 或 `RtcSleep_PortGetDischargeCurrentMa() > 10` 阻塞普通 RTC sleep。该阈值来自 AFE 采样结果 `u16Ichg/u16IDischg`，在 `DataDeal.c:807-850` 由 `DataLoad_Current()` 更新。

建议：

- `LP_BLOCK_CHARGE` 同时看 `GPIO_CHG_IN` 和 `u16Ichg`。
- `LP_BLOCK_DISCHARGE` 看 `u16IDischg`，并保留当前电流阈值，避免轻微零点抖动阻塞。

### 4.2 通信活跃

当前已有信号但没有统一进入低功耗阻塞：

- 串口协议 busy：`Sci_IsAnyPortBusy()`，见 `Sci_Upper.c:1678-1690`。
- 串口外部帧活动：`RTC_ExtComCnt++`，见 `Sci_Upper.c:1479`；当前 `rtc_sleep.c:202-207` 只用计数变化阻塞一次。
- CAN 活跃：`Can_IsBusActive()`，见 `Can_HDX.c:896-899`。
- CAN RTC 唤醒服务：`Can_RtcWakeService()`，见 `Can_HDX.c:906-929`。

建议：

- 第一版 `LP_BLOCK_COMM` 只要 `Sci_IsAnyPortBusy()`、外部通信窗口未过期、`Can_IsBusActive()` 任一为真就阻塞 Stop。
- `Can_RtcWakeService()` 属于 RTC 服务窗口，不应反过来无限阻塞下一轮 Stop；需要独立超时。

### 4.3 AFE 与保护

当前 AFE 信号比较完整：

- 入睡前：`RtcSleep_AfePortIsSleepBlocked()` 读取 `BSTATUS1/2/3`，见 `rtc_sleep_afe_sh367309.c:11-27`。
- 唤醒后：`RtcSleep_AfePortUpdateRtcData()` 更新电压温度，见 `rtc_sleep_afe_sh367309.c:30-42`。
- 电流唤醒：`RtcSleep_AfePortHasCurrentWake()` 计算电流，见 `rtc_sleep_afe_sh367309.c:44-72`。
- MOS/故障唤醒：`RtcSleep_AfePortHasAfeWake()` 同步 MOS 和故障，见 `rtc_sleep_afe_sh367309.c:74-106`。

建议：

- `LP_BLOCK_AFE_BUSY` 覆盖 AFE 通信失败、BSTATUS 非空、PCHG/L0V、AFE EEPROM 写状态。
- `LP_BLOCK_FAULT` 覆盖 `g_stCellInfoReport.unMdlFault_Third.all != 0` 和关键 `System_ERROR_UserCallback(ERROR_STATUS_*)`。
- 低压 deep 判断应使用单独 deep request，而不是简单当作 block。

### 4.4 Flash、日志和升级

当前写 Flash 是同步过程，但没有统一忙标志：

- RW 参数：`EEPROM_SaveRWParametersToFlash()`。
- SOC：`StorageFlash_SaveSocData()`。
- AFE 参数：`StorageFlash_SaveAfeData()`。
- 日志：`StorageFlash_SaveLogData()`。
- 工厂老化：`StorageFlash_SaveFactoryAgingData()`。
- 升级：`u8FlashUpdateE2PROM/u8FlashUpdateFlag` 和 `App_FlashUpdate()`。

建议：

- 后续新增 `StorageFlash_IsBusyOrPending()` 或低功耗 port 层函数，不直接在 `app_lowpower.c` 里读一堆全局变量。
- 在 `LP_STATE_PREPARE_SLEEP` 前二次检查 `LP_BLOCK_FLASH_BUSY | LP_BLOCK_UPGRADE`。
- 复位式 deep 前可以主动保存 `BMS_SLEEP`、SOC、老化进度，但保存失败必须回 RUN 或 ERROR，不能继续睡。

## 5. 深度低功耗请求不应混入 block

以下不是“禁止休眠原因”，而是“请求更深休眠原因”：

| 建议 deep reason | 当前依据 |
| --- | --- |
| `LP_DEEP_LOW_CELL_FORCE` | `LOW_POWER_FORCE_DEEP_SLEEP_MV` 与 `LOW_POWER_FORCE_DEEP_SLEEP_SECONDS`，见 `rtc_sleep.c:7-9`、`rtc_sleep.c:154-163` |
| `LP_DEEP_LOW_CELL_PARAM` | `OtherElement.u16Sleep_Vlow/u16Sleep_TimeVlow`，见 `rtc_sleep_port.c:36-44`、`rtc_sleep.c:167-178` |
| `LP_DEEP_HOST_REQUEST` | 上位机功能号 `0x0A`，见 `Sci_Upper.c:2160-2176` |
| `LP_DEEP_CHARGER_REMOVED` | 充电检测状态机在拔充电时 `entersleep(DEEP_MODE)`，见 `DataDeal.c:63-107` |

建议将 deep reason 与 block reason 分开记录。原因是：工厂老化、LED、通信可以阻塞普通 Stop，但不应该压过过放深睡；如果把“低压”做成普通 block，很容易把优先级写反。

## 6. 第一版 `LP_CanSleep()` 建议判定顺序

建议后续实现时按以下顺序组织，而不是在多个业务模块里分散 `entersleep()`：

1. 采集硬件和业务快照：电压、电流、充电 GPIO、通信、AFE、故障、Flash、升级、LED、IWDG。
2. 先判断 deep request：过放/低压、上位机强制、充电移除。
3. 如果 deep request 存在，只允许 `LP_BLOCK_FLASH_BUSY`、`LP_BLOCK_UPGRADE`、`LP_BLOCK_IWDG_UNSAFE` 这类会破坏数据或流程完整性的原因短暂阻塞；工厂老化、LED、普通通信不能永久阻塞过放 deep。
4. 如果没有 deep request，再判断普通 Stop block：充放电、通信、按键、AFE、故障、Flash、升级、LED、IWDG。
5. 入睡前在 `LP_STATE_PREPARE_SLEEP` 再采一次位图，防止刚收到 Modbus/CAN 帧或刚置位 Flash 保存请求时误睡。

## 7. 第一阶段结论

1. 当前 `LOW_POWER_RTC_BLOCK_*` 枚举覆盖了电流、MCU_WAKE、工厂老化、外部通信、AFE 非空闲，但不足以支撑后续可复用框架。
2. 必须新增内部位图，至少覆盖 `LP_BLOCK_CHARGE/DISCHARGE/COMM/KEY/AFE_BUSY/FLASH_BUSY/UPGRADE/FAULT/LED_ACTIVE/IWDG_UNSAFE`。
3. 深度低功耗请求应单独记录，不应混进 block reason。过放 deep 必须高于工厂老化、LED、普通通信。
4. 当前最需要补齐的业务信号是 Flash busy/pending、升级 pending、串口 busy、CAN active、LED active 和 IWDG unsafe。
5. 为兼容现有上位机和调试字段，短期可以保留 `g_stLowPowerRtcStatus.blockReason`，但内部决策必须使用完整 `uint32_t` 位图。
