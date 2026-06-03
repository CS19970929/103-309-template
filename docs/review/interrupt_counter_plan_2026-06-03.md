# 中断梳理与中断计数方案

文档状态：部分验证
源码验证：已按源码静态核对；中断计数已实现；Keil FD_Release 已编译通过，待上板验证
日期：2026-06-03
范围：`103 + 309` BMS 主固件为主体；`firmware/comm_tool_f103ret6` 仅单独列出，不纳入本次主固件计数模块

## 2026-06-03 实施记录

本轮按用户“开始实现”执行主固件中断计数，不修改上位机协议、不新增 Modbus/CAN 可见寄存器、不烧录。

源码变更：

- 新增 `103 + 309/Project/Source/IrqDebug.h`、`103 + 309/Project/Source/IrqDebug.c`，提供 `g_stIrqDebug` 全局计数、分阶段计数、最近事件环和未实现向量兜底记录。
- `Project_Config.h` 新增 `PROJECT_CFG_IRQ_DEBUG_ENABLE`、`PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE`，默认打开轻量计数和事件环。
- `stm32f10x_it.c` 为异常、EXTI、USART 入口增加计数；EXTI 按 pending line 分源，保留原 pending 清除和 `g_irq_t` 行为。
- `RTC.c`、`System_Init.c`、`LedBar.c`、`Can_HDX.c` 分别为 RTC、TIM3、TIM4、CAN RX0 增加轻量计数。
- `AppInit.c`、`rtc_sleep.c`、`rtc_sleep_port.c`、`SleepDeal.c` 接入 `BOOT/RUN/SLEEP_PREPARE/STOP_WAIT/STOP_WAKE_RAW/STOP_RESTORE/RESET_SLEEP_WAIT/FAULT` 阶段标记。
- `SystemDebug.h/.c` 将 `g_stIrqDebug` 的代表性计数和最近中断摘要同步到 `g_dbg.irq`，完整数据仍直接观察 `g_stIrqDebug`。
- `startup_stm32f10x_hd.s` 默认 weak handler 记录 `last_vectactive` 后仍停住，不改变未处理向量的停住语义。
- Keil `FD_Release`/`FD_Debug` 两个 Target 均已加入 `IrqDebug.c`。

注意：`conf.c` 含历史非 UTF-8 字节，本轮没有重编码该文件；STOP 阶段标记放在 `rtc_sleep_port.c` 和 `SleepDeal.c` 的调用层完成。

验证结果：
- `git diff --check`：通过；仅有 CRLF 换行提示。
- `tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release`：Keil 日志显示 `0 Error(s), 3 Warning(s)`，已生成 `FD_Release.axf` 和 `FD_Release.bin`。
- `py -3.9 tools\project_check.py --quiet`：`100 OK / 0 Warnings / 39 Errors`；剩余失败为仓库既有基线问题。
- 未执行上板 RTC STOP、按键/充电/CAN/USART 唤醒实测。

## 参考源码

- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/conf/conf.h`
- `103 + 309/Project/Source/conf/Project_Config.h`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/rtc_sleep_port.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/System_Init.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/SystemDebug.c`
- `103 + 309/Project/Source/SystemDebug.h`
- `firmware/comm_tool_f103ret6/source/stm32f10x_it.c`
- `firmware/comm_tool_f103ret6/source/bsp/board.c`
- `firmware/comm_tool_f103ret6/source/bsp/board_uart.c`
- `firmware/comm_tool_f103ret6/source/bsp/board_can.c`
- `firmware/comm_tool_f103ret6/source/iap/ct_iap.c`

## 当前结论

主固件中断实现分散在标准外设库驱动文件和业务模块里。当前已经有部分计数字段，例如 `sys_time.rtc_sec_cnt`、`sys_time.rtc_alm_cnt`、`sys_time.can_rcv_cnt`、`sys_time.sci1_irq_cnt`、`sys_time.cnt_PA0_irq`、`sys_time.cnt_bms1_keyirq`，但实际递增不完整：RTC 秒中断、RTC alarm 和 CAN RX 已经递增，SCI/PA0/按键等字段多数只预留未使用。

