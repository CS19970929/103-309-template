# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

锂电池保护板 BMS（Battery Management System）固件项目，基于 STM32F103 系列 MCU，使用 SH367309 AFE 芯片进行电池单体监控。项目包含三个固件目标：

- **`103 + 309/Project/`** — 主 BMS 固件 (STM32F103RCT6 + SH367309)，管理 10~16 串电池组
- **`firmware/comm_tool_f103ret6/`** — 通信工具固件 (STM32F103RET6)，用于 RS485/CAN 协议桥接
- **`upgrader_mcu/keil/`** — MCU 升级器固件 (STM32F103C8)

## Build System

使用 **Keil MDK (uVision)** IDE，项目文件为 `.uvprojx`：
- `103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx` — 主 BMS
- `firmware/comm_tool_f103ret6/keil/COMM_TOOL_F103RET6.uvprojx` — 通信工具
- `firmware/comm_tool_f103ret6/keil/COMM_TOOL_IAP.uvprojx` — 通信工具 IAP 引导

使用 STM32F10x Standard Peripheral Library V3.5.0（位于 `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/`）。

编译输出为 `.axf`、`.bin`、`.hex` 文件，生成在 `Objects/` 目录下。

## Architecture

### 主 BMS 固件核心架构

`main.c` 极其简洁：

```c
int main(void) {
    Runtime_Boot();    // 一次性初始化
    while (1) {
        Runtime_RunOnce();  // 主循环，按固定顺序执行各模块任务
    }
}
```

**`main.h` 是中央包含文件**，所有模块都通过它互相引用。它包含了 `stm32f10x.h`、标准库、全部业务模块头文件，并定义了通用宏 (`UPDNLMT16`、`S2U`、`MCU_RESET()`、`BOOL`、延时常量等)。

### 模块分层

**初始化层 (`Runtime_Boot` 调用顺序)：**
`SystemInit()` → `InitDelay()` → `SleepDeal_HandleBootSleepStartup()` → JTAG 禁用 → `InitNVIC()` → `InitIO()` → `InitUSART_CommonUpper()` → `InitE2PROM()` → `InitAFE1()` → `InitCan()` → `InitADC()` → `InitData_SOC()` → `InitTimer()` → `__enable_irq()` → EEPROM 数据初始化 → `SystemRuntime_MarkBootReady()` → `LedBar_Init()` → `InitProID()` → `LogRecord_RequestStartup()` → `Init_RTC()` → `DBG_Init()` → `Init_IWDG()`

**主循环层 (`Runtime_RunOnce` 任务顺序)：**
1. `SysTime_LatchTaskFlags()` — 10/50/100/200/1000ms 系统时基
2. `FactoryAging_Task()` — 工厂老化任务
3. `APP_LedBar()` — LED 数码管显示
4. `App_AFEGet()` — 通过 I2C 读取 AFE 电池数据
5. `App_CommonUpper()` — RS485 上位机通信处理
6. `App_AnlogCal()` — ADC 采样（总压、电流、温度）
7. `rtc_sleep()` — 低功耗休眠判断与进入
8. `App_Can()` — CAN 总线通信（飞道协议）
9. `App_FlashUpdate()` — Flash 参数持久化写入
10. `App_LogRecord()` — 事件日志记录
11. `App_ProID_Deal()` — 产品 ID 处理
12. `Feed_IWatchDog` — 喂狗
13. `DBG_Task()` — 调试系统轮询

### 配置系统

**三层配置结构：**

1. **`conf/Project_Config.h`** — 用户可见配置，支持 Keil Configuration Wizard 可视化编辑。包含电池类型、化学体系、固件版本、额定电流、AFE 类型、功能开关、唤醒源、串口角色、SOC 校准参数、升级策略等
2. **`conf/conf.h`** — 将 `Project_Config.h` 的 `PROJECT_CFG_*` 宏翻译为代码中使用的 legacy `#define` 名称（如 `TERNARYLI`、`__FUNC_RTC__`、`_COMMOM_UPPER_SCI1` 等），保持向后兼容
3. **`conf/Project_BuildGuard.h`** — 编译时断言，验证所有配置值的合法范围，Release 构建时禁止启用调试功能

**配置修改流程：** 在 Keil 中右键 `Project_Config.h` → Configuration Wizard 修改 → 重新编译。修改 `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` 可触发固件升级后的参数重置策略。

### 通信协议

