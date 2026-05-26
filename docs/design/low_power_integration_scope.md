# RTC 低功耗框架第三阶段集成范围设计

> 阶段：第二阶段设计文档。本文只定义第三阶段建议新增和最小修改范围，不修改源码、不编译、不提交。  
> 目标：在当前 `STM32F103C8 + 标准外设库` 项目中，以最小改动接入可复用 RTC 低功耗框架，保护现有 Modbus/CAN、SOC、AFE、Flash、LED 功能。

## 1. 集成原则

1. 第三阶段只收敛现有 `HICCUP_MODE = Stop + RTC Alarm 周期唤醒`，不推翻当前链路。依据：`rtc_sleep_run_hiccup_cycle()` 已经串起 `RtcSleep_PortPrepareRtcStop()`、`RtcSleep_PortEnterStop()`、`RtcSleep_PortRestoreAfterStop()`，见 `103 + 309/Project/Source/rtc_sleep.c:303-345`。
2. `NORMAL_MODE/DEEP_MODE` 仍保持当前复位式睡眠，不在第三阶段改成硬件 Standby。依据：`SleepDeal_Continue()` 写 BKP 标志后调用 `InitAFE1_Sleep(0)`、`AFE_Sleep()`、`MCU_RESET()`，见 `103 + 309/Project/Source/SleepDeal.c:83-114`。
3. 第三阶段新增框架只做外层适配和阻塞准入，不重构 SOC、AFE、Flash、LED、CAN、Modbus 的业务规则。
4. 第一版不做 CAN/USART Stop 唤醒；通信活跃时禁止进入 Stop。依据：当前已有 `Sci_IsAnyPortBusy()` 和 `Can_IsBusy()`，见 `103 + 309/Project/Source/Sci_Upper.c:1678-1690`、`103 + 309/Project/Source/Can_HDX.c:865-880`。
5. IWDG 作为硬约束：RTC 周期必须小于 IWDG 最短安全窗口。依据：`Init_IWDG()` 在 `__FUNC_RTC__` 分支使用 `IWDG_Prescaler_256 + Reload 0x0FFF`，见 `103 + 309/Project/Source/System_Init.c:33-48`；当前 `RTC_GetWakeupPeriodSeconds()` 中安全裁剪已被注释，见 `103 + 309/Project/Source/RTC.c:366-399`。
6. Stop 唤醒后必须先恢复时钟，再恢复依赖时钟的外设。依据：`Sys_StopMode()` Stop 返回后调用 `cpu_frequency_conf()`，见 `103 + 309/Project/Source/conf/conf.c:374-385`；`InitRunAfterStopWakeup()` 随后恢复 RTC/IO/ADC/USART/CAN/TIM3/AFE IIC，见 `103 + 309/Project/Source/conf/conf.c:392-421`。

## 2. 第三阶段建议新增文件

新增文件建议放在 `103 + 309/Project/Source/` 根目录，避免新增 include path；Keil 工程只做“加入源文件”，不改 target、scatter、宏定义和烧录地址。

