# 低功耗管理模块分析

## 1. 模块概述：三层架构

低功耗管理采用分层隔离设计，共三层：

| 层级 | 文件 | 职责 |
|------|------|------|
| **决策层** | `rtc_sleep.c` / `rtc_sleep.h` | 睡眠模式选择、阻塞条件判断、休眠请求调度 |
| **移植层** | `rtc_sleep_port.c` / `rtc_sleep_port.h` | 将硬件操作封装为 Port 接口，隔离 MCU 外设差异 |
| **驱动层** | `conf.c` (GPIO/WakeUp/STOP)、`RTC.c`、`LowPowerSleep.c`、`SleepDeal.c`、`rtc_sleep_afe_sh367309.c` | 具体硬件初始化、STOP 进入/恢复、AFE 通信、BootFlag 管理 |

**调用关系**：`Runtime.c` 主循环每秒调用 `rtc_sleep()` (line 83)，该函数评估所有条件后决定进入 HICCUP/NORMAL/DEEP 三种休眠模式之一。

---

## 2. 三种睡眠模式

### 2.1 HICCUP 模式（打嗝模式）

**定义值**：`FLASH_HICCUP_SLEEP_VALUE = 0x1236`（`Flash.h` line 54）

**行为**：
- 在运行态进入 STOP 模式，RTC 定时唤醒后恢复运行
- 每次唤醒检查是否有异常（电流、AFE 状态）
- 若无异常，继续休眠（`rtc_sleep_run_hiccup_cycle` 循环）
- 若有异常，退出休眠，处理唤醒事件
- **不触发 MCU 复位**，在 STOP/WFI 之间循环

**核心流程**（`rtc_sleep.c` lines 218-257）：
1. `rtc_sleep_prepare_rtc()` → 保存核心状态、初始化 RTC、配置 GPIO
2. `RtcSleep_PortEnterStop()` → 喂狗、进入 STOP (WFI)
3. 唤醒后读取 RTC 周期秒数，累加 `sleep` 计时
4. 检查 `rtc_sleep_has_wakeup_exception()` → 有异常则退出循环
5. 无异常则继续下一轮休眠

**适用场景**：空闲超时后的常规低功耗状态，保持快速唤醒能力。

### 2.2 NORMAL 模式

**定义值**：`FLASH_NORMAL_SLEEP_VALUE = 0x1234`（`Flash.h` line 52）

**行为**：
- 通过 `SleepDeal_Continue()` → `MCU_RESET()` 触发复位
- 复位后 `SleepDeal_HandleBootSleepStartup()` 读取 BootFlag，识别为 NORMAL 唤醒
- 配置较少的 WakeUp 中断，进入 STOP 等待唤醒
- 唤醒后通过 `MCU_RESET()` 恢复

**GPIO 配置**（`conf.c` line 291）：
- `IOstatus_NormalMode()` → 调用 `IOstatus_Base()`
- 所有 GPIO 配置为模拟输入（`Conf_InitAllPortsAnalog`）
- 主电源轨复位

**唤醒源**（`conf.c` lines 204-241）：
- Base: CHG_IN (PA0, EXTI0, 下降沿)、SW (PA9, EXTI9, 下降沿)
- 额外: UART1_RX (PB7, EXTI7, 上升沿, 条件编译)、INT_WK_CMNT (PB12, EXTI12, 上升沿)、MCU_WK (PB13, EXTI13, 上升沿)

**触发时机**：`DataDeal.c` line 973 — AFE 错误延时超时后请求。

### 2.3 DEEP 模式

**定义值**：`FLASH_DEEP_SLEEP_VALUE = 0x1235`（`Flash.h` line 53）

**行为**：
- 与 NORMAL 类似通过复位进入，但唤醒源最少
- BootFlag 复位后进入 STOP，仅 Base 唤醒源生效
- 唤醒后通过 `MCU_RESET()` 恢复

**GPIO 配置**（`conf.c` line 296）：
- `IOstatus_DeepMode()` → 调用 `IOstatus_Base()`
- 所有 GPIO 模拟输入

**唤醒源**（`conf.c` lines 252-255）：
- 仅 Base: CHG_IN (PA0)、SW (PA9)
- **无** INT_WK_CMNT、MCU_WK、UART1 等唤醒

