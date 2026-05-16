# 可移植解耦基础层说明 2026-05-16

本文记录本次独立分支对工程耦合关系的阶段性收口。目标不是一次性把整套 F103 标准库工程改成通用框架，而是在不改变当前量产运行行为的前提下，先建立后续移植 STM32F0 或裁剪功能时必须具备的边界。

## 1. 当前主要耦合点

当前工程仍然是典型历史嵌入式项目结构：

1. `main.h` 汇总包含大部分业务、驱动和协议头文件，很多 `.c` 文件通过它间接获得全部全局对象和硬件定义。
2. `g_stCellInfoReport`、`OtherElement`、`SystemStatus` 等全局数据被通信、显示、SOC、保护和低功耗模块直接读取。
3. `Runtime.c` 原本直接按固定顺序调用 AFE、RS485、低功耗、CAN、存储、日志、Production ID 等任务，模块是否启用不够集中。
4. 保护策略没有显式模式区分：当前 309 项目依赖 AFE 硬件保护，但 `Fault.c` 中仍保留完整 MCU 软件保护轮询，容易误判为当前量产路径正在运行软件保护。
5. `stm32f10x.h`、`NVIC_SystemReset()`、`IWDG_Feed()`、`SysTime_Get10msTickCount()` 等 MCU/标准库接口散落在业务路径里，后续移植 STM32F0 时没有统一替换点。

这些问题不会必然导致死机，但会让“删掉某个功能”“移植到其他 MCU”“只保留 SOC/通信/采样的一部分”变得风险很高，容易出现编译依赖、运行顺序或全局数据遗漏。

## 2. 本次新增的基础边界

### 2.1 `Project_Types.h`

新增项目自有类型头：

- `bms_u8/bms_u16/bms_u32`
- `bms_i8/bms_i16/bms_i32`
- `bms_bool_t`
- `BMS_INLINE`

注意：本次没有替换旧 `UINT8/UINT16/UINT32`，因为这些类型当前来自 `stm32f10x.h`，一次性替换会影响面过大。新增自有类型只是为后续新模块和移植层提供干净入口。

### 2.2 `Project_Features.h`

新增模块级运行开关映射：

| 运行开关 | 配置来源 | 当前默认 |
| --- | --- | --- |
| `PROJECT_FEATURE_AFE` | `PROJECT_CFG_FEATURE_AFE` | 1 |
| `PROJECT_FEATURE_AFE_HARDWARE_PROTECTION` | `PROJECT_CFG_PROTECTION_MODE` | 1 |
| `PROJECT_FEATURE_SOFTWARE_PROTECTION` | `PROJECT_CFG_PROTECTION_MODE` | 0 |
| `PROJECT_FEATURE_SOC` | `PROJECT_CFG_FEATURE_SOC` | 1 |
| `PROJECT_FEATURE_ANALOG_ADC` | `PROJECT_CFG_FEATURE_ANALOG_ADC` | 1 |
| `PROJECT_FEATURE_RS485` | `PROJECT_CFG_FEATURE_RS485` | 1 |
| `PROJECT_FEATURE_CAN` | `PROJECT_CFG_FEATURE_CAN` | 1 |
| `PROJECT_FEATURE_LEDBAR` | `PROJECT_CFG_FEATURE_LEDBAR` | 1 |
| `PROJECT_FEATURE_STORAGE` | `PROJECT_CFG_FEATURE_STORAGE` | 1 |
| `PROJECT_FEATURE_LOG_RECORD` | `PROJECT_CFG_FEATURE_LOG_RECORD` | 1 |
| `PROJECT_FEATURE_PRODUCTION_ID` | `PROJECT_CFG_FEATURE_PRODUCTION_ID` | 1 |

现有 `PROJECT_CFG_WDOG_ENABLE`、`PROJECT_CFG_RTC_ENABLE`、`PROJECT_CFG_HEAT_ENABLE`、`PROJECT_CFG_FACTORY_AGING_ENABLE` 继续作为对应功能的来源，不另起第二套配置。

### 2.3 `Project_Protection.h`

新增保护策略模式头，提供一键切换软/硬件保护：

| `PROJECT_CFG_PROTECTION_MODE` | 保护模式 | 当前含义 |
| --- | --- | --- |
| `0` | `PROJECT_PROTECTION_MODE_AFE_HARDWARE_ONLY` | SH367309 硬件保护为准，MCU 只镜像 AFE fault bit |
| `1` | `PROJECT_PROTECTION_MODE_MCU_SOFTWARE` | 主循环运行 `App_WarnCtrl()` 软件保护判断，适合 bq769x0 等移植项目 |
| `2` | `PROJECT_PROTECTION_MODE_HYBRID` | AFE fault 镜像和 MCU 软件保护都启用 |

