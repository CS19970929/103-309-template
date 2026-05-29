todo


调研行业保护板bms soc逻辑，如何保证整个使用过程soc准确，例如电池休眠后，用户长期静止不使用也需要保证soc准确，梳理目前项目soc逻辑，并看可能会有哪些问题，该如何优化解决，或者完全重写soc模块，不在目前基础上改

增加rtc，除过放休眠，永远rtc，方便soc策略，调研rtc、低功耗配置，休眠与唤醒


对于我来说使用云端codex方便吗，我一般都是做嵌入式保护板bms开发，我最烦的问题是低功耗和soc模块还有flash参数读写问题

code reivew整个项目和文档，看是否有bug，是否有需要优化的地方，能否优化整个架构，保证清晰简单、稳定,旧文档也需要同步更新
soc100逻辑需要调整，要求不能太严，例如有时候满足不了电流和时间，充电到不了100用户会觉得很奇怪
soc必须要保证能到0，同时要体验好，不能说到0后，实际电池还有很多电，也不能说实际没电了，soc还有很多

soc融合逻辑，充电有点快，待仔细测试，确认soc计算频率

需要实现每次rtc唤醒后，都发送一次can并保证发送成功，再进入rtc休眠
rtc_sleep_run_hiccup_cycle中的feidao_send_soc_1000ms();调用没有用了，can总线没收到

测试soc

RTC_IT_SEC中断和RTC_IT_ALR区别，RTC_IT_SEC能唤醒stop吗，我调试发现进入stop后，虽然被唤醒了，但好像没有进入	if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
	{
		RTC_HandleAlarmWakeup();
		sys_time.rtc_alm_cnt++;
	}

s_u8FeidaoCanPowerState有几率阻止进入rtc好像，
目前开了RTC_IT_SECz中断，不影响rtc stop

soc安时积分改成任意

datadeal中的g_stCellInfoReport.u16VCellTotle使用afe采样累加值，不要使用adc采样总压

修改g_u16TypeCOutCurrent_mA由直接g_u16TypeCOutDelta_mV计算，这样更快，不需要滤波，我调试发现没什么用，反而一直滞后于g_u16TypeCOutDelta_mV

e172988af06bfd92ed8e80b343907e0658373abd这次cimmit导致不能进入rtc，而且can没有之前的低功耗逻辑了，功耗一直很高
提交本次git，然后adc测量计算总压是怎么计算的，我怎么计算最终的电池总压，电路是一个分压电路，你帮我处理好，我后面自己调整分压电阻，并输出文档

adc测量typec电流是准的，但是断开typec充电后g_u16TypeCOutCurrent_mA还是之前测量的输出电流

 阅读readme
-  数码管好像有点闪烁感，然后充电图表偶尔会闪一下，进入rtc低功耗状态时，有时某些段数码管会无缘无故亮
-  soc校准，低压时容量直接校准为0，soc慢慢降？
- 梳理soc模块逻辑，并优化soc策略、计算、架构 （soc计算不依赖200ms
- 梳理休眠、低功耗逻辑，进入、退出条件，唤醒源等等，统一、简化休眠逻辑和软件框架的清晰,结合soc策略优化
- codex接管todo
- flash可靠性测试
- 简化软件架构、功能，不必要的功能去掉，保证代码、功能精简稳定，方便移植
- typeC具体逻辑
- adc current、vBat
- 异常电压不校准、降soc
- codex接管文档，没修改需要自动加入待测试文档

//已完成 
-  梳理项目中所有的宏，具体功能、怎么用，输出文档
- 梳理led休眠 mcu wk为高时，led不休眠

1、过充不允许进入rtc，rtc均衡？
2、

再详细梳理eeprom地址和逻辑，考虑能否优化eeprom读、写逻辑，保证安全、稳定、简单

所有模块的参数写操作后必须回读校验,例如eeprom、afe参数写


休眠(sleep or stop)

中断 wakeup
- 通信（uart、485、can）
- 开关
- tim、systick
- exti
- rtc sec、alarm

low power
- power
- 外设
- 


sleep mode下，虽然串口中断可以触发，但是sleep状态下，cpu stop很久的话，会丢data，必须设置进入sleep时间