- **RS485/SCI** (`Sci_Upper.c/h`)：Modbus-like 协议，通过 `RS485_CMD_READ_REGS`/`WRITE_REG`/`WRITE_REGS` 读写寄存器。SCI1 配置为 Host 角色 (`_COMMOM_UPPER_SCI1`)
- **CAN** (`Can_HDX.c/h`, `CanFeidaoFrames.c/h`)：飞道 (Feidao) 私有 CAN 协议帧
- **I2C** (`I2C_AFE1.c/h`)：通过软件 TWI（PB8=SCL, PB9=SDA，位拆裂模拟 I2C 时序）与 SH367309 AFE 芯片通信，读取单体电压和温度。**非 STM32 硬件 I2C 外设**，含 CRC8 校验
- **SCI 上位机协议**：寄存器地址映射管理 SOC 数据、故障标志、系统状态等

### Flash 持久化存储

`Flash.h` 定义完整的存储布局，使用 **A/B 双槽位** 进行磨损均衡：

| 地址范围 | 内容 |
|---|---|
| `0x08000000` | IAP 引导区 |
| `0x08004800` | APP 应用区 |
| `0x0801C000` | AFE 参数 (A/B) |
| `0x0801C400` | 读写参数 (A/B) |
| `0x0801D000` | 事件日志 (A/B) |
| `0x0801E000` | SOC 数据持久化 |
| `0x0801F000` | 升级参数标志 |
| `0x0801F400` | 工厂老化标志 |
| `0x0801F800` | 固件更新标志 |
| `0x0801FC00` | 休眠模式标志 |

`App_FlashUpdate()` 在主循环中延迟写入（避免阻塞）。SleepDeal 模块提供 BOOT_FLAG 用于跨复位/唤醒状态传递。

### SOC (State of Charge) 系统

- **`SOC.c/h`** — SOC 入口/接口
- **`SocEnhance.c/h`** — 核心 SOC 算法：安时积分（库仑计数）、开路电压校准、满充校准、低尾校准、静置 OCV 漂移修正
- **`DataDeal.c/h`** — 中间数据处理层，聚合 AFE 数据、计算电压/电流/温度、SOC 校准触发
- **`SH367309_DataDeal.c/h`** — SH367309 特定数据处理
- **`SH367309_Func.c/h`** — SH367309 寄存器配置、保护参数设置

SOC 配置参数通过 `Project_Config.h` 中的 `PROJECT_CFG_SOC_*` 宏调整。

### 低功耗休眠系统

三种休眠模式 (`rtc_sleep.c/h`, `SleepDeal.c/h`)：

- **NORMAL_MODE** — 常规 RTC 定时休眠，定期唤醒
- **HICCUP_MODE** — 间歇休眠（打嗝模式），短睡长醒
- **DEEP_MODE** — 深度休眠（如过放保护后），长期休眠直到外部触发唤醒

休眠进入条件通过 `LP_BLOCK_*` 位掩码检查（充电中、放电中、通信活跃、Flash 忙、升级中等阻止休眠）。唤醒源包括 UART1、RS485、充电检测 (`PA0`)、按键、CAN 等。

**Boot Flag 系统**（存储在 BKP_DR2/DR3 + Flash `0x0801FC00`，软件复位保留）：

| 值 | 含义 |
|----|------|
| `0x1234` | NORMAL 休眠 |
| `0x1235` | DEEP 休眠 |
| `0x1236` | HICCUP 休眠 |
| `0x1237` | 充电器唤醒 |
| `0xFFFF` | 正常上电 |

`RTC.c/h` 管理 RTC 时钟和闹钟唤醒配置。IO 状态在不同休眠模式间切换 (`conf.h` 中的 `IOstatus_NormalMode()`/`IOstatus_RTCMode()`/`IOstatus_DeepMode()`)。

### 调试基础设施

**所有调试功能在 Release 构建时必须关闭** (`BuildGuard` 强制检查)：

- **`DebugWatch.c/h`** — 全局数据结构观察（绑定点注册）
- **`DebugHooks.c/h`** — 主循环各段的执行时间标记（用于 Keil 调试器实时观察）
- **`SystemDebug.c/h`** — 系统级调试变量导出（`g_dbg` 全局结构体）
- **`IrqDebug.c/h`** — 中断计数和唤醒源追踪
- **`debug_hub.c/h`** — 调试数据集中管理（magic `0x44424748`），通过 `DBG_Task()` 轮询
- **`System_Monitor.c/h`** — 系统错误状态机、功能启停控制、MOS 管状态管理

### 故障保护系统

`Fault.h` 定义三级故障体系（一级/二级/三级保护），涵盖：
- 单体过压/欠压 (CellOvp/Uvp)
- 总压过压/欠压 (BatOvp/Uvp)
- 充放电过流 (IchgOcp/IdischgOcp)
- 温度保护 (CellChgOTp/CellDsgOTp, MosOTp)
- 压差过大 (VdeltaOvp)
- 均衡相关、CBC 保护等

保护阈值保存在 Flash 读写参数区，可通过 RS485 修改。

### 关键全局变量速查

