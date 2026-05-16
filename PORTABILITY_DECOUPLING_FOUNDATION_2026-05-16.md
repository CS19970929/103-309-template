# 可移植解耦基础层说明 2026-05-16

本文记录本次独立分支对工程耦合关系的第一阶段收口。目标不是一次性把整套 F103 标准库工程改成通用框架，而是在不改变当前量产运行行为的前提下，先建立后续移植 STM32F0 或裁剪功能时必须具备的边界。

## 1. 当前主要耦合点

当前工程仍然是典型历史嵌入式项目结构：

1. `main.h` 汇总包含大部分业务、驱动和协议头文件，很多 `.c` 文件通过它间接获得全部全局对象和硬件定义。
2. `g_stCellInfoReport`、`OtherElement`、`SystemStatus` 等全局数据被通信、显示、SOC、保护和低功耗模块直接读取。
3. `Runtime.c` 原本直接按固定顺序调用 AFE、RS485、低功耗、CAN、存储、日志、Production ID 等任务，模块是否启用不够集中。
4. `stm32f10x.h`、`NVIC_SystemReset()`、`IWDG_Feed()`、`SysTime_Get10msTickCount()` 等 MCU/标准库接口散落在业务路径里，后续移植 STM32F0 时没有统一替换点。

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
| `PROJECT_FEATURE_SOC` | `PROJECT_CFG_FEATURE_SOC` | 1 |
| `PROJECT_FEATURE_ANALOG_ADC` | `PROJECT_CFG_FEATURE_ANALOG_ADC` | 1 |
| `PROJECT_FEATURE_RS485` | `PROJECT_CFG_FEATURE_RS485` | 1 |
| `PROJECT_FEATURE_CAN` | `PROJECT_CFG_FEATURE_CAN` | 1 |
| `PROJECT_FEATURE_LEDBAR` | `PROJECT_CFG_FEATURE_LEDBAR` | 1 |
| `PROJECT_FEATURE_STORAGE` | `PROJECT_CFG_FEATURE_STORAGE` | 1 |
| `PROJECT_FEATURE_LOG_RECORD` | `PROJECT_CFG_FEATURE_LOG_RECORD` | 1 |
| `PROJECT_FEATURE_PRODUCTION_ID` | `PROJECT_CFG_FEATURE_PRODUCTION_ID` | 1 |

现有 `PROJECT_CFG_WDOG_ENABLE`、`PROJECT_CFG_RTC_ENABLE`、`PROJECT_CFG_HEAT_ENABLE`、`PROJECT_CFG_FACTORY_AGING_ENABLE` 继续作为对应功能的来源，不另起第二套配置。

### 2.3 `Platform_Port.h`

新增 MCU 平台边界，当前实现仍映射到 STM32F1 SPL：

- `Platform_ResetMcu()`
- `Platform_FeedWatchdog()`
- `Platform_Get10msTick()`
- `Platform_LatchTaskFlags()`

后续移植 STM32F0 时，优先替换这里以及更底层的 GPIO/CAN/RTC/Flash port，而不是在业务代码里到处替换标准库调用。

### 2.4 `BmsModel.h`

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

`CanFeidaoFrames.c` 已改为通过 `BmsModel.h` 读取运行数据，不再直接引用 `g_stCellInfoReport`。这只是第一处落点，后续可按模块逐步把 RS485、LedBar、低功耗等直接读全局的地方迁移过来。

## 3. 运行流程变化

当前默认配置全部保持开启，因此量产行为不应变化。

`Runtime.c` 现在按模块开关分发：

```c
Runtime_RunOnce()
  -> Runtime_RunFrontTasks()
       Platform_LatchTaskFlags()
       FactoryAging_Task()       // PROJECT_FEATURE_FACTORY_AGING
       APP_LedBar()              // PROJECT_FEATURE_LEDBAR
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

## 4. 如何裁剪功能

在 `103 + 309/Project/Source/conf/Project_Config.h` 的“模块裁剪开关”中把对应宏改为 `0`：

```c
#define PROJECT_CFG_FEATURE_CAN 0
#define PROJECT_CFG_FEATURE_LEDBAR 0
```

第一阶段效果是“不再从主运行循环调用该任务”。这适合快速验证、移植前裁剪、或构建轻功能版本。

需要注意：

1. 关闭运行入口不等于完全删除编译单元。Keil 工程里仍可能编译对应 `.c` 文件。
2. 如果要做到彻底移除某功能，还需要第二阶段增加 stub 或按 Keil target 排除对应源文件。
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

1. `main.h` 仍然是重包含入口，后续需要逐步减少各模块对 `main.h` 的依赖。
2. 新增 feature gate 只保证运行入口可关闭，不保证所有依赖都能从 Keil 工程中直接删除。
3. `BmsModel.h` 当前还是 inline accessor，底层仍读取原全局对象；它是迁移入口，不是最终数据模型重构。
4. 移植 STM32F0 时，CAN、RTC、Flash、BKP、EXTI 和 GPIO AFIO 差异仍需要逐项实测。

## 7. 验收方式

本次新增 `tools/project_check.py` 检查项：

1. 确认新增基础头文件存在。
2. 确认 `Project_Config.h` 暴露模块裁剪开关。
3. 确认 `Runtime.c` 使用 feature gate 和 platform wrapper。
4. 确认 `DataDeal.c` 的 SOC 触发受 `PROJECT_FEATURE_SOC` 控制。
5. 确认 `CanFeidaoFrames.c` 通过 `BmsModel.h` 读取运行数据。

建议每次做模块裁剪或移植前执行：

```bash
python3 tools/project_check.py --quiet
python3 tools/soc_replay_test.py
python3 tools/run_soc_host_c_test.py
```

上板验证时，仍需按真实硬件确认 CAN ACK、低功耗 RTC 唤醒、AFE 恢复和 LedBar 显示窗口。