**触发时机**：
- 低电压强制: 最低单体电压 ≤ 2800mV 且充电电流 ≤ 5mA，持续 600 秒 (`rtc_sleep.c` lines 10-11, 124-135)
- 低电压可配: 最低单体电压 ≤ `u16Sleep_Vlow` 且充电电流 ≤ 5mA，持续 `u16Sleep_TimeVlow` 分钟 (`rtc_sleep.c` lines 138-148)
- 上位机指令: 寄存器写入 0x0A (`Sci_Upper.c` line 2010)
- 充电器拔出: `DataDeal.c` line 140 — 充电器拔出后请求 DEEP

---

## 3. 关键函数与行号

### rtc_sleep.c（决策层核心）

| 函数 | 行号 | 功能 |
|------|------|------|
| `LP_GetBlockReason()` | 30-88 | 逐项检查阻塞原因，返回位掩码 |
| `lp_refresh_status()` | 90-94 | 刷新 RTC 唤醒标志和空闲最大值 |
| `low_power_log_and_commit_sleep()` | 96-105 | 校验模式后提交休眠 |
| `LowPower_Request()` | 107-122 | 对外接口：设置请求模式 |
| `lp_select_deep_if_low_voltage()` | 124-153 | 低电压检测，决定是否强制 DEEP |
| `lp_update_sleep_request()` | 155-177 | 空闲超时计数，达到阈值请求 HICCUP |
| `rtc_sleep_has_wakeup_exception()` | 179-201 | 检查 AFE 数据、电流、AFE 状态异常 |
| `rtc_sleep_prepare_rtc()` | 203-216 | 休眠前准备：保存状态、初始化 RTC |
| `rtc_sleep_run_hiccup_cycle()` | 218-257 | HICCUP 单次休眠-唤醒循环 |
| `rtc_sleep()` | 259-294 | 主入口：每秒调度一次 |

### rtc_sleep_port.c（移植层）

| 函数 | 行号 | 功能 |
|------|------|------|
| `RtcSleep_PortPrepareRtcStop()` | 68-75 | 保存核心状态 + RTC 初始化 + GPIO 配置 + 唤醒配置 |
| `RtcSleep_PortEnterStop()` | 77-84 | 喂狗 → 设置调试阶段 → `Sys_StopMode()` |
| `RtcSleep_PortDisableStopWakeup()` | 86-90 | 禁用 EXTI 唤醒 + RTC 停止唤醒 |
| `RtcSleep_PortRestoreAfterStop()` | 92-97 | 恢复外设：ADC/UART/CAN/Timer |
| `RtcSleep_PortGuessWakeupSource()` | 123-136 | 无中断信息时猜测唤醒源 (CHG_IN 或 SW) |
| `RtcSleep_PortOnWakeupSource()` | 138-145 | 唤醒后动作：soc_key 时点亮 LED |
| `RtcSleep_PortApplySocRtcRest()` | 109-114 | 将休眠时间补偿给 SOC 算法 |
| `RtcSleep_PortAddRuntimeSeconds()` | 116-121 | 累加运行时间秒计数 |

### rtc_sleep_afe_sh367309.c（AFE 驱动层）

| 函数 | 行号 | 功能 |
|------|------|------|
| `RtcSleep_AfePortUpdateRtcData()` | 11-22 | 从 SH367309 读取电压/温度数据 |
| `RtcSleep_AfePortHasCurrentWake()` | 24-46 | 检查是否有充放电电流 |
| `RtcSleep_AfePortHasAfeWake()` | 48-79 | 读取 AFE 寄存器，检查 MOS 状态和故障 |

### SleepDeal.c（BootFlag 管理）

| 函数 | 行号 | 功能 |
|------|------|------|
| `SleepDeal_Continue()` | 100-125 | 保存状态 → 写 BootFlag → AFE 休眠 → MCU 复位 |
| `BootFlag_Write()` | 136-141 | 写 BKP_DR2 + BKP_DR3 反码 |
| `BootFlag_Read()` | 159-183 | 读取并校验 BKP 标志 |
| `BootFlag_Clear()` | 185-188 | 清除 BootFlag |
| `SleepDeal_HandleBootSleepStartup()` | 217-265 | 复位后根据 BootFlag 分发处理 |
| `SleepDeal_WaitStopWakeup()` | 206-215 | 循环进入 STOP 直到有效唤醒 |
| `SleepDeal_IsWakeupValid()` | 39-98 | 唤醒验证：充电器优先，按键需长按 500ms |

### LowPowerSleep.c（状态保存）

| 函数 | 行号 | 功能 |
|------|------|------|
| `LowPowerSleep_SaveCoreState()` | 5-10 | 保存 CAN/SOC/FactoryAging 状态 |
| `LowPowerSleep_SaveResetState()` | 12-16 | 在 CoreState 基础上额外保存 LED SOC |