| 变量 | 类型 | 文件 | 用途 |
|------|------|------|------|
| `g_stCellInfoReport` | `stCell_Info` | `Sci_Upper.h` | BMS 核心状态（电压/电流/温度/SOC/故障标志），被 RS485/CAN/休眠/日志/调试消费 |
| `PRT_E2ROMParas` | `PRT_E2ROM_PARAS` | `Fault.h` | 13 种故障 × 5 参数 = 65 字段的保护阈值，来源为 Flash 读写参数区 |
| `OtherElement` | `OTHER_ELEMENT` | `DataDeal.h` | 系统配置参数（均衡电压/电流限制/休眠参数/SOC 容量/串数/采样电阻等） |
| `SH367309_Read_AFE1` | `SH367309_Read` | `I2C_AFE1.h` | AFE 测量结果（16 串电压 + 3 路温度 + 总压 + 电流），200ms 更新 |
| `Registers_AFE1` | AFE 寄存器结构 | `I2C_AFE1.h` | AFE 状态寄存器原始值本地镜像（BSTATUS1-3, BALANCE 等） |
| `SH367309_Reg_Store` | AFE 控制寄存器 | `SH367309_Func.h` | AFE 控制寄存器位域结构（MTP_CONF, BSTATUS, BALANCE） |
| `g_stLowPowerRtcStatus` | `LOW_POWER_RTC_STATUS` | `rtc_sleep.h` | 低功耗状态机（模式/空闲计数/阻塞原因/累计休眠时间） |
| `sys_time` | `Time_T` | `conf.h` | 系统运行计数器（中断计数/PEC 错误/唤醒原因/调试标志） |
| `System_ErrFlag` | `SYSTEM_ERROR` | `System_Monitor.h` | 系统错误标志位域（AFE/CAN/EEPROM/CBC/HSE/LSE 等通信和硬件错误） |
| `SeriesNum` | `UINT8` | `Runtime.c` | 电池串数（默认 10，可通过 RS485 修改） |

**故障标志位域** (`MDLCHGFAULT_REG`)：`UINT16 all` + 14-bit bitfield（CellOvp/Uvp, BatOvp/Uvp, IchgOcp/IdischgOcp, CellChgOTp/CellDsgOTp, CellChgUtp/CellDsgUtp, VcellDeltaBig, TempDeltaBig, SocLow, TmosOTp）

**系统时基** (`SYS_TIME`)：`UINT16 all` + 5-bit bitfield（10ms/50ms/100ms/200ms/1000ms），TIM3 中断驱动

### 已知风险和注意事项

**安全相关（修改代码时必须注意）：**

1. **SW OVP 硬编码 4280mV**（`DataDeal.c:1135`）— 不跟随 RS485 可配置的 `u16VcellOvp` 阈值，LiFePO4 下保护行为不一致
2. **化学体系阈值分散 4 处**（`Fault.h`, `SH367309_Func.h`, `SH367309_DataDeal.h`, `DataDeal.h`）— 各自 `#ifdef TERNARYLI`，修改时极易遗漏
3. **均衡电压硬编码 4160mV**（`SH367309_DataDeal.c:75`）— `OtherElement.u16Balance_OpenVoltage` 配置值被忽略
4. **认证模式误熔丝风险**（`DataDeal.c:1097`）— I2C 干扰导致 PEC 错误可能误触 `GPIO_RF_EN` 硬件熔断（不可逆）
5. **HICCUP 阻塞主循环**（`rtc_sleep.c:285`）— `while()` 循环期间 CAN/RS485/喂狗全部停摆
6. **`Fault_Flag_*` vs `unMdlFault_*` 两套故障标志** — `Fault_Flag_First/Second/Third` 从未被设置，实际有效数据在 `g_stCellInfoReport.unMdlFault_*`

**代码质量：**

- `DataDeal.c` 中 `#if 0` 死代码块、`DataLoad_soc_test()` 等未调用函数
- `I2C_AFE1.c:719` — `UpdateVoltageFromBqMaximo()` 函数名含 "BQ"（TI 芯片），实际操作 SH367309
- `new_todo_logi()` ~140 行 5 层嵌套；`Sci_Deal_ReadRegs_0x03()` 7+ 级 if-else
- Cortex-M3 上 `int64_t`/`uint64_t` 运算无硬件支持，SOC 计算中可避免

### 通信工具固件 (`firmware/comm_tool_f103ret6/`)

分层结构：
- **`source/bsp/`** — 板级支持包（UART、CAN、GPIO）
- **`source/app/`** — 应用层（协议解析 `ct_protocol`、帧处理 `ct_app`、CAN 网关 `ct_can_gateway`、升级管理 `ct_upgrade_manager`、IAP `ct_self_iap`）
- **`source/iap/`** — IAP 引导加载程序应用

主循环模式：UART 字节 → 协议解析 → 帧处理 → 轮询任务。

### IAP 系统架构

