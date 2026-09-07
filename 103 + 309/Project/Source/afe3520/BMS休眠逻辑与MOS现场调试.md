# BMS 完整休眠逻辑与 MOS 现场调试

依据 2026-09-07 当前源码和 ST-Link 实测。入口为 Runtime_Boot / Runtime_RunOnce，主要实现为 rtc_sleep.c、SleepDeal.c、rtc_sleep_port.c、rtc_sleep_afe3520.c、RTC.c、conf/conf.c、LowPowerSleep.c。本文件区分已实测结果和从源码推导的路径。

## 本次 MOS 不开：已实板确认的根因

ST-Link V2J37S7 / SWD 950 kHz，目标约 3.28 V，Cortex-M3。首次附着没有下载程序，板上 ER_IROM1 与当时 FD_Release.axf 完全匹配；RW_IRAM1 在运行时被修改，GDB compare-sections 报 RAM 不同是预期现象。

| 观察量 | 故障现场 | 仅修正 RAM 请求后 |
| --- | --- | --- |
| requestedCharge / requestedDischarge | 0 / 0 | 1 / 1 |
| actualCharge / actualDischarge | 0 / 0 | 1 / 1 |
| configValid / mosFeedbackValid | 1 / 1 | 1 / 1 |
| chargeBlocks / dischargeBlocks / globalBlocks | 全部 0 | 全部 0 |
| AFE BSTATUS1 | 0x00 | 0x03 |
| 采样 | 19 串约 3.13～3.31 V，温度约 25～27 ℃ | 正常 |
| 通信 | 初始 CRC/ACK/超时均 0 | 调试复位期间出现一次超时，自动恢复，lastError=OK |

ProtectionInit 清零请求；Runtime_Boot 只初始化 AFE，没有调用 MosStartup_ApplyInitialState。上一轮删除重复 MOS 策略后，没有任何正常启动入口发出开启请求，原重复逻辑曾掩盖该问题。修复是在 InitData_SOC 后、开中断前，仅调用一次 MosStartup_ApplyInitialState；第一次保护服务完成有效检查前保持关断。日常循环不会重写请求，因此保留手动关断与故障禁止逻辑。

RAM 验证先确认采样/configValid 有效且全部阻止位为 0，再置正常请求为 1，继续 MCU，让现有服务仲裁并回读；没有直接改 AFE MOS 位或跳过保护。实板恢复为双 MOS 开启。该 RAM 设置复位后丢失。

## 为什么此次会睡眠

初次暂停观察到 MCU 位于 PWR_EnterSTOPMode；之前未保留进入睡眠瞬间的调用栈/分支原因，无法据此单独证明具体入口。

唤醒后读取：空闲门限 idleMax=10 秒，commandSleepPending=0，故障保底计数=0、低压保底计数=0，电芯最小约 3133 mV，充放电电流为 0，放电请求与反馈均为 0。结合源码，明确存在以下路径：空闲 10 秒进入 HICCUP → 默认每 10 秒 RTC 唤醒检查 → 电压正常但放电 MOS 关闭 → 请求 DEEP → 关断确认后写备份标志并复位 → 启动早期进入 STOP。这个路径可解释本次 MOS 初始化遗漏导致的深睡；属于源码与现场共同支持的推断，未捕获分支瞬间。

ST-Link 暂停不被 LP_GetBlockReason 识别为外部通信，不会自动取消空闲休眠；调试暂停会改变运行计时和通信时序，因此不能用墙钟时间推定固件累计秒数。

## 调度优先级

每次 Runtime_RunOnce 执行 AFE/上位机/模拟校准后调用 rtc_sleep：

1. 先处理上位机显式休眠锁存；不等 1 秒节拍。等待应答、CAN/串口空闲、存储事务与升级完成后执行强制休眠。
2. 不是 1 秒节拍则只刷新显示状态，退出。
3. 判断故障保底和临界低压保底，任一到期直接执行 EmergencySleep。
4. 检查普通阻止条件；允许时优先判断低压，随后累计普通空闲。
5. DEEP 请求走复位休眠；HICCUP/NORMAL 请求走 RTC STOP 循环。

## 四个软件模式与实际 MCU 状态