建议新增一个独立 `IrqDebug` 模块，所有 ISR 只做轻量计数和少量快照，不在中断里打印、不访问 Flash、不走协议发送、不使用 `malloc`。调试观察优先通过 Keil Watch/ST-Link 读取全局 `volatile` 结构，暂不改 Modbus/CAN 客户可见协议。

## 已实现或实际相关中断清单

| 中断/异常 | 当前位置 | 当前行为 | 计数建议 |
|---|---|---|---|
| `NMI_Handler` | `stm32f10x_it.c` | 空实现 | 计入 `IRQDBG_NMI`，异常进入后保留现场 |
| `HardFault_Handler` | `stm32f10x_it.c` | 写 BKP fault reason，Debug 下停住，Release 下复位 | 先计入 `IRQDBG_HARDFAULT`，再保持原 fault 流程 |
| `MemManage_Handler` | `stm32f10x_it.c` | 同 fault 流程 | 计入 `IRQDBG_MEMMANAGE` |
| `BusFault_Handler` | `stm32f10x_it.c` | 同 fault 流程 | 计入 `IRQDBG_BUSFAULT` |
| `UsageFault_Handler` | `stm32f10x_it.c` | 同 fault 流程 | 计入 `IRQDBG_USAGEFAULT` |
| `SVC_Handler` | `stm32f10x_it.c` | 空实现 | 计入 `IRQDBG_SVC`，正常应为 0 |
| `DebugMon_Handler` | `stm32f10x_it.c` | 空实现 | 计入 `IRQDBG_DEBUGMON` |
| `PendSV_Handler` | `stm32f10x_it.c` | 空实现 | 计入 `IRQDBG_PENDSV`，无 RTOS 时正常应为 0 |
| `SysTick_Handler` | `stm32f10x_it.c` | 空实现；主固件当前主要用 TIM3 10ms tick | 计入 `IRQDBG_SYSTICK`，正常应为 0，用于发现误开 SysTick |
| `EXTI0_IRQHandler` | `stm32f10x_it.c` | 检查并清 `EXTI_Line0`；对应 `GPIO_CHG_IN/PA0` 下降沿唤醒 | 计入 `IRQDBG_EXTI0_CHG_IN`，同时记录阶段 |
| `EXTI2_IRQHandler` | `stm32f10x_it.c` | 只清 `EXTI_Line2`，未看到本轮配置入口 | 计入 `IRQDBG_EXTI2_STRAY` |
| `EXTI3_IRQHandler` | `stm32f10x_it.c` | 只清 `EXTI_Line3`，未看到本轮配置入口 | 计入 `IRQDBG_EXTI3_STRAY` |
| `EXTI9_5_IRQHandler` line5 | `stm32f10x_it.c` | 只清 pending，未看到本轮配置入口 | 计入 `IRQDBG_EXTI5_STRAY` |
| `EXTI9_5_IRQHandler` line6 | `stm32f10x_it.c` | 只清 pending，未看到本轮配置入口 | 计入 `IRQDBG_EXTI6_STRAY` |
| `EXTI9_5_IRQHandler` line7 | `stm32f10x_it.c`/`conf.c` | `UART1_WAKEUP_ENABLE` 时作为 `USART1 RX/PB7` 上升沿唤醒 | 计入 `IRQDBG_EXTI7_UART1_WAKE` |
| `EXTI9_5_IRQHandler` line9 | `stm32f10x_it.c`/`conf.c` | `GPIO_SW/PA9` 下降沿，当前写 `g_irq_t = soc_key` | 计入 `IRQDBG_EXTI9_SW_KEY`，保留原 `g_irq_t` 行为 |
| `EXTI15_10_IRQHandler` line12 | `stm32f10x_it.c`/`conf.c` | `GPIO_INT_WK_CMNT/PB12` 上升沿唤醒，只清 pending | 计入 `IRQDBG_EXTI12_CMNT_WAKE` |
| `EXTI15_10_IRQHandler` line13 | `stm32f10x_it.c`/`conf.c` | `GPIO_MCU_WK/PB13` 上升沿唤醒，只清 pending | 计入 `IRQDBG_EXTI13_MCU_WAKE` |
| `USART1_IRQHandler` | `stm32f10x_it.c` -> `Sci1_CommonUpper_IRQHandler()` | SCI1 RXNE/IDLE/TXE/TC/error 由 `Sci_PortIRQHandler()` 处理 | 入口计 `IRQDBG_USART1`，内部可选分源计 RX/IDLE/TX/error |
| `USART2_IRQHandler` | `stm32f10x_it.c` -> `Sci2_CommonUpper_IRQHandler()` | 受 `_COMMOM_UPPER_SCI2` 控制 | 入口计 `IRQDBG_USART2` |
| `USART3_IRQHandler` | `stm32f10x_it.c` -> `Sci3_CommonUpper_IRQHandler()` | 受 `_COMMOM_UPPER_SCI3` 控制 | 入口计 `IRQDBG_USART3` |
| `RTCAlarm_IRQHandler` | `RTC.c` | 处理 RTC alarm 和 `EXTI_Line17`，递增 `sys_time.rtc_alm_cnt` | 入口计 `IRQDBG_RTC_ALARM`，保留原计数 |
| `RTC_IRQHandler` 秒中断 | `RTC.c` | `RTC_IT_SEC` 时递增 `sys_time.rtc_sec_cnt` | 计入 `IRQDBG_RTC_SEC` |
| `RTC_IRQHandler` alarm 分支 | `RTC.c` | 如果 `RTC_IT_ALR` 置位，调用 alarm wakeup 处理 | 计入 `IRQDBG_RTC_ALARM_IN_RTC_IRQ` |
| `TIM3_IRQHandler` | `System_Init.c` | 10ms 系统节拍，`SysTime_Post10msTick()` | 计入 `IRQDBG_TIM3_10MS`，只做一次自增 |
| `TIM4_IRQHandler` | `LedBar.c` | 灯板 1ms 扫描，`LedBar_Scan1ms()` | 计入 `IRQDBG_TIM4_LEDBAR`，只做一次自增 |
| `USB_LP_CAN1_RX0_IRQHandler` | `Can_HDX.c` | CAN FIFO0 有消息时循环接收，递增 `sys_time.can_rcv_cnt` | 入口计 `IRQDBG_CAN1_RX0`，消息数仍用原计数 |