---

## 4. 唤醒源配置

### 4.1 EXTI 唤醒中断

| 唤醒源 | GPIO | EXTI Line | 触发方式 | 使用模式 |
|--------|------|-----------|----------|----------|
| CHG_IN（充电器插入） | PA0 | EXTI0 | 下降沿 | Normal, RTC, Deep |
| SW（用户按键） | PA9 | EXTI9 | 下降沿 | Normal, RTC, Deep |
| INT_WK_CMNT（AFE 中断） | PB12 | EXTI12 | 上升沿 | Normal, RTC |
| MCU_WK（SOC 唤醒） | PB13 | EXTI13 | 上升沿 | Normal, RTC |
| UART1_RX（串口） | PB7 | EXTI7 | 上升沿 | Normal（条件编译） |
| EXTI17（RTC 周期唤醒） | — | EXTI17 | 上升沿 | RTC Mode |

### 4.2 休眠后唤醒验证

`SleepDeal_IsWakeupValid()`（`SleepDeal.c` lines 39-98）实现了唤醒验证机制：

1. **充电器优先**：若 `CHG_IN` 有效，直接标记充电器唤醒，返回成功
2. **按键长按**：PC13 按键需持续按住 500ms（`DI1_LONG_PRESS_WAKE_10MS = 50`，每步 10ms）
3. **按键抖动处理**：松开后等待显示时间 `LEDBAR_SOC_DISPLAY_10MS`，若期间按键恢复则继续长按计数
4. **充电器插拔**：在按键等待期间持续检测充电器插入

### 4.3 唤醒源猜测

当 `RTC_ClearStopWakeup()` 未检测到明确唤醒源时，`RtcSleep_PortGuessWakeupSource()`（`rtc_sleep_port.c` lines 123-136）通过 GPIO 电平反推：
- PA0 低电平 → `PA0_irq`（充电器）
- PA9 低电平 → `soc_key`（SOC 按键）

---

## 5. GPIO 配置

### 5.1 Normal 模式 GPIO

`IOstatus_NormalMode()`（`conf.c` line 291）→ `IOstatus_Base()`（`conf.c` lines 257-271）：
- 所有 GPIO 配置为**模拟输入**（最低功耗）
- 主电源轨部分关闭：`Bit_RESET, Bit_RESET, Bit_SET, Bit_RESET, Bit_RESET, Bit_RESET`
- 调用 `Conf_PrepareStopEntry()` → LED 休眠 + ADC 停止

### 5.2 RTC 模式 GPIO

`IOstatus_RTCMode()`（`conf.c` lines 273-289）：
- GPIOA: 除 `PIN_2737_EN` 外全部模拟输入
- GPIOB: 除 `GPIO_Pin_14` 外全部模拟输入
- GPIOC/D/E: 全部模拟输入
- **DC_EN 拉低**（`GPIO_WriteBit(Bit_RESET)`），然后配置为推挽输出
- LED 休眠

### 5.3 Deep 模式 GPIO

`IOstatus_DeepMode()`（`conf.c` line 296）→ `IOstatus_Base()`，与 Normal 相同。

### 5.4 进入 STOP 前的 GPIO 操作

`Conf_PrepareStopEntry()`（`conf.c` lines 113-117）：
- `LedBar_SetSleep(1u)` — 标记 LED 进入休眠
- `ADC_StopForLowPower()` — 停止 ADC 采样

---

## 6. BKP 备份域使用

### 6.1 寄存器分配

| 寄存器 | 用途 | 定义 |
|--------|------|------|
| `BKP_DR2` (`SLEEP_BKP_FLAG_REG`) | 休眠标志 | `SleepDeal.c` line 133 |
| `BKP_DR3` (`SLEEP_BKP_INV_REG`) | 休眠标志反码 | `SleepDeal.c` line 134 |

### 6.2 校验机制

`BootFlag_Read()`（`SleepDeal.c` lines 159-183）：
- 读取 DR2（flag）和 DR3（~flag）
- 检查 `flag ^ inverse_flag == 0xFFFF`
- 校验失败返回 `BOOT_FLAG_RESET_VALUE`（0xFFFF）

### 6.3 BootFlag 值定义

| 值 | 含义 | 文件位置 |
|----|------|----------|
| `0x1234` | NORMAL 唤醒 | `Flash.h` line 52 |
| `0x1235` | DEEP 唤醒 | `Flash.h` line 53 |
| `0x1236` | HICCUP 唤醒 | `Flash.h` line 54 |
| `0x1237` | 充电器唤醒 | `Flash.h` line 55 |
| `0xFFFF` | 复位/清除 | `Flash.h` line 56 |

