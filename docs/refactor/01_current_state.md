# 当前状态与功能盘点

文档状态：部分验证  
验证方式：只读源码审查，未编译，未连接硬件，未跑上位机/CAN 实机测试。  
源码基准：`103 + 309/Project/Source/`、Keil 工程文件、`tools/project_check.py` 静态检查结果。  
重构原则：源码是第一可信来源；本阶段只做分析和文档，不修改固件源码。

## 1. 当前主流程

当前工程是裸机轮询架构：

```text
main()
  -> AppInit_Boot()
     -> AppInit_InitDevice()
     -> AppInit_InitRuntimeState()
     -> Init_RTC()
     -> LP_Init()
  -> while (1)
     -> Runtime_RunOnce()
```

主要依据：

- `main.c`：只负责启动和无限循环调用 `Runtime_RunOnce()`。
- `AppInit.c`：负责系统时钟、休眠唤醒判断、GPIO/NVIC/SCI/Flash/AFE/CAN/ADC/SOC/Timer/Watchdog 初始化。
- `Runtime.c`：把主循环拆成 front、IO/power、background 三段任务。
- `System_Init.c`：`TIM3_IRQHandler()` 生成 10ms/50ms/100ms/200ms/1000ms 任务标志。

当前主循环任务顺序：

```text
Runtime_RunFrontTasks()
  -> SysTime_LatchTaskFlags()
  -> FactoryAging_Task()
  -> APP_LedBar()
  -> App_AFEGet()

Runtime_RunIoAndPowerTasks()
  -> AppInit_ServiceSci()
  -> App_AnlogCal()
  -> LP_Task()
  -> App_Can()

Runtime_RunBackgroundTasks()
  -> StorageFlash_AppUseTest_Task()
  -> App_FlashUpdate()
  -> App_LogRecord()
  -> App_ProID_Deal()
  -> Feed_IWatchDog
```

`App_AFEGet()` 是当前 200ms 核心采样与业务入口：

```text
App_AFEGet()
  -> SysTime_Take200msTaskPeriod()
  -> UpdateVoltageFromBqMaximo()
  -> DataLoad_CellVolt()
  -> DataLoad_CellVoltMaxMinFind()
  -> DataLoad_Temperature()
  -> DataLoad_TemperatureMaxMinFind()
  -> DataLoad_Current()
  -> App_SH367309()
  -> new_todo_logi()
  -> App_SOC()
```

## 2. 编译配置现状

当前默认量产配置来自 `conf/Project_Config.h` 和 `conf/Project_BuildGuard.h`：

- `PROJECT_CFG_BUILD_PROFILE = 0`，量产 Release。
- `PROJECT_CFG_WDOG_ENABLE = 1`，量产启用 IWDG。
- `PROJECT_CFG_RTC_ENABLE = 1`，启用 RTC 低功耗。
- `PROJECT_CFG_IAP_ENABLE = 1`，启用 IAP 跳转。
- `PROJECT_CFG_FACTORY_AGING_ENABLE = 1`，启用老化流程。
- `PROJECT_CFG_HOST_WRITE_ENABLE = 1`，允许上位机写寄存器；地址窗口有边界判断，但 AFE 参数写入口仍存在写前未复用 min/max 实时校验的缺口，不能把源码注释中的“range check”视为所有写入口都已安全闭环。
- `PROJECT_CFG_SOC_TEST_MODE_ENABLE = 0`，量产关闭 SOC 注入式测试。
- `PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE = 0`，量产关闭破坏性 Flash 测试。
- `PROJECT_CFG_FLASH64K_USE_TEST_ENABLE = 0`，量产关闭运行期 Flash 存储测试。
- `PROJECT_CFG_DEBUG_WATCH_ENABLE = 0`，量产关闭 Keil Watch 调试导出。
- `PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE = 0`，量产关闭串口日志。
- `PROJECT_CFG_SCI1_ROLE = 1`，SCI1 是通用上位机协议。
- `PROJECT_CFG_SCI2_ROLE = 0`、`PROJECT_CFG_SCI3_ROLE = 0`，SCI2/SCI3 当前默认禁用。

`Project_BuildGuard.h` 对量产配置有硬性检查：Release 下禁止 debug/test/log/Flash64K 测试/SOC 测试等开关打开。

## 3. 当前功能盘点

分类说明：