sleep
- 无通信
- 无充放电
- 持续N秒

阅读charlie_595_display_framework，先阅读这个框架文档，然后code review整个框架，是否有问题、可优化点，然后移植到目前项目中，spi io定义在conf_gpio.h文件中，5 pin led定义在charlie_595_display_framework目录中的LedBar.c中，LedBar.c是我之前单独测试5 pin Charlieplexing控制数码管显示测试代码。当时没用74hc595，现在需要结合74hc595在当前项目中实现数码管显示soc、充电状态显示等等



todo
- 整体开关、休眠、数码管显示逻辑、低功耗（sleep、stop）？？？
- 双电池？
- soc 
- can结合低功耗？
- 74hc595 + 5pin ->> N * (N - 1) = 20pin是led数码管控制
- 分口

5 pin led不是直接由mcu控制的，而是74hc595的前5pin分别控制的，mcu只通过spi控制74hc595来控制数码管显示


IsSleepStartUp使用到了备份域寄存器，我从之前的030 mcu上复制过来的，但应该不适用103，帮我解决，并输出文档


理论分析通过74hc595控制5pin Charlieplexing能否完美实现数码管显示


梳理用到的所有外设、中断、备份域寄存器、flash地址范围、大小、ram，输出文档

目前项目没有eeprom了，我把InitE2PROM屏蔽了，一切和eeprom相关功能都需要修改了，使用内部flash替换，给出解决方案

在优化soc模块的同时，考虑在不影响功能的情况下，降低运行功耗，给出策略和具体程序实现，比如空闲时进入sleep

要保证flash擦写寿命，理论分析目前架构soc、日志、等等flash相关模块的寿命是否够用


给出适合保护板bms使用的soc架构，保证用户体验好，考虑自耗、用户长时间不使用，电流不一定准等等问题

梳理目前所有和存储相关部分代码，flash、备份域存储等等，是否会有风险，soc、日志等存储是否有用，是否会有风险

优化目前的串口通信模块，保证功能、modbus地址不变，可以完全重构架构，比如目前发送使用轮询效率太低，等等，同时考虑后续需要同时兼容对接用户的协议，需要方便兼容用户协议

没有日志？我测试过充、过放都没日志，其他我还没测试

串口是否有超时机制，比如先发送0x01,过一段时间再发送正常帧，能否正常通信

低功耗期间不会定时唤醒检测是否有充放电行为、保护状态、soc校准等等吗？那一直sleep不是会soc不准，甚至纯软件保护项目，不会触发保护吗


梳理LedBar模块逻辑、原理并输出文档

现在需要加入74hc595来间接控制ledbar中的5pin，来实现数码管显示逻辑，是否可行？
这个数码管是特殊的数码管，5pin控制，原理是Charlieplexing，能否实现

如果可行，conf_gpio.h中有74hc595的spi定义，

梳理整个项目系统的时基，能否优化、统一，给出方案

App_AnlogCal调用时基修改的话，是否会影响adc采样计算结果，例如修改到1s
优化目前架构、逻辑，方便我后续修改App_AnlogCal调用时基，不会影响adc计算结果

目前整个mcu资源是怎么分布的，使用了哪些外设、中断等等，能否优化整体架构,flash、ram、堆栈等等的使用，项目的读写参数flash存储区域范围是多少，不会对啊app程序和iap升级造成影响吧，后续app程序增大的话是否会有影响

能否完全去掉App_SysTime，同时降低定时器频率，保护板bms 各任务不需要这个快中断频率，
经常中断不是会退出sleep，反而降低不了功耗吗，整体考虑下，重构整个项目，同时要不影响目前的功能


stm32f0、f1串口异常有哪些？软件如何处理异常，使用标准库

梳理分析System_Monitor模块，特别是其中的各个变量在整个项目中作用，有些变量应该已经没用了，老项目

Fault模块中App_CellOvp_FirstCheck这些都是软件板保护逻辑，目前这个项目是纯硬件保护方案，目前项目不需要这些逻辑,因此可以考虑整体优化方案