### 6.4 时序

1. `SleepDeal_Continue()` → `BootFlag_Write(boot_flag)` → 写 BKP_DR2 + DR3
2. `AFE_Sleep()` → AFE 进入休眠
3. `MCU_RESET()` → MCU 复位
4. 复位后 `SleepDeal_HandleBootSleepStartup()` → `BootFlag_Read()` → 识别休眠类型
5. 处理完成后 `BootFlag_Clear()` → 写 0xFFFF

---

## 7. BootFlag 机制

### 7.1 完整状态机

```
运行态
  │
  ├─ rtc_sleep() 每秒检查
  │     │
  │     ├─ 空闲超时 → HICCUP_MODE
  │     │     └─ rtc_sleep_run_hiccup_cycle() 循环
  │     │         ├─ STOP → RTC 唤醒 → 无异常 → 继续循环
  │     │         └─ STOP → RTC 唤醒 → 有异常 → 退出，处理唤醒源
  │     │
  │     ├─ 低电压 → DEEP_MODE → SleepDeal_Continue() → 复位
  │     │
  │     └─ AFE 错误 → NORMAL_MODE → SleepDeal_Continue() → 复位
  │
  └─ SleepDeal_Continue()
        │
        ├─ 写 BootFlag (BKP_DR2 + DR3)
        ├─ LowPowerSleep_SaveResetState() 保存 CAN/SOC/LED 状态
        ├─ InitAFE1_Sleep(0) + AFE_Sleep()
        └─ MCU_RESET()
              │
              ▼
        SleepDeal_HandleBootSleepStartup()
              │
              ├─ HICCUP: 清标志 → RTC+GPIO 初始化 → WaitStop → 复位
              ├─ NORMAL: 清标志 → GPIO 初始化 → WaitStop → 复位
              ├─ DEEP:   清标志 → GPIO 初始化 → WaitStop → 复位
              ├─ 充电器唤醒: 标记 chg_wake，不清标志
              └─ 复位值: 清标志
```

### 7.2 充电器唤醒特殊路径

`SleepDeal_IsBootFromSleepChargerWakeup()`（`SleepDeal.c` lines 196-204）：
- 若 `chg_wake == 0` 但 BKP 中仍为 `FLASH_SLEEP_CHARGER_WAKE_VALUE`，补标 `chg_wake = 1`
- 这确保即使 BootFlag 未在 `HandleBootSleepStartup` 中被清除（因为 switch 命中 case 254 后不清标志），后续查询仍能正确识别

### 7.3 外部通信计数器

`s_sleep.ext_comm`（`SleepDeal.c` line 8）记录休眠期间外部通信次数：
- `SleepDeal_RecordExternalComm()` 每次通信递增
- `RtcSleep_PortGetExternalCommCounter()` 读取
- `LP_GetBlockReason()` 检测计数变化，若变化则设置 `LP_BLOCK_EXT_COMM`，阻止进入休眠

---

## 8. 潜在问题与风险

### 8.1 阻塞条件遗漏

`LP_GetBlockReason()`（`rtc_sleep.c` lines 30-88）检查了 10 项阻塞条件，但以下场景未覆盖：
- **ADC 采样进行中**：若 ADC 正在采样时恰好通过所有检查，可能导致采样数据丢失
- **NVM 写入**：`StorageFlash_IsBusy()` 检查了 Flash 忙，但若 BKP 写入正在进行（`BootFlag_Write`），无额外保护
- **CAN 帧发送中**：`Can_IsBusy()` 仅检查忙标志，不检查是否有待发送队列

### 8.2 HICCUP 模式的无限循环风险

`rtc_sleep()` lines 286-288：
```c
while (rtc_sleep_run_hiccup_cycle())
{
}
```
若 AFE 持续返回正常状态（无异常、无唤醒源），且 RTC 周期正常，此循环将持续运行。虽然每次循环都会进入 STOP 并唤醒，但**没有最大循环次数限制**。若 `g_stLowPowerRtcStatus.cycles` 溢出（uint32_t，约 136 年），理论上不会发生，但逻辑上缺少退出条件。

### 8.3 低电压判断的竞态

`lp_select_deep_if_low_voltage()`（`rtc_sleep.c` lines 124-153）：
- 检查 `RtcSleep_PortGetCellMinMv()` 时，电压可能在阈值附近波动
- `force` 和 `vlow` 计数器在电压恢复时清零（line 150-151），但若电压在 2800mV 附近反复跳变，可能导致计数器频繁重置
- `u16Sleep_Vlow` 来自可配置参数 `OtherElement`，若配置不当（如设为 0）可能导致立即进入 DEEP

