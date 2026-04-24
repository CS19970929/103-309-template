# 休眠与低功耗逻辑审计及优化建议

日期：2026-04-23

## 1. 结论摘要

基于当前仓库源码，项目实际生效的低功耗框架可以概括为三层：

1. 主循环空闲 `WFI` 轻睡眠。
2. `HICCUP_MODE` 的 RTC 周期性 `STOP` 回睡。
3. `NORMAL_MODE / DEEP_MODE` 的 reset-based 睡眠链路。

其中第 2 层与第 3 层并不是同一套架构：

- `HICCUP_MODE` 在运行态直接进入 `STOP`，唤醒后继续在当前上下文内运行。
- `NORMAL_MODE / DEEP_MODE` 先写启动标志、让 AFE 进入 sleep、再触发 `NVIC_SystemReset()`，随后在下次启动早期进入 `STOP`。

这意味着当前工程不是“一个统一低功耗状态机”，而是“RTC 运行态睡眠 + reset-based 睡眠”并存。

## 2. 本文边界

### 2.1 已确认事实

- 主循环实际调用的是 `App_LowPowerProcess()`，不是 `App_SleepDeal()`。
- 实际底层休眠调用是 `PWR_EnterSTOPMode(...)`，不是 `STANDBY`。
- 睡眠启动标志当前走 BKP 备份寄存器，不走 Flash 睡眠页。
- RTC 单次唤醒周期会被 IWDG 安全窗口压缩。

### 2.2 明确假设

- 本文只基于当前仓库源码判断，不包含原理图和板级实测电流。
- “DEEP” 与 “NORMAL” 的命名语义按软件现状理解，不强行等同于芯片手册的电源模式。

### 2.3 推断项

- 如果调试器连接且 `DBGMCU_STOP` 打开，实验室测得的 STOP 电流可能显著高于脱机量产状态。
- `Init_RTC()` 的重复全量初始化会增加 RTC/HICCUP 周期的进入退出成本，并存在备份域状态被反复重置的风险。

## 3. 当前有效低功耗架构

### 3.1 主循环入口

主循环每轮执行：

- `App_SysTime()`
- AFE / 通信 / ADC / EEPROM / CAN / SOC 等业务
- `App_LowPowerProcess()`
- `MainLoop_EnterIdleSleep()`

对应文件：

- `103 + 309/Project/Source/main.c`

关键信息：

- `App_LowPowerProcess()` 才是当前深层低功耗入口。
- `MainLoop_EnterIdleSleep()` 是 CPU 空闲态轻睡眠补层。
- `App_SleepDeal()` 在主循环里已被注释，不是当前主链。

### 3.2 三种实际模式

| 模式 | 入口 | 底层行为 | 唤醒后语义 |
| --- | --- | --- | --- |
| Idle Sleep | `MainLoop_EnterIdleSleep()` | 清 `SLEEPDEEP` 后 `__WFI()` | 中断返回后继续主循环 |
| HICCUP_MODE | `rtc_sleep()` -> `rtc_sleep_run_hiccup_cycle()` | RTC + `STOP` | RTC 正常唤醒则继续回睡；异常则退出 HICCUP |
| NORMAL_MODE | `rtc_sleep()` -> `SleepDeal_Continue()` | 写 BKP 标志 + AFE sleep + `MCU_RESET()`；启动早期再 `STOP` | 仅“有效外部唤醒”后退出 |
| DEEP_MODE | `rtc_sleep()` -> `SleepDeal_Continue()` | 与 NORMAL 同链路，但唤醒配置更少 | 仅“有效外部唤醒”后退出 |

### 3.3 关键参数

休眠相关参数位于 `OtherElement`：

- `u16Sleep_VNormal`
- `u16Sleep_TimeNormal`
- `u16Sleep_Vlow`
- `u16Sleep_TimeVlow`
- `u16Sleep_VirCur_Chg`
- `u16Sleep_VirCur_Dsg`
- `u16Sleep_TimeRTC`

默认值按当前编译宏为：

- `u16Sleep_VNormal = 3200 mV`
- `u16Sleep_TimeNormal = 7200 min`
- `u16Sleep_Vlow = 3000 mV`
- `u16Sleep_TimeVlow = 10 min`
- `u16Sleep_VirCur_Chg = 10`，即 `1.0 A`
- `u16Sleep_VirCur_Dsg = 10`，即 `1.0 A`
- `u16Sleep_TimeRTC = 3 min`

注意：