## 当前启用源与未启用源

| 来源 | 证据 | 判断 |
|---|---|---|
| RTC 秒中断 | `RTC_ITConfig(RTC_IT_SEC, ENABLE)` | 已启用，运行态会周期进入 |
| RTC alarm/EXTI17 | `RTC_AlarmConfig()` 配 EXTI17 和 `RTCAlarm_IRQn`，`RTC_EnableAlarmAfterSeconds()` 使能 alarm | 已启用，STOP 周期唤醒关键路径 |
| TIM3 update | `InitTimer()` 使能 `TIM3_IRQn` 和 `TIM_IT_Update` | 已启用，系统 10ms tick |
| TIM4 update | `LedBar_ScanTimerInit()` 使能 `TIM4_IRQn` 和 `TIM_IT_Update` | 已启用，灯板扫描 |
| USART1/2/3 | `Sci_InitCommonPort()` 根据端口使能 NVIC 和 RXNE/IDLE | 端口配置依赖宏和初始化入口 |
| CAN1 RX0 | `InitCan_CAN1()` 使能 `CAN_IT_FMP0`，`InitCan_NVIC()` 使能 `USB_LP_CAN1_RX0_IRQn` | 已启用 |
| DMA1_Channel1 | `ADC.c` 中 NVIC/DMA 中断配置在注释块里 | 当前未启用；若后续打开必须补 ISR 和计数 |
| EXTI2/3/5/6 | 当前 ISR 会清 pending，但本轮未看到配置入口 | 应作为 stray/异常进入观察项 |

## 建议实现方案

### 1. 新增独立模块

建议新增：

- `103 + 309/Project/Source/IrqDebug.h`
- `103 + 309/Project/Source/IrqDebug.c`

核心结构：