### 8.4 BootFlag 写入与复位的时序窗口

`SleepDeal_Continue()`（`SleepDeal.c` lines 100-125）：
```c
BootFlag_Write(boot_flag);  // 写 BKP
InitAFE1_Sleep(0);          // AFE 初始化
AFE_Sleep();                // AFE 休眠
MCU_RESET();                // 复位
```
若在 `BootFlag_Write` 之后、`MCU_RESET()` 之前发生异常复位（如看门狗），BKP 已写入但 AFE 未正确休眠，可能导致：
- 复位后进入错误的休眠路径
- AFE 处于未知状态

### 8.5 BKP 校验的局限性

`BootFlag_Read()` 使用 XOR 反码校验（`SleepDeal.c` line 167）：
```c
if ((UINT16)(flag ^ inverse_flag) != 0xFFFF)
```
- 仅能检测单 bit 翻转，无法检测双 bit 同时翻转
- BKP 寄存器在 VBAT 断电时可能丢失，此时返回 `BOOT_FLAG_RESET_VALUE`（正常行为）
- 无 CRC 或 ECC 保护

### 8.6 GPIO 模拟输入配置的潜在功耗

`IOstatus_RTCMode()`（`conf.c` lines 279-283）：
- GPIOA 保留 `PIN_2737_EN` 不设为模拟输入
- GPIOB 保留 `GPIO_Pin_14` 不设为模拟输入
- 若这两个引脚外接高阻抗信号，可能产生微小漏电流
- `DC_EN` 被拉低后配置为推挽输出，确保 DC-DC 关闭

### 8.7 唤醒后复位的副作用

所有三种休眠模式最终都通过 `MCU_RESET()` 恢复（`conf.c` lines 301-314）。这意味着：
- 每次从 STOP 唤醒后都会执行完整复位
- 无法区分"从 STOP 直接恢复"和"从复位恢复"
- `InitRunAfterStopWakeup()`（`conf.c` lines 334-356）中的恢复逻辑（ADC/CAN/Timer 重初始化）仅在 HICCUP 模式的 `rtc_sleep_run_hiccup_cycle()` 中使用，NORMAL/DEEP 模式不经过此路径

### 8.8 `g_irq_t` 全局变量的非原子操作

`rtc_sleep.c` line 14：
```c
enum irqWakeup g_irq_t = NO_IRQ;
```
- 在 `rtc_sleep_has_wakeup_exception()` 中被写入（line 190, 197）
- 在 `rtc_sleep_run_hiccup_cycle()` 中被读取（line 246-250）
- `enum irqWakeup` 为 32 位，STM32F103 为 32 位 MCU，读写原子性取决于对齐
- 虽然当前在单线程主循环中使用，但若未来引入中断上下文访问需注意

### 8.9 看门狗喂养时机

`RtcSleep_PortEnterStop()`（`rtc_sleep_port.c` lines 77-84）在 STOP 前后各喂一次狗：
```c
Feed_IWatchDog;        // 进入前喂
Sys_StopMode();        // STOP (WFI)
Feed_IWatchDog;        // 唤醒后喂
```
若 STOP 时间超过看门狗超时周期，会触发复位。当前 RTC 唤醒周期未见明确配置，需确保小于 WDG 超时。

---

## 附录：文件索引

| 文件 | 路径 |
|------|------|
| 决策层头文件 | `103 + 309/Project/Source/rtc_sleep.h` |
| 决策层实现 | `103 + 309/Project/Source/rtc_sleep.c` |
| 移植层头文件 | `103 + 309/Project/Source/rtc_sleep_port.h` |
| 移植层实现 | `103 + 309/Project/Source/rtc_sleep_port.c` |
| AFE 驱动层 | `103 + 309/Project/Source/rtc_sleep_afe_sh367309.c` |
| AFE 驱动头文件 | `103 + 309/Project/Source/rtc_sleep_afe_port.h` |
| BootFlag 管理 | `103 + 309/Project/Source/SleepDeal.c` |
| 状态保存 | `103 + 309/Project/Source/LowPowerSleep.c` |
| 状态保存头文件 | `103 + 309/Project/Source/LowPowerSleep.h` |
| GPIO/WakeUp 配置 | `103 + 309/Project/Source/conf/conf.c` |
| Flash 地址定义 | `103 + 309/Project/Source/Flash.h` |