| 新增文件 | 角色 | 第一版内容 | 不做的事 |
| --- | --- | --- | --- |
| `app_lowpower.h` | 对业务层公开低功耗框架接口 | 定义 `LP_STATE_*`、`LP_BLOCK_*`、`LP_Init()`、`LP_Task()`、`LP_CanSleep()`、`LP_GetBlockReason()`、`LP_SetWakeupPeriod()`、`LP_EnterStop()`、`LP_BeforeSleep()`、`LP_AfterWakeup()`、`LP_GetLastSleepSeconds()` | 不定义 Modbus/CAN 协议寄存器，不改 SOC/AFE 数据结构 |
| `app_lowpower.c` | 低功耗状态机和阻塞位图 | 计算 `COMM/FLASH_BUSY/UPGRADE/FAULT/LED_ACTIVE/IWDG_UNSAFE` 等位图；保持旧 `rtc_sleep()` 为执行器；将位图摘要映射到旧 `g_stLowPowerRtcStatus.blockReason` | 不直接操作 AFE 寄存器，不直接改 MOS |
| `bsp_rtc.h` | RTC 抽象接口 | 定义 F1/F0 可移植接口：初始化、配置唤醒、关闭 Stop 唤醒、读取上次休眠秒数、设置请求周期 | 不替换整份 `RTC.c` |
| `bsp_rtc.c` | 当前 F1 RTC 适配 | 第一版包装 `Init_RTC()`、`RTC_WKTimeConfig()`、`RTC_DisableStopWakeup()`、`RTC_GetLastWakeupPeriodSeconds()`，并集中处理 IWDG 安全周期 | 不引入 HAL，不启用 F0 Wakeup Timer |
| `bsp_power.h` | PWR/Stop 抽象接口 | 定义 `BspPower_EnterStop()`、`BspPower_ClearWakeupPending()`、`BspPower_DisableWakeupExti()` | 不改成 Standby 策略 |
| `bsp_power.c` | 当前 F1 Stop 适配 | 包装 `Sys_StopMode()`、`LowPower_ClearWakeupPending()`、`LowPower_DisableWakeupExti()` | 不移动 `conf.c` 内现有 GPIO 低功耗配置 |
| `bsp_clock.h` | Stop 后时钟恢复接口 | 定义 `BspClock_RestoreAfterStop()` | 不改 PLL/HSE 策略 |
| `bsp_clock.c` | 当前 F1 时钟恢复适配 | 第一版包装现有 `cpu_frequency_conf()` 或等价 `SystemInit()`、`SystemCoreClockUpdate()`、`InitDelay()` | 不立刻重写 `system_stm32f10x.c` |

## 3. 第三阶段最小修改现有文件

### 3.1 Keil 工程文件

| 文件/位置 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`，`FD_Release` 和调试 target 的源文件列表 | 只加入 `app_lowpower.c`、`bsp_rtc.c`、`bsp_power.c`、`bsp_clock.c` | 让新增框架参与编译 | 工程 XML 顺序或 group 变动导致 Keil 工程显示混乱 | 删除新增 `<File>` 节点即可 | Keil 编译通过；确认 `FD_Release.bin` 仍由原 target 生成 |

### 3.2 主循环入口

| 文件/函数 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Source/Runtime.c:23-30`，`Runtime_RunIoAndPowerTasks()` | 将 `App_LowPowerProcess()` 替换为 `LP_Task()`，或在 `App_LowPowerProcess()` 内部转调 `LP_Task()`；推荐替换主循环一行并显式 `#include "app_lowpower.h"` | 低功耗入口统一到新状态机，便于输出位图和诊断 | 若 `LP_Task()` 未保持旧执行链，会导致不再进入原 `rtc_sleep()` | 恢复原来的 `App_LowPowerProcess()` 调用 | 空闲达到 `sys_time.time_enter_rtc` 后仍进入 HICCUP；通信活跃时不进入 HICCUP；过放 DEEP 逻辑仍触发 |

### 3.3 旧低功耗执行器

