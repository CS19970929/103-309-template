# 系统时基调度说明

## 背景

旧实现里，`App_SysTime()` 通过比较 `g_u81msClockCnt`、`g_u810msClockCnt` 的当前值来置位 `g_st_SysTimeFlag.bits`。如果主循环刚好被 EEPROM、AFE、休眠、日志或其他长任务拖慢，定时器计数可能已经跨过一个或多个相位，主循环再次调用 `App_SysTime()` 时只能看到最新值，之前的 `b1Sys1msFlag` 或 `b1Sys10msFlag1~5` 就会被跳过。

这会导致依赖 `b1Sys10msFlag1` 的 `App_WarnCtrl()`、`APP_LedBar()`，以及依赖其他相位标志的 CAN、启动延时、MOS/继电器控制等模块出现调度抖动或漏调度。

## 当前方案

TIM3 中断不再让主循环直接采样瞬时计数，而是把节拍事件放入小型 pending 队列：

- `s_u8Sys1msPending`：累计 1ms 事件。
- `s_u8Sys10msPhasePending`：累计 `b1Sys10msFlag1~5` 的分相事件。
- `App_SysTime()` 每次调用最多消费一个 1ms 事件和一个 10ms 分相事件，并按原有 `g_st_SysTimeFlag.bits` 接口派发。

这样旧模块接口保持不变，但主循环不会因为错过瞬时计数变化而直接丢标志。主循环如果短时间变慢，事件会排队，后续循环按顺序补发。

## 中断影响

TIM3 ISR 只做 `UINT8` pending 计数的饱和递增和原有 200ms/1000ms 标志维护，没有把 `App_WarnCtrl()`、`APP_LedBar()` 等业务逻辑放回中断里。

主循环消费 pending 时会短暂保存/关闭/恢复 PRIMASK，只覆盖一次 `UINT8` 递减，临界区很短，不会明显影响 USART 收发中断。

## 过载判断

新增两个调试计数：

- `gu16_SysTime1msOverrunCnt`
- `gu16_SysTime10msPhaseOverrunCnt`

这两个计数只要递增，就说明主循环长期没有及时消费时基事件，pending 队列已经饱和。此时软件无法继续保证实时性，应优先检查长耗时任务、阻塞延时、EEPROM/Flash 写入、AFE 访问和日志路径。

## App_WarnCtrl 简化

`App_WarnCtrl()` 保留原有二级、三级保护检查顺序和调用周期，不把保护拆成 50ms 轮询，避免改变保护响应时间。

实现上改为一个保护检查清单宏展开直接调用：

- 维护时只调整 `APP_WARN_CHECK_LIST()`。
- 编译后仍是直接函数调用，不引入函数指针间接调用开销。

## 后续建议

如果现场仍认为“10ms 绝对时间不准”，需要单独校准 TIM3 的硬件基准：确认 APB1 定时器时钟、`TIM_Prescaler`、`TIM_Period` 和实际晶振/PLL 配置，并用示波器或逻辑分析仪量测一个 GPIO 翻转周期。

本次改动解决的是主循环调度漏标志和业务任务不该进 ISR 的问题；硬件定时基准误差需要通过时钟配置校准解决。