- `u16Sleep_RTC_WakeUpTime` 这个字段仍在结构体中，但当前代码未实际使用。
- RTC 周期实际执行值不是简单等于 `u16Sleep_TimeRTC`，还会被 IWDG 限制。

## 4. 详细调用链

### 4.1 空闲轻睡眠

主循环尾部的 `MainLoop_EnterIdleSleep()` 只有在以下条件全部满足时才执行：

- `b1OnOFF_Sleep` 允许。
- 没有系统时基标志。
- 没有 `200ms/1000ms` 累计任务标志。
- 串口端口不 busy。
- 当前未全局关中断。

然后它显式清掉 `SLEEPDEEP`，执行：

- `__DSB()`
- `__WFI()`
- `__ISB()`

这个路径不进入 `STOP`，只是典型的 idle sleep。

### 4.2 HICCUP_MODE 路径

`rtc_sleep()` 每 1 秒由 `gu8_1000msAccClock_Flag` 驱动一次。

其决策入口是 `BQ769x0_SleepMode_Ctrl()`：

1. 如果 `VcellMin <= 2600 mV` 且无充电，累计 60 秒请求 `DEEP_MODE`。
2. 否则如果 `VcellMin <= u16Sleep_Vlow` 且无充电，累计 `u16Sleep_TimeVlow` 分钟请求 `DEEP_MODE`。
3. 否则在“无热、无均衡、无明显充放电、无最近通信活动、AFE 允许进入 RTC”时，累计 `sys_time.time_enter_rtc` 秒请求 `HICCUP_MODE`。

进入 `HICCUP_MODE` 后实际流程是：

1. `rtc_sleep_prepare_rtc()`
2. `Init_RTC()`
3. `IOstatus_RTCMode()`
4. `InitWakeUp_RTCMode()`
5. `Sys_StopMode()`
6. STOP 唤醒后 `Init()`
7. 若 `is_rtc_wakekup && !isException()`，则做 `update_rtc_soc()` 后继续回睡
8. 否则退出 HICCUP，推断唤醒源、打日志、回到正常业务

这一条链路的特点：

- 它不是 reset-based。
- 它会在一次 HICCUP 会话里重复多轮 STOP。
- `sys_time.rtc_sleep_cnt` 记录的是 RTC 成功唤醒次数。

### 4.3 NORMAL_MODE / DEEP_MODE 路径

这两种模式不在运行态直接 STOP，而是先提交“下次启动进入睡眠”的意图。

`SleepDeal_Continue()` 执行步骤：

1. 根据 `Sleep_Mode.bits` 选择模式。
2. 向 BKP 写入模式标志和反码。
3. `InitAFE1_Sleep(0)`
4. `AFE_Sleep()`
5. `MCU_RESET()`

下次启动很早期，`InitDevice()` 调用 `IsSleepStartUp()`：

1. 读取 BKP 标志。
2. 识别 `NORMAL / HICCUP / DEEP`。
3. 清掉 BKP 标志。
4. 配置对应低功耗 IO 和唤醒源。
5. 在 `do { Sys_StopMode(); } while (!IsSleepWakeupValid());` 中反复 STOP。
6. 直到 `IsSleepWakeupValid()` 判断为有效外部唤醒，再 `IORecover_*()` 复位回正常启动。

这条路径的特点：

- 一次“进入睡眠”的提交，会被拆成“软件复位 + 启动早期 STOP 循环”。
- 外部业务代码看到的是“进入 NORMAL/DEEP 会立刻 reset 掉”。
- 睡眠期间并不是一直运行 RTC 巡检逻辑，而是在启动早期阻塞式 STOP 循环里等待有效唤醒。

## 5. 唤醒源与退出条件

### 5.1 HICCUP_MODE 的唤醒源

RTC/HICCUP 下启用的唤醒源包括：

- `PA0` 充电唤醒
- `PA9` 按键唤醒
- `PB12` RS485 唤醒
- `PB7` UART1 唤醒
- `RTC Alarm`

其中：

- RTC 唤醒由 `RTC_IRQHandler()` 把 `is_rtc_wakekup` 置真。
- 其他外部线大多只是在 EXTI handler 里清 pending bit，并没有第一时间锁存 `g_irq_t`。
- HICCUP 退出后的唤醒原因很多时候是由 `low_power_guess_wakeup_source()` 通过 GPIO 电平做 best-effort 推断。

### 5.2 NORMAL/DEEP 的退出条件

`IsSleepWakeupValid()` 当前只认两类“有效唤醒”：

1. `PA0` 充电输入为有效。
2. `PA9` 对应按键持续按下约 3 秒。