- 必须保留：当前产品功能、协议、保护、安全边界依赖，重构不能改变行为。
- 建议保留但可简化：功能有价值，但实现可以减变量、减分支、减历史包装。
- 疑似无用，需要我确认：源码显示未启用、无调用或历史遗留，但删除前必须由用户确认。
- 可删除：源码证据显示是明显死代码、空实现或注释残留；实际删除仍需进入确认阶段。
- 高风险，必须重构：功能必须保留，但当前耦合、时序或安全风险较高，应拆分和验证。

| 功能 | 分类 | 依据 | 调用关系 | 当前判断 |
|---|---|---|---|---|
| 系统启动与主循环 | 必须保留 | `main.c`、`AppInit.c`、`Runtime.c` | `main -> AppInit_Boot -> Runtime_RunOnce` | 当前结构清楚，但 `main.h` 伞状 include 造成全局耦合。 |
| 10ms 系统时基与任务标志 | 必须保留 | `System_Init.c`、`System_Init.h` | `TIM3_IRQHandler -> SysTime_LatchTaskFlags` | 所有周期任务依赖，迁移时必须保持 tick 语义。 |
| IWDG 看门狗 | 必须保留 | `Project_Config.h`、`Runtime.c`、`AppInit.c` | 初始化后主循环和阻塞等待中喂狗 | 量产硬性启用，不能删除。 |
| GPIO/电源/STOP 恢复 | 必须保留 | `conf.c`、`bsp_power.c`、`bsp_clock.c` | 低功耗和初始化调用 | 当前 GPIO、电源、唤醒配置混在 `conf.c`，可拆到 BSP。 |
| ADC + DMA + TIM2 采样 | 必须保留 | `ADC.c` | `InitADC`、`App_AnlogCal` | Type-C 电流、VBAT、MOS 温度依赖；可整理命名和注释。 |
| SH367309 AFE I2C bitbang | 必须保留 | `I2C_AFE1.c`、`I2C_AFE1.h` | `InitAFE1`、`UpdateVoltageFromBqMaximo`、`MTPRead/MTPWrite` | 必须保留 PB8/PB9 bitbang 时序，返回值约定需要统一。 |
| AFE MTP 参数刷新 | 高风险，必须重构 | `SH367309_DataDeal.c` | `InitAFE1 -> SH367309_UpdataAfeConfig` | 写 MTP、复位 AFE、默认参数、Flash 参数耦合，适合拆成配置服务。 |
| AFE 状态监控和故障映射 | 高风险，必须重构 | `SH367309_Func.c` | `App_SH367309 -> App_SH367309_Monitor -> Fault_ChangeToMCU` | 直接改 MOS 状态、故障、系统错误和 GPIO，需保留行为后拆边界。 |
| 单体电压采集/最大最小/总压 | 必须保留 | `DataDeal.c` | `App_AFEGet -> DataLoad_CellVolt*` | 13 串特殊映射和未用 cell 填充值需要保留到确认。 |
| 温度采集/最大最小 | 建议保留但可简化 | `DataDeal.c`、`ADC.c` | `DataLoad_Temperature`、`ADC_TTC` | ENV2/ENV3 当前强制 -40，需要确认硬件是否未贴。 |
| AFE 电流采样和自动归零 | 必须保留 | `DataDeal.c` | `DataLoad_Current` | 影响 SOC、保护、休眠唤醒；可整理状态变量但不能改阈值。 |
| Type-C 输出电流换算 | 建议保留但可简化 | `ADC.c`、`SOC.c` | `ADC_GetTypeCOutCurrentMilliAmp -> SOC_GetTypeCEquivalentBatteryCurrent` | 当前参与 SOC 等效电流，需保留或由用户确认删除。 |
| `__VIRTURE_CURRENT__` 调试电流 | 疑似无用，需要我确认 | `conf.h`、`DataDeal.c` | `sys_time.isdebugenable` 条件下覆盖电流 | 量产配置仍定义该宏，但入口依赖调试开关；建议确认是否保留。 |
| 保护参数和故障位 | 必须保留 | `Fault.h`、`Fault.c` | 多模块读写 `g_stCellInfoReport.unMdlFault_*` | 协议和保护逻辑依赖，不能改变位含义。 |
| 系统错误和功能开关 | 建议保留但可简化 | `System_Monitor.c/h` | `System_ERROR_UserCallback` 被 AFE/ADC/SCI/CAN/Log/SOC 调用 | 偏移表和保留位较难读，协议布局未确认前不能删位。 |
| MOS 启动策略 | 必须保留 | `MosStartup.c` | `InitAFE1 -> MosStartup_ApplyInitialState` | 5V 充电、老化、默认放电状态依赖。 |
| 充电器检测、认证、保险丝/MOS 策略 | 高风险，必须重构 | `DataDeal.c:new_todo_logi` | `App_AFEGet -> new_todo_logi` | 保护、MOS、UL 认证、charger、RF_EN、AFE error 混在一起，是首要拆分对象。 |
| 休眠请求和 reset-sleep | 高风险，必须重构 | `rtc_sleep.c`、`SleepDeal.c`、`LowPowerSleep.c` | `LP_Task -> rtc_sleep`、`SleepDeal_Continue` | HICCUP/NORMAL/DEEP、BKP flag、AFE sleep、MCU reset、LED 快显强耦合。 |
| RTC STOP 周期唤醒 | 高风险，必须重构 | `rtc_sleep.c`、`rtc_sleep_port.c` | `rtc_sleep_run_hiccup_cycle` | 唤醒后更新 AFE/SOC/CAN，涉及电流唤醒和故障唤醒，必须小步验证。 |
| CAN 低功耗保活和 RTC 唤醒广播 | 高风险，必须重构 | `Can_HDX.c`、`rtc_sleep_port.c` | `RtcSleep_PortRunCanRtcWakeService -> Can_RtcWakeService` | CAN 与低功耗耦合深，广播周期和 bus active hold 是客户可见行为。 |
| 飞道 CAN 周期帧 | 必须保留 | `CanFeidaoFrames.c`、`Can_HDX.c` | `App_Can -> feidao_can_schedule_periodic -> CanFeidao_SendNextPending` | CAN ID 和帧数据含义是客户协议，不能改。 |
| CAN App 命令 | 必须保留 | `Can_HDX.c` | 接收 `FEIDAO_CAN_APP_CMD_*`，读写 Modbus 寄存器，IAP，老化控制 | 与上位机和升级/老化流程相关，必须保留协议行为。 |
| SCI1/Modbus 上位机协议 | 必须保留 | `Sci_Upper.c/h` | `AppInit_ServiceSci -> App_SCI*` | 寄存器地址、读写行为、错误码都是协议边界。 |
| SCI2/SCI3 角色 | 疑似无用，需要我确认 | `Project_Config.h`、`Sci_Upper.c` | 默认 role=0，代码条件编译 | 如果当前产品只用 SCI1，可后续裁剪。 |
| IAP 跳转 | 必须保留 | `Flash.c`、`Sci_Upper.c`、`Can_HDX.c` | `AppUpgrade_RequestIap`、`APP_To_IAP_Jump` | App 地址固定 `0x08004800`，任何重构不能破坏。 |
| Flash 参数和数据存储 | 必须保留 | `Flash.c/h`、`EEPROM.c` | SOC/AFE/RW/log/aging 数据 load/save | Flash 地址布局是持久化协议，不能随意改。 |
| 升级参数策略 | 建议保留但可简化 | `EEPROM.c`、`Project_Config.h` | `InitE2PROM -> UpgradeParamPolicy_ApplyOnce` | 当前会按版本重置多类参数，需确认量产策略。 |
| 伪 EEPROM 兼容 API | 疑似无用，需要我确认 | `EEPROM.c/h` | `ReadEEPROM_Byte/Word` 等函数无真实存储 | 源码搜索未见有效调用，建议删除或归档。 |
| SOC 增强算法 | 必须保留 | `SOC.c`、`SocEnhance.c/h` | `App_SOC -> SOC_IntEnhance_Ctrl` | 功能复杂但有真实业务价值，先整理接口，不动算法。 |
| SOC 注入式测试模式 | 疑似无用，需要我确认 | `SOC.c`、`Project_Config.h`、`Project_BuildGuard.h` | 默认关闭，仅 Factory/Test 可开 | 若长期需要测试工具，应迁移到 tests_or_tools 并保持隔离。 |
| LED 数码管显示 | 必须保留 | `LedBar.c/h` | `APP_LedBar`、`TIM4_IRQHandler`、低功耗唤醒快显 | 当前实现较长但职责清楚；可拆硬件扫描和显示策略。 |
| 长按按键关机 | 必须保留 | `LedBar.c`、`SleepDeal.c`、`Project_Config.h` | `_DI_SWITCH_longKEY_ONOFF` 下进入 `DEEP_MODE` | 与低功耗/用户操作相关，不能直接删。 |
| 老化流程 | 必须保留 | `FactoryAging.c/h`、`Can_HDX.c`、`CanFeidaoFrames.c` | `FactoryAging_Task`、CAN/Modbus 查询和控制 | 当前需求要求 UI 可见老化剩余时间，必须保留。 |
| 事件日志 | 必须保留 | `LogRecord.c/h`、`Flash.c`、`Sci_Upper.c` | `App_LogRecord`、`Sci_ACK_0x03_ReadRegs_EventRecord` | 保护事件和休眠/启动日志依赖，后续可简化写 Flash 节流。 |
| BMS SN/硬件/软件版本 | 必须保留 | `ProductionID.c/h`、`Sci_Upper.c/h` | `App_ProID_Deal`，协议 `0xC002` | 当前只是默认值初始化；上位机要求实时监控底部显示。 |
| Flash64K 存储测试 | 疑似无用，需要我确认 | `Flash64KAppTest.c/h`、`Project_Config.h` | 默认关闭，Release guard 禁止打开 | 建议迁移到 tests_or_tools 或单独测试固件，不放主业务路径。 |
| EasyLogger 串口日志 | 建议保留但可简化 | `easylogger/`、`AppInit.c`、`Project_Config.h` | Debug/Factory 可启用，Release 禁止 | 可保留为调试资产，量产隔离必须继续由 build guard 保证。 |
| `ShortFunc` 负载移除短路恢复 | 疑似无用，需要我确认 | `ShortFunc.c/h`、`SH367309_DataDeal.c` | `InitShortCur()` 只有注释调用 | 若当前硬件不使用，可删除或移动到历史归档。 |
| `App_WarnCtrl` | 可删除 | `Fault.h`、`Runtime.c` | 仅声明和注释调用，未见实现 | 可作为首批死代码清理候选。 |