当前 309 项目默认 `0`。`SH367309_Func.c` 和 `rtc_sleep.c` 中 `Fault_ChangeToMCU()` 受 `PROJECT_PROTECTION_USES_AFE_HARDWARE` 控制；`Runtime.c` 中 `App_WarnCtrl()` 受 `PROJECT_FEATURE_SOFTWARE_PROTECTION` 控制。非法保护模式会编译期报错。这样后续移植到其他 AFE 时，不需要搜索多个散落开关。

### 2.4 `Platform_Port.h`

新增 MCU 平台边界，当前实现仍映射到 STM32F1 SPL：

- `Platform_ResetMcu()`
- `Platform_FeedWatchdog()`
- `Platform_Get10msTick()`
- `Platform_LatchTaskFlags()`

后续移植 STM32F0 时，优先替换这里以及更底层的 GPIO/CAN/RTC/Flash port，而不是在业务代码里到处替换标准库调用。

### 2.5 `BmsModel.h`

新增运行数据访问入口，当前先把常用全局对象收口到 accessor：

- `BmsModel_CellInfoConst()`
- `BmsModel_SystemStatusConst()`
- `BmsModel_GetPackVoltageMv()`
- `BmsModel_GetChargeCurrentA10()`
- `BmsModel_GetDischargeCurrentA10()`
- `BmsModel_GetSocPercent()`
- `BmsModel_GetSohPercent()`
- `BmsModel_GetCapacityNowAh100()`
- `BmsModel_GetCapacityFactoryAh100()`

`CanFeidaoFrames.c` 已改为通过 `BmsModel.h` 读取运行数据，不再直接引用 `g_stCellInfoReport`。`LedBar.c` 也已把 SOC、fault、MOS 状态读取切到 `BmsModel.h`。`LogRecord.c` 已把 fault/status 读取切到 `BmsModel.h`，并脱离 `main.h`。后续可按模块逐步把 RS485、低功耗等直接读全局的地方继续迁移过来。

`SOC.c` 已脱离 `main.h`，并且不再直接引用 `g_stCellInfoReport`、`OtherElement`、`System_Func_StartUp` 和 `g_u32AfeCurrentSampleSeq`。SOC 入口现在显式依赖：

- `SOC.h`：SOC 模块自身接口和增强算法声明。
- `ADC.h`：Type-C 输出电流和总压 ADC fallback。
- `BmsModel.h`：AFE 样本序号、运行数据、SOC 参数和启动标志访问。

### 2.6 `Project_AppTasks.h`

新增应用层任务声明头，集中声明启动和主循环需要调用的业务入口：

- `InitSci()`
- `InitSystemWakeUp()`
- `App_AFEGet()`
- `App_Sci()`
- `App_AnlogCal()`
- `App_LowPowerProcess()`
- `App_Can()`
- `APP_LedBar()`
- `App_FlashUpdate()`
- `App_LogRecord()`
- `App_ProID_Deal()`

这样 `Runtime.c` 不再需要包含 `main.h` 这个总入口头，后续拆主循环、复用运行框架或移植到其他 MCU 时，不必把整套旧工程头文件一起带过去。

### 2.7 `BoardControl.h`

新增板级控制声明头，集中声明 MOS 和 factory mode 控制入口：

- `open_chg_close_dsg()`
- `open_dsg_close_chg()`
- `enter_fac_mode(bool on)`

`FactoryAging.c` 已通过 `BoardControl.h` 调用老化模式开关，不再通过 `main.h` 间接获得这些入口。后续迁移到 STM32F0 时，这类“产品动作”可以保留接口，底层 MOS/AFe 写法再按目标板适配。

### 2.8 测试模块依赖显式化

`Flash64KAppTest.c` 已从 `main.h` 改为显式包含：

- `Flash64KAppTest.h`
- `PubFunc.h`
- `System_Init.h`
- `<stdio.h>`
- `<string.h>`

这样存储测试模块的真实依赖更清楚，后续要在移植工程里保留或删除该测试入口时，不需要把完整应用头文件一并拖入。

`LowPowerSleep.c` 和 `ProductionID.c` 也已脱离 `main.h`：

- `LowPowerSleep.c` 显式依赖 `Can_HDX.h`、`FactoryAging.h`、`LedBar.h`、`SocEnhance.h`。
- `ProductionID.c` 显式依赖 `DataDeal.h`、`ProductionID.h` 和 `<string.h>`。
- `FactoryAging.c` 显式依赖 `BoardControl.h`、`Flash.h`、`System_Init.h`、`System_Monitor.h` 和 `Project_Features.h`。
- `PubFunc.c`、`System_Monitor.c`、`ChargerLoadFunc.c`、`LedBar.c`、`ADC.c` 已改为显式 include 自己需要的模块头，不再依赖 `main.h`。
- `Project_Types.h` 已承接 10ms 延时常量、`BOOL`、`UPDNLMT16` 等 legacy 公共定义；`IODrivers.c` 不再重复定义这些宏。
- `ADC.c` 的喂狗和 10ms tick 读取已改为 `Platform_Port.h` 包装，Type-C 观测值写入通过 `BmsModel.h` 访问运行模型。