| 模式 | 数值 | 当前处理 |
| --- | ---: | --- |
| NORMAL_MODE | 0 | 主调度中进入 RTC STOP 循环，RTC 周期 20 秒；另有 SleepDeal_Continue(NORMAL) 的复位标志路径 |
| HICCUP_MODE | 1 | 空闲自动选择；RTC 周期 10 秒唤醒采样 |
| DEEP_MODE | 2 | 安全关 MOS、AFE Sleep 成功后保存状态、写标志、复位；启动早期等待外部唤醒 |
| NO_SLEEP | 3 | 正常主循环 |

“深度休眠”在本工程实际使用 PWR_EnterSTOPMode(低功耗调压器, WFI)，不是 MCU STANDBY，也不是关掉全部板级电源。普通深睡和保底深睡有不同的入口检查及唤醒策略。

## 普通自动休眠阻止条件

LP_GetBlockReason 每次重新检查：AFE 采样无效/配置脏/配置无效、通信/配置/系统阻止位；AFE WDT 编译开启；充电或放电电流 ≥0.5 A；串口/CAN 正忙；PA0 唤醒输入有效；存储忙或待参数保存；升级标志；二/三级报告故障或 AFE SC；外部通信计数变化。

这些条件通常清除 idle、force、vlow 并返回 NO_SLEEP。故障报告可被低压分支优先处理，但 AFE 不可信、WDT、升级、存储、充放电、通信、唤醒输入等关键阻止条件不能由普通低压请求绕过。

当前 LP_BLOCK_KEY 的输入实际来自 PA0 的 MCU_WAKE 接口；PB5 按键另由底层 STOP 前检查和 EXTI 唤醒处理，不能仅按阻止位名字理解硬件引脚。LP_BLOCK_LED_ACTIVE 仅有定义，当前 LP_GetBlockReason 没有设置它。

## 自动低压与故障保底

| 路径 | 条件和时间 | 是否受普通阻止条件限制 |
| --- | --- | --- |
| 常规低压深睡 | 有效最小电压 ≤ OtherElement.u16Sleep_Vlow，充电 ≤0.5 A；累计 TimeVlow×60 个 1 秒节拍 | 是 |
| 普通临界低压 | 三元 ≤2750 mV / 磷酸铁锂 ≤2650 mV，充电 ≤0.5 A，60 秒 | 是 |
| 保底临界低压 | 有效采样、上述临界低压，充电 ≤0.5 A；AFE3520_CFG_EMERGENCY_LOW_SECONDS 默认60秒 | 否 |
| 故障保底 | 采样/配置无效，或 AFE通信/配置、SC、WDT、内部温度全局阻止；默认300秒 | 否 |
| MCU 致命异常 | AFE3520_CFG_FATAL_SLEEP_ENABLE 默认1；走异常处理中的保底复位入口 | 不等待主循环 |

保底计数饱和，条件消失归零；无效旧电压不作为可信低压依据。保底不受通信/升级/存储忙阻止，也不等待普通关断 ACK。主循环完全卡死且无法执行异常处理时，软件累计超时不能保证生效；默认未调用 Init_IWDG。

普通低压存在两个计数器 force/vlow；不同低压区间相互切换时源码没有在每个分支清掉另一个计数器，因此不能将其表述为所有情况下严格连续计时。保底计数独立且每次不满足立即归零。本次没有调整这些既有条件。

MonitorAFE_UpdateSleepDelay 对持续错误累计5分钟后提出 NORMAL_MODE；实际仍受主调度有效性和保底路径管理，不能保证该请求立即睡。可选 __SOC_5_PROTECT_ 分支也会请求 DEEP；默认未开启，不能作为当前板上已生效策略解释。

## RTC 周期休眠流程

保存 CAN/SOC 状态，GPIO 进入低功耗配置；HICCUP 使用 RTC、PA0、PB5、PB12 通信唤醒配置。每轮设 RTC 闹钟，记录 RTC 起点，进入 STOP。

唤醒后计算真实 RTC 差值，恢复 SPI 引脚并标记配置待校验。RTC 唤醒进行电压/温度/电流采样及 AFE 常驻保护服务。采样失败、通信故障、保护阻止或检测到电流时退出周期休眠，恢复运行初始化；通过检查后进行 SOC 静置补偿。