```c
enum IRQ_DEBUG_ID {
    IRQDBG_NMI = 0,
    IRQDBG_HARDFAULT,
    IRQDBG_MEMMANAGE,
    IRQDBG_BUSFAULT,
    IRQDBG_USAGEFAULT,
    IRQDBG_SVC,
    IRQDBG_DEBUGMON,
    IRQDBG_PENDSV,
    IRQDBG_SYSTICK,
    IRQDBG_EXTI0_CHG_IN,
    IRQDBG_EXTI2_STRAY,
    IRQDBG_EXTI3_STRAY,
    IRQDBG_EXTI5_STRAY,
    IRQDBG_EXTI6_STRAY,
    IRQDBG_EXTI7_UART1_WAKE,
    IRQDBG_EXTI9_SW_KEY,
    IRQDBG_EXTI12_CMNT_WAKE,
    IRQDBG_EXTI13_MCU_WAKE,
    IRQDBG_USART1,
    IRQDBG_USART2,
    IRQDBG_USART3,
    IRQDBG_RTC_SEC,
    IRQDBG_RTC_ALARM,
    IRQDBG_RTC_ALARM_IN_RTC_IRQ,
    IRQDBG_TIM3_10MS,
    IRQDBG_TIM4_LEDBAR,
    IRQDBG_CAN1_RX0,
    IRQDBG_UNHANDLED_VECTOR,
    IRQDBG_COUNT
};

enum IRQ_DEBUG_PHASE {
    IRQDBG_PHASE_BOOT = 0,
    IRQDBG_PHASE_RUN,
    IRQDBG_PHASE_SLEEP_PREPARE,
    IRQDBG_PHASE_STOP_WAIT,
    IRQDBG_PHASE_STOP_WAKE_RAW,
    IRQDBG_PHASE_STOP_RESTORE,
    IRQDBG_PHASE_RESET_SLEEP_WAIT,
    IRQDBG_PHASE_FAULT,
    IRQDBG_PHASE_COUNT
};

struct IRQ_DEBUG_STATE {
    volatile uint32_t total[IRQDBG_COUNT];
    volatile uint32_t phase[IRQDBG_PHASE_COUNT][IRQDBG_COUNT];
    volatile uint16_t last_id;
    volatile uint16_t last_vectactive;
    volatile uint8_t  current_phase;
    volatile uint8_t  last_phase;
    volatile uint32_t last_tick_10ms;
    volatile uint32_t last_exti_pr;
    volatile uint32_t last_nvic_ispr0;
    volatile uint32_t last_nvic_iabr0;
};
```

接口约束：

- `IrqDebug_Count(id)` 只能做数组自增、保存 `SCB->ICSR`、`EXTI->PR`、`NVIC->ISPR[0]`、`NVIC->IABR[0]` 等只读快照。
- 高速 ISR 只允许调用宏或 `static inline`，不要在 TIM3/TIM4/CAN RX 中做复杂逻辑。
- 所有字段用静态全局内存，不使用动态分配。
- 32 位计数溢出后自然回绕，调试时按差值观察。

### 2. ISR 插点原则

1. 在每个 ISR 入口第一行计数，避免后续分支提前返回漏计。
2. EXTI 组中断按 pending line 分源计数；如果组中断进来但没有已知 pending line，额外计 `IRQDBG_UNHANDLED_VECTOR` 或 `IRQDBG_EXTI_GROUP_SPURIOUS`。
3. RTC 需要区分 `RTCAlarm_IRQHandler` 和 `RTC_IRQHandler` 内的 `RTC_IT_ALR` 分支，因为两者都可能参与 alarm 清除。
4. USART 入口先按端口计数，`Sci_PortIRQHandler()` 内部如果需要细分，再记录 RXNE/IDLE/TXE/TC/error，不改变原协议处理。
5. Fault 类中断先计数再执行原 `Fault_ResetOrHold()`，不能改变 Debug 停住和 Release 复位行为。

### 3. 未实现向量兜底

启动文件中大量未使用外设向量当前由 `startup_stm32f10x_hd.s` 的 weak 默认处理器 `B .` 停住。为了发现“哪个未实现中断误进来”，建议修改默认处理块，在死循环前调用一个 C 入口：

```c
void IrqDebug_RecordUnhandledVector(void)
{
    g_stIrqDebug.total[IRQDBG_UNHANDLED_VECTOR]++;
    g_stIrqDebug.last_vectactive = (uint16_t)(SCB->ICSR & 0x1FFU);
}
```

