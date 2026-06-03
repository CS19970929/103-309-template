# BMS 项目模块完整参考文档

> 生成日期: 2026-06-01
> 项目: T3 BMS (Battery Management System)
> MCU: STM32F103 (标准外设库)
> AFE: SH367309

---

## 目录

1. [项目架构概览](#1-项目架构概览)
2. [配置文件与宏系统](#2-配置文件与宏系统)
3. [主循环与运行框架](#3-主循环与运行框架)
4. [SOC 模块](#4-soc-模块)
5. [ADC 采样模块](#5-adc-采样模块)
6. [CAN 通讯模块](#6-can-通讯模块)
7. [LED 数码管显示模块](#7-led-数码管显示模块)
8. [RTC 低功耗休眠模块](#8-rtc-低功耗休眠模块)
9. [AFE 通信模块 (I2C)](#9-afe-通信模块-i2c)
10. [工厂老化模块](#10-工厂老化模块)
11. [保护参数与故障模块](#11-保护参数与故障模块)
12. [串口通信模块 (Modbus/RS485)](#12-串口通信模块-modbusrs485)
13. [Flash 存储模块](#13-flash-存储模块)
14. [日志记录模块](#14-日志记录模块)
15. [系统监控模块](#15-系统监控模块)
16. [MOS 管启动控制](#16-mos-管启动控制)
17. [低功耗调度模块](#17-低功耗调度模块)
18. [全局变量总览](#18-全局变量总览)
19. [中断向量与外设资源](#19-中断向量与外设资源)
20. [Flash 地址映射](#20-flash-地址映射)

---

## 1. 项目架构概览

### 1.1 文件组织

```
103 + 309/Project/Source/
├── main.c/h              - 主入口
├── AppInit.c/h           - 启动初始化
├── Runtime.c/h           - 主循环调度
├── conf/
│   ├── conf.c/h          - IO/唤醒配置, 全局类型定义
│   ├── conf_gpio.h       - GPIO 引脚定义
│   ├── Project_Config.h  - 核心编译配置 (Keil Configuration Wizard)
│   └── Project_BuildGuard.h - 编译时配置校验
├── SOC.c/h               - SOC 顶层调度
├── SocEnhance.c/h        - SOC 核心增强算法 (安时积分+OCV校准)
├── ADC.c/h               - ADC 采样 (VBUS, TypeC电流, 温度)
├── Can_HDX.c/h           - CAN 通信 (飞道协议 + 应用层命令)
├── CanFeidaoFrames.c/h   - CAN 周期帧生成
├── LedBar.c/h            - LED 数码管 Charlieplexing 显示
├── RTC.c/h               - RTC 硬件驱动
├── rtc_sleep.c/h         - RTC 低功耗休眠核心逻辑
├── rtc_sleep_port.c/h    - RTC 休眠硬件抽象层 (port)
├── rtc_sleep_afe_sh367309.c/h - SH367309 AFE 的 port 实现
├── SleepDeal.c/h         - 深度休眠处理
├── LowPowerSleep.c/h     - 低功耗进入/退出
├── bsp_clock.c/h         - 时钟配置
├── bsp_power.c/h         - 电源管理
├── bsp_rtc.c/h           - RTC 基础驱动封装
├── DataDeal.c/h          - 数据处理 (AFE数据获取, 零电流校准)
├── I2C_AFE1.c/h          - AFE I2C 通信驱动
├── SH367309_DataDeal.c/h - SH367309 寄存器解析
├── SH367309_Func.c/h     - SH367309 功能控制
├── FactoryAging.c/h      - 工厂老化模式
├── Fault.c/h             - 保护参数与故障标志
├── FaultSnapshot.h       - 故障快照
├── Sci_Upper.c/h         - 串口 Modbus 协议处理
├── Flash.c/h             - Flash 存储管理
├── EEPROM.c/h            - EEPROM 兼容层 (已废弃, 改用Flash)
├── LogRecord.c/h         - 事件日志记录
├── System_Init.c/h       - 系统定时器, 位带操作, IWDG
├── System_Monitor.c/h    - 系统状态/错误/功能开关管理
├── MosStartup.c/h        - MOS 管启动控制
├── PubFunc.c/h           - 公共函数 (CRC, 查表, 排序等)
├── ProductionID.c/h      - 产品序列号/版本管理
├── ShortFunc.c/h         - 短函数助手
└── UpgradeParamPolicy.c/h - 升级参数策略
```

### 1.2 模块依赖关系

```
main()
 ├─ AppInit_Boot()
 │   ├─ SystemInit()                    - CMSIS 系统初始化
 │   ├─ InitIO()                        - GPIO 初始化
 │   ├─ InitNVIC()                      - NVIC 配置
 │   ├─ AppInit_InitSci()               - 串口初始化
 │   ├─ InitE2PROM()                    - EEPROM/Flash 参数加载
 │   ├─ InitAFE1()                      - AFE I2C 初始化
 │   ├─ InitCan()                       - CAN 初始化
 │   ├─ InitADC()                       - ADC+DMA 初始化
 │   ├─ InitData_SOC()                  - SOC 数据初始化
 │   ├─ InitTimer()                     - TIM3 10ms 时基启动
 │   ├─ Init_IWDG()                     - 看门狗初始化
 │   ├─ InitSystemMonitorData_EEPROM()  - 系统状态初始化
 │   └─ Init_RTC()                      - RTC 初始化
 │
 └─ Runtime_RunOnce() [主循环]
     ├─ SysTime_LatchTaskFlags()        - 锁存时基标志
     ├─ FactoryAging_Task()             - 老化任务
     ├─ APP_LedBar()                    - LED 显示
     ├─ App_AFEGet()                    - AFE 数据获取
     ├─ AppInit_ServiceSci()            - 串口服务
     ├─ App_AnlogCal()                  - ADC 模拟量计算
     ├─ rtc_sleep()                     - 低功耗调度
     │   └─ rtc_sleep()                 - RTC 休眠核心
     │       └─ rtc_sleep_run_hiccup_cycle() - STOP 模式循环
     ├─ App_Can()                       - CAN 服务
     ├─ StorageFlash_AppUseTest_Task()  - Flash 应用测试
     ├─ App_FlashUpdate()               - Flash 参数更新
     ├─ App_LogRecord()                 - 日志记录
     └─ Feed_IWatchDog                  - 喂狗
```

---

## 2. 配置文件与宏系统

### 2.1 Project_Config.h - 核心编译配置

#### 构建配置
| 宏定义 | 默认值 | 说明 |
|--------|--------|------|
| `PROJECT_CFG_BUILD_PROFILE` | 0 | 0=Release, 1=Debug, 2=Factory/Test |
| `PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG` | 0x2445 | EEPROM 初始化标志 |
| `PROJECT_CFG_BAT_TYPE` | 1 | 0=BAT_MASTER(20A), 1=BAT_SLAVE(40A) |
| `PROJECT_CFG_BAT_CHEMISTRY` | 0 | 0=TERNARYLI(三元锂), 1=LIFEPO(磷酸铁锂) |
| `PROJECT_CFG_AFE_TYPE` | 1 | 0=bq76xx, 1=sh36xx |
| `PROJECT_CFG_LEVEL_CURR` | 2 | 0=80A,1=100A,2=150A,3=200A,4=250A |
| `PROJECT_CFG_VERSION` | 5 | 固件版本号 |
| `PROJECT_CFG_FD_YEAR` | 26 | 固件年份 |
| `PROJECT_CFG_FD_MONTH` | 5 | 固件月份 |
| `PROJECT_CFG_FD_DAY` | 9 | 固件日期 |

#### 功能开关
| 宏定义 | 默认值 | 说明 |
|--------|--------|------|
| `PROJECT_CFG_WDOG_ENABLE` | 1 | 使能 IWDG 独立看门狗 |
| `PROJECT_CFG_RTC_ENABLE` | 1 | 使能 RTC 时钟 |
| `PROJECT_CFG_IAP_ENABLE` | 1 | 使能 IAP 升级 |
| `PROJECT_CFG_FACTORY_AGING_ENABLE` | 1 | 使能工厂老化模式 |
| `PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS` | 259200 | 老化时间(3天) |
| `PROJECT_CFG_IDLE_SLEEP_ENABLE` | 0 | 空闲时进入 Sleep 模式 |
| `PROJECT_CFG_LED_FUNC_ENABLE` | 0 | 使能旧 LED 功能标志 |
| `PROJECT_CFG_DEBUG_CODE_ENABLE` | 0 | 使能调试代码 |
| `PROJECT_CFG_DEBUG_WATCH_ENABLE` | 0 | 使能 Keil Watch 调试导出 |
| `PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE` | Debug=1 | 使能串口日志 |
| `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE` | 0 | 使能 Flash 启动诊断打印 |

#### 唤醒源配置
| 宏定义 | 默认值 | 说明 |
|--------|--------|------|
| `PROJECT_CFG_UART1_WAKEUP_ENABLE` | 1 | UART1 唤醒 |
| `PROJECT_CFG_UART2_WAKEUP_ENABLE` | 0 | UART2 唤醒 |
| `PROJECT_CFG_RS485_WAKEUP_ENABLE` | 1 | RS485 唤醒 |
| `PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE` | 1 | 长按键开关 |

#### CAN 配置

当前 CAN 运行态已删除 active/probe/no-ACK 软件退避配置；周期广播按固定 1000ms/5000ms 调度。

#### SOC 配置
| 宏定义 | 默认值 | 说明 |
|--------|--------|------|
| `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE` | 0 | 允许运行时写 SOC 表 |
| `PROJECT_CFG_HOST_WRITE_ENABLE` | 1 | 允许上位机写寄存器 |
| `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` | 30 | 板子自耗电(mA) |
| `PROJECT_CFG_SOC_REST_OCV_SECONDS` | 1800 | 静置 OCV 等待时间 |
| `PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT` | 1 | SOC 校准步长(%) |
| `PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS` | 5 | 正常显示下降速度(秒/1%) |
| `PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS` | 1 | 低压显示下降速度 |
| `PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS` | 15 | 满电确认时间 |
| `PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS` | 30 | 电压跌落抑制时间 |
| `PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV` | 400 | 低压尾部开始偏移 |

#### LED 配置
| 宏定义 | 默认值 | 说明 |
|--------|--------|------|
| `PROJECT_CFG_LEDBAR_SLEEP_ENABLE` | 1 | 休眠关闭 LED |
| `PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS` | 500 | SOC 显示时间(5秒) |
| `PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS` | 1000 | 唤醒显示时间(10秒) |
| `PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS` | 50 | 扫描定时器周期 |

#### 升级参数策略
| 宏定义 | 默认值 | 说明 |
|--------|--------|------|
| `PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE` | 1 | 使能升级参数策略 |
| `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` | 0x0005 | 策略版本号 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_AFE` | 1 | 升级时复位 AFE 参数 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_PROTECT` | 1 | 升级时复位保护参数 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE` | 1 | 升级时复位 SOC 表 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG` | 1 | 升级时复位 SOC 配置 |
| `PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT` | 1 | 升级时复位 SOC 快照 |

### 2.2 conf.h - 宏条件编译派生

conf.h 将 Project_Config.h 中的数值宏转换为条件编译宏:

```c
// 化学体系
TERNARYLI  (PROJECT_CFG_BAT_CHEMISTRY == 0)
LIFEPO     (PROJECT_CFG_BAT_CHEMISTRY == 1)

// 功能宏
wdog_enable          (PROJECT_CFG_WDOG_ENABLE)
__FUNC_RTC__         (PROJECT_CFG_RTC_ENABLE)
_IAP                 (PROJECT_CFG_IAP_ENABLE)

// 唤醒源
UART1_WAKEUP_ENABLE  (PROJECT_CFG_UART1_WAKEUP_ENABLE)
RS485_WAKEUP_ENABLE  (PROJECT_CFG_RS485_WAKEUP_ENABLE)
_DI_SWITCH_longKEY_ONOFF (PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE)

// 调试
_DEBUG_CODE          (PROJECT_CFG_DEBUG_CODE_ENABLE)
ELOG_OUTPUT_ENABLE   (debug serial log)

// 通讯角色
_COMMOM_UPPER_SCI1   上位机通讯 SCI1
_COMMOM_UPPER_SCI2   上位机通讯 SCI2
_COMMOM_UPPER_SCI3   上位机通讯 SCI3

// 其他功能
__VIRTURE_CURRENT__  虚拟电流
FLASH64K_APP_QUICK_TEST_ENABLE  64K Flash 快速测试
FLASH64K_APP_USE_TEST_ENABLE    64K Flash 应用测试
SOC_TEST padding                兼容占位，当前无活动 SOC 测试模式宏
```

### 2.3 conf_gpio.h - GPIO 引脚定义

| 宏定义 | GPIO | 功能 |
|--------|------|------|
| `MCUO_DEBUG_LED1` | PB15 | 调试 LED |
| `MCUO_AFE_VPRO` | PB0 | AFE 供电控制 |
| `MCUO_AFE_CTLC` | PB14 | AFE CTLC 控制 |
| `GPIO_SCI1_TX/RX` | PB6/PB7 | 串口1 |
| `GPIO_CHG_IN` | PA0 | 充电检测输入 |
| `GPIO_INT_WK_CMNT` | PB12 | 通讯唤醒中断 |
| `GPIO_MCC_C` | PA8 | MCC 控制 |
| `GPIO_MCU_WK` | PB13 | MCU 唤醒信号 |
| `GPIO_SW` | PA9 | 用户开关 |
| `GPIO_AFE1_CTL` | PB14 | AFE1 控制 |
| `GPIO_DC_EN` | PA10 | DC 使能 |
| `GPIO_DBG_LED` | PB15 | 调试 LED |
| `GPIO_SPI_MOSI` | PA6 | SPI MOSI (LED) |
| `GPIO_RF_EN` | PA7 | RF 使能 |
| `GPIO_AFE1_PRO_EN` | PB0 | AFE1 保护使能 |
| `GPIO_ADC_VBUS` | PA1 | VBUS ADC 输入 |
| `GPIO_SPI1_NSS` | PA4 | SPI NSS (LED) |
| `GPIO_SPI1_SCK` | PA5 | SPI SCK (LED) |
| `GPIO_ADC_NMOS` | PB1 | NMOS ADC 输入 |
| `GPIO_ADC_CUR` | PA2 | TypeC 电流 ADC |
| `GPIO_2727_EN` | PA3 | 升压使能 |
| `GPIO_SEG_EN` | PB10 | 数码管段使能 (LED) |
| `GPIO_M_STB` | PA15 | 主电源待机 |
| `GPIO_AD_EN` | PB3 | AD 使能 |
| `GPIO_CMNT_EN` | PB4 | CAN 收发器供电 |
| `GPIO_ADC_BUS_EN` | PB5 | ADC 总线使能 |

---

## 3. 主循环与运行框架

### 3.1 main.c

**文件**: `main.c`

```c
int main(void) {
    AppInit_Boot();    // 一次性初始化
    while (1) {
        Runtime_RunOnce();  // 主循环单次迭代
    }
}
```

### 3.2 AppInit.c - 启动初始化

**文件**: `AppInit.c`

**全局变量**:
- `UINT8 SeriesNum = 10` - 电池串数

**函数**:

| 函数 | 功能 |
|------|------|
| `AppInit_InitDevice()` | 硬件初始化: SystemInit→InitDelay→IsSleepStartUp→jtag_disable→InitNVIC→InitIO→串口→FlashBootPrint→FlashQuickTest→InitE2PROM→InitAFE1→InitCan→InitADC→InitData_SOC→InitTimer→__enable_irq→elog→EnableLowPowerDebug→Init_IWDG |
| `AppInit_InitRuntimeState()` | 运行时状态初始化: 加载 SystemMonitor 参数, 设置 CS 电阻比, 标记启动完成 |
| `AppInit_Boot()` | 总启动入口: 设备初始化 → 运行时状态 → Init_RTC |

### 3.3 Runtime.c - 主循环调度

**文件**: `Runtime.c`

**主循环流程** (Release 模式):

```
Runtime_RunOnce()
  └─ Runtime_RunNormalOnce()
      ├─ Runtime_RunFrontTasks()
      │   ├─ SysTime_LatchTaskFlags()    - 锁存10ms时基标志
      │   ├─ FactoryAging_Task()         - 老化任务(每10ms)
      │   ├─ APP_LedBar()                - LED刷新(每100ms)
      │   └─ App_AFEGet()               - AFE数据获取
      ├─ Runtime_RunIoAndPowerTasks()
      │   ├─ AppInit_ServiceSci()        - 串口服务
      │   ├─ App_AnlogCal()              - ADC计算
      │   ├─ rtc_sleep()                 - 低功耗调度
      │   └─ App_Can()                   - CAN服务
      ├─ Runtime_RunBackgroundTasks()
      │   ├─ StorageFlash_AppUseTest_Task()
      │   ├─ App_FlashUpdate()           - Flash更新
      │   ├─ App_LogRecord()             - 日志记录
      │   ├─ App_ProID_Deal()            - 产品ID heartbeat hook
      │   └─ Feed_IWatchDog              - 喂狗
      └─ Runtime_TryIdleSleep()          - 空闲时 WFI (可配置)
```

**Debug 模式**:
- 只运行 `App_AFEGet()` + `AppInit_ServiceSci()`

### 3.4 System_Init - 时基系统

**文件**: `System_Init.c/h`

**全局变量**:
- `volatile union SYS_TIME g_st_SysTimeFlag` - 时基标志位
  - `bits.b1Sys10msFlag` - 10ms 标志
  - `bits.b1Sys50msFlag` - 50ms 标志
  - `bits.b1Sys100msFlag` - 100ms 标志
  - `bits.b1Sys200msFlag` - 200ms 标志
  - `bits.b1Sys1000msFlag` - 1000ms 标志

**时基生成**:
- TIM3 产生 10ms 中断 (10ms = 1 tick)
- `SysTime_LatchTaskFlags()` 在主循环开始时锁存所有标志
- `SysTime_Get10msTickCount()` 返回累计的 10ms tick 数

**函数**:
| 函数 | 功能 |
|------|------|
| `InitTimer()` | 初始化 TIM3, 100μs 中断→10ms 时基 |
| `InitDelay()` | 初始化 SysTick 延时 |
| `SysTime_LatchTaskFlags()` | 锁存当前时基标志 |
| `SysTime_HasPendingTaskFlags()` | 检查是否有待处理时基任务 |
| `SysTime_Get10msTickCount()` | 获取 10ms tick 计数 |
| `InitNVIC()` | NVIC 优先级分组配置 |
| `Init_IWDG()` | 独立看门狗初始化 |
| `IWDG_Feed()` / `Feed_IWatchDog` | 喂狗 |
| `EnableLowPowerDebug()` | 使能低功耗调试 (DBGMCU) |

---

## 4. SOC 模块

### 4.1 架构

SOC 模块分为两层:
- **SOC.c** - 顶层调度、配置装载、Type-C 电流折算、AFE sample seq 触发
- **SocEnhance.c** - 核心算法：容量积分、满电/低压/中段/静置/RTC 校准、SOC 显示平滑和 snapshot 保存

当前完整逻辑、源码 review、校准条件和验证边界以 `docs/design/soc_design.md` 为准；无放电快降专题见 `docs/review/soc_rest_fast_drop_analysis_2026-06-03.md`。

### 4.2 SOC.c - 顶层

**文件**: `SOC.c/h`

**当前关键事实**:

- `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE = 0` 时，上位机运行时 SOC 表不参与量产算法。
- 当前 `PROJECT_CFG_BAT_CHEMISTRY = 0`，编译期使用 `SocTable_TernaryLi`。
- `SOC.c` 不直接持有当前 SOC 表；OCV 表选择在 `SocEnhance.c` 中完成。

**函数**:

| 函数 | 功能 |
|------|------|
| `InitData_SOC()` | SOC 初始化: 加载配置→初始化参数库；内部由 `soc_param_lib_init()` 完成强制发布 |
| `App_SOC()` | SOC 主任务: 检测新 AFE 采样→计算净电流→调用核心算法→发布 |

**TypeC 电流折算**:
```c
// 将 TypeC 输出电流折算为电池侧等效电流
电池侧电流 = TypeC电流 × 9V / 电池电压 / 90%效率
```

**关键逻辑**:
- 通过 `AfeCurrent_GetSeq()` 检测是否有新 AFE 采样
- 净电流 = 报告充电电流 - (报告放电电流 + TypeC折算电流)
- 每次新采样调用 `SOC_UpdateSampleData()` 和 `SOC_IntEnhance_Ctrl()`

### 4.3 SocEnhance - SOC 核心算法

**文件**: `SocEnhance.c/h`

**全局变量**:
- `struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element` - SOC 核心数据
- `struct SOC_DEBUG_WATCH * const g_dbg_soc_watch` - 调试观察 (仅 DEBUG_WATCH)

**SOC_ENHANCE_ELEMENT 结构体**:
| 字段组 | 字段 | 说明 |
|---|---|---|
| 配置快照 | `u16_SOC_Ah/u16_SOC_CycleT_Ever/u16_SOC_TableSelect/u16_SOC_0_Vol/u16_SOC_100_Vol` | 从 `OtherElement` 装载 |
| 命令 payload | `u8_SetSocOnce/u16_RefreshData_Flag` | 由 `SOC_Request*()` 写入，`soc_handle_command()` 消费 |
| 输入采样 | `u16_VCellMax/u16_VCellMin/u16_Ichg/u16_Idsg` | SOC 计算输入 |
| 发布输出 | `u8_SOC/u8_SOH/u16_CapacityNow/u16_CapacityFull/u16_CapacityFactory/u16_Cycle_times` | 对外显示 SOC、SOH、容量、循环次数 |

**核心函数**:

| 函数 | 功能 |
|------|------|
| `soc_param_lib_init()` | 初始化 SOC 参数库 (容量, 表, 快照恢复等) |
| `SOC_UpdateSampleData(vmax,vmin,ichg,idsg)` | 更新 SOC 采样输入 |
| `SOC_IntEnhance_Ctrl()` | SOC 核心控制: 命令、积分、tail/full/deferred/rest、保存、发布 |
| `SOC_PublishReportData()` | 将显示 SOC 和容量字段发布到 `g_stCellInfoReport` |
| `SOC_ApplyRtcRelaxationCompensation()` | HICCUP RTC STOP 周期内静置 OCV 补偿；当前不额外扣 RTC 自耗 |
| `SOC_SaveSnapshotBeforeSleep()` | 休眠前保存 SOC 快照 |
| `SOC_ResetStoredSnapshotToDefault()` | 复位 SOC 快照到默认值 |

**SOC 校准源** (enum SOC_WATCH_CALIB_SOURCE):
- `INTEGRATE_CHG/DSG` - 安时积分
- `FULL_ANCHOR` - 满电锚定 (充满确认)
- `EMPTY_TAIL` - 低压尾端
- `MID_TAIL` - 中段尾端
- `REST_TARGET` - 静置目标接近
- `DEFERRED_OCV` - 延迟 OCV 校准
- `LONG_REST_DOWN` - 长时间静置下调
- `RTC_REST` - RTC 休眠静置补偿
- `BOARD_SELF_CONSUMPTION` - 正常运行 RELAX 自耗积分
- `STARTUP_SNAPSHOT/OCV/DEFAULT` - 启动时恢复
- `MANUAL_OCV/PARAM_RESET/SET_ONCE` - 上位机命令校准

**自耗与 RTC 口径**:

- 正常运行 `RELAX/CHG/DSG` 都会把 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA` 计入容量积分。
- RTC STOP 补偿当前不额外扣自耗，只按休眠秒数推进静置 OCV 计数和下修。

**发布口径**:

- 内部估算值是 `s_soc.soc`。
- 用户显示和通信发布值是 `s_soc.display_soc`。
- `g_stCellInfoReport.SocElement.u16Soc` 当前发布 `display_soc`，不是直接发布内部估算值。
- `NORMAL/DEEP` reset sleep 快显读取 BKP 中睡前保存的显示 SOC；`HICCUP_MODE` RTC STOP 周期会先做 SOC 休眠补偿，最终按键唤醒后才请求 LedBar 显示。

**调试观察**:

- `SOC_WATCH_BLOCK_REASON` 和 `u8LastBlockReason` 已删除。
- 当前优先观察 `u8LastCalibSource`、`u8LowTailActive/u8MidTailActive`、`u8InternalSoc/u8DisplaySoc`、`u32RestTicks/u32LongRestDownTicks`。
- debug monitor 中删除了固定 0 或伪造派生字段，保留真实内部计数和 `display_ticks`。

**OCV 表** (42 个条目, 21对电压-SOC):
- 三元锂: 4160mV(100%) → 3000mV(0%)
- 磷酸铁锂: 独立表

---

## 5. ADC 采样模块

### 5.1 架构

**文件**: `ADC.c/h`

**ADC 通道**:
| 枚举 | ADC通道 | 功能 |
|------|---------|------|
| `ADC_TEMP_MOS1` | PB1(ADC9) | MOS 温度 (NTC 10K) |
| `ADC_CUR_AMP` | PA2(ADC2) | TypeC 充电电流检测 |
| `ADC_VBC` | PA1(ADC1) | 电池总压 (分压) |

**模块内运行态**:
- `static ADC_RUNTIME s_adc` - 保存 DMA raw、滤波值、计算结果、电池总压、Type-C 输出电流和调度计数

ADC 结果不再作为跨文件全局变量暴露。外部通过 `ADC_GetResult()`、`ADC_GetRaw()`、`ADC_GetVbatMilliVolt()`、`ADC_GetTypeCOutCurrentMilliAmp()` 读取。

### 5.2 ADC 配置

- **触发源**: TIM2 CC2 (PWM 触发)
- **采样率**: 100kHz TIM2 周期
- **分辨率**: 12位
- **DMA**: DMA1 Channel1, 循环模式
- **采样时间**:
  - PB1(NMOS): 239.5 cycles
  - PA2(CUR): 55.5 cycles
  - PA1(VBUS): 239.5 cycles

### 5.3 关键计算

**电池总压**:
```
Vbat = ADC_mV × (Rtop + Rbottom) / Rbottom
     = ADC_mV × (470K + 15K) / 15K
```
- 8次平均 + 1/8 低通滤波

**TypeC 电流**:
```
I_out(mA) = ADC_mV × 1000 / 10mΩ
```
- 32次平均
- 零电流死区: AD_CurZeroDeadband=4
- 零电流确认: 3次连续零

**温度** (NTC 10K):
- 查表法: `iSheldTemp_10K[56]`
- 1/8 低通滤波
- 输出: (温度+40°C)×10

### 5.4 函数

| 函数 | 功能 |
|------|------|
| `InitADC()` | ADC+DMA+TIM2 完整初始化 |
| `ADC_StopForLowPower()` | 完全停止 ADC/DMA/TIM2, 关闭时钟 |
| `App_AnlogCal()` | ADC 计算主函数: 处理 TTC/VBC/电流, 限速追赶 |
| `ADC_ResetAnlogCalSchedule()` | 重置 ADC 计算调度 |
| `ADC_GetVbatMilliVolt()` | 获取电池总压 |
| `ADC_GetTypeCOutCurrentMilliAmp()` | 获取 TypeC 输出电流 |

---

## 6. CAN 通讯模块

### 6.1 架构

**文件**: `Can_HDX.c/h`, `CanFeidaoFrames.c/h`

CAN 模块实现:
- 硬件 CAN1 驱动 (STM32F103 bxCAN)
- 软件 TX 队列 (32条)
- 应用层命令队列 (4条)
- CAN 收发器电源管理 (GPIO_CMNT_EN)
- 运行态固定周期广播调度
- RTC 睡前关闭 CMNT，唤醒恢复后重新打开

### 6.2 诊断入口

- `Can_GetDebugSnapshot()` - 填充 debug CAN 快照。
- `bus_off` 从 `CAN1->ESR & CAN_ESR_BOFF` 只读获取。

### 6.3 静态运行时结构

**FeidaoCanTxRuntime** (TX 队列):
| 字段 | 说明 |
|------|------|
| `queue[32]` | 发送队列 |
| `head/tail/count` | 环形队列管理 |
| `mailbox` | 当前使用的硬件邮箱 (0-2 或 NoMailBox) |
| `start_tick` | 当前发送开始 tick |

**FeidaoCanRuntime** (CAN 状态):
| 字段 | 默认值 | 说明 |
|------|--------|------|
| `tick` | 0 | 当前 10ms tick |
| `last_1000ms_tick` | 0 | 上次 1000ms 周期帧调度 tick |
| `last_5000ms_tick` | 0 | 上次 5000ms 周期帧调度 tick |
| `schedule_init` | 0 | 周期调度初始化标志 |

### 6.4 CAN 配置

- **波特率**: 36MHz / 4 / (1+5+2) = 250kbps (估算)
- **模式**: Normal, ABOM=ENABLE (自动退出 BUS-OFF), NART=ENABLE (自动重传关闭)
- **过滤器**: 不过滤, 全部接收

### 6.5 电源管理

```
上电: GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, Bit_RESET)
断电: GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, Bit_SET)
```

运行态 `InitCan()` 打开 CMNT；`Can_PrepareSleep()` 在进入 RTC STOP 或 reset sleep 前关闭 CMNT；唤醒恢复后 `InitCan()` 重新打开。

### 6.6 运行态周期调度

- `App_Can()` 按 10ms tick 服务 TX queue、App 命令和 read-block stream。
- 运行态固定检查 `CAN_FEIDAO_1000MS_MSG_MASK` 和 `CAN_FEIDAO_5000MS_MSG_MASK`，不再根据 ACK/RX 判断 active 或 probe。
- `CAN_NART = ENABLE` 关闭硬件自动重发，无 ACK 时发送状态较快失败；软件只释放 mailbox，不再维护 no-ACK 退避状态。

### 6.7 RTC 休眠关系

- `Can_PrepareSleep()` 清 TX、清 App 命令、停止 block stream，并关闭 CMNT。
- RTC HICCUP 周期唤醒后不主动发送 CAN。
- 唤醒恢复后由 `InitRunAfterStopWakeup()` 调 `InitCan()` 重新打开 CMNT，通信回到运行态。

### 6.8 应用层 CAN 命令 (0x60→0x61)

| 命令 | 代码 | 功能 |
|------|------|------|
| GET_STATUS | 0x01 | 获取 SOC/SOH |
| ENTER_IAP | 0x02 | 进入 IAP 模式 |
| READ_REG | 0x03 | 读寄存器 |
| WRITE_PREP | 0x04 | 写准备 |
| WRITE_COMMIT | 0x05 | 写提交 |
| READ_BLOCK | 0x06 | 批量读取 (流式) |
| AGING_START | 0x07 | 开启老化模式 |
| AGING_STOP | 0x08 | 提前结束老化 |
| AGING_RESET_TIME | 0x09 | 重置老化时间 |
| AGING_SET_HOURS | 0x0A | 设置老化时长 |

### 6.9 函数

| 函数 | 功能 |
|------|------|
| `InitCan()` | CAN 硬件+GPIO+NVIC+过滤器全部重新初始化 |
| `App_Can()` | CAN 主服务: 超时检测→调度周期帧→处理应用命令→服务TX→流式读取→IAP延时 |
| `Can_HDX_Transmit()` | 外部发送接口, 入队 |
| `Can_PeekBusy()` | 无副作用检查 CAN 是否忙，供 debug/heartbeat 使用 |
| `Can_IsBusy()` | 检查 CAN 是否忙；低功耗路径用它确认并消费 CAN 接收活动 |
| `Can_PrepareSleep()` | 休眠前清理 |

---

## 7. LED 数码管显示模块

### 7.1 架构

**文件**: `LedBar.c/h`

使用 **Charlieplexing** 原理: 5 个 GPIO 控制 N×(N-1)=20 个 LED 段。

**5个GPIO**:
| Pin ID | GPIO | 说明 |
|--------|------|------|
| P1 | PB11 | LED Pin 1 |
| P2 | PA4 (SPI1_NSS) | LED Pin 2 |
| P3 | PA5 (SPI1_SCK) | LED Pin 3 |
| P4 | PA6 (SPI_MOSI) | LED Pin 4 |
| P5 | PB10 (SEG_EN) | LED Pin 5 |

### 7.2 18条路由 (LED段)

| Route ID | 段 |
|----------|-----|
| 0-1 | 百位1 (上下) |
| 2-8 | 十位 A-G |
| 9-15 | 个位 A-G |
| 16 | 充电图标 |
| 17 | 百分比图标 |

### 7.3 扫描机制

- **定时器**: TIM4, 100kHz (每 tick 0.01ms)
- **扫描周期**: `LEDBAR_SCAN_TIMER_100KHZ_TICKS` (50 tick ≈ 0.5ms 每段)
- **中断**: `TIM4_IRQHandler` → `LedBar_Scan1ms()`

### 7.4 全局运行时

**LedBarRuntime**:
| 字段 | 说明 |
|------|------|
| `initialized` | 初始化标志 |
| `sleep` | 休眠 (关闭显示) |
| `blank` | 空白 (不显示) |
| `number` | 当前显示数字 (SOC 0-100) |
| `indicator_mask` | 图标掩码 (充电/百分比) |
| `frame` | 当前扫描帧 |
| `soc_display_10ms` | SOC 显示剩余时间 |
| `startup_display_armed` | 启动显示使能 |
| `key_active` | 按键滤波后状态 |
| `mcu_wk_active` | MCU_WK 滤波后状态 |
| `key_hold_10ms` | 按键按住时间 |

### 7.5 显示逻辑

1. **SOC 显示窗口**: 按键短按/唤醒后显示 5 秒
2. **唤醒显示**: RTC 唤醒后显示 10 秒
3. **休眠存储**: 休眠前将 SOC 存到备份域寄存器 BKP_DR4/DR5
4. **充电图标**: 放电 MOS 打开时亮
5. **故障闪烁**: 故障时特殊显示
6. **帧优化**: 贪心算法 + 局部搜索改善扫描顺序, 减少鬼影

### 7.6 函数

| 函数 | 功能 |
|------|------|
| `LedBar_Init()` | 初始化 GPIO + 状态 |
| `APP_LedBar()` | 主服务: MCU_WK滤波→开关滤波→显示窗口→更新数字/图标→刷新 |
| `LedBar_Scan1ms()` | 1ms 扫描 (TIM4 ISR 调用) |
| `LedBar_SetNumber()` | 设置显示数字 |
| `LedBar_SetIndicators()` | 设置图标 |
| `LedBar_SetSleep()` | 进入/退出休眠 |
| `LedBar_SaveSleepSoc()` | 保存 SOC 到备份域 |
| `LedBar_LoadSleepSoc()` | 从备份域恢复 SOC |
| `LedBar_PrepareForStop()` | STOP 前准备 (全低电平) |
| `LedBar_IsActiveForLowPower()` | 检查是否需要阻止低功耗 |

---

## 8. RTC 低功耗休眠模块

### 8.1 架构

RTC 休眠系统由 4 层组成:
- **rtc_sleep.c/h** - 核心休眠逻辑 (与硬件无关)
- **rtc_sleep_port.c/h** - 硬件抽象层接口
- **rtc_sleep_afe_sh367309.c/h** - SH367309 AFE 的具体实现
- **SleepDeal.c/h / LowPowerSleep.c/h** - reset sleep 提交、睡前保存和启动早期唤醒处理

### 8.2 休眠模式

| 模式 | 值 | 说明 |
|------|-----|------|
| `NORMAL_MODE` | 0 | 普通休眠 (MCU 复位) |
| `HICCUP_MODE` | 1 | RTC STOP 打嗝模式 (定时唤醒) |
| `DEEP_MODE` | 2 | 深度休眠 (过放, MCU 复位) |
| `NO_SLEEP` | 3 | 不休眠 |

### 8.3 休眠阻塞原因

| 原因 | 说明 |
|------|------|
| `LOW_POWER_RTC_BLOCK_CURRENT` | 充放电电流 >10mA |
| `LOW_POWER_RTC_BLOCK_MCU_WAKE` | MCU_WK 引脚高 |
| `LOW_POWER_RTC_BLOCK_FACTORY_AGING` | 工厂老化 running，只阻塞 HICCUP RTC STOP |
| `LOW_POWER_RTC_BLOCK_EXT_COMM` | 有外部通讯 |
| `LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE` | AFE 忙 |
| `LOW_POWER_RTC_BLOCK_FRAMEWORK` | 框架层面阻塞 (CAN/LED/Flash等) |

### 8.4 休眠进入条件

1. 无充放电 (>10mA)
2. MCU_WK 不为高
3. 无外部通讯计数变化
4. 框架阻塞位为 0 (`LP_GetBlockReason() == 0`)

空闲延迟: `sys_time.time_enter_rtc` 秒后进入 HICCUP

### 8.5 HICCUP 模式循环

```
rtc_sleep_run_hiccup_cycle():
  1. Prepare: 保存 SOC, 配置 RTC 唤醒, 清标志
  2. Enter STOP (PWR_Regulator_LowPower + WFI)
  3. Wake: RTC 闹钟或 EXT INT
  4. Restore: 恢复时钟, IO, 外设
  5. Check exception: 电流/故障唤醒 → 退出 HICCUP
  6. Update SOC: RTC 静置 SOC 补偿
  7. Return true → 继续下一轮 HICCUP
```

### 8.6 唤醒源

```c
enum irqWakeup {
    uart1_irq, uart2_irq, uart3_irq,  // 串口唤醒
    PA0_irq,                            // 充电检测
    bms_keyirq,                         // BMS 按键
    soc_key,                            // SOC 按键
    CHG_IRQ,                            // 充电中断
    current_wake,                       // 电流唤醒
    chg_dsg_close,                      // 充放电关闭
    error_wake,                         // 错误唤醒
    cuv_wake, cov_wake,                // 欠压/过压唤醒
    rs485_irq,                          // RS485 唤醒
    NO_IRQ
};
```

### 8.7 强制深度休眠

条件: VCellMin ≤ 2800mV 且充电电流 ≤ 5mA, 持续 60 秒

### 8.8 全局变量

- `struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus` - RTC 休眠状态
  - `mode` - 当前模式
  - `blockReason` - 阻塞原因
  - `rtcWake` - RTC 唤醒标志
  - `delaySeconds/elapsedSeconds` - 延迟/经过时间

### 8.9 函数

| 函数 | 功能 |
|------|------|
| `rtc_sleep()` | 休眠主逻辑: 1s tick→模式选择→执行休眠 |
| `LowPower_Request(mode)` | 请求切换休眠模式 |
| `entersleep(mode)` | 同上 |
| `rtc_sleep_run_hiccup_cycle()` | HICCUP 单次 STOP 循环 |
| `RtcSleep_PortCommitResetSleep(mode)` | NORMAL/DEEP reset sleep 提交点 |
| RTC SOC rest | 由 `RtcSleep_PortApplySocRtcRest()` 直接触发 SOC 休眠补偿 |

---

## 9. AFE 通信模块 (I2C)

### 9.1 架构

**文件**: `I2C_AFE1.c/h`, `SH367309_DataDeal.c/h`, `SH367309_Func.c/h`

- **I2C_AFE1** - 底层 GPIO 模拟 I2C 驱动
- **SH367309_DataDeal** - 寄存器解析, 电压/电流/温度计算
- **SH367309_Func** - 保护配置, MOS 控制, 均衡控制

### 9.2 I2C 驱动

使用 GPIO 模拟 I2C (PB8=SCL, PB9=SDA):
- `MTPWrite(addr, len, buf)` - 写寄存器
- `MTPRead(addr, len, buf)` - 读寄存器
- `MTPWriteROM(addr, len, buf)` - 写 OTP/MTP

### 9.3 SH367309 寄存器映射

| 寄存器 | 地址 | 说明 |
|--------|------|------|
| MTP_CONF | 0x40 | 配置寄存器 |
| MTP_BALANCEH/L | 0x41-0x42 | 均衡控制 |
| MTP_BSTATUS1/2/3 | 0x43-0x45 | 状态寄存器 |
| MTP_TEMP1/2/3 | 0x46,48,4A | 温度 |
| MTP_CUR | 0x4C | 电流 |
| MTP_CELL1-16 | 0x4E-0x6C | 电芯电压 |
| MTP_ADC2 | 0x6E | ADC2 |
| MTP_BFLAG1/2 | 0x70-0x71 | 故障标志 |
| MTP_RSTSTAT | 0x72 | 复位状态 |

### 9.4 全局变量

- `struct SH367309_Read SH367309_Read_AFE1` - AFE 读取数据
- `AFEDATA Registers_AFE1` - AFE 寄存器缓存
- `SH367309_Reg_Store` - AFE 完整寄存器存储

### 9.5 函数

| 函数 | 功能 |
|------|------|
| `InitAFE1()` | AFE I2C 初始化 + 参数配置 |
| `initAFE1_IIC()` | 仅 I2C GPIO 初始化 |
| `UpdateVoltageFromBqMaximo()` | 从 AFE 读取所有数据 |
| `App_AFEGet()` | AFE 数据获取主任务 |
| `App_SH367309_Monitor()` | AFE 监控 |
| `enter_fac_mode()` | 进入/退出工厂模式 (强制开 MOS) |

---

## 10. 工厂老化模块

### 10.1 架构

**文件**: `FactoryAging.c/h`

工厂老化: 首次上电后自动开启充放电 MOS, 运行指定时间, 完成后写入 Flash 标志。

### 10.2 状态机

```
UNINIT → RUNNING → DONE
              ↓
           STOPPED
```

### 10.3 存储策略

- **备份域** (BKP_DR6-10): 每1秒保存, 断电保持
- **Flash**: 每2小时保存, 永久保存

### 10.4 全局变量 (静态)

| 变量 | 说明 |
|------|------|
| `s_u8FactoryAgingState` | 状态 (UNINIT/RUNNING/DONE/STOPPED) |
| `s_u32FactoryAgingElapsed10ms` | 已用时间 (10ms单位) |
| `s_u16FactoryAgingDurationHours` | 老化时长 (小时) |
| `s_u8FactoryAgingMosMode` | MOS 模式 (工厂/5V充电) |

### 10.5 函数

| 函数 | 功能 |
|------|------|
| `FactoryAging_Task()` | 老化任务: 启动→累计时间→完成检测 |
| `FactoryAging_IsActive()` | 是否老化中 |
| `FactoryAging_GetState()` | 获取公开状态 |
| `FactoryAging_GetRemainingSeconds()` | 剩余时间 (秒) |
| `FactoryAging_StartByHost()` | CAN 命令开启 |
| `FactoryAging_StopByHost()` | CAN 命令提前结束 |
| `FactoryAging_ResetTimeByHost()` | CAN 命令重置时间 |
| `FactoryAging_SetDurationHoursByHost()` | CAN 命令设置时长 |
| `FactoryAging_SaveProgressBeforeSleep()` | 休眠前保存进度 |

---

## 11. 保护参数与故障模块

### 11.1 架构

**文件**: `Fault.c/h`, `FaultSnapshot.h`

保护参数分为三级: First (一级), Second (二级), Third (三级), 每级有恢复值。

### 11.2 保护参数结构体

`struct PRT_E2ROM_PARAS` (65个UINT16):
| 参数组 | 5个参数 |
|--------|---------|
| 单节过压 | 一级,二级,三级,恢复,滤波 |
| 单节低压 | 一级,二级,三级,恢复,滤波 |
| 总压过压 | 一级,二级,三级,恢复,滤波 |
| 总压低压 | 一级,二级,三级,恢复,滤波 |
| 充电过流 | 一级,二级,三级,恢复,滤波 |
| 放电过流 | 一级,二级,三级,恢复,滤波 |
| 充电高温 | 一级,二级,三级,恢复,滤波 |
| 充电低温 | 一级,二级,三级,恢复,滤波 |
| 放电高温 | 一级,二级,三级,恢复,滤波 |
| 放电低温 | 一级,二级,三级,恢复,滤波 |
| 驱动高温 | 一级,二级,三级,恢复,滤波 |
| 压差过大 | 一级,二级,三级,恢复,滤波 |
| 电量过低 | 一级,二级,三级,恢复,滤波 |

### 11.3 默认保护值 (三元锂)

| 保护项 | 一级 | 二级 | 三级 | 恢复 |
|--------|------|------|------|------|
| 单节过压 | 4200mV | 4200mV | 4250mV | 4100mV |
| 单节低压 | 3000mV | 3000mV | 2900mV | 3100mV |
| 总压过压 | 420×N | 420×N | 420×N | 400×N |
| 充电过流 | 65A | 65A | 65A | 1A |
| 放电过流 | 150A | 150A | 150A | 1A |
| 充电高温 | 50°C | 50°C | 50°C | 40°C |
| 驱动高温 | 80°C | 90°C | 95°C | 60°C |

### 11.4 故障标志联合体

- `union FAULT_FLAG_FIRST` (UINT16) - 一级故障状态
- `union FAULT_FLAG_SECOND` (UINT16) - 二级故障状态
- `union FAULT_FLAG_THIRD` (UINT16) - 三级故障状态

### 11.5 全局变量

- `struct PRT_E2ROM_PARAS PRT_E2ROMParas` - 保护参数
- `union FAULT_FLAG_FIRST Fault_Flag_Fisrt` - 一级故障
- `union FAULT_FLAG_SECOND Fault_Flag_Second` - 二级故障
- `union FAULT_FLAG_THIRD Fault_Flag_Third` - 三级故障
- `UINT16 Fault_record_Third[10]` - 三级故障记录
- `UINT8 FaultPoint_Third` - 三级故障指针

### 11.6 函数

| 函数 | 功能 |
|------|------|
| `App_WarnCtrl()` | 警告控制 |
| `FaultWarnRecord2()` | 故障记录 |

---

## 12. 串口通信模块 (Modbus/RS485)

### 12.1 架构

**文件**: `Sci_Upper.c/h`

实现 Modbus-like 协议:
- 读寄存器 (0x03)
- 写单个寄存器 (0x06)
- 写多个寄存器 (0x10)
- CRC16 校验

### 12.2 地址空间

| 地址范围 | 说明 |
|----------|------|
| `0x1000-0x10FF` | 校正系数 K/B (只读) |
| `0x1100-0x11FF` | 系统功能开关 |
| `0x2000-0x20FF` | 校正系数 (读写) |
| `0x2100-0x21FF` | 保护参数 (读写) |
| `0x2200-0x22FF` | SOC/其他参数 (读写, 可禁用) |
| `0x2300-0x23FF` | 均衡/系统参数 (读写) |
| `0x2500` | 历史 SOC 测试命令；当前源码未见独立处理 |
| `0xC000-0xC0FF` | LCD/SN/事件记录 (只读) |
| `0xD000-0xD0FF` | 实时数据 (只读, 98字) |
| `0xD100-0xD1FF` | RTC 实时数据 |
| `0xD200-0xD2FF` | 故障快照 |
| `0xD300-0xD3FF` | SOC_TEST 兼容 padding；当前填充 16 word 0 |
| `0xFFF0-0xFFFF` | 序列号/版本 |

### 12.3 全局变量

- `UINT8 u8FlashUpdateFlag` - 请求进 IAP 标志
- `UINT8 u8FlashUpdateE2PROM` - 请求写 Flash 参数
- `struct stCell_Info g_stCellInfoReport` - 上报数据结构 (核心全局)

### 12.4 g_stCellInfoReport 结构

| 字段 | 说明 |
|------|------|
| `u16VCell[32]` | 32节电芯电压 (mV) |
| `u16VCellMax/Min` | 最高/最低电芯电压 |
| `u16VCellDelta` | 电芯压差 |
| `u16VCellTotle` | 总压 (V×100) |
| `u16Temperature[10]` | 温度数组 |
| `u16Ichg/u16IDischg` | 充/放电电流 (A×10) |
| `SocElement` | SOC/SOH/容量/循环 |
| `unMdlFault_First/Second/Third` | 三级故障标志 |
| `u16BalanceFlag1/2` | 均衡标志 |

### 12.4 函数

| 函数 | 功能 |
|------|------|
| `InitUSART_CommonUpper()` | 初始化所有使能的串口 |
| `App_CommonUpper()` | 串口服务主函数 |
| `Sci_HostReadWords()` | 上位机读寄存器 (通过地址) |
| `Sci_HostWriteWords()` | 上位机写寄存器 (通过地址) |
| `Sci_IsAnyPortBusy()` | 是否有串口忙 |
| `Sci_CRC16RTU()` | CRC16 计算 |
| `Sci_ACK_0x03_ReadRegs_*()` | 各种读命令处理 |
| `Sci_WrReg_0x06_*()` | 各种写命令处理 |

---

## 13. Flash 存储模块

### 13.1 Flash 地址分配

| 地址 | 大小 | 用途 |
|------|------|------|
| `0x08000000` | 18KB | IAP Bootloader |
| `0x08004800` | ~94KB | APP 程序 |
| `0x0801C000` | 1KB | AFE 参数 Slot A |
| `0x0801C400` | 1KB | RW 参数 Slot A |
| `0x0801C800` | 1KB | AFE 参数 Slot B |
| `0x0801CC00` | 1KB | RW 参数 Slot B |
| `0x0801D000` | 2KB | 日志 Slot A |
| `0x0801D800` | 2KB | 日志 Slot B |
| `0x0801E000` | 2KB | SOC 数据 Slot A |
| `0x0801E800` | 2KB | SOC 数据 Slot B |
| `0x0801F000` | 1KB | 升级参数策略标志 |
| `0x0801F400` | 1KB | 工厂老化数据 |
| `0x0801F800` | 1KB | 升级跳转标志 (兼容) |
| `0x0801FC00` | 1KB | 休眠标志 |

### 13.2 双槽设计

所有存储区使用 A/B 双槽设计, 交替写入, 防止断电丢失。

### 13.3 存储数据结构

- `STORAGE_FLASH_SOC_DATA` - SOC 快照 (版本号, SOC, 容量, 循环, 学习状态)
- `STORAGE_FLASH_RW_PARAM_DATA` - 保护参数 + 其他参数
- `STORAGE_FLASH_FACTORY_AGING_DATA` - 老化数据 (时间, 状态, 时长)

### 13.4 函数

| 函数 | 功能 |
|------|------|
| `StorageFlash_LoadSocData()` | 加载 SOC 数据 (自动选较新槽) |
| `StorageFlash_SaveSocData()` | 保存 SOC 数据 |
| `StorageFlash_LoadAfeData()` | 加载 AFE 参数 |
| `StorageFlash_SaveAfeData()` | 保存 AFE 参数 |
| `StorageFlash_LoadRwParamData()` | 加载 RW 参数 |
| `StorageFlash_SaveRwParamData()` | 保存 RW 参数 |
| `StorageFlash_LoadLogData()` | 加载日志 |
| `StorageFlash_SaveLogData()` | 保存日志 |
| `StorageFlash_IsBusy()` | Flash 操作忙检测 |
| `App_FlashUpdate()` | Flash 更新请求处理 |
| `FlashWriteOneHalfWord()` | Flash 半字写入 |
| `FlashReadOneHalfWord()` | Flash 半字读取 |
| `AppUpgrade_RequestIap()` | 请求进入 IAP |

---

## 14. 日志记录模块

### 14.1 架构

**文件**: `LogRecord.c/h`

### 14.2 事件类型

| 事件 | 代码 | 说明 |
|------|------|------|
| BMS_START_UP | 1 | 开机 |
| BMS_SLEEP | 2 | 休眠 |
| BALANCE_OPEN | 3 | 均衡开启 |
| VCELL_OVP | 6 | 单节过压 |
| VBUS_OVP | 7 | 总压过压 |
| CHG_OCP | 8 | 充电过流 |
| VCELL_UVP | 9 | 单节欠压 |
| VBUS_UVP | 10 | 总压欠压 |
| DSG_OCP | 11 | 放电过流 |
| CHG_UTP | 12 | 充电低温 |
| DSG_UTP | 13 | 放电低温 |
| CHG_OTP | 14 | 充电高温 |
| DSG_OTP | 15 | 放电高温 |
| VDELTA_OP | 16 | 压差过大 |
| CBC_ERR | 17 | CBC 错误 |
| AFE1_ERR | 18 | AFE1 通信错误 |
| EEPROM_ERR | 20 | 存储错误 |

### 14.3 重复抑制

`PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC` (默认3600秒) 内同类型事件不重复写 Flash。

### 14.4 全局变量

- `UINT32 su32_Interval_S_Tcnt` - 时间计数器

### 14.5 函数

| 函数 | 功能 |
|------|------|
| `App_LogRecord()` | 日志服务主函数 |
| `LogRecord_RequestStartup()` | 请求记录开机 |
| `LogRecord_RequestSleep()` | 请求记录休眠 |
| `LogEvent_Record()` | 记录事件 |
| `EEPROM_ResetData_EventRecord_ToDefault()` | 清除所有日志 |

---

## 15. 系统监控模块

### 15.1 架构

**文件**: `System_Monitor.c/h`

### 15.2 全局变量 (静态)

- `volatile struct SYSTEM_ERROR System_ErrFlag` - 系统错误标志
- `volatile union System_OnOFF_Function s_system_onoff_func` - 功能开关
- `volatile union System_Status s_system_status` - 系统状态

### 15.3 错误类型

| 错误 | 说明 |
|------|------|
| ERROR_AFE1/2 | AFE 通信错误 |
| ERROR_CAN | CAN 错误 |
| ERROR_EEPROM_COM/STORE | EEPROM 错误 |
| ERROR_HSE/LSE | 时钟错误 |
| ERROR_ADC | ADC 错误 |
| ERROR_CBC_CHG/DSG | CBC 错误 |
| ERROR_TEMP_BREAK | 温度断线 |
| ERROR_SOC_CAIL | SOC 校准错误 |

### 15.4 系统状态位

`union System_Status`:
- `b1StartUpBMS` - BMS 首次开机 (1→0 表示启动完成)
- `b1Status_MOS_CHG/DSG` - MOS 管状态
- `b1Status_AFE1/2` - AFE 状态
- `b1Status_ToSleep` - 允许休眠
- `b1Status_Balance` - 均衡状态

### 15.5 功能开关

`union System_OnOFF_Function`:
- `b1OnOFF_Balance` - 均衡开关
- `b1OnOFF_MOS_Relay` - MOS 控制
- `b1OnOFF_SOC_Fixed` - SOC 固定
- `b1OnOFF_SOC_Zero` - SOC 强置零
- `b1OnOFF_Sleep` - 休眠开关

### 15.6 函数

| 函数 | 功能 |
|------|------|
| `InitSystemMonitorData_EEPROM()` | 初始化系统状态/功能开关默认值 |
| `SystemRuntime_MarkBootReady()` | 标记启动完成 |
| `SystemRuntime_SetAfeStatus()` | 设置 AFE 状态 |
| `SystemRuntime_SetMosStatus()` | 设置 MOS 状态 |
| `SystemRuntime_IsChargeMosOpen()` | 检查充电 MOS |
| `SystemRuntime_IsDischargeMosOpen()` | 检查放电 MOS |
| `SystemFeature_SetById()` | 按位设置功能开关 |
| `SystemFeature_IsSocFixed()` | SOC 是否固定 |
| `SystemFeature_IsSocZero()` | SOC 是否强零 |
| `System_ERROR_UserCallback()` | 错误回调 (Set/Remove/Get) |

---

## 16. MOS 管启动控制

### 16.1 文件: `MosStartup.c/h`

### 16.2 功能

- 管理 CHG/DSG MOS 管的开启/关闭
- 工厂模式强制开启 MOS
- 5V 充电检测
- 预充电时序

### 16.3 函数

| 函数 | 功能 |
|------|------|
| `MosStartup_Is5vChargeActive()` | 检测 5V 充电 |
| `enter_fac_mode()` | 进入/退出工厂模式 |

---

## 17. 低功耗调度模块

### 17.1 文件: `rtc_sleep.c/h`

### 17.2 主路径

```
Runtime_RunOnce() -> rtc_sleep()
```

`rtc_sleep.c/h` 同时承载运行态低功耗入口、框架层 block reason bitmask 和最近一次睡眠秒数。真实低功耗模式、RTC wake、block reason 由 `g_stLowPowerRtcStatus` 观察；ready 只由 debug/ST-Link 按 `mode != NO_SLEEP` 派生，不再是控制字段。

### 17.3 框架层阻塞原因位掩码

| 位 | 说明 |
|----|------|
| LP_BLOCK_CHARGE | 充电中 |
| LP_BLOCK_DISCHARGE | 放电中 |
| LP_BLOCK_COMM | 通讯忙 |
| LP_BLOCK_KEY | 按键活动 |
| LP_BLOCK_FLASH_BUSY | Flash 操作中 |
| LP_BLOCK_UPGRADE | 升级中 |
| LP_BLOCK_FAULT | 有故障 |
| LP_BLOCK_LED_ACTIVE | LED 显示中 |

说明：工厂老化 running 使用粗粒度 `LOW_POWER_RTC_BLOCK_FACTORY_AGING` 表示，不加入 `LP_BLOCK_*` 通用 bitmask，避免误解为阻塞 `DEEP_MODE/NORMAL_MODE` reset sleep。

### 17.4 函数

| 函数 | 功能 |
|------|------|
| `LP_GetBlockReason()` | 获取框架层阻塞 bitmask；返回 0 表示框架允许休眠 |
| `LP_GetLastSleepSeconds()` | 获取最近一次 RTC sleep 累计秒数 |
| `LP_RecordLastSleepSeconds()` | 记录最近一次 RTC sleep 累计秒数 |

---

## 18. 全局变量总览

### 18.1 conf.c - 系统时间与调试

```c
Time_T sys_time = {
    .time_enter_rtc = 10,
    .power_on = false,
};
```

**Time_T 结构**:
| 字段 | 说明 |
|------|------|
| `can_rcv_cnt` | CAN 接收计数 |
| `test_driver_cnt` | 驱动测试计数 |
| `App_AFEGet_cnt` | AFE 获取计数 |
| `sci1/2/3_irq_cnt` | 串口中断计数 |
| `cnt_PA0_irq` | PA0 中断计数 |
| `cnt_bms1_keyirq` | 按键中断计数 |
| `pec_err_cnt` | PEC 错误计数 |
| `CHG/DSG` | 充放电状态 |
| `wakeup_reason` | 唤醒原因 |
| `wakeup_rtc` | RTC 唤醒标志 |
| `power_on` | 上电标志 |
| `rtc_sleep_cnt` | RTC 休眠计数 |
| `rtc_sec_cnt` | RTC 秒中断计数 |
| `rtc_alm_cnt` | RTC 闹钟计数 |

### 18.2 System_Init - 时基

```c
volatile union SYS_TIME g_st_SysTimeFlag;
```

### 18.3 System_Monitor - 系统状态

```c
volatile struct SYSTEM_ERROR System_ErrFlag;  // 23个错误标志
```

### 18.4 DataDeal - 数据与校准

```c
UINT16 g_u16CalibCoefK[KB_NUM];    // 校准系数 K (47个)
INT16 g_i16CalibCoefB[KB_NUM];     // 校准系数 B (47个)
UINT16 CopperLoss[16];             // 铜损补偿
UINT16 CopperLoss_Num[16];         // 铜损数量
struct OTHER_ELEMENT OtherElement;  // 其他可配置参数
UINT32 g_u32CS_Res_AFE;            // AFE 电流采样电阻比
```

AFE 电流采样序号当前通过 `AfeCurrent_GetSeq()` 读取，不再暴露 `g_u32AfeCurrentSampleSeq` extern。

### 18.5 Sci_Upper - 通讯

```c
UINT8 u8FlashUpdateFlag;           // 请求进 IAP
UINT8 u8FlashUpdateE2PROM;         // 请求写 Flash
struct stCell_Info g_stCellInfoReport; // 核心上报数据
```

### 18.6 SOC/SocEnhance

```c
struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;
UINT16 SOC_Table_Set[42];          // 仅 runtime table 宏路径引用，当前量产关闭
```

### 18.7 Fault

```c
struct PRT_E2ROM_PARAS PRT_E2ROMParas;     // 保护参数
union FAULT_FLAG_FIRST Fault_Flag_Fisrt;   // 一级故障
union FAULT_FLAG_SECOND Fault_Flag_Second; // 二级故障
union FAULT_FLAG_THIRD Fault_Flag_Third;   // 三级故障
UINT16 Fault_record_Third[10];             // 故障记录
UINT8 FaultPoint_Third;                    // 故障指针
```

### 18.8 I2C_AFE1

```c
struct SH367309_Read SH367309_Read_AFE1;   // AFE 读取数据
AFEDATA Registers_AFE1;                    // AFE 寄存器缓存
```

### 18.9 ADC

```c
static ADC_RUNTIME s_adc;          // ADC.c 模块内运行态
```

对外接口为 `ADC_GetResult()`、`ADC_GetRaw()`、`ADC_GetVbatMilliVolt()` 和 `ADC_GetTypeCOutCurrentMilliAmp()`。

### 18.10 LedBar

```c
bool key_release_wakeup = false;    // 按键释放唤醒标志
```

### 18.11 rtc_sleep

```c
struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus; // RTC 休眠状态
```

### 18.12 SleepDeal

```c
UINT8 RTC_ExtComCnt;   // 外部通讯计数
```

### 18.13 AppInit

```c
UINT8 SeriesNum = 10;  // 电池串数
```

### 18.14 LogRecord

```c
UINT32 su32_Interval_S_Tcnt;  // 日志时间计数
```

---

## 19. 中断向量与外设资源

### 19.1 中断使用

| 中断 | 用途 | 优先级 |
|------|------|--------|
| SysTick | 延时 (__delay_ms) | 最低 |
| TIM3 | 10ms 时基 | 0,0 |
| TIM2 | ADC 触发 (PWM) | - |
| TIM4 | LED 扫描 | 1,3 |
| EXTI0 | PA0 充电检测 | 1,1 |
| EXTI9_5 | PA9 按键, PB7 UART1 RX | 1,1 |
| EXTI15_10 | PB12 通讯唤醒, PB13 MCU_WK | 1,1 |
| USB_LP_CAN1_RX0 | CAN RX | 1,1 |
| USART1 | 串口1 收发 | 1,1 |
| USART2 | 串口2 收发 | 1,1 |
| USART3 | 串口3 收发 | 1,1 |
| RTC | RTC 秒中断/闹钟 | 1,1 |
| DMA1_Channel1 | ADC DMA | - |

### 19.2 外设使用

| 外设 | 用途 |
|------|------|
| TIM2 | ADC 触发源 |
| TIM3 | 10ms 时基 |
| TIM4 | LED 扫描 |
| ADC1 | 电压/电流/温度采样 |
| DMA1_CH1 | ADC 数据搬运 |
| CAN1 | CAN 通信 |
| USART1 | 串口通信 (上位机) |
| USART2 | 串口通信 (可选) |
| USART3 | 串口通信 (可选) |
| I2C (GPIO模拟) | AFE 通信 (PB8/PB9) |
| SPI1 (GPIO复用) | LED 数码管 Charlieplexing |
| RTC | 低功耗唤醒 |
| IWDG | 独立看门狗 |
| PWR | 低功耗模式控制 |
| BKP | 备份域寄存器 (SOC保存, 老化进度) |

### 19.3 内存资源 (STM32F103C8)

- Flash: 64KB
- RAM: 20KB
- IAP 地址: 0x08000000-0x080047FF (18KB)
- APP 地址: 0x08004800-0x0800FFFF (~94KB)

---

## 20. Flash 地址映射

| 地址范围 | 内容 |
|----------|------|
| 0x08000000 - 0x080047FF | IAP Bootloader (18KB) |
| 0x08004800 - 0x0801BFFF | APP 程序 (~94KB) |
| 0x0801C000 - 0x0801C3FF | AFE 参数 Slot A (1KB) |
| 0x0801C400 - 0x0801C7FF | RW 参数 Slot A (1KB) |
| 0x0801C800 - 0x0801CBFF | AFE 参数 Slot B (1KB) |
| 0x0801CC00 - 0x0801CFFF | RW 参数 Slot B (1KB) |
| 0x0801D000 - 0x0801D7FF | 日志 Slot A (2KB) |
| 0x0801D800 - 0x0801DFFF | 日志 Slot B (2KB) |
| 0x0801E000 - 0x0801E7FF | SOC 快照 Slot A (2KB) |
| 0x0801E800 - 0x0801EFFF | SOC 快照 Slot B (2KB) |
| 0x0801F000 - 0x0801F3FF | 升级参数策略 (1KB) |
| 0x0801F400 - 0x0801F7FF | 工厂老化 (1KB) |
| 0x0801F800 - 0x0801FBFF | 升级标志 (1KB) |
| 0x0801FC00 - 0x0801FFFF | 休眠标志 (1KB) |

---

> 文档版本: 1.0
> 源码分支: t3-master-new-new-new
