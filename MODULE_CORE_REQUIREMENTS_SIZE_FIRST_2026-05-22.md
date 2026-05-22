# 五个复杂模块根本需求与减码优先边界 2026-05-22

## 总原则

当前目标不是继续给模块叠加状态、调试入口或兜底分支，而是在不改变现有量产功能的前提下，把能在编译期确定、能用统一数据结构表达、能由现有硬件状态替代的代码优先压掉。

减码优先级：

1. 默认关闭的调试/测试/可写配置路径，必须编译期裁剪，不留运行时空判断。
2. 同一类状态只保留一个权威来源，显示、通信和 SOC 只消费结果，不重新计算。
3. 帧、段、邮箱、表项这类固定集合优先用掩码、索引或小表表达，不复制多套 if/switch。
4. P0/P1 功能不得直接删除。保护、上位机兼容、SOC 可见值、低功耗唤醒、IAP 地址和烧录规则必须保持可验证。
5. 每轮减码必须记录 Release 构建的 Code/RO/RW/ZI/ROM 变化。

## 数码管 LED

根本需求：

- 显示 0-100 的 SOC 数字和百分号，休眠/唤醒后能短暂显示最近 SOC。
- 按键短按只请求显示窗口；长按休眠逻辑保持由低功耗模块承接。
- 充电、放电、故障只决定显示状态或图标，不反向驱动 SOC、保护和低功耗策略。
- 扫描必须稳定，非空帧切换不应长时间关 TIM4 或重配 GPIO。
- 单段测试、常亮测试只能通过编译配置或显式接口进入，量产默认关闭。

本轮减码：

- `LedBar.c` 去掉运行时 `routes[18] + length` 帧数组，改为 `uint32_t frame_mask`。
- 段码构建只产出掩码，TIM4 扫描从掩码里寻找下一个 route。
- 保留原 route 顺序、休眠预览、SOC 显示窗口、单段测试和按键长按路径。

后续可裁剪点：

- 若确认量产不需要单段测试接口，可用 `PROJECT_CFG_LEDBAR_SINGLE_SEG_TEST_ENABLE` 编译期裁掉。
- 充电图标来源后续应统一到可靠硬件信号或明确产品状态，本轮不改变现有图标行为。

## 出厂老化模式

根本需求：

- 只在首次出厂未完成时自动进入。
- 只累计 MCU 正常运行时间，STOP/RTC 休眠时间不计入老化。
- 运行进度必须保存，BKP 用于短周期掉电恢复，Flash 用于低频长期保存。
- 完成后关闭老化 MOS 策略并写入完成标志，后续上电不再自动进入。
- 保存失败可以有限重试，但不应扩展成通用调试/测试框架。

本轮处理：

- 未改 `FactoryAging.c` 行为。该模块当前已经基本收敛到启动判定、进度保存、完成退出三类职责。

后续可裁剪点：

- 如确认断电恢复只需要 Flash，不需要 BKP 秒级进度，可删除 BKP 镜像路径，但会改变掉电续跑精度，需台架确认。
- 如老化模式只在 Factory/Test 固件使用，可把 `PROJECT_CFG_FACTORY_AGING_ENABLE` 从量产默认开启改为目标配置差异，但必须先确认出厂流程。

## SOC

根本需求：

- 输入只来自 AFE 单体电压、净充放电电流、Type-C 折算到电池侧的等效电流、配置参数和持久化快照。
- 输出只发布给上位机、CAN、LedBar 和低功耗判断，不在这些模块里重复估算 SOC。
- 保留容量积分、SOH/循环次数、满电锚点、空电尾段、静置 OCV 下修、RTC 休眠补偿和显示平滑。
- 静置/RTC OCV 修正不得向上拉高 SOC；测试注入模式必须由 `PROJECT_CFG_SOC_TEST_MODE_ENABLE` 和 Factory/Test 档位隔离。
- `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE=0` 时只保留当前电芯体系表，运行时写 SOC 表入口应拒绝。

本轮减码：

- `PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT=0` 和 `PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT=0` 时，故障阻断函数不再进入 Release 镜像。
- `PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT=0` 且 `PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT=100` 时，空电软尾段二次调参函数编译期裁掉。
- 配置改成非默认值时，原阻断和调参逻辑仍会编译恢复。

后续可裁剪点：

- 若产品确认不需要中段 tail 修正，可新增配置裁掉 `s_mid_tail_table` 和 `soc_mid_tail_config()`，但需要路测低电区显示。
- `SOC_DEBUG_WATCH` 已默认关闭，不建议再在量产里增加新的 watch 字段。

## AFE 零电流校准

根本需求：

- CADC 原始二补码是唯一电流采样源，先做符号转换，再做零点补偿，再换算 mA/A*10。
- 上电零点只在闭合条件允许、采样稳定、原始值在限幅内时学习。
- 运行时自动零点只在接近零电流且原始值稳定时缓慢更新。
- 输出 0.2A deadband 后再进入 SOC，避免零漂驱动 SOC 积分。
- K/B 电流校准默认关闭，若开启必须走编译期开关，不留运行时死分支。

本轮处理：

- 未改 `DataDeal.c` 零电流实现，避免混入当前已有的电压 K/B 未提交改动。
- 当前代码已用 `AFE_CURRENT_KB_CALIB_ENABLE 0U` 编译期关闭电流 K/B 校准，符合减码方向。

后续可裁剪点：

- 如上位机不再读取完整 `AFE_CURRENT_OBSERVE`，可把观察字段缩到少量状态字，节省 RAM 和写字段代码。
- 冷/热启动零点参数可合并为一个配置表项，但需要确认休眠唤醒后的 settle 时间不变。

## CAN

根本需求：

- CAN 初始化保持 250 kbit/s、NART 开启，避免无 ACK 时自动重发带来高功耗。
- 正常有设备时按 1s/5s 广播飞道协议帧。
- 无 ACK 多次后进入低频探测，不持续高频广播。
- RTC 唤醒服务窗口有界，记录本次唤醒是否发成功、是否超时，然后准备睡眠。
- 帧格式、大小端和 ID 不因低功耗调度调整而改变。

本轮减码：

- `Can_HDX.c` 把 3 个邮箱的 `TME/RQCP` 处理从 switch 和三连调用改为公式和小循环。
- 保留 ACK、无 ACK、超时、取消发送、RTC 服务状态和低功耗供电策略。

后续可裁剪点：

- 如果现场不需要 `g_stCanErrorSnapshot` 的全部计数，可裁成少量最后错误和累计失败数。
- 如果产品只发固定 1s 基础状态帧，可用配置裁掉 5s 扩展帧，但会改变外部协议可见内容，需客户确认。

## 本轮构建结果

基线来自本轮修改前同一未提交工作区的 `FD_Release` 构建：

```text
Before: Code=46588 RO-data=2568 RW-data=808 ZI-data=5256 ROM=49400
After:  Code=46220 RO-data=2568 RW-data=792 ZI-data=5256 ROM=49032
Delta:  Code -368, RW -16, ROM -368
```

验证：

```text
py -3 tools\project_check.py -q
OK: 141, Warnings: 0, Errors: 0

FD_Release rebuild
0 Error(s), 0 Warning(s)
```

说明：本轮构建包含工作区中已有的 `Fault.*`、`RTC.c`、`DataDeal.c`、`Sci_Upper.c` 等未提交改动，因此体积数字只能作为当前工作区内本轮增量对比。提交时应只提交本轮触碰的 LED/SOC/CAN 和本文档，避免把已有暂存改动混入。