## 4. 当前模块规模与阅读压力

按源码行数和职责混杂程度，最影响阅读的模块：

| 模块 | 主要问题 |
|---|---|
| `Sci_Upper.c` | 2291 行，Modbus 寄存器映射、帧解析、串口驱动、写寄存器策略混在一个文件。 |
| `SocEnhance.c` | 1764 行，SOC 算法集中但状态较多，适合先封装输入/输出边界，不宜先改算法。 |
| `DataDeal.c` | 1249 行，采样、保护、MOS、charger、认证、调试电流、SOC 入口混杂。 |
| `Can_HDX.c` | 1213 行，CAN 硬件、电源、队列、周期帧、App 命令、低功耗服务混杂。 |
| `Flash.c` | 1146 行，Flash driver、typed storage、IAP、升级复位混在一起。 |
| `LedBar.c` | 1070 行，GPIO Charlieplexing 扫描、按键、低功耗快显、业务显示策略混在一起。 |
| `I2C_AFE1.c` | 759 行，bitbang I2C、AFE MTP、初始化、采样读取混杂。 |
| `FactoryAging.c` | 617 行，状态机、BKP、Flash、MOS 控制混杂，但功能边界相对明确。 |
| `RTC/rtc_sleep/app_lowpower` | 多文件互相调用，涉及 STOP、BKP、CAN、SOC、AFE、LED，是高风险链路。 |

