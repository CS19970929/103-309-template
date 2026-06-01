# 103-309 BMS 软件架构

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`main.c`, `AppInit.c`, `Runtime.c`, `System_Init.c`, `DataDeal.c`, `SOC.c`, `Sci_Upper.c`, `Can_HDX.c`, `Flash.c`, `rtc_sleep.c`
最后更新时间：2026-05-26
未确认事项：AFE 电流路径、均衡、低功耗周期 CAN、Host 写权限。

## 1. 软件分层

```text
应用策略层
  SOC / 工厂老化 / 低功耗策略 / LED 显示 / 客户逻辑

业务数据层
  g_stCellInfoReport / OtherElement / PRT_E2ROMParas / System_ErrFlag

协议适配层
  Sci_Upper(Modbus) / Can_HDX + CanFeidaoFrames

驱动和硬件层
  SH367309 AFE / ADC / GPIO / RTC / TIM / IWDG / Flash / USART / CAN

启动和调度层
  AppInit / Runtime / TIM3 10ms tick
```

## 2. 主循环

```text
main()
  AppInit_Boot()
  while (1)
    Runtime_RunOnce()
```

`Runtime_RunOnce()` 当前顺序：

1. 锁存时基、老化、LedBar、AFE/SOC。
2. 处理 Modbus、ADC、低功耗、CAN。
3. 处理 Flash update、日志、产品信息、喂狗。

## 3. 任务调度

| 周期/触发 | 任务 | 说明 |
|---|---|---|
| 每轮主循环 | `App_CommonUpper()`, `LP_Task()`, `App_Can()` | 通信和低功耗持续服务 |
| TIM3 10ms | `SysTime_Post10msTick()` | 产生软件标志 |
| 约 200ms | `App_AFEGet()` | AFE 采样、DataLoad、SOC、保护辅助 |
| ADC TIM2 触发 | `ADC1 + DMA` | PA1/PA2/PB1 模拟采样 |
| TIM4 | `LedBar_Scan1ms()` | 查理复用显示扫描 |
| RTC sec/alarm | `RTC_IRQHandler`, `RTCAlarm_IRQHandler` | 秒更新和 STOP 唤醒 |

## 4. 核心数据流

```text
SH367309 / ADC / Flash 参数
  -> DataDeal / SOC_UpdateSampleData
  -> g_stCellInfoReport
  -> Modbus / CAN / LED / LowPower / Log
```

当前最重要的数据总线是 `g_stCellInfoReport`。后续重构不能随意改变其字段布局，因为 Modbus、CAN、LED、低功耗和日志都在消费它。

## 5. 控制流

| 控制主题 | 入口 | 当前行为 |
|---|---|---|
| AFE 配置 | `SH367309_UpdataAfeConfig()` | 参数变化后写 AFE ROM/寄存器并 reset AFE |
| MOS | `SH367309_DriverMos_Ctrl()`, `MosStartup_*`, `new_todo_logi()` | 启动、保护、老化和客户逻辑都可能影响 MOS |
| IAP | `AppUpgrade_RequestIap()`, `App_FlashUpdate()` | SRAM mailbox + reset |
| 低功耗 | `LP_Task()`, `rtc_sleep()`, `SleepDeal_Continue()` | HICCUP STOP 或 reset sleep |
| 上位机写 | `Sci_Deal_WrReg_0x06()`, `Sci_Deal_WrRegs_0x10()` | 写参数、保存 Flash、触发副作用 |

## 6. 当前架构问题

1. `DataDeal.c` 职责过多。
2. 协议写入和业务副作用耦合深。
3. AFE 未抽象，SH367309 强绑定。
4. 文档和阶段方案过多，容易引用旧结论。
5. 部分需求可能是客户定制或历史残留。

## 7. 后续架构方向

1. 先确认需求，不直接重构。
2. 固化协议、Flash、IAP、安全逻辑测试。
3. 将 AFE、采集、保护、SOC、低功耗、显示按边界拆清。
4. 把客户协议适配和核心 BMS 逻辑分层。
