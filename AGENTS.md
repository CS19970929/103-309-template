# AGENTS.md

## 项目概述

BMS 保护板固件，适用于 16S SH367309 AFE，MCU 为 STM32F103C8（Cortex-M3，64KB Flash，20KB RAM）。使用 Keil MDK-ARM V5 构建，ST 标准外设库 V3.5.0，语言 C99。

## 构建

Keil 工程文件：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`

### 构建目标

| 目标 | 输出目录 | 用途 |
|------|---------|------|
| Production | `Users/Objects/` | 正式发布构建 |
| Debug_Trace | `Users/Objects_DebugTrace/` | 调试构建，启用 `APP_TRACE_ENABLE=1` |

### 命令行构建（UV4.exe 在 PATH 中时）

```
UV4.exe -b "103+309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx" -t Production -o build.log
```

构建后步骤自动通过 `fromelf.exe --bin` 生成 `.bin` 文件。

### 编译宏定义

- `STM32F10X_MD, USE_STDPERIPH_DRIVER`（两个目标共用）
- `APP_TRACE_ENABLE=1`（仅 Debug_Trace）

### 清理

`103 + 309/Project/Users/Clean.bat` — 将输出的 .hex/.bin 复制到上级目录，并删除构建产物。

## 目录结构

```
103 + 309/Project/
  Source/           # 全部应用源码（约79个.c/.h文件）
    conf/           # conf.h, conf_gpio.h, conf.c — 功能开关和引脚定义
    easylogger/     # EasyLogger 日志库
    mcu_sdk/        # MCU SDK（4G/GPS协议，非BMS核心）
  STM32F10x_StdPeriph_Lib_V3.5.0/  # ST HAL库
  Users/            # Keil工程文件、Objects/、Listings/
C030v1.0/Project/   # 旧版 BQ769x0 AFE 变体（仅供参考）
firmware/comm_tool_f103ret6/  # 通讯工具固件（STM32F103RET6）
upgrader_mcu/       # IAP/Bootloader（UPG_F103C8）
build/              # Python 工具构建产物
dist/               # BMS_CommTool_Upgrade_UI.exe
docs/               # 设计文档（11篇——理解子系统从这里开始）
```

## 架构

### 入口

`103 + 309/Project/Source/main.c` → `main()` → `InitDevice()` → `InitVar()` → while(1)

### 主循环任务（按执行顺序）

1. `App_SysTime()` — 系统时基调度（基于 TIM3 的 pending 队列）
2. `App_WarnCtrl()` + `APP_LedBar()` — 仅在 10ms 标志时执行
3. `App_AFEGet()` — 读取 AFE 电压/电流/温度
4. `PowerUi_ProcessRequests()` — 开关机状态机
5. `App_Sci()` — 与上位机串口通信
6. `App_SOC()` — 荷电状态估算
7. `App_BmsEUavcan()` — CAN EUAVCAN 广播
8. `App_SleepDeal()` — 休眠进入逻辑
9. `App_FlashUpdate()` — Flash 参数持久化
10. `App_LogRecord()` — 事件日志记录

### IAP

`_IAP` 在 `main.h` 中定义。APP 固件起始地址为 **0x08004800**（18KB 偏移）。Bootloader 在 `upgrader_mcu/` 目录。

## 配置

### 功能开关（`Source/conf/conf.h`）

- `AFE_TYPE` — `sh36xx`（当前使用）或 `bq76xx_afe`
- `LEVEL_CURR` — 电流档位：`CURR_80A`/`100A`/`150A`/`200A`/`250A`
- `wdog_enable` — 独立看门狗
- `__FUNC__HEAT__` — 加热功能
- `__VIRTURE_CURRENT__` — 虚拟电流模式
- `_DEBUG_CODE` — 定义时屏蔽整个主循环

### 编译时 SCI 通道选择（`Source/main.h`）

每个 SCI 通道只能选择一种模式：`_COMMOM_UPPER_SCI1`、`_CLIENT_SCI1` 或 `_LCD_SCI1`。

### GPIO 引脚映射（`Source/conf/conf_gpio.h`）

关键引脚：PA0=WK_MCU（充电唤醒），PA9=SOC_KEY，PB5=KEY1，PB15=DBG_LED，PA4=CMNT_EN（CAN 收发器），PB14=AFE_CTL。

## 注意事项

- **自定义类型**：`UINT8`、`UINT16`、`UINT32`、`UINT64` 定义在 `PubFunc.h`，非 `stdint.h`。两者都在用，新代码不要混用。
- **休眠状态持久化**：休眠状态使用 BKP 寄存器（BKP_DR6/DR7），不用 Flash。不要将这些 BKP 槽位用于其他用途。
- **生产环境日志宏禁用**：`log_i()` / `log_w()` 在 `conf.h` 中是空操作。调试日志需要手动重新启用。
- **LED 状态机**：`LedBar.c` 使用基于 tick 的定时系统，不要在 LED 上下文中调用 `delay_ms()`。
- **AFE I2C 阻塞**：`I2C_AFE1.c` 做阻塞式 I2C 通信，长时间 AFE 读取会阻塞主循环，影响系统时基调度。
- **无命令行构建工具链**：没有 Makefile、CMake 或 PlatformIO 配置，构建必须使用 Keil MDK 或 UV4.exe。

## 设计文档

深入理解各子系统请阅读 `docs/` 目录：

- `can_low_power_design.md` — CAN NART、重试和 BusOff 恢复
- `sleep_wakeup_flow.md` — 完整休眠/唤醒链路（PA0 充电唤醒，PA9/PB5 按键唤醒）
- `led_power_design.md` — LED 状态机、备份域快照、电源 UI
- `led_key_requirements.md` — 各 SOC 档位的 LED 显示规格
- `system_time_scheduler.md` — TIM3 pending 队列、过载检测
- `bms_euavcan_protocol.md` — EUAVCAN 帧布局和时序
