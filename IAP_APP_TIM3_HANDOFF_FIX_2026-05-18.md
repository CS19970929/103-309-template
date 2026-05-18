# IAP 跳转后 TIM3 时基不运行修复说明

## 问题现象

使用 `E:\work\a002\new 030\IAP 103CB` 的 `dev` 分支 IAP 跳转当前 App 后，Keil 调试时主循环可以运行，但 `App_AFEGet()` 一直停在：

```c
if (0U == SysTime_Take200msTaskPeriod())
    return;
```

`SysTime_Take200msTaskPeriod()` 依赖 `TIM3_IRQHandler()` 每 10ms 调用 `SysTime_Post10msTick()`，累计 20 次后投递 200ms 周期 token。如果 TIM3 中断没有进入，`s_u8Sys200msPendingPeriods` 会一直为 0。

## 根因

IAP 的 `IAP_To_APP_Jump()` 在切换到 App 前执行了 `__disable_irq()`，清理 SysTick 和 NVIC 后直接跳转 App，没有恢复 `PRIMASK`。

Cortex-M 的 `PRIMASK` 会跨函数跳转保留，因此 App 虽然能从 `0x08004800` 跑起来，但全局中断仍被屏蔽：

- `InitTimer()` 能配置 TIM3 和 NVIC。
- 主循环能进入 `Runtime_RunOnce()`。
- 但 `TIM3_IRQHandler()` 不会执行。
- 200ms token 不会产生，`App_AFEGet()` 永远提前返回。

## master 与 dev 对比结论

`master` 分支的 `IAP_To_APP_Jump()` 只做 App 栈顶粗校验，然后设置 MSP 并跳转，没有执行 `__disable_irq()`，所以 App 进入 Reset_Handler 时 `PRIMASK` 仍为 0，TIM3 中断可以正常运行。

`dev` 分支新增了更可靠的 App 向量校验、SysTick 关闭、NVIC 禁用/清 pending、`SCB->VTOR = 0x08004800` 等跳转现场清理。这些方向是正确的，但新增 `__disable_irq()` 后漏掉恢复，导致 App 带着 `PRIMASK=1` 启动。这就是 `master` 正常、`dev` 异常的差异点。

## 本次修复

1. 在 IAP 工程 `E:\work\a002\new 030\IAP 103CB\Source\main.c` 的 `IAP_To_APP_Jump()` 中，设置 App MSP 后、跳转 App Reset_Handler 前调用 `__enable_irq()`。
2. 在当前 App `103 + 309\Project\Source\main.c` 中，`InitTimer()` 完成后调用 `__enable_irq()`，作为对旧 IAP 或调试入口带 `PRIMASK=1` 的防御。

IAP 侧同时补强了交接清理：

- 清除 SysTick 与 PendSV pending，避免恢复中断后带旧 pending 进入 App。
- 设置 `CONTROL=0`，确保跳转到 App 时使用特权线程模式和 MSP。
- 保留 App 向量合法性检查、`SCB->VTOR = 0x08004800`、NVIC 禁用/清 pending。

## Keil 验证点

进入 App 后可观察：

- `__get_PRIMASK()` 应为 `0`。
- `SCB->VTOR` 应为 `0x08004800`。
- `TIM3->CR1 & TIM_CR1_CEN` 应非 0。
- `TIM3->DIER & TIM_DIER_UIE` 应非 0。
- `NVIC->ISER[0]` 中 TIM3 对应位应打开。
- `SysTime_Get10msTickCount()` 应持续递增。
- `SysTime_Take200msTaskPeriod()` 每约 200ms 返回一次 1。

若 `SCB->VTOR` 不是 `0x08004800`，优先检查 App 是否以 `_IAP` 构建，以及 `system_stm32f10x.c` 中 `VECT_TAB_OFFSET` 是否保持 `0x4800`。