若最小电芯 ≤ 软件 UV 保护阈值，或实际放电 MOS 为关闭，则调用 low_power_log_and_commit_sleep(DEEP_MODE)。后者先重新检查关键阻止条件，再提交复位深睡。这解释为什么本次“电压正常但 MOS 一直没开”也会进入深睡。

Sys_StopMode 在屏蔽中断的临界段检查待处理唤醒、PB5按下、PA0有效，避免刚到来的事件被丢弃。退出 STOP 后恢复系统时钟/延时及运行外围；SPI 按实际时钟适配。

## 普通深睡提交与启动

SleepDeal_Continue 先检查模式、AFE WDT、采样与配置/通信故障；置系统禁止，由仲裁关闭 MOS。必须同时有可靠的双 MOS 关闭反馈及 AFE EnterSleep 成功才保存 SOC 状态、写备份标志并复位。失败则报告 AFE 错误，撤销临时系统禁止并取消该次普通深睡。

BootFlag 使用 BKP_DR2 保存值、BKP_DR3 保存反码，校验不符视为普通启动。0x1234=NORMAL，0x1235=DEEP，0x1236=HICCUP，0x1237=充电唤醒，0x1238=保底，0xFFFF=清除。

复位后 Runtime_Boot 在 RTC/AFE/通信初始化前处理 NORMAL/DEEP 标志：先清标志，配置深睡 IO，等待 PA0 充电唤醒有效或 PB5 按键。获准后继续完整初始化。不能把启动时读取到已清空的标志当作“之前没睡过”的证据。

## 上位机显式休眠与保底深睡

上位机功能开启命令中功能号0x0A设置 commandSleepPending；自动调度和持续轮询不能取消它。待当前应答、存储和升级结束，保存状态后复用 EmergencySleep。它不要求电流为零，也不要求 AFE 正常响应；但如果升级/存储一直忙，显式命令仍会等待。

EmergencySleep 拉低 PB14，有限次尝试关 MOS、关闭 AFE WDT、关闭均衡并进入 AFE Sleep；失败仍写0x1238并复位。故障保底不执行额外 Flash/SOC 保存，优先降低耗电。

保底启动关闭 RTC/SysTick，PA15/PB3/PB4/PB14驱动低、PA4 CS高，其余非唤醒 IO低功耗。默认只接受 PB5 新下降沿；按键卡在按下电平不能让 MCU持续运行。AFE3520_CFG_EMERGENCY_CHARGER_WAKE=1 时才允许 PA0。保底标志保留至获准新边沿，因此备份域仍在时掉压/看门狗复位会继续进 STOP。AFE无法通信时，仍不能保证其均衡/放电MOS/看门狗实际关闭，整板耗电须实测。

## 板上参数与真正生效入口

本次读到 TimeVlow=1分钟、Vlow=3000mV、idleMax=10秒。OtherElement 中 VNormal=3200、TimeNormal=7200、RTC_WakeUpTime=240、TimeRTC=3虽然可读取，但当前自动调度没有用这些字段选择周期；RTC.c 直接使用10/20秒常量。更改这些旧字段不等于更改当前RTC行为，应以实际调用链为准。

## 验证与交付边界

新增启动回归：Boot中AFE初始化后且开中断前恰好一次初始请求；第一次服务前不放行，第一次有效服务后双MOS开启，后续手动关闭不会被重开。24组合共504项驱动测试以及现有休眠/迁移/映射测试通过。Release/Debug均0错误、7条既有警告。

重要：当前Keil链接配置允许0xA000字节（到0x0800E800），而 Flash.h 和安全烧录脚本规定日志从0x0800E000开始，App上限实际是0x9800=38912字节。当前修复版Release BIN为39880字节，结束0x0800E3C8，超出安全范围968字节。安全脚本dry-run已拒绝，未烧录。之前文档用“未超过40KB”作为安全结论不充分，应以0x0800E000存储边界为准。

本次保留启动修复源码和成功编译结果，未移动日志/参数区、未扩大可烧录范围、未下载不安全镜像。Flash空间冲突仍需进一步压缩代码并收紧链接边界，当前BIN不能作为可安全发布产物。本次实板结果来自RAM请求验证，不能写成修复固件已完成上板验证。
