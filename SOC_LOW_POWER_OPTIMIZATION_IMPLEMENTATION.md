# SOC 与低功耗优化实现说明

## 1. 目标

本次优化同时解决两类问题：

- SOC 在保护板场景下的用户体验问题。
- 在不影响原有功能和唤醒机制的前提下，降低 MCU 运行期无效功耗。

设计约束如下：

- 不改变现有 BMS 保护主流程。
- 不破坏 RTC/按键/串口等唤醒链路。
- 不依赖高成本电流精度，允许电流零漂、长期静置、自耗等现实问题存在。
- 优先做“可量产、可维护、可解释”的改动，而不是引入重型估算器。

## 2. SOC 优化策略

### 2.1 总体思路

SOC 采用“积分主线 + 静置纠偏 + 弱单体约束 + 显示层平滑”的结构：

- 运行中仍以现有安时积分为主。
- 静置时按时间桶触发 OCV 渐进纠偏，不做突兀跳变。
- 接近放空时增加最弱单体约束，避免用户看到剩余百分比仍较高却突然掉保护。
- 内部计算值与显示值分离，显示值单独平滑，改善百分比来回抖动。

### 2.2 关键实现

文件：`103 + 309/Project/Source/SocEnhance.c`

- 新增 `SOC_RUNTIME_CONTEXT`，维护显示 SOC、静置累计时间、已应用静置校正档位。
- `SOC_ApplyRestCompensation()`：
  - 按 10min、30min、1h、6h 分档纠偏。
  - 静置时间越长，允许的 OCV 修正步长越大。
  - 对低压区优先下修，对满充区允许更积极上修。
- `SOC_UpdateRestMonitor()`：
  - 仅在无充放电、单体电压有效时累计静置时间。
  - 一旦检测到电流活动，立即清空静置累计，避免误把动态电压当 OCV。
- `SOC_ApplyWeakCellGuard()`：
  - 依据最低单体电压对低 SOC 区做保守下拉。
  - 低压越接近 `u16_SOC_0_Vol`，下拉越快；进入临界窗口时允许快速降到 0~2%。
- `SOC_UpdateDisplaySoc()`：
  - 显示值独立于内部计算值。
  - 上升以慢速跟随为主，下降在低压风险场景允许更快跟随。
- `SOC_ApplyRtcRelaxationCompensation()`：
  - 供 RTC 静置唤醒后直接调用。
  - 使用休眠累计时间和最新最小/最大单体电压做恢复校正，并回写外部报文结构。

### 2.3 持久化与长期不用场景

- `SOC_PersistSnapshotIfChanged()` 统一管理 SOC、放电积分和循环次数保存。
- 只在关键数据变化时落盘，减少无意义写 Flash/EEPROM 的耗能和磨损。
- 对长期不用场景，不再盲信上次保存的显示值，而是在 RTC 唤醒后结合静置时长重新估算。

## 3. 低功耗策略

### 3.1 主循环空闲睡眠

文件：`103 + 309/Project/Source/main.c`

新增两级空闲判断：

- `MainLoop_HasPendingWork()`：
  - 检查系统时基标志。
  - 检查 200ms/1000ms 累计任务标志。
  - 检查 SCI1/SCI2/SCI3 发送使能，避免串口发送过程中进入空闲睡眠。
- `MainLoop_EnterIdleSleep()`：
  - 仅在 `b1OnOFF_Sleep` 允许时执行。
  - 若中断被屏蔽，则不进入 WFI，避免极端情况下睡死。
  - 显式清除 `SLEEPDEEP`，确保这里只进入普通 Sleep，而不会误入 STOP。
  - 执行 `__DSB()` + `__WFI()` + `__ISB()`，把主循环无效空转替换为中断唤醒。

这样做的效果是：

- 有任务时立刻继续跑，不影响原有处理时序。
- 无任务时让 CPU 停在 Sleep，等 SysTick、RTC、EXTI、串口中断再唤醒。
- 不改变原本 `sleep()` 中 RTC/Stop 模式的深睡逻辑，只补一个“主循环空闲态”的轻睡眠层。

### 3.2 减少无意义外设耗电

文件：`103 + 309/Project/Source/SOC.c`

- 将 `MCUO_DEBUG_LED1` 的 200ms 翻转限制在 `_DEBUG_CODE` 下。
- 量产路径不再因调试指示灯周期翻转产生额外功耗。
- 该修改不影响 SOC 功能和对外数据。

### 3.3 RTC 静置恢复

文件：`103 + 309/Project/Source/rtc_sleep.c`

- `rtc_sleep_get_period_seconds()` 改为依据 `OtherElement.u16Sleep_TimeRTC` 计算真实静置周期。
- `update_rtc_soc()` 在 RTC 唤醒后把累计静置时间换算成秒，交给 `SOC_ApplyRtcRelaxationCompensation()`。
- 同时清理了历史遗留的多份废弃 `update_rtc_soc()` 实现，避免编译误报和维护歧义。

## 4. 本次修改文件

- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/SocEnhance.h`
- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/main.c`
- `103 + 309/Project/Source/rtc_sleep.c`

## 5. 验证结果

构建工程：

- 工程文件：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- Target：`Target 1`

构建结果：

- `0 Error(s), 0 Warning(s)`
- `Program Size: Code=40968 RO-data=2120 RW-data=1216 ZI-data=6048`
- `Build Time Elapsed: 00:00:03`

产物位置：

- `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.axf`
- `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.bin`

## 6. 预期收益

- 长期静置后，SOC 更接近真实开路状态，不容易“醒来还显示老百分比”。
- 低电量区会更保守，减少“剩余电量还不少却突然掉电”的体感问题。
- SOC 百分比显示更平滑，减少 1%~2% 来回跳动。
- 主循环无任务时不再持续空转，CPU 平均运行功耗下降。
- 去除量产路径下的调试灯翻转，进一步降低板级静态运行损耗。

## 7. 后续建议

- 实测记录空闲电流，对比本次改动前后的运行态平均电流。
- 补一组日志验证 RTC 静置 10min、30min、1h、6h 四个时间桶的 SOC 变化是否符合预期。
- 若后续继续做低功耗，可再评估：
  - 非活跃阶段关闭部分周期性日志输出。
  - 对无变化的采样/通讯任务进一步降频。
  - 在进入更深层睡眠前统一关断不必要 GPIO 驱动状态。