如果现在通过升级程序修改afe保护参数、soc模块参数，该如何做，可以实现吗

u32E2P_Pro_VolCur_WriteFlag这类变量还有用吗？没用的话，全部删除

目前哪些参数是可以写存储、修改到flash的

写测试程序，确保后64Kflash 读、写可靠性

我会中途故意断电测试，看怎么加日志，例如开机日志用于提示是否有问题

PRT_E2ROMParas,OtherElement,Heat_Cool_Element需要可读写

休眠后，用户一按开关需要立即数码管显示当前soc，长按则开机，未长按则继续休眠,然后数码管平时也不显示，只有用户单击开关，显示5s，然后继续不显示，然后直接把App_DI1_Switch逻辑集成到ledbar中


检查上位机设置参数，一二级过流等等是否正确

第一次烧录程序的板子的soc模块各变量初始值是多少，后续烧程序可以修改吗，梳理整个soc模块逻辑并优化、简化，不必要的可以删掉


todo 待测试
- 休眠

需要实现通过升级程序可控制历史 SOC 快照更新


梳理所有的休眠、低功耗逻辑，输出文档，并考虑能否优化、简化

目前的MainLoop_EnterIdleSleep是否有用

看App_Can，GPIO_CMNT_EN是控制给can供电的，我想通过控制供电降低can功耗，发送前给电，发送完断电，但是目前用的是__delay_ms阻塞，另一个问题是不清楚这个延时到底多久不影响can收发器芯片正常工作，还有一个问题，can一直发送，不接通信设备时，功耗会高，接了通信设备，功耗会降低,然后发送周期需要调整下，根据feidao_send_volage_current_1000ms名字调整，发送具体函数名字都体现了我的发送周期，一起调整下


使用调试器就会一直卡在RTC_WaitForSynchro，正常上电好像不会

提交之前修改git，然后实现：空闲时rtc休眠可自动调整，例如当检测到总线上有其他设备时，rtc休眠1s，然后can发送逻辑还需要保证正常发送逻辑和时序，时间尽可能保证正确，当总线没有其他设备时，rtc休眠10s，不需要发送can

梳理soc模块逻辑，并优化soc策略、计算、软件架构等等
梳理休眠、低功耗逻辑，进入、退出条件，唤醒源等等，统一、简化休眠逻辑


can总线有设备时，中途断开总线其他can设备，没有切换到10s休眠，然后没有设备时，中途

还是不行，can总线有设备时，进入了10s rtc休眠，没有发送can通信，然后唤醒退出rtc低功耗后，can永远不会发送了

目前问题：can总线上有设备时，总线上还是收不到bms的can广播，以下是我的具体需求，你可以看项目中其他相关文档
CAN通信和RTC低功耗需求整理：
- 空闲时允许进入RTC低功耗，核心目标是保证RTC休眠功耗低，同时CAN通信功能和时序正常。
- CAN收发器由GPIO_CMNT_EN控制供电，发送前上电，等待硬件稳定后再发，发送完成、失败或超时后及时断电，避免CAN空转耗电。
- 上电或总线状态未知时，先按“可能有CAN设备”处理；检测到CAN发送成功ACK或RX收到报文后，认为总线上有其它设备。
- 总线上有其它CAN设备时，RTC休眠周期为1s；每次RTC唤醒后应恢复CAN供电并发送到期的1s/5s周期报文，尽量保证原有发送节奏和时间正确。
- 总线上没有其它CAN设备时，RTC休眠周期切换为10s；不发送完整业务CAN报文，只保留轻量探测，降低低功耗期间CAN发送功耗。
- 有设备运行中如果中途断开其它CAN设备，需要通过连续无ACK、发送失败或超时判定切换到10s RTC休眠。
- 无设备10s RTC休眠期间如果中途接入CAN设备，软件必须能通过探测帧重新发现设备；任一探测帧发送成功ACK或RX收到报文后，应恢复1s休眠和正常CAN周期发送。
- 退出RTC低功耗后，CAN不能进入永久不发送状态；即使已经判定无设备，普通主循环也要保留周期性探测路径。
- 判断策略可以冗余，例如多帧探测、多次失败阈值、发送窗口超时和供电稳定等待，但最终状态必须正确收敛：有设备正常通信，无设备低功耗，中途接入/断开都能切换。
- 调试器下RTC_WaitForSynchro不能死等，需要超时和RTC时钟异常恢复，避免调试复位后卡死。

