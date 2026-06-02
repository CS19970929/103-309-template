# 低功耗官方与行业调研及当前优化映射

文档状态：部分验证
源码验证：已按当前源码核对，未上板实测
日期：2026-06-02
范围：STM32F1/F0 类 MCU 低功耗原则、BMS 行业低功耗分层、当前项目可执行简化项。

## 参考资料

- ST `RM0008`：STM32F10xxx 提供 Sleep、Stop、Standby。Sleep 只停 CPU clock；Stop 停止 1.8V domain clocks、HSI/HSE 关闭，SRAM 和寄存器保持，可由 EXTI 唤醒；Standby 关闭内部调压器，唤醒后近似 reset 流程。链接：https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
- ST `AN2629`：面向 STM32F101/F102/F103 的 low-power modes 应用说明，核心是结合时钟、寄存器、外设和唤醒源做低功耗管理。链接：https://www.st.com/resource/en/application_note/cd00171691-stm32f101xx-stm32f102xx-and-stm32f103xx-low-power-modes-stmicroelectronics.pdf
- ST `AN2821`：STM32F10xxx 低功耗不影响内部 RTC；RTC alarm 可从低功耗自动唤醒；Stop 模式保留 SRAM/register，RTC/IWDG 继续运行，退出 Stop 后时钟回到 HSI，需要恢复时钟配置。链接：https://www.st.com/resource/en/application_note/an2821-clockcalendar-implementation-on-the-stm32f10xxx-microcontroller-rtc-stmicroelectronics.pdf
- ST low-power 入门资料：选择低功耗模式时，需要在功耗、唤醒时间、可用外设、可用唤醒源之间取舍；RTC/低速时钟是周期唤醒的核心。链接：https://wiki.st.com/stm32mcu/wiki/Introduction_to_Low_power_with_STM32
- TI `BQ769x2 Frequently Asked Questions`：BMS AFE 常见分层是 NORMAL、SLEEP、DEEPSLEEP、SHUTDOWN；SLEEP 保留多数保护和低频采样，DEEPSLEEP 只保留较少电路并可让 MCU 继续供电，SHUTDOWN 仅保留唤醒检测。链接：https://www.ti.com/lit/pdf/sluaaq5

## 调研结论

### STM32 官方低功耗逻辑

1. 不需要 CPU 时优先进入 Stop，而不是让主循环空跑。
2. Stop 前只保留真正需要的唤醒源，关闭或降功耗外设；ADC、通信收发器、显示扫描、调试保持都可能显著影响实测电流。
3. RTC alarm/wakeup 是周期唤醒的标准做法；周期唤醒后只做必要检查，完成后继续 Stop。
4. Stop 唤醒后必须恢复系统时钟和必要外设；这个恢复动作应集中在一个真实入口，避免多个 wrapper 分散维护。
5. Standby/Reset-like sleep 适合深睡、运输、长期存储或明确关机；进入前必须保存必要上下文，启动早期再消费 wake intent。
6. DBGMCU 低功耗调试保持只适合定位问题。量产功耗测试必须关闭 DBG_SLEEP/STOP/STANDBY/IWDG_STOP/WWDG_STOP，否则功耗结论失真。

### BMS 行业低功耗逻辑

1. BMS 不应为了“软件在线”长期 RUN；应让 MCU 在无电流、无通信、无按键、无显示、无 Flash 写入、无故障处理时进入低功耗。
2. AFE 和 MCU 通常分层：AFE 负责保护/低频测量/唤醒检测，MCU 负责策略、通信、显示和记录。
3. 充电器、负载、按键、通信、电流变化是常见唤醒源；每个唤醒源都应有明确的硬件电平和软件判定。
4. 通信收发器应按需上电。休眠中周期广播通常不是低功耗优先策略，除非客户明确要求在线可见。
5. 深睡/关机路径必须让 MOS、AFE、LED、CAN/RS485 电源和持久化状态都有确定动作，不能依赖分散的隐式状态。

## 映射到当前项目

当前项目已经接近官方/行业推荐的主体方向：

- 运行态空闲后进入 `HICCUP_MODE`，使用 RTC 周期唤醒后继续 Stop。
- `NORMAL_MODE/DEEP_MODE` 走 reset sleep，使用 BKP 保存 sleep intent，启动早期 `IsSleepStartUp()` 判断合法唤醒。
- CAN/CMNT 睡前关闭，RTC 周期唤醒中不主动广播 CAN。
- LED 真实显示窗口阻塞 Stop，窗口结束释放低功耗。
- 睡前保存 SOC snapshot 和工厂老化进度。

当前主要问题不是方向错误，而是实现层数和状态缓存偏多：