## 3. 运行流程变化

当前默认配置全部保持开启，因此量产行为不应变化。

启动初始化和主循环现在都按同一套模块开关分发。

### 3.1 启动初始化

```c
main()
  -> InitDevice()
       InitSci()        // PROJECT_FEATURE_RS485
       InitAFE1()       // PROJECT_FEATURE_AFE
       InitCan()        // PROJECT_FEATURE_CAN
       InitADC()        // PROJECT_FEATURE_ANALOG_ADC
       InitData_SOC()   // PROJECT_FEATURE_SOC
       Init_IWDG()      // PROJECT_FEATURE_WATCHDOG
  -> InitVar()
  -> Init_RTC()         // PROJECT_FEATURE_RTC_LOW_POWER
  -> while (1) Runtime_RunOnce()
```

`InitE2PROM()` 当前仍保留为基础参数加载入口，不随 `PROJECT_FEATURE_STORAGE` 关闭，因为保护参数、通信参数和 SOC 参数都依赖 EEPROM/RW 参数区。

### 3.2 主循环调度

`Runtime.c` 现在按模块开关分发，且不再包含 `main.h`：

```c
Runtime_RunOnce()
  -> Runtime_RunFrontTasks()
       Platform_LatchTaskFlags()
       FactoryAging_Task()       // PROJECT_FEATURE_FACTORY_AGING
       APP_LedBar()              // PROJECT_FEATURE_LEDBAR
       App_WarnCtrl()            // PROJECT_FEATURE_SOFTWARE_PROTECTION
       App_AFEGet()              // PROJECT_FEATURE_AFE
  -> Runtime_RunIoAndPowerTasks()
       App_Sci()                 // PROJECT_FEATURE_RS485
       App_AnlogCal()            // PROJECT_FEATURE_ANALOG_ADC
       App_LowPowerProcess()     // PROJECT_FEATURE_RTC_LOW_POWER
       App_Can()                 // PROJECT_FEATURE_CAN
  -> Runtime_RunBackgroundTasks()
       StorageFlash_AppUseTest_Task() // PROJECT_FEATURE_STORAGE
       App_Heat_Cool_Ctrl()           // PROJECT_FEATURE_HEAT
       App_FlashUpdate()              // PROJECT_FEATURE_STORAGE
       App_LogRecord()                // PROJECT_FEATURE_LOG_RECORD
       App_ProID_Deal()               // PROJECT_FEATURE_PRODUCTION_ID
       Platform_FeedWatchdog()        // PROJECT_FEATURE_WATCHDOG
```

`DataDeal.c` 中 AFE 采样完成后的 `App_SOC()` 也受 `PROJECT_FEATURE_SOC` 控制。

`Project_BuildGuard.h` 已增加约束：当前运行架构下 `PROJECT_CFG_FEATURE_SOC=1` 时必须保持 `PROJECT_CFG_FEATURE_AFE=1`。原因是现有 SOC 正常更新入口挂在 `App_AFEGet()` 采样完成之后；若关闭 AFE 但保留 SOC，SOC 不会得到新样本。

## 4. 如何裁剪功能

在 `103 + 309/Project/Source/conf/Project_Config.h` 的“模块裁剪开关”中把对应宏改为 `0`：

```c
#define PROJECT_CFG_FEATURE_CAN 0
#define PROJECT_CFG_FEATURE_LEDBAR 0
```

当前效果是“不再从启动初始化和主运行循环调用该任务”。这适合快速验证、移植前裁剪、或构建轻功能版本。

需要注意：

1. 关闭运行入口不等于完全删除编译单元。Keil 工程里仍可能编译对应 `.c` 文件。
2. 如果要做到彻底移除某功能，还需要继续增加 stub 或按 Keil target 排除对应源文件。
3. 低功耗、SOC、通信之间仍共享运行数据，不能只删 `.c` 文件不处理接口。

## 5. 移植 STM32F0 的推荐路线

### 第一阶段：保持业务不动，先换 platform 层

1. 保留 `Runtime.c`、SOC、通信协议和数据模型。
2. 新建 F0 工程后先实现等价的 GPIO、TIM、RTC、CAN、Flash、IWDG 基础驱动。
3. 把 `Platform_Port.h` 的实现切到 STM32F0 SPL 或 HAL。
4. 保持 `Project_Features.h` 默认开启，先跑通完整功能；再逐项关闭不需要的模块。