| 文件/函数 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Source/rtc_sleep.c:130-145`，`low_power_get_rtc_block_reason()` | 保留现有电流、MCU_WAKE 判断，追加读取 `LP_GetBlockReason()` 的 HICCUP 阻塞摘要；只在 `low_power_select_sleep_mode()` 的低压/强制深睡判断之后生效 | 将 `LP_BLOCK_COMM/FLASH_BUSY/UPGRADE/LED_ACTIVE/IWDG_UNSAFE` 接入现有空闲 Stop 准入，同时不挡过放深睡优先级 | 如果把新阻塞放在低压深睡判断之前，会破坏过放深睡优先级 | 删除追加调用，恢复旧 `LOW_POWER_RTC_BLOCK_*` 判断 | 低压过放仍进入 `DEEP_MODE`；串口/CAN busy 时 `HICCUP_MODE` 不触发；`g_stLowPowerRtcStatus.blockReason` 有兼容摘要 |
| `103 + 309/Project/Source/rtc_sleep.h:39-58`，`LOW_POWER_RTC_STATUS` | 不扩大旧结构为 `uint32_t`，只保留旧单字节摘要；新增完整位图放在 `app_lowpower.c` 内部并由 `LP_GetBlockReason()` 暴露 | 避免破坏既有监控/上位机对旧结构的隐含依赖 | 如果直接改结构字段宽度，调试脚本或 Watch 观察方式可能失效 | 保持旧结构不变 | Watch 中旧字段仍可读；新位图可通过新 API 或后续诊断寄存器读出 |
| `103 + 309/Project/Source/rtc_sleep_port.c:108-134`，`RtcSleep_PortPrepareRtcStop()`、`RtcSleep_PortEnterStop()`、`RtcSleep_PortDisableStopWakeup()`、`RtcSleep_PortRestoreAfterStop()` | 可选地改为调用 `BspRtc/BspPower/BspClock` 包装函数；第一版允许先不改，只让新 BSP 包装旧函数 | 逐步形成可移植层，降低后续 F0/F1 迁移成本 | 过早替换 Stop/恢复链路可能引入唤醒失败 | 回滚为当前直接调用 `Init_RTC()`、`Sys_StopMode()`、`InitRunAfterStopWakeup()` | Stop 前后仍喂狗；RTC Alarm 唤醒后 `is_rtc_wakekup` 置位；CAN RTC 服务仍执行 |

### 3.4 RTC 周期和 IWDG 约束

| 文件/函数 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Source/RTC.c:366-399`，`RTC_GetWakeupPeriodSeconds()` | 恢复 IWDG 安全裁剪；优先采用 `LP_SetWakeupPeriod()` 请求值，否则沿用 `Can_GetIdleRtcPeriodSeconds()`；裁剪后记录 `LP_BLOCK_IWDG_UNSAFE` | 防止后续把 RTC 周期调到几十秒导致 Stop 中 IWDG 复位 | 裁剪逻辑若估算错误，可能导致唤醒过频或误报不安全 | 恢复为当前 `Can_GetIdleRtcPeriodSeconds()` 固定来源 | 将周期设为 1s、10s、30s 分别观察：1s 正常；超过安全窗时实际 Alarm 被裁剪且无 IWDG 复位 |
| `103 + 309/Project/Source/RTC.h:66-69` | 只新增必要声明，例如 `RTC_SetWakeupPeriodRequestSeconds()` 或由 `bsp_rtc.h` 暴露，不删除现有 `RTC_WKTimeConfig()`、`Init_RTC()` | 让 `LP_SetWakeupPeriod()` 对当前 F1 RTC 生效 | API 命名与旧函数混淆 | 删除新增声明和实现 | 编译引用清晰；现有 `RtcSleep_PortGetLastWakeupSeconds()` 仍返回 `RTC_GetLastWakeupPeriodSeconds()` |
| `103 + 309/Project/Source/RTC.c:280-317`，`RTC_ClearAlarmPending()`、`RTC_EnableAlarmAfterSeconds()` | 第三阶段只检查 safe wait 返回值，不改变 EXTI17/NVIC 清 pending 顺序 | 防止 RTC 写等待失败后仍继续进 Stop | 如果处理过严，RTC 时钟异常时可能长期禁止睡眠 | 回滚到当前忽略返回值 | 调试器连接、RTC 时钟异常场景不会卡死在等待点 |

### 3.5 通信阻塞