## 5. 当前架构主要问题

1. `main.h` 引入几乎所有模块，导致任何 C 文件都能直接访问其他模块全局变量和函数。
2. `g_stCellInfoReport`、`sys_time`、`OtherElement`、`SH367309_Reg_Store` 是跨模块共享大状态，读写边界不清。
3. `DataDeal.c:new_todo_logi()` 同时做 charger 检测、MOS 控制、认证熔丝、AFE error 特判和保护阈值判断。
4. `Can_HDX.c` 同时承担 CAN driver、TX queue、周期调度、App 命令、IAP、老化命令和低功耗保活。
5. `Sci_Upper.c` 同时承担串口底层、Modbus 解析、寄存器映射和业务写入逻辑。
6. `rtc_sleep_port.c` 作为低功耗端口层却直接触达 AFE、SOC、CAN、日志、LED、运行秒计数。
7. 调试/测试/历史逻辑通过宏、`#if 0`、空函数、注释调用散落在业务文件里。
8. 部分函数命名仍是历史名称，例如 `new_todo_logi`、`App_AnlogCal`、`EEPROM_*`，与真实职责不匹配。

## 6. 本阶段验证边界

已做：

- 只读检查主入口、初始化、主循环、配置、保护、AFE、SOC、Flash、SCI、CAN、低功耗、LED、老化、事件日志。
- 运行 `tools/project_check.py`，结果为 147 OK、1 WARN、0 Error。

未做：

- 未执行 Keil/IAR/ARMCC 编译。
- 未生成新固件。
- 未烧录。
- 未连接 COM4/19200、CAN 或 ST-Link 实机验证。
- 未验证上位机 UI 和 exe。

当前结论只用于重构前需求确认和迁移设计，不能等同于端到端验证。