### 第二阶段：按模块建立 port/stub

建议顺序：

1. GPIO/时基/IWDG
2. Flash/BKP/RTC
3. AFE/I2C
4. CAN/RS485
5. LedBar
6. Production ID/日志/测试入口

每个模块都应提供明确的 `*_Port.h` 或 stub，而不是继续让业务层直接包含 MCU 头文件。

### 第三阶段：收口全局数据访问

优先迁移这些直接读写全局数据的模块：

1. `Sci_Upper.c`
2. `LedBar.c`
3. `rtc_sleep.c`
4. `IO_Control.c`
5. `Fault.c`

目标是让通信、显示、低功耗读取 `BmsModel.h` 或后续 `BmsModel.c` 提供的接口，而不是到处直接操作 `g_stCellInfoReport`。

## 6. 风险与边界

本次改动属于“解耦基础设施”，不是完整架构重写。当前仍有这些风险：

1. `main.h` 仍然是多数旧模块的重包含入口；本轮已先移除 `Runtime.c`、`CanFeidaoFrames.c`、`SOC.c`、`Flash.c`、`EEPROM.c`、`Flash64KAppTest.c`、`LogRecord.c`、`LowPowerSleep.c`、`ProductionID.c`、`FactoryAging.c`、`Fault.c`、`Heat_Cool.c`、`ShortFunc.c`、`System_Init.c`、`RTC.c`、`SleepDeal.c` 对它的依赖，并把 `LedBar.c` 的运行数据读取切到 `BmsModel.h`，后续需要逐步推进到 `Sci_Upper.c`、`rtc_sleep.c` 等模块。
2. 新增 feature gate 只保证运行入口可关闭，不保证所有依赖都能从 Keil 工程中直接删除。
3. `BmsModel.h` 当前还是 inline accessor，底层仍读取原全局对象；它是迁移入口，不是最终数据模型重构。
4. 保护策略现在可一键切换，但 `Fault.c` 的软件保护阈值、滤波节拍和目标板 MOS 动作仍需按新 AFE/新产品重新上板验证。
5. 移植 STM32F0 时，CAN、RTC、Flash、BKP、EXTI 和 GPIO AFIO 差异仍需要逐项实测。

## 7. 验收方式

本次新增 `tools/project_check.py` 检查项：

1. 确认新增基础头文件存在。
2. 确认 `Project_Config.h` 暴露模块裁剪开关。
3. 确认 `Runtime.c` 使用 feature gate 和 platform wrapper。
4. 确认 `DataDeal.c` 的 SOC 触发受 `PROJECT_FEATURE_SOC` 控制。
5. 确认 `CanFeidaoFrames.c` 通过 `BmsModel.h` 读取运行数据。
6. 确认 `Runtime.c` 和 `CanFeidaoFrames.c` 不再包含 `main.h`。
7. 确认 `main.c` 启动初始化也按 feature gate 裁剪。
8. 确认 `LedBar.c` 不再直接读取 `g_stCellInfoReport` 或 `SystemStatus.bits`。
9. 确认 `SOC.c` 不再包含 `main.h`，且不直接读取核心全局模型对象。
10. 确认 `Flash.c`、`EEPROM.c`、`Flash64KAppTest.c` 使用显式依赖而不是 `main.h`，并删除未调用的 `FlashTest()`、byte EEPROM stub 和旧 offset 变量。
11. 确认 `LogRecord.c` 不再包含 `main.h`，且通过 `BmsModel.h` 读取 fault/status。
12. 确认 `LowPowerSleep.c` 和 `ProductionID.c` 使用显式依赖而不是 `main.h`。
13. 确认 `FactoryAging.c` 通过 `BoardControl.h` 调用板级 MOS/factory mode 入口，不再包含 `main.h`。
14. 确认 `Project_Protection.h` 提供三种保护模式，`Runtime.c`、`SH367309_Func.c` 和 `rtc_sleep.c` 都按保护模式调度软/硬件保护。
15. 确认 `PubFunc.c`、`System_Monitor.c`、`ChargerLoadFunc.c`、`LedBar.c`、`ADC.c`、`Fault.c`、`Heat_Cool.c`、`ShortFunc.c`、`System_Init.c`、`RTC.c`、`SleepDeal.c`、`Flash.c`、`EEPROM.c` 已脱离 `main.h`，公共 legacy 宏统一在 `Project_Types.h`。

建议每次做模块裁剪或移植前执行：

```bash
python3 tools/project_check.py --quiet
python3 tools/soc_replay_test.py
python3 tools/run_soc_host_c_test.py
```

上板验证时，仍需按真实硬件确认 CAN ACK、低功耗 RTC 唤醒、AFE 恢复和 LedBar 显示窗口。