BMS 项目涉及 **3 个 MCU + 4 个固件** 的升级体系。IAP 引导程序源码在独立仓库 `103c8-iap`。

**SRAM 邮箱握手（APP ↔ IAP）：**

- 地址：`0x20004FE0`（SRAM 最高 32 字节，软件复位不清除）
- 结构：`magic(0x49415031) + magic_inv + request(0x5AA55AA5) + request_inv + crc`
- 流程：APP 填充邮箱 → `NVIC_SystemReset()` → IAP 检查邮箱 → 有请求则留在 IAP，无请求则验证 APP 向量表后跳转

**CAN IAP 协议：**

| 帧类型 | CAN ID | 说明 |
|--------|--------|------|
| 控制帧 | `0x14F8F000 \| node_id` | IAP 命令/响应 |
| 确认帧 | `0x14F8F100 \| node_id` | ACK/NACK |
| 数据帧 | `0x14000000 \| (seq<<8) \| node_id` | 固件数据块 (32帧×8B=256B) |

命令序列：HELLO → START → DATA+COMMIT (循环) → END → 复位 → 跳转新 APP

**升级参数策略：** `PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION` 控制固件升级后是否重置 AFE 参数/保护参数/SOC 快照等，通过 `UpgradeParamPolicy.h` 中的 `UPGRADE_PARAM_RESET_*` 宏配置。

**IAP → APP 跳转：** 验证 APP 向量表 (SP 在 SRAM 范围, PC 在 Flash 范围) → `SCB->VTOR = 0x08004800` → `__set_MSP(*0x08004800)` → 调用 APP 复位处理程序

## Git Hooks

- **pre-commit** — 运行 `tools/project_check.py --quiet`（项目配置检查脚本）
- **pre-push** — 同样运行 `project_check.py`，然后检查 `TODO.md` 中是否有未完成项 (`- [ ]`)，有则打印警告但不阻止推送

## 编码约定

- 源文件编码为 **GBK**（中文注释和字符串可能用 GBK）
- 数据类型使用自定义别名：`UINT8`、`UINT16`、`UINT32`、`UINT64`（来自 ST 标准库）
- 全局变量命名：`g_` 前缀（如 `g_st_SysTimeFlag`、`g_stLowPowerRtcStatus`）
- 结构体命名：全大写 + 下划线（如 `SYSTEM_ERROR`、`CBC_ELEMENT`）
- 枚举命名：模块前缀 + 下划线（如 `RS485_CMD_E`、`SLEEP_MODE`）
- 位域使用 `union { UINTxx all; struct { ... } bits; }` 模式便于整体和单 bit 操作
- 延时以 10ms 为基准单位：`DELAYB10MS_*` 宏（见 `main.h`）

## 注意事项

- **无自动化测试框架** — 这是一个嵌入式裸机项目，没有单元测试或 CI。验证依赖实际硬件测试。
- **无 Makefile/CMake** — 完全依赖 Keil uVision IDE 构建。修改源文件后需在 Keil 中重新编译。
- **`tools/` 目录被 `.gitignore` 排除** — 工具脚本可能仅在本地存在。
- **GBK 编码** — 处理中文注释时注意编码，git 默认处理可能乱码。
- **`TODO.md`** 是主要的工作追踪文件，项目级 TODO（非代码内）。

## 项目文档

详细分析文档位于 `docs/project-review/`（14 个文件）：

| 文档 | 内容 |
|------|------|
| `00_项目总览.md` | 固件组成、技术栈、架构决策、文件统计 |
| `01_目录结构说明.md` | 完整目录树，按功能分组 |
| `02_启动流程和main流程.md` | Runtime_Boot 26 步 + 主循环 13 任务 |
| `03_任务调度和周期函数.md` | 时基系统、各周期任务、顺序依赖 |
| `04_中断和外设流程.md` | 23 个 ISR + 8 个外设 + HardFault |
| `05_保护逻辑说明.md` | 13 种故障 × 3 级 + AFE→MCU 转换 |
| `06_SOC算法说明.md` | 安时积分 + 5 种校准机制 |
| `07_通信协议说明.md` | RS485(Modbus) + CAN(Feidao) + I2C(TWI) |
| `08_参数存储和Flash说明.md` | 256KB Flash 布局、3 种存储策略 |
| `09_低功耗流程说明.md` | 3 种休眠模式 + LP_BLOCK + Boot Flag |
| `10_IAP和Flash地址分配.md` | 3 MCU 架构 + SRAM 邮箱 + CAN IAP 协议 |
| `11_关键数据结构和全局变量.md` | 10 大全局变量 + 数据流图 |
| `12_风险点和待确认问题.md` | 7 安全风险 + 3 架构风险 + 22 待确认项 |
| `13_后续维护建议.md` | P1-P4 优先级 × 19 条建议 |