这意味着：

- `NORMAL/DEEP` 不是“任何中断一来就退出”。
- 对于 reset-based 睡眠，RTC 不是有效退出条件。
- 短按按键也不是有效退出条件。

### 5.3 NORMAL 与 DEEP 的真实区别

当前代码中两者底层都是 `STOP`。

实际差异主要只有两点：

1. 唤醒源配置不同。
2. 进入前的模式标志不同。

从芯片功耗模式角度看，它们并不是两个真正不同的电源态。

## 6. 当前源码里的历史分叉

### 6.1 旧状态机仍存在，但不在主链

`SleepDeal.c` 中仍保留了老的：

- `SleepDeal_Normal_Select()`
- `SleepDeal_Normal_L2()`
- `SleepDeal_Normal_L3()`
- `App_SleepDeal()`

但主循环当前不再调用 `App_SleepDeal()`。

这套代码的现状是：

- 仍能编译。
- 仍有数据结构和时间计数。
- 仍可被外部调用。
- 但不是当前主路径。

它会持续提高维护难度，因为维护者很容易误以为它还在生效。

### 6.2 文档与源码已有偏差

仓库里已有多份低功耗说明，但至少有一处关键描述与现源码不一致：

- 文档描述 `Init_RTC()` 已经做成幂等初始化。
- 现源码 `RTC_ClockConfig()` 仍然无条件调用 `BKP_DeInit()`。

由于 `BKP_DeInit()` 底层是 `RCC_BackupResetCmd(ENABLE/DISABLE)`，这会重置整个备份域。

这意味着：

- 现代码并不满足“真正幂等初始化”的定义。
- RTC/HICCUP 每次 `Init_RTC()` 都会额外带来备份域重置风险。
- BKP 中的 RTC 初始化哨兵和睡眠标志都可能受影响。

### 6.3 命名语义仍然带有历史包袱

当前仍存在以下“名字看起来像 A，实际已经是 B”的情况：

- `FLASH_NORMAL_SLEEP_VALUE` 等宏名仍保留 `FLASH_` 前缀，但实际写入 BKP。
- `DEEP_MODE` 名字像 deep power mode，但底层仍是 `STOP`。
- `b1OnOFF_Sleep` 看起来像总休眠使能，但当前只控制 idle sleep。

这些问题不一定会立刻造成功能错误，但会持续提高理解成本。

## 7. 优化评估

### 7.1 可以明确做的优化

#### 优化 1：把 RTC 初始化拆成“首次初始化”和“每次回睡前 re-arm”

事实：

- `rtc_sleep_prepare_rtc()` 每轮 HICCUP 都会调用 `Init_RTC()`。
- `Init_RTC()` 又会走到 `RTC_ClockConfig()`。
- `RTC_ClockConfig()` 当前无条件 `BKP_DeInit()`。

问题：

- 这不是幂等初始化。
- 重复重置备份域会增加 RTC 进入成本。
- 也会让 BKP 作为持久状态存储的可信度下降。

建议：

1. 把 `RTC_ClockConfig()` 改成只在首次初始化时重置备份域。
2. 后续 RTC 回睡只做：
   - 清闹钟标志
   - 清 `EXTI17`
   - 重设 alarm
   - 重新使能 `RTC_IT_ALR`

优先级：高

#### 优化 2：把 low power 决策与 low power 执行拆开

现状：

- `BQ769x0_SleepMode_Ctrl()` 同时做条件判断、计时、请求模式。
- `rtc_sleep()` 同时做状态推进和执行。
- `SleepDeal_Continue()` 又在另一文件做模式翻译和提交。

问题：

- 决策层和执行层耦合过重。
- 未来只要加一个新触发源，就得同时改多个文件。

建议：

拆出三个层次：

1. `low_power_policy`：只判断该不该睡、该睡哪种模式。
2. `low_power_executor`：只负责执行 `HICCUP` 或 reset-based sleep。
3. `low_power_boot`：只负责启动恢复。

优先级：高

#### 优化 3：统一唤醒原因记录机制

现状：

- `g_irq_t` 很多时候不是 ISR 第一现场记录。
- HICCUP 退出时大量依赖 GPIO 电平推断。

问题：

- 电平推断不是严格时序真相。
- 多源同时唤醒时，容易丢失首因。

建议：

1. 在 EXTI / USART / CAN 唤醒中断入口统一调用一个轻量锁存函数。
2. 只记录第一个有效来源，不在后续覆盖。
3. 退出睡眠后统一消费并清空。

优先级：中高