数码管控制方案改了，不用74hc芯片了，直接用mcu gpio控制，现在数码管显示可以完美了吧，先输出我的需求，新方案gpio定义，然后之前的74hc方案代码先保留，然后使用新的gpio方案实现，dbled、spi nss、spi sck、spi mosi、seg en这几个gpio分别用于控制查理复用原理的5pin数码管，数码管之前我测试过没问题，测试代码在根目录LedBar.c .h中

typeC具体逻辑

进入低功耗，有时候数码管还有部分段亮着

目前的adc是怎么配置的，分别测量什么的，我怎么计算最终结果

led平时长灭，短按开关，显示soc 5秒，长按开关3s分别进入休眠、开机，休眠状态下，短按开关，显示soc 3秒，长按才进入开机正常状态，充电时，进入充电跑马动画，放电长灭

两个开关，灯板一个开关pa6，还有一个总开关pa9逻辑没写，总开关断开进入深度休眠，总开关

can通信是大端模式还是小端模式，BAT_MASTER和BAT_SLAVE对can通信有什么区别影响

充电判断使用充电电流

总开关和led上的开关需求逻辑：总开关断开，进入休眠，led开关不允许唤醒，总开关闭合，退出休眠。总开关闭合情况下，led开关可长按进入休眠，长按退出休眠。两个开关闭合都是低电平，通过中断边沿来退出休眠。

充电动画不是这种，是：亮当前soc，闪烁即将充满的soc

充电使用5V，而不是

问题1：休眠后，单击soc开关就退出休眠了，需要实现的是休眠后，单击soc开关，看soc但不退出休眠，长按才退出休眠，CHG_IN充电可以直接退出休眠。问题2：长按soc开关休眠的时间太长了，不像3s，是不是有什么影响导致变慢













请使用 subagent 工作流，围绕当前 STM32 BMS 项目调研 RTC 低功耗相关资料、行业/官方推荐架构，并对比当前项目用法，最后给出优化方案。

背景：
我是 BMS/嵌入式软件工程师，项目常用 STM32F0/F1，优先使用标准外设库或寄存器，不使用 HAL。项目涉及 AFE 采样、SOC、保护、均衡、CAN、Modbus、RS485、Flash 参数/日志存储、LED/Charlieplexing 显示、IWDG、RTC、低功耗和任务调度。

当前目标：
为当前项目设计并逐步实现一个可复用的 RTC 低功耗框架，适合 BMS 保护板使用，后续可移植到不同 STM32F0/F1 项目。

重要要求：
1. 第一阶段只读分析，不要修改代码。
2. 先查资料，再看当前项目代码，再做差异对比。
3. 所有结论必须有依据：要么引用官方资料/手册/应用笔记，要么引用当前项目的具体文件和函数。
4. 不允许泛泛而谈。
5. 不要一开始追求最低电流，先追求稳定睡眠、稳定唤醒、通信不乱、保护不丢、IWDG 不误复位。
6. 不破坏现有 Modbus/CAN 协议。
7. 不破坏现有 SOC、保护、AFE、Flash、LED 功能。
8. 所有关键分析和修改都必须生成文档。

请 Spawn 以下 subagents：

1. ResearchAgent
   任务：
   - 查 STM32F0/F1 RTC、Stop、Standby、Sleep、IWDG、SysTick、LSE/LSI、Wakeup/Alarm 相关官方资料。
   - 优先查 ST 官方 Reference Manual、Application Note、Datasheet。
   - 整理和当前 BMS 项目相关的规则，不要全文总结。
   输出：
   - docs/research/stm32_low_power_research.md
   - docs/research/rtc_stop_standby_rules.md

