# 休眠与均衡逻辑复核

日期：2026-05-18

范围：主项目 `103 + 309`，对照官方 `SH3673520+STM32F072CBT6 DemoCode V1.2_20241227` 中 AFE sleep 与 balance 的寄存器写法。

## 主循环关系

主循环当前执行顺序是：

1. `App_AFEGet()` 读取 AFE 电压、电流、状态。
2. `App_WarnCtrl()` 更新保护和故障状态。
3. `App_CellBalance()` 按 1s 节拍计算并写入 AFE 均衡寄存器。
4. `App_SleepDeal()` 按 1s 节拍处理普通休眠、强制休眠和过放深睡。

这个顺序合理：先采样，再保护判断，再均衡，最后进入休眠。休眠前必须关闭均衡，否则即使 AFE sleep 理论上会停均衡，软件状态和寄存器仍可能短时间不一致。

## 休眠逻辑

当前实际使用的是 `SleepDeal.c` 的 `App_SleepDeal()`；`__FUNC_RTC__` 未打开，因此 `rtc_sleep.c` 里的长 RTC sleep 状态机不是主路径，但外部代码仍可能调用 `entersleep()`。

休眠命令通过 `Sleep_Mode` 位域表达：

- bit0：测试休眠。
- bit1~bit3：普通 L1/L2/L3 休眠。
- bit4~bit6：外部强制 L1/L2/L3 休眠。
- bit8~bit12：过流、压差、CBC、过压、过放休眠。
- bit13：`b1_ToSleepFlag`，只是状态标志，不应作为触发命令。

本次修复后，`App_SleepDeal()` 使用 `SLEEP_CMD_MASK = 0x1FFF` 触发 `SleepDeal_Continue()`，覆盖 bit0~bit12，排除 bit13。这样过放、过压、过流等高位保护休眠命令不会再被 `0x00ff` 漏掉。

## 过放休眠

原风险：

- `force_sleep_delay` 原来是 `uint8_t`，但目标阈值是 `60 * 60 = 3600` 秒，8 位计数永远到不了 3600，`VCellMin < 2500mV` 的 1 小时强制深睡实际失效。
- 普通休眠选择里，放电虚电流误用了充电阈值 `u16Sleep_VirCur_Chg`，应使用 `u16Sleep_VirCur_Dsg`。
- `entersleep(NORMAL_MODE)` 原为空实现，其他模块调用普通休眠时不会真正产生休眠命令。
- AFE sleep 前没有显式关闭均衡。

修复后逻辑：

- `force_sleep_delay` 改为 `UINT32`，计数到 `SLEEP_FORCE_DEEP_DELAY_S = 3600s` 后执行 `entersleep(DEEP_MODE)`。
- 强制深睡条件集中在 `SleepDeal_ForceDeepRequired()`：
  - 单体最小电压必须是有效采样值，范围 1000~5000mV。
  - 充电电流大于 `u16Sleep_VirCur_Chg` 时不强制深睡，避免正在有效充电时进入深睡。
  - `VCellMin < 2500mV` 时进入强制深睡计时。
  - AFE CUV、软件单体欠压、总压欠压标志出现时也进入强制深睡计时。
- `SleepDeal_Normal_Select()` 的放电虚电流阈值修正为 `u16Sleep_VirCur_Dsg`。
- `entersleep(NORMAL_MODE)` 设置 `b1ForceToSleep_L2`，恢复普通休眠命令入口。
- `AFE_Sleep()` 先调用 `CellBalance_ForceOff()` 清零均衡寄存器，再写 `SCONF1 = 0xAA` 进入 AFE sleep。

过放休眠需要实板确认的点：

- 欠压保护标志和 `VCellMin < 2500mV` 条件在放电负载下能稳定持续 3600s。
- 有效充电电流高于 `u16Sleep_VirCur_Chg` 时不会误深睡。
- 写入 sleep flag 后 MCU reset，下一次启动能按 `IsSleepStartUp()` 进入对应低功耗 IO 状态。

## 均衡逻辑

官方例程做法是每 1s 判断均衡允许条件，满足条件后写 `BALANCEH/BALANCEM/BALANCEL` 三个寄存器；不满足时写 0 关闭均衡。

主工程当前保留“轻载/静置均衡”策略，不强行改成官方充电均衡策略，避免改变产品行为。当前允许条件是：

- 均衡开关 `b1OnOFF_Balance` 打开。
- 串数有效，最多按 20 串掩码保护。
- `VCellMin` 为 1000~5000mV 的有效采样值。
- 无三级保护故障。
- 无 AFE1、SPI、CBC_DSG 系统错误。
- 充电和放电电流都不超过 `CB_BALANCE_CURRENT_LIMIT = 10`，即 1.0A。
- 最低单体电压不低于 `u16Balance_OpenVoltage`。
- 没有均衡打开时，压差达到 `u16Balance_OpenWindow` 才允许开启。
- 已有均衡打开时，压差降到 `u16Balance_CloseWindow` 以下才关闭，形成滞回。

本次修复点：

- 新增文件级 `s_u32CB_ActiveMask`，把 AFE 寄存器状态、软件上报状态和滞回判断统一。
- 新增 `CellBalance_ForceOff()`，集中写 0 到 `BALANCEH/BALANCEM/BALANCEL`，并清空滤波、刷新计数和上报状态。
- 均衡写寄存器顺序保持官方顺序：`BALANCEH -> BALANCEM -> BALANCEL`。
- 均衡 mask 构造使用 32 位比较，避免 `vcell_min + window` 发生 16 位溢出。
- 使用 `u16Balance_CloseWindow` 做关闭窗口，避免临界压差附近频繁开关。
- AFE/SPI/CBC/三级保护故障时，下一次 1s 均衡周期会写 0 关闭 AFE 均衡。

仍需实板确认的点：

- AFE 三字节均衡寄存器读回与目标 mask 一致。
- 低压、故障、SPI 错误、CBC 错误出现时，`BALANCEH/M/L` 被清零。
- OpenWindow/CloseWindow 参数是否满足实际热设计，尤其是小压差边界不应来回抖动。
- `CB_BALANCE_CURRENT_LIMIT = 1.0A` 是否符合当前板子的“轻载/静置均衡”要求；如果产品要求只在充电期间均衡，需要另开策略变更。

## 编译结果

Keil MDK 编译命令：

```powershell
UV4.exe -b CommomSH367309_16series_103RCT6_C.uvprojx -t "Target 1"
```

结果：

- `0 Error(s), 50 Warning(s)`
- 产物：`Objects/CommomSH367309_16series_103RCT6_C.axf`
- 大小：`Code=53312 RO-data=2372 RW-data=1248 ZI-data=6040`

这些 warning 为工程既有隐式声明、未使用变量、缺少返回等问题，本次改动没有引入编译错误。