- `app_lowpower.c` 曾同时保留主路径一行 wrapper `LP_Task()->rtc_sleep()`、一组未调用的 STOP wrapper 和独立 bitmask 模块，增加阅读成本；本轮已删除该模块，`LP_GetBlockReason()` 收口到 `rtc_sleep.c/h`。
- `app_lowpower.c` 曾维护 `LP_State_t`，但当前业务和 debug 都看 `g_stLowPowerRtcStatus`，这个状态缓存没有真实消费者。
- `conf.h` 曾无条件定义 `__EnableLowPowerDebug__`，导致 Release 也打开 DBGMCU 低功耗调试保持，和已确认的功耗测试要求冲突。
- AFE not idle、`OtherElement` 普通休眠/RTC 参数仍有“接口存在但主路径未使用或参数语义不清”的风险，需要需求确认后再决定接入或删除。FactoryAging active 已按确认只阻塞 HICCUP RTC STOP，不影响 deep/reset sleep。

## 本轮可执行优化

### 已执行

| 项目 | 类型 | 处理 | 行为影响 |
|---|---|---|---|
| Release DBGMCU 低功耗调试保持 | 已确认回归修复 | 删除 `conf.h` 中无条件 `__EnableLowPowerDebug__` | Release 默认清除 DBGMCU low-power debug bits；Debug 仍可通过显式宏打开 |
| 未使用 STOP wrapper | 净删减 | 删除 `LP_SetWakeupPeriod()`、`LP_BeforeSleep()`、`LP_AfterWakeup()`、`LP_EnterStop()` | 主路径不变；减少重复 STOP 入口 |
| 无消费者 runtime 状态 | 净删减 | 删除 `LP_State_t`、`LP_GetState()`、`LP_Runtime_t/s_lp_runtime`，只保留 `s_u32LastSleepSeconds` | 主路径不变；debug 继续读取 `g_stLowPowerRtcStatus` 和现算 block mask |
| 未触发 block 宏 | 净删减 | 删除 `LP_BLOCK_AFE_BUSY`、`LP_BLOCK_IWDG_UNSAFE` | 不改变主判断；避免看似支持但永远不触发 |
| RTC SOC 临时缓存 | 净删减 | 删除 `s_u8RtcSoc/get_rtc_soc/set_rtc_soc`，`RtcSleep_PortApplySocRtcRest()` 改为 `void` | 保留 SOC 休眠补偿副作用，不保存无消费者返回值 |
| 无效 port 参数 | 净删减 | 删除 `RtcSleep_PortPrepareRtcStop()` 的无用参数 | 主路径不变，减少误导 |
| 重复阻塞判断 | 净删减 | 删除无调用 `LP_CanSleep()`，`rtc_sleep` 直接使用 `LP_GetBlockReason()` 映射粗粒度原因 | 电流/按键判断不再写两遍，`g_dbg.lp.block_mask` 仍保留详细 bitmask |
| 独立低功耗 wrapper 模块 | 净删减 | 删除 `app_lowpower.c/h`，从 Keil 工程移除，`LP_BLOCK_*` 和 `LP_GetBlockReason()` 迁入 `rtc_sleep.c/h` | 主路径和 debug 口径不变；少一个模块和 include 依赖 |

### 暂不执行

| 项目 | 原因 | 下一步 |
|---|---|---|
| HICCUP 前让 AFE sleep | 会改变功耗和周期测量/唤醒恢复行为 | 结合 SH367309 手册和实测电流确认 |
| FactoryAging active 阻塞 RTC STOP | 已确认并接入 | 只阻塞 HICCUP idle 进入 RTC STOP，不阻塞低压或外部请求的 `DEEP_MODE/NORMAL_MODE` reset sleep |
| `OtherElement` 普通休眠/RTC 参数接入或删除 | 会影响上位机参数语义 | 确认哪些参数仍是客户可见需求 |
| 运行态长按/睡眠中按键/拔 5V 行为 | 属于产品交互 | 确认产品定义后再改 |

## 推荐后续结构

低功耗代码应收敛为三条清晰路径：

1. `Runtime_RunOnce()->rtc_sleep()`：唯一运行态低功耗入口。
2. `rtc_sleep()`：唯一 HICCUP/Reset sleep 策略入口，只判断是否要 sleep、记录 block reason、执行真实 sleep。
3. `RtcSleep_Port*` / `SleepDeal_Continue()`：硬件准备、Stop 入口、唤醒恢复、reset sleep commit，避免新增 wrapper。

不建议再新增“完整状态机框架”。如果需要可观测性，优先让 `g_stLowPowerRtcStatus` 和 `LP_GetBlockReason()` 反映真实阻塞原因，而不是额外缓存状态。