| 文件/函数 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Source/app_lowpower.c` 引用 `Sci_IsAnyPortBusy()`，见 `103 + 309/Project/Source/Sci_Upper.c:1678-1690` | `Sci_IsAnyPortBusy()!=0` 时置 `LP_BLOCK_COMM` | Modbus/RS485 半包、应答发送期间禁止 Stop | 如果 busy 判断粘住，会导致不入睡 | 将串口 busy 阻塞临时关闭或加超时诊断 | 连续读写 Modbus 时不睡；停止通信并超过空闲延迟后可睡 |
| `103 + 309/Project/Source/app_lowpower.c` 引用 `Can_IsBusy()`，见 `103 + 309/Project/Source/Can_HDX.c:865-880` | CAN 队列、邮箱、读块流忙时置 `LP_BLOCK_COMM` | CAN 发送或读块流期间禁止 Stop | `Can_IsBusActive()` 是粘性状态，若误用会长期阻塞；应使用 `Can_IsBusy()` 做当前 busy | 回滚为不接 CAN busy，保留旧 `RTC_ExtComCnt` 判断 | CAN 周期报文/读块时不睡；空闲时仍可进入 RTC 周期唤醒 |
| `103 + 309/Project/Source/Can_HDX.c:613-629`、`103 + 309/Project/Source/Sci_Upper.c:1948-1964` | 不改协议，只让 `app_lowpower.c` 读取升级 pending 状态，例如 `u8FlashUpdateFlag/u8FlashUpdateE2PROM` 或新增只读 API | IAP 请求后禁止进入普通 Stop，避免升级窗口睡眠 | 直接改 CAN/Modbus 命令会破坏协议 | 不改协议；只删除 lowpower 对 pending 的读取 | CAN/Modbus 触发 IAP 后进入复位升级流程，不被低功耗打断 |

### 3.6 Flash/日志/参数保存阻塞

| 文件/函数 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Source/Flash.c:650-672`，`FlashWriteOneHalfWord()`；`103 + 309/Project/Source/Flash.c:760-778`、`:829-838` 等 `StorageFlash_Save*()` | 新增轻量 busy 标志和 `StorageFlash_IsBusy()`；优先在底层擦写/保存入口置位，退出清零 | 为 `LP_BLOCK_FLASH_BUSY` 提供依据，避免 Flash 擦写/参数保存窗口进入 Stop | 若异常提前 return 未清 busy，会导致长期不睡 | busy 标志默认返回 0；删除置位/清零代码 | 写参数、写日志、保存 SOC/AFE 时不入睡；写完后可睡；失败路径 busy 能清零 |
| `103 + 309/Project/Source/LogRecord.c:97-150`、`:178-210` | 第三阶段不改日志格式；若已有睡眠日志 pending，则由 app_lowpower 延迟 HICCUP，直到日志任务处理 | 保护睡眠事件和故障日志不丢 | 如果日志 pending 无清除条件，可能阻塞睡眠 | 仅保留 Flash busy，不接日志 pending | 触发睡眠事件后日志保存完成；重复事件限频不受影响 |

### 3.7 LED 阻塞

| 文件/函数 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Source/LedBar.c:817-842`，`LedBar_SetSleep()`；`103 + 309/Project/Source/LedBar.c:1039-1045` | 新增只读 `LedBar_IsActiveForSleepBlock()`，用于显示窗口、按键显示、充电图标活跃时置 `LP_BLOCK_LED_ACTIVE`；不改变显示状态机 | 避免显示还没收尾时直接 Stop，造成界面闪烁或 SOC 显示不完整 | 若判定过宽，会影响空闲入睡 | API 固定返回 0 或删除 app_lowpower 引用 | 按键显示 SOC 期间不睡；显示结束后可睡；`LedBar_SetSleep(1u)` 仍由原链路控制 |

### 3.8 AFE/SOC/保护同步

| 文件/函数 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Source/LowPowerSleep.c:5-16` | 不改现有保存顺序：`Can_PrepareSleep()`、`SOC_SaveSnapshotBeforeSleep()`、`FactoryAging_SaveProgressBeforeSleep()`、`LedBar_SaveSleepSoc()` | 保护现有 SOC 快照、老化进度和 LED 休眠 SOC | 改顺序可能影响 CAN 断电前发送或 SOC 保存时机 | 保持文件不改 | 睡前 SOC 快照仍保存；老化进度仍保存 |
| `103 + 309/Project/Source/conf/conf.c:392-421`，`InitRunAfterStopWakeup()` | 第三阶段不把 `initAFE1_IIC()` 改成完整 `InitAFE1()`；只在 `LP_AfterWakeup()` 中记录需要 AFE/MOS 同步，由现有 AFE 周期任务和 `rtc_sleep_afe_sh367309.c` 处理 | 避免 Stop 唤醒后 MOS 反复动作或重配 AFE 保护参数 | 如果强行完整重初始化 AFE，可能影响保护状态 | 保持当前 `initAFE1_IIC()` | RTC 唤醒后 AFE 通信恢复；`Fault_ChangeToMCU()`、`SystemRuntime_SetMosStatus()` 路径仍由现有代码执行 |
| `103 + 309/Project/Source/rtc_sleep.c:289-300`、`103 + 309/Project/Source/rtc_sleep_port.c:166-177` | 保留 `SOC_ApplyRtcRelaxationCompensation()` 的休眠秒数输入；`LP_GetLastSleepSeconds()` 读取同一来源 | 保护 SOC 静置补偿 | 重复补偿会导致 SOC 漂移 | 回滚新 API，只保留旧调用 | RTC 连续唤醒后 SOC 只按累计休眠时间补偿一次 |