2. IndustryArchitectureAgent
   任务：
   - 总结嵌入式低功耗固件的常见架构。
   - 总结 BMS 保护板低功耗推荐架构。
   - 重点分析状态机、外设休眠恢复、RTC 周期唤醒、IWDG 兼容、通信禁止休眠、Flash 写入禁止休眠。
   输出：
   - docs/architecture/low_power_industry_architecture.md

3. CurrentProjectAgent
   任务：
   - 扫描当前项目中和低功耗相关的代码。
   - 包括但不限于：RTC、PWR、RCC、SysTick、TIM、IWDG、ADC、CAN、UART、Modbus、AFE、SOC、Flash、LED。
   - 找出当前项目是否已有低功耗逻辑、RTC 初始化、IWDG 初始化、SysTick 定时框架、任务调度框架。
   输出：
   - docs/current/low_power_current_usage.md
   - docs/current/mcu_resource_related_to_low_power.md

4. RtcAgent
   任务：
   - 分析当前项目 RTC 是否使用。
   - 判断当前 MCU 是 F0 还是 F1，分别说明 RTC 实现方式。
   - 如果是 STM32F1，重点检查 RTC Alarm、EXTI Line17、Backup Domain、LSE/LSI、RTC_WaitForSynchro 是否可能卡死。
   - 如果是 STM32F0，重点检查 RTC Wakeup Timer、Alarm、EXTI、LSE/LSI。
   输出：
   - docs/current/rtc_usage_analysis.md
   - docs/design/rtc_wakeup_design.md

5. ClockAgent
   任务：
   - 分析当前项目系统时钟初始化。
   - 判断 Stop 唤醒后是否需要恢复 HSE/PLL/SYSCLK/AHB/APB。
   - 找出当前项目是否已有 SystemClock_Config、SystemInit 或自定义 Clock_ReConfigAfterStop。
   输出：
   - docs/current/clock_usage_analysis.md
   - docs/design/clock_restore_after_stop.md

6. IwdgAgent
   任务：
   - 分析当前项目 IWDG 使用方式。
   - 判断 IWDG 超时时间、喂狗位置、低功耗期间是否会导致误复位。
   - 给出 RTC 唤醒周期与 IWDG 超时时间的安全关系。
   输出：
   - docs/current/iwdg_usage_analysis.md
   - docs/design/iwdg_low_power_strategy.md

7. PeripheralAgent
   任务：
   - 分析 SysTick、TIM、ADC、UART、CAN、LED、AFE 在休眠前和唤醒后的处理。
   - 判断哪些外设需要关闭，哪些需要保持，哪些需要重新初始化。
   - 判断通信活跃时是否应该禁止进入 Stop。
   输出：
   - docs/current/peripheral_sleep_analysis.md
   - docs/design/peripheral_sleep_resume_plan.md

8. BmsLogicAgent
   任务：
   - 分析 BMS 业务层对低功耗的约束。
   - 包括保护状态、MOS 状态、AFE 状态、SOC 静置校准、Flash 参数保存、日志保存、过放深度休眠。
   - 判断哪些状态禁止休眠，哪些状态应该进入深度低功耗。
   输出：
   - docs/design/bms_low_power_state_machine.md
   - docs/design/low_power_block_reason.md

9. RiskAgent
   任务：
   - 汇总低功耗改造风险。
   - 包括唤醒失败、时钟未恢复、通信异常、IWDG 误复位、Flash 写入中断、SOC 时间丢失、AFE 状态不同步、MOS 状态错误。
   - 按 P0/P1/P2 排序。
   输出：
   - docs/risk/low_power_risk_list.md

10. TestAgent
    任务：
    - 生成低功耗测试矩阵。
    - 必须覆盖：RTC 唤醒、Stop 进入退出、IWDG、SysTick、CAN、Modbus、ADC、AFE、SOC、Flash、LED、过放保护、充电唤醒、通信活跃禁止休眠。
    输出：
    - docs/test/low_power_test_matrix.md
    - docs/test/low_power_manual_test_steps.md

11. DocsAgent
    任务：
    - 汇总所有 subagent 结果。
    - 生成最终设计文档、迁移计划、变更记录。
    输出：
    - docs/low_power_rtc_final_report.md
    - docs/low_power_rtc_migration_plan.md
    - docs/low_power_rtc_change_log.md