这样不用给 WWDG、PVD、DMA、I2C、SPI、TIM5 等所有未使用向量分别写空 ISR，也能通过 `VECTACTIVE` 看出异常号。该修改涉及启动汇编，属于较大变更，必须在你确认后执行。

### 4. 生命周期阶段标记

建议在以下位置设置 `current_phase`：

| 阶段 | 建议插点 | 用途 |
|---|---|---|
| `BOOT` | `AppInit_InitDevice()` 开始到 `__enable_irq()` 前 | 判断初始化期间是否误开中断 |
| `RUN` | `__enable_irq()` 后、正常主循环 | 运行态基准计数 |
| `SLEEP_PREPARE` | `rtc_sleep_prepare_rtc()`、`RtcSleep_PortPrepareRtcStop()`、`SleepDeal_Continue()` 写 sleep flag 前 | 观察休眠准备期间是否还有 CAN/USART/TIM/EXTI 进入 |
| `STOP_WAIT` | `Sys_StopMode()` 调 `PWR_EnterSTOPMode()` 前 | 记录真正进入 STOP 等待窗口 |
| `STOP_WAKE_RAW` | `PWR_EnterSTOPMode()` 返回后、恢复外设前 | 判断唤醒瞬间是 RTC、EXTI 还是串口/CAN 干扰 |
| `STOP_RESTORE` | `InitRunAfterStopWakeup()` 开始到 `InitTimer()` 后 | 观察恢复外设期间是否提前进通信/定时器中断 |
| `RESET_SLEEP_WAIT` | `IsSleepStartUp()` 中 sleep flag 分支的 STOP 等待循环 | 观察 reset sleep 路径的唤醒中断 |
| `FAULT` | fault handler 入口 | 保留异常进入时的阶段 |

这样同一个中断会同时有总计数和分阶段计数，例如 `phase[STOP_WAIT][IRQDBG_EXTI9_SW_KEY]` 可以直接看到 RTC STOP 等待中是否被按键打断。

### 5. 观察方式

优先观察全局变量：

- `g_stIrqDebug.total[]`
- `g_stIrqDebug.phase[][]`
- `g_stIrqDebug.last_id`
- `g_stIrqDebug.last_vectactive`
- `g_stIrqDebug.last_exti_pr`
- `g_stIrqDebug.current_phase`

可选增强：

- 把 `g_stIrqDebug` 的摘要镜像到 `SystemDebug` 的 `g_dbg`，便于统一 Keil Watch。
- 增加一个 16 或 32 项事件环，只记录最近中断 ID、阶段、10ms tick、EXTI_PR、VECTACTIVE。事件环不建议记录 TIM3/TIM4 每次进入，否则很快刷屏。
- 暂不新增 Modbus/CAN 只读寄存器。若需要上位机直接读中断计数，必须单独确认协议地址和兼容性。

## RTC/低功耗重点观察项

| 场景 | 预期计数变化 | 异常信号 |
|---|---|---|
| 正常运行，不进入 STOP | `TIM3_10MS` 持续增加；RTC 秒中断按配置增加；TIM4 只在灯板扫描启用时增加 | `SYSTICK`、`EXTI2/3/5/6` 增加 |
| HICCUP 进入 STOP 前 | `SLEEP_PREPARE` 阶段不应有大量 USART/CAN/TIM4 | 休眠准备阶段通信/灯板中断持续增加，说明外设未关净 |
| STOP 等待 RTC alarm | `STOP_WAIT` 阶段主要应看到 `RTC_ALARM` 或合法唤醒 EXTI | `CAN1_RX0`、`USART1/2/3`、`TIM3/TIM4` 在 STOP_WAIT 增加 |
| STOP 返回到恢复外设前 | `STOP_WAKE_RAW` 记录最初唤醒来源 | `last_exti_pr` 与 `g_irq_t` 不一致，或 `RTC_ALARM` 与 EXTI17 未对应 |
| reset sleep 启动等待 | `RESET_SLEEP_WAIT` 只应出现合法唤醒源 | 非法 EXTI 或通信中断增加，说明 sleep flag 分支唤醒条件可能过宽 |

## 需要确认的需求表

| Requirement ID | Requirement description | Evidence from code | Current behavior | Risk | Codex judgment | Question for user | Suggested decision | User decision placeholder |
|---|---|---|---|---|---|---|---|---|
| IRQ-CNT-001 | 为主固件所有已实现/已启用 ISR 增加计数 | `stm32f10x_it.c`, `RTC.c`, `System_Init.c`, `LedBar.c`, `Sci_Upper.c`, `Can_HDX.c` | 只有 RTC/CAN 部分已有有效计数 | 低风险，但 TIM3/TIM4 属高频 ISR，插点必须极轻 | `KEEP_BUT_REFACTOR` | 是否确认先覆盖主固件，不改上位机/网关固件？ | 确认主固件优先 | 待确认 |
| IRQ-CNT-002 | 新增独立 `IrqDebug` 模块，不继续扩展 `sys_time` | `conf.h` 中已有计数字段但职责混杂 | `sys_time` 同时放运行参数、调试计数和低功耗状态 | 继续堆字段会让运行状态和调试状态混在一起 | `CHANGE_NEEDED` | 是否接受新增 `IrqDebug.c/h`？ | 接受，保持接口小而静态 | 待确认 |
| IRQ-CNT-003 | 记录分生命周期阶段计数 | `rtc_sleep.c`, `rtc_sleep_port.c`, `SleepDeal.c`, `conf.c` | 当前只能看到少量总计数，不能知道中断发生在哪个阶段 | 无法判断“休眠前后不该进的中断” | `CHANGE_NEEDED` | 是否需要分阶段数组 `phase[phase][irq]`？ | 需要 | 待确认 |
| IRQ-CNT-004 | 修改启动汇编默认处理器，记录未实现向量 | `startup_stm32f10x_hd.s` 默认 weak handler 为 `B .` | 未使用中断一旦进入只会死循环，无法知道来源 | 改 startup 属较大变更，需要谨慎验证 | `CHANGE_NEEDED` | 是否允许修改 startup 默认处理块？ | 允许，但只加记录后停住 | 待确认 |
| IRQ-CNT-005 | 调试信息只通过 Keil Watch/ST-Link 观察，不改协议 | `SystemDebug` 已有全局 `g_dbg`，协议文档强调不能随意移动 D000/D300/C002 | 当前协议窗口已有固定用途 | 改 Modbus/CAN 地址会破坏上位机兼容 | `MUST_KEEP` | 是否暂不新增上位机可读寄存器？ | 暂不改协议 | 待确认 |
| IRQ-CNT-006 | 高速中断只做计数，不记录事件环 | `TIM3_IRQHandler`, `TIM4_IRQHandler` | TIM3 10ms，TIM4 1ms | 事件环被高频中断刷掉，且增加 ISR 时延 | `MUST_KEEP` | 是否同意 TIM3/TIM4 只做总计数和阶段计数？ | 同意 | 待确认 |
| IRQ-CNT-007 | Release/量产配置是否保留中断计数 | `Project_Config.h` 当前 `PROJECT_CFG_DEBUG_MONITOR_ENABLE 1` | Release 也可能保留部分 debug monitor | 计数有少量 RAM/CPU 开销，但有利于现场排查 | `UNKNOWN` | 计数默认在 Release 打开，还是只在 Debug/显式宏打开？ | 建议默认打开计数，事件环可宏控 | 待确认 |
| IRQ-CNT-008 | `comm_tool_f103ret6` 固件是否也要实现同类计数 | `firmware/comm_tool_f103ret6` 有 SysTick、UART、CAN 中断 | 与主 BMS 固件是另一套工程 | 混在同一批改动会扩大范围 | `UNKNOWN` | 是否第二阶段再处理 comm tool 固件？ | 第二阶段处理 | 待确认 |

## 实施步骤建议

1. 新建 `IrqDebug.h/c`，定义 ID、阶段、全局状态和轻量计数宏。
2. 在 `Project_Config.h` 增加 `PROJECT_CFG_IRQ_DEBUG_ENABLE` 和事件环开关，默认按用户确认执行。
3. 在 `stm32f10x_it.c`、`RTC.c`、`System_Init.c`、`LedBar.c`、`Can_HDX.c`、`Sci_Upper.c` 增加 ISR 入口计数。
4. 在 `rtc_sleep.c`、`rtc_sleep_port.c`、`SleepDeal.c`、`conf.c` 增加阶段设置和 STOP 前后快照。
5. 如确认允许，修改 `startup_stm32f10x_hd.s` 默认处理器，记录未实现向量后保持停住。
6. 在 `SystemDebug` 增加摘要字段或快照函数，保持 Keil Watch 可读；不改现有协议寄存器。
7. 更新 `docs/review/test_plan.md`、`docs/test_plan.md` 和 `docs/change_log.md`，记录验证方式。
8. 编译 `FD_Release`，静态检查 map/htm 中 handler 覆盖情况。

## 验证计划

| 验证项 | 方法 | 通过标准 |
|---|---|---|
| 编译 | Keil `FD_Release` | 编译通过，无新增未解析符号 |
| 静态覆盖 | `rg "IRQDBG_"` 对照 ISR 清单 | 每个已实现 ISR 都有计数插点 |
| 高速 ISR 开销 | 观察 TIM3/TIM4 计数和主循环周期 | 主循环、灯板扫描无明显异常 |
| RTC STOP | 进入 HICCUP/RTC STOP，观察 `phase[STOP_WAIT]` | 只出现 RTC alarm 或合法 EXTI 唤醒 |
| 按键唤醒 | STOP 中按 PA9 | `EXTI9_SW_KEY` 在 STOP 阶段增加，`g_irq_t` 保持原行为 |
| 充电唤醒 | STOP 中触发 PA0 | `EXTI0_CHG_IN` 增加，sleep wake 判断不变 |
| 串口干扰 | STOP 前后发 USART1 数据 | 若 `USART1` 在 STOP_WAIT 增加，可定位为异常唤醒/干扰 |
| CAN 干扰 | CAN 上有设备和无设备两种状态 | `CAN1_RX0` 只在 CAN 电源/外设恢复后按预期增加 |
| 未实现向量 | 人工触发或审查 startup 默认路径 | `last_vectactive` 能显示异常向量号，随后保持停住 |

## 已知风险与注意事项

- 不要在 ISR 中打印日志、写 Flash、发 CAN/Modbus、调用耗时函数。
- TIM4 是灯板扫描中断，任何计数实现都必须是单次内存自增级别。
- `SystemDebug` 当前已经读取 `NVIC->ISER/ISPR/IABR` 和 `EXTI->PR`，后续应复用，不要再做一套复杂快照。
- 不要改变现有 Modbus/CAN 寄存器、帧格式、CAN ID 或数据含义；上位机读取计数属于另一个需求，必须单独确认。
- App scatter 地址仍是 `0x08004800`。本次中断计数不应修改烧录脚本、scatter 地址和 IAP 地址。
- `system_stm32f10x.c` 中 `SCB->VTOR` 只有 `_IAP` 定义时才使用 `0x4800` 偏移；当前 Keil Target 宏未看到 `_IAP`。这属于已有向量基址风险，和中断计数不是同一项修改，建议单独确认。

## 第二工程中断清单

`firmware/comm_tool_f103ret6` 不是本次主固件方案主体，但静态看到以下中断：

| 中断 | 位置 | 当前行为 |
|---|---|---|
| `SysTick_Handler` | `bsp/board.c` 或 `iap/ct_iap.c` | 1ms tick |
| `USART1_IRQHandler`/`USART3_IRQHandler` | `bsp/board_uart.c` 或 `iap/ct_iap.c` 通过宏映射 | 串口升级/板级 UART 收发 |
| `USB_LP_CAN1_RX0_IRQHandler` | `bsp/board_can.c` 或 `iap/ct_iap.c` | CAN FIFO0 接收 |
| fault handlers | `firmware/comm_tool_f103ret6/source/stm32f10x_it.c` | fault 后停住 |

建议待主固件方案确认并验证后，再为该工程单独做轻量计数，避免一次性扩大修改范围。