### 3.9 时钟/Power BSP 包装

| 文件/函数 | 修改点 | 目的 | 风险 | 回滚方式 | 测试点 |
| --- | --- | --- | --- | --- | --- |
| `103 + 309/Project/Source/rtc_sleep_port.c:207-212`，`cpu_frequency_conf()` | 第一版只由 `bsp_clock.c` 包装，不移动实现；后续再拆出专用 Stop 后时钟恢复 | 避免第三阶段大改 RCC 逻辑 | 直接替换 `SystemInit()` 可能影响 VTOR/RCC 初始化 | 保留原 `cpu_frequency_conf()` | Stop 唤醒后 USART/CAN/TIM3 波特率和周期正常 |
| `103 + 309/Project/Source/conf/conf.c:374-385`，`Sys_StopMode()` | 第一版不改变 Stop 参数；`bsp_power.c` 只是包装调用 | 保留当前稳定进入 Stop 的方式 | 改 Stop entry 或 regulator 参数可能影响唤醒 | 保持原函数不变 | RTC Alarm 唤醒后主循环继续运行，TIM3 10ms 恢复 |

## 4. 第三阶段不建议修改的文件和内容

| 文件/模块 | 不修改内容 | 原因 |
| --- | --- | --- |
| `103 + 309/Project/Source/Sci_Upper.c` | 不改 Modbus 地址、0x03/0x06/0x10 解析、CRC、应答格式、`Sci_WrRegs_0x10_FlashConnect()` 协议 | 保护现有上位机和量产协议 |
| `103 + 309/Project/Source/Can_HDX.c` | 不改 CAN 帧 ID、周期报文、APP/IAP 命令、读块流格式；只读 `Can_IsBusy()` 和升级 pending 状态 | 保护 CAN 通信和升级路径 |
| `103 + 309/Project/Source/SOC.c`、`103 + 309/Project/Source/SocEnhance.c` | 不改 SOC 算法、OCV 表、存储格式、补偿公式 | 低功耗框架只提供休眠秒数，不重新定义 SOC |
| `103 + 309/Project/Source/I2C_AFE1.c`、`103 + 309/Project/Source/SH367309_Func.c`、`103 + 309/Project/Source/SH367309_DataDeal.c` | 不改 AFE 初始化参数、保护阈值、MOS 控制策略、EEPROM/寄存器写入流程 | 防止保护和 MOS 状态被低功耗改造带偏 |
| `103 + 309/Project/Source/Flash.h` 地址常量、Flash 存储结构体 | 不改 IAP 地址、App 地址、SOC/AFE/RW_PARAM/LOG 存储布局 | 防止破坏升级、参数和日志兼容性 |
| `103 + 309/Project/Users/Objects/FD_Release.sct` | 不改 App scatter 和地址 | App 固定从 `0x08004800` 安全烧录，不能影响 IAP |
| `tools/soc_flash_app_safe.ps1` | 不改安全烧录地址检查和 dry-run 输出 | 保留 `0x08004800` 防误烧规则 |
| 上位机工程和 `dist/BMS_CommTool_Upgrade_UI.exe` | 第三阶段不涉及 PC 工具 | 本阶段只做 MCU 低功耗框架 |

## 5. 建议第三阶段实施顺序

