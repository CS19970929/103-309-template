# CAN 电源、RTC 休眠和 bus-off 简化记录

文档状态：已按源码验证
源码参考：`Can_HDX.c`, `Can_HDX.h`, `RTC.c`, `rtc_sleep.c`, `rtc_sleep_port.c`, `Project_Config.h`
日期：2026-06-02

## 本次决策

- RTC 休眠可以关闭 `CMNT` 电源，唤醒恢复后重新打开即可。
- RTC 周期唤醒不再广播 CAN，也不再为了 CAN active/idle 切换 RTC 周期。
- bus-off 不再由软件状态机监控，保留 `CAN_ABOM = ENABLE` 让 bxCAN 自动恢复。
- 不修改 CAN ID、payload、App 命令、Modbus 寄存器桥接和 IAP 地址规则。

## 源码变化

- `Can_PrepareSleep()`：休眠前取消 TX、清命令队列、停止 block stream，并关闭 `GPIO_CMNT_EN`。
- `InitCan()` / `InitCan_GPIO()`：运行态初始化后打开 `GPIO_CMNT_EN`，STOP 唤醒恢复路径会重新打开。
- `rtc_sleep.c`：删除 RTC HICCUP 周期唤醒后的 CAN 服务调用。
- `RTC.c`：默认 RTC wake period 固定为 10s，不再调用 CAN 查询 active/idle 周期。
- `Can_HDX.c/.h`：删除 `Can_RtcWakeService()`、`Can_GetIdleRtcPeriodSeconds()`、`Can_IsBusActive()`。
- `Can_HDX.c`：删除软件 bus-off monitor 和 `s_runtime.bus_off`，debug 仍可只读 `CAN1->ESR` 的 BOFF 位。
- `Project_Config.h`：删除 `PROJECT_CFG_CAN_RTC_WAKE_PERIOD_SECONDS`。

## 当前边界

- 运行态 CAN：保持 1000ms/5000ms 周期广播和 CAN App 服务。
- RTC STOP 中：CMNT 关闭，周期醒来不主动发送 CAN。
- 外部唤醒后：恢复时钟、IO、ADC、USART、CAN、TIM3、AFE I2C，随后由主循环继续 CAN 通信。
- 软件不再统计 bus-off 进入/恢复次数；异常分析看 `CAN1->ESR` 和 no-ACK 计数。

## 验证要求

1. Keil 编译确认旧函数没有未定义引用。
2. RTC STOP 前后实测 `GPIO_CMNT_EN` 电平：睡前关闭，恢复后打开。
3. 正常运行态抓包确认周期广播和 CAN App 服务不变。
4. 模拟 bus-off 后恢复总线，确认 ABOM 自动恢复，周期帧能继续发送。