#### 优化 4：把 reset-based NORMAL/DEEP 的“唤醒 profile”显式化

现状：

- `NORMAL_MODE` 和 `DEEP_MODE` 的主要差异其实是唤醒源配置。

建议：

不要继续用“normal/deep”表达芯片电源层含义，而是改成更贴近业务的 profile，例如：

- `LOW_POWER_PROFILE_RTC`
- `LOW_POWER_PROFILE_EXT_WAKE`
- `LOW_POWER_PROFILE_MIN_WAKE`

优先级：中

#### 优化 5：收敛外部 `entersleep()` 调用源

现状：

- `DataDeal.c`
- `ChargerLoadFunc.c`
- `IO_Control.c`
- `Sci_Upper.c`
- `rtc_sleep.c`

都可能直接调 `entersleep()`。

问题：

- 模式请求来源过散。
- 很难回答“当前为什么要睡”。

建议：

先不立即删掉这些调用点，但统一改为：

- 只允许它们提交 `LowPower_RequestReason`
- 最终是否执行，由单一 low power manager 汇总

优先级：中

### 7.2 不建议马上做的优化

#### 不建议 1：直接切到真正 STANDBY

原因：

- 当前恢复链路大量依赖现有 STOP 返回、RTC/BKP、软件复位流程。
- 真正 `STANDBY` 会改变上下文恢复模型。
- 在没有先收敛架构之前直接切换，风险高于收益。

#### 不建议 2：一次性删除 `SleepDeal.c` 历史代码

原因：

- 尽管老路径当前不在主链，但其中还有 BKP 启动恢复链和 reset-based 执行链被现代码复用。
- 应先拆模块，再删历史分叉，不要直接硬砍。

#### 不建议 3：先做“更激进的关外设/关时钟”

原因：

- 当前主要问题还不是“外设没有关到极致”，而是“状态机和初始化边界不清”。
- 先把架构收敛，再做板级极限功耗优化更稳。

## 8. 推荐优化顺序

### 第一阶段：低风险、直接收益

1. 修正 `Init_RTC()` 的幂等性问题。
2. 把 RTC alarm re-arm 从 RTC 首次初始化里拆出来。
3. 统一唤醒原因锁存接口。
4. 补一张正式状态图，明确主链与遗留链。

### 第二阶段：结构收敛

1. 新建 `low_power/` 目录。
2. 拆分：
   - `low_power_manager.c`
   - `low_power_rtc.c`
   - `low_power_boot.c`
   - `low_power_wakeup.c`
3. 把 `rtc_sleep.c` 和 `SleepDeal.c` 中的职责重新分布。

### 第三阶段：接口化

1. 把通信忙状态收敛成 `LowPower_IsCommBusy()`。
2. 把热管理、均衡、充放电活动收敛成统一查询接口。
3. 限制外部模块直接写 `Sleep_Mode.bits`。

### 第四阶段：行为清理

1. 删除主流程不再使用的 `sleep()` 兼容入口。
2. 下线 `App_SleepDeal()` 的主流程语义。
3. 把 `NORMAL/DEEP` 更名为更贴近真实行为的 profile。

## 9. 推荐验证项

### 9.1 功能验证

- `HICCUP_MODE` 是否能连续 RTC 回睡并正确退出。
- `NORMAL_MODE` 是否仍能按 charger / 长按键有效退出。
- `DEEP_MODE` 是否仍能按最小唤醒集退出。

### 9.2 状态验证

- 每次 `Init_RTC()` 前后，检查 `BKP_DR1/DR2/DR3` 是否被意外清空。
- 检查 `is_rtc_wakekup`、`RTC_FLAG_ALR`、`EXTI_Line17` 的配合是否一致。
- 检查 `RTC_ExtComCnt` 在串口/CAN 活动时是否持续刷新。

### 9.3 功耗验证

- 脱机状态测：
  - 正常运行平均电流
  - idle sleep 电流
  - HICCUP 单周期平均电流
  - NORMAL/DEEP 挂起期间平均电流
- 联机调试状态单独测一组，排除 `DBGMCU_STOP` 对结果的干扰。

## 10. 最终判断

当前低功耗逻辑不是“不可用”，而是“能跑，但架构上仍有历史叠层”。

最值得优先动的不是模式名字，也不是继续堆更多 sleep 条件，而是先解决两件根问题：

1. RTC/备份域初始化边界不清。
2. 低功耗决策、执行、恢复分散在多处。

如果先把这两点收敛，再做更深的功耗优化和模式裁剪，风险最低，后续维护成本也最低。