1. 新增四组框架文件，先做 no-op/包装实现：`LP_Task()` 仍调用旧 `rtc_sleep()`，BSP 包装现有 RTC/PWR/Clock 函数。
2. 将主循环入口从 `App_LowPowerProcess()` 切到 `LP_Task()`，验证行为不变。
3. 在 `app_lowpower.c` 中实现阻塞位图：通信 busy、升级 pending、Flash busy、LED active、IWDG unsafe；先只影响 HICCUP Stop，不挡 DEEP。
4. 修改 `rtc_sleep.c` 的 HICCUP 准入，读取新位图摘要；保留旧 `g_stLowPowerRtcStatus` 字段。
5. 恢复 `RTC_GetWakeupPeriodSeconds()` 的 IWDG 安全裁剪，并接入 `LP_SetWakeupPeriod()`。
6. 只读观察通过后，再考虑把 `rtc_sleep_port.c` 中的直接调用逐步替换为 `BspRtc/BspPower/BspClock` 包装函数。
7. 每一步更新 `docs/low_power_rtc_change_log.md` 和对应设计文档，再编译、上板测试。

## 6. 最小测试点清单

| 测试项 | 触发条件 | 通过标准 |
| --- | --- | --- |
| 空闲 RTC Stop | 无充放电、无通信、无 Flash 保存，等待 `sys_time.time_enter_rtc` | 进入 HICCUP；RTC Alarm 唤醒；`InitRunAfterStopWakeup()` 后 TIM3、ADC、USART、CAN 恢复 |
| IWDG 安全窗口 | 设置 RTC 周期为超过安全窗口的值 | 实际 Alarm 周期被裁剪或置 `LP_BLOCK_IWDG_UNSAFE`，无 IWDG 误复位 |
| Modbus 活跃禁止睡眠 | COM4/19200 连续读写寄存器 | `LP_BLOCK_COMM` 置位，不进入 Stop；停止通信后可恢复入睡 |
| CAN 活跃禁止睡眠 | CAN 周期报文或读块流进行中 | `Can_IsBusy()` 时不睡；空闲后仍能入睡；帧格式不变 |
| 升级窗口禁止睡眠 | CAN 或 Modbus 触发 IAP 请求 | 进入既有升级复位流程，不被低功耗打断 |
| Flash 保存禁止睡眠 | 保存参数、SOC、AFE、日志、老化进度 | 保存期间不睡；保存完成后 busy 清零 |
| LED 活跃禁止睡眠 | 按键显示 SOC 或充电显示窗口 | 显示窗口期间不睡，显示结束后可入睡 |
| AFE/MOS 同步 | RTC 唤醒后读取 AFE、保护、MOS 状态 | AFE IIC 恢复；MOS 状态不被完整重初始化反复动作 |
| SOC 休眠补偿 | 连续 RTC 周期唤醒后退出睡眠 | `SOC_ApplyRtcRelaxationCompensation()` 使用累计休眠秒数，未重复补偿 |
| 过放深睡优先级 | 单体低于强制深睡阈值且无充电电流 | 即使 LED/通信等普通阻塞存在，也不长期挡住 `DEEP_MODE` 策略 |

## 7. 回滚边界

第三阶段每个子步骤都应能独立回滚：

1. 新增文件回滚：从 Keil 工程移除新增文件，删除 `app_lowpower/bsp_*` 文件。
2. 主循环入口回滚：`Runtime_RunIoAndPowerTasks()` 恢复调用 `App_LowPowerProcess()`。
3. 阻塞位图回滚：`low_power_get_rtc_block_reason()` 删除对 `LP_GetBlockReason()` 的引用。
4. RTC 周期回滚：`RTC_GetWakeupPeriodSeconds()` 恢复为 `Can_GetIdleRtcPeriodSeconds()` 固定来源。
5. Flash busy 回滚：`StorageFlash_IsBusy()` 固定返回 0，删除保存入口置位/清零。
6. LED active 回滚：`LedBar_IsActiveForSleepBlock()` 固定返回 0。

只要不修改协议、存储布局、AFE 参数和 scatter 文件，回滚应不影响原有 Modbus/CAN、SOC、AFE、Flash、LED 功能。