阶段划分：

第一阶段：只读分析
- 不修改任何源码。
- 输出当前项目用法、官方资料要点、差异分析、风险清单。

第二阶段：设计方案
- 给出最小可行架构。
- 明确新增哪些模块、修改哪些文件、不修改哪些文件。
- 输出接口设计和状态机设计。

第三阶段：最小实现
在我确认后，再允许修改代码。
优先新增以下模块：
- bsp_rtc.c / bsp_rtc.h
- bsp_power.c / bsp_power.h
- bsp_clock.c / bsp_clock.h
- app_lowpower.c / app_lowpower.h

建议接口：
- void LP_Init(void);
- void LP_Task(void);
- uint8_t LP_CanSleep(void);
- uint32_t LP_GetBlockReason(void);
- void LP_SetWakeupPeriod(uint32_t seconds);
- void LP_EnterStop(uint32_t seconds);
- void LP_BeforeSleep(void);
- void LP_AfterWakeup(void);
- uint32_t LP_GetLastSleepSeconds(void);

低功耗状态机建议：
- LP_STATE_RUN
- LP_STATE_IDLE_CHECK
- LP_STATE_PREPARE_SLEEP
- LP_STATE_STOP_SLEEP
- LP_STATE_WAKEUP_RESTORE
- LP_STATE_DEEP_STANDBY
- LP_STATE_ERROR

禁止休眠原因位图建议：
- LP_BLOCK_CHARGE
- LP_BLOCK_DISCHARGE
- LP_BLOCK_COMM
- LP_BLOCK_KEY
- LP_BLOCK_AFE_BUSY
- LP_BLOCK_FLASH_BUSY
- LP_BLOCK_UPGRADE
- LP_BLOCK_FAULT
- LP_BLOCK_LED_ACTIVE
- LP_BLOCK_IWDG_UNSAFE

优化原则：
1. 先做 Stop + RTC 周期唤醒。
2. 第一版不做 CAN/USART Stop 唤醒。
3. 通信活跃时禁止休眠。
4. Flash 擦写或参数保存未完成时禁止休眠。
5. RTC 唤醒周期必须小于 IWDG 超时时间，除非明确进入复位式 Standby 策略。
6. 唤醒后必须恢复系统时钟，再恢复 SysTick、TIM、ADC、UART、CAN、LED。
7. SOC 要记录休眠时间，用于静置时间和 OCV 校准。
8. AFE/MOS/保护状态唤醒后必须重新同步。
9. 每一步修改后必须更新文档。
10. 不要大规模重构无关模块。

最终输出请包含：
1. 官方资料结论摘要
2. 当前项目低功耗相关代码位置
3. 当前项目和推荐架构的差异
4. P0/P1/P2 风险清单
5. 最小优化方案
6. 后续增强方案
7. 测试矩阵
8. 建议修改文件清单
9. 不建议现在修改的内容





































comm tool用串口升级，会卡住，升级不成功,串口升级用的协议和上位机是和bms app一样的，就是波特率改成115200了


老化模式定义：老化模式剩余时间未到时，enter_fac_mode(true)自动打开充放电，不需要GPIO_CHG_IN 5V识别信号，有5V充电信号时，打开充电关闭放电，冲洗重新梳理需求，并实现逻辑，顺便看能否简化FactoryAging模块


[16:37:40] 缓存校验通过，开始 CAN 升级 BMS
[16:37:40] 升级状态: state=1 percent=0% error=0x00 written=0/63212 expect_seq=0
[16:37:45] 一键升级 失败: 升级状态: state=3 percent=0% error=0x02 written=0/63212 expect_seq=0

一键升级过程中，在将bin文件烧录到comm tool期间，bms会进入rtc，导致升级失败

开关断开休眠后，到底是什么休眠？？？怎么好像是rtc休眠，灯板开关按一下为什么会休眠？？？
串口唤醒了好像，又进主循环了

在can上位机其他所有页中，人没有操作的时候，默认每一页是否有和bms保持通信帧，bms是否会进入rtc

soc key预览led，但不能唤醒，也没毛病，完整检查一遍逻辑