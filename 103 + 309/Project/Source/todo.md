tod

梳理can、rtc、soc，整体架构优化


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

目前数码管部分代码太过复杂，梳理数码管需求，完全重写数码管逻辑

梳理项目所有变量及其作用



重要变量都做成全局变量怎么样？方便在keil中加入watch在线调试、观察
SOC_Enhance_Element
g_stCellInfoReport
s_soc

直接对当前soc模块代码进行各种场景的测试，soc逻辑是否没问题，是否准确，各种校准是否有用，要直接对当前代码进行模拟测试


soc不同场景校准测试

高压 低soc
低压 高soc
评估目前项目flash磨损寿命

修改PROJECT_CFG_SOC_REST_OCV_SECONDS 为60秒，超过60秒后，为什么没校准

直接用当前soc模块代码，完整测试一遍，自耗的加入是否会影响其他逻辑，校准等等，是否会导致体验变差

soc表调整，用户体验好，soc静置校准时不要

soc ocv数据
30%
20%
10%
5%

梳理待机逻辑，soc校准等等，抽象，可移植性

待机测试、优化


10.4Ah 电池   5A放电   静置
3975   3973    80
3926   3924    75
3871   3870    70
3786   3784    60
3690   3688    50
3658   3647    45
3636   3634    40
3617    3616   35
3603    3601   30
3587    3586   25

在不影响功能的前提下，整体代码架构优化、精简，方便人阅读、调试

优化、简化led数码管逻辑、代码，数码管偶尔还会闪烁，充电标志偶尔无缘无故亮一下


需要修改部分：
1、从休眠状态下，5V充电，充满后，断开充电器，不能开放电，要直接休眠。
2、数码管充电指示图标需要增加功能，当电池放电管开着，有输出时，充电指示图标亮。

处于工厂模式状态时，过放休眠没有用了？这是不允许的，电压低于2600必须要休眠

看todo.md文档中的todo优化一节，先给出规划，然后继续

todo
can通讯唤醒


todo优化
完整讲述如何阅读map文件，优化整个项目，例如thumb code和data的区别

在不影响功能前提下，优化整个项目，资源占用、软件架构等等

串口1、2、3在可视化配置中可以配置，禁用后也要禁用相关的变量，这样可以减少code大小？afe的电压、电流k、b校准等都不需要了，需要全部去掉



去掉所有没有使用到的变量和函数

SOC_Table_Set和SOC_Table_Default也没用，soc校准表格在编译时就要确定，磷酸铁锂和三元锂宏定义来确定

优化afe零电流校准，去掉不需要的变量，只保留实现必须的，在不影响功能前提简化逻辑

Heat_Cool和IODrivers用不到，全部删除

删除用不到的宏、变量、函数，包括实际没有意义用不到的

完整梳理rtc sleep。唤醒后配置都对吗？？？

根据map文件，优化项目，在不影响功能前提下，可以去掉不重要、没用的功能，先给出规划


阅读map文件，并结合项目源码，优化整个项目，不影响功能的前提下，大幅减小code大小，资源占用，优化整个软件架构，方便人阅读

去掉Can_BusOFF_Monitor，使能AN_InitStructure.CAN_ABOM,怎么样？是否有问题













afe
soc
rtc
sleep
adc
can
uart
led


























在不影响功能前提下，继续
能否去掉SeriesSelect_AFE1

在不影响功能前提下，分别去优化soc、led、can、工厂模式等等占据code大的模块



完全重构整个项目与各个模块，优化软件架构，方便阅读，并减小code

soc.c和SocEnhance.c合并，同时优化，并去除没用的中间变量

梳理老化模式需求，为什么老化模式代码这么多、复杂，简化老化模式模块，减小code同时方便移植

要梳理我的项目根本需求,各个模块的需求，从整个框架、各模块上根本重构，来优化架构、方便人阅读管理，并以减小code为第一优先级


梳理数码管led、老化模式、soc、零电流校准、can各模块根本需求，目前都太复杂了，在不影响功能前提下，以减小code为第一优先级

还是太复杂，code占用太大，逻辑太复杂,需要从根本上重构，例如去掉不必要的变量、函数























实现一个comm tool



需要完全重构一版程序，需求：1、实现comm tool，能够和pc通过串口连接，通过can和当前项目bms连接，pc可以读、写bms信息，pc可以下载待升级bms程序到comm tool，comm tool可以通过can给bms升级。comm tool暂定使用stm32f1ret6，flash大可以直接存储待升级程序，通过comm tool可以一键给bms升级。
2、需要完全重构目前bms，提取关键功能，不必要的功能、逻辑都可以去掉，保证bms程序简单、稳定、减小code大小，加入can升级功能，除了串口通信协议不变，其他iap地址等等都可以变，完全重构的一版，不需要考虑以前iap怎么实现，iap也要完全重构，支持can升级
3、实现pc上位机工具，可以通过串口读取comm tool来读、写bms信息、保护参数等等，可以将bms升级程序写入comm tool
mcu都使用标准库实现，要求简单、稳定，升级保证不死机，先规划，设计开发过程中都需要对应文档，比如串口协议、can升级协议等等






需求：1、实现comm tool，能够和pc通过串口连接，通过can和当前项目bms连接，pc可以读、写bms信息，pc可以下载待升级bms程序到comm tool，comm tool可以通过can给bms升级。comm tool暂定使用stm32f1ret6，flash大可以直接存储待升级程序，通过comm tool可以一键给bms升级。
2、bms中加入can升级功能，iap地址等等都可以变，不需要考虑以前iap怎么实现，iap也要完全重构，支持can升级，iap工程目录E:\work\a002\new 030\IAP 103CB ,单独开分支实现新的iap，重构iap实现can升级，保证升级稳定，不死机，pc和comm tool、comm tool和bms的can读写、升级协议你自己决定
3、实现pc上位机工具，可以通过串口读取comm tool来读、写bms信息、保护参数等等，可以将bms升级程序写入comm tool
mcu都使用标准库实现，要求简单、稳定，升级保证不死机，先规划，设计开发过程中都需要对应文档，比如串口协议、can升级协议等等






Project_Config.h显示乱码了，keil工程中可视化也是






comm tool建立keil工程了吗？全部需求都实现了吗，我可以测试？具体怎么用





当前项目bms保护完全有afe硬件做的，能否优化一些地方，例如rtc待机逻辑，越简单越好，保证功能正常，soc准确

bms app中需要加入can协议和comm tool通信，pc可以通过串口和comm tool读、写bms的信息、保护参数等等

pc上位机实现了吗

目前iap升级逻辑是什么？if ((FlashReadOneHalfWord(FLASH_ADDR_UPDATE_FLAG) == FLASH_TO_APP_VALUE) &&
		(CanIap_IsValidAppVector(FLASH_ADDR_APP_START, CAN_IAP_APP_LIMIT_ADDR) != 0U))
	{
		IAP_To_APP_Jump(); // 跳回去不能开各种中断或者初始化，也即下面的初始化不能放上来
	}这段逻辑是什么？需要串口升级和can都能升级。通过这种flash跳转逻辑感觉不好，能否使用其他方式，这是新项目，app和iap除了串口升级协议不能修改，其他跳转逻辑、方式都能修改


[已完成]
把comm tool补全keil工程，参考E:\TODO\code\c058 from c030工程，这个是ret6，串口默认使用串口3，把debug led也增加过来，其他不要加，串口、can驱动、供电配置配置过来就行。


1、app跳转iap升级必须通过flash吗？这样浪费1Kflash，不太好，是否有其他更好的方法，要求稳定、不死机。2、之前的串口和现在的can需要单独能够升级

[todo]
全部需求完成了吗，我先测试一下comm tool给bms升级，给出具体使用步骤

iap和app的升级逻辑是怎样的，对比主流方式，给出你的评价，根据你对我的需求了解，能否优化，保证稳定好用


"App 请求进 IAP 改成 SRAM mailbox，不再擦 0x0801F800；IAP 的断电保护改成“擦首页、缓存首页、首页最后写、MSP 最后写”。我现在做静态检查和 Keil 编译，重点确认串口和 CAN 两条升级路径都能独立编译通过。"这句话什么意思，sram mailbox是什么

先确认下iap和comm tool的can配置是否有问题，iap和comm tool都加一个心跳报文是否可以，我总线上有can通讯盒，可以监测

py -3.9 tools\comm_tool_host.py info --port COM4 --baud 115200
py -3.9 tools\comm_tool_host.py can-diag --port COM4 --baud 115200 --clear
py -3.9 tools\comm_tool_host.py upgrade --port COM4 --baud 115200 --long-timeout 60 --confirm-upgrade
py -3.9 tools\comm_tool_host.py upgrade-status --port COM4 --baud 115200
py -3.9 tools\comm_tool_host.py can-diag --port COM4 --baud 115200

当前电脑有创芯科技的usb can tool，已经打开了



py -3.9 tools\comm_tool_host.py info --port COM4 --baud 115200
py -3.9 tools\comm_tool_host.py fw-info --port COM4 --baud 115200
py -3.9 tools\comm_tool_host.py can-diag --port COM4 --baud 115200 --clear
py -3.9 tools\comm_tool_host.py upgrade --port COM4 --baud 115200 --long-timeout 120 --confirm-upgrade
py -3.9 tools\comm_tool_host.py upgrade-status --port COM4 --baud 115200
py -3.9 tools\comm_tool_host.py bms-read --port COM4 --baud 115200 --address 0xD000 --count 2 --long-timeout 10


进行不同状态升级测试

comm tool也要加入iap升级功能，可以给comm tool升级，串口协议共用目前的bms，可这样可以复用之前的串口上位机，也可以通过其他的comm tool给当前comm tool升级

加入读、写bms功能
根据文档，加入适配飞道协议

每次给bms一键升级，都需要先把程序下载烧录到comm tool？那我批量升级，这样效率是否太低了


分析这个ui日志“[13:40:36] 开始: 连接检测
[13:40:36] 连接检测 完成
[13:40:38] 开始: 使用缓存升级
[13:40:38] 缓存与当前文件一致，跳过串口下载
[13:40:38] 缓存校验通过，开始 CAN 升级 BMS
[13:40:46] 升级状态: state=2 percent=100% error=0x00 written=49132/49132 expect_seq=6142
[13:40:48] 等待 BMS App 恢复响应: 第 1 次未响应
[13:40:49] BMS App 第 2 次确认成功
[13:40:49] BMS App 状态: SOC=14 SOH=100
[13:40:49] 使用缓存升级 完成14:11:22] 写入缓存: 204/204
[14:11:23] 缓存校验通过，开始 CAN 升级 BMS
[14:11:29] 升级状态: state=2 percent=100% error=0x00 written=51988/51988 expect_seq=6499
[14:11:31] 等待 BMS App 恢复响应: 第 1 次未响应
[14:11:32] BMS App 第 2 次确认成功
[14:11:32] BMS App 状态: SOC=60 SOH=100
[14:11:32] 一键升级 完成
[14:11:35] 开始: 读取BMS信息
[14:11:37] 读取BMS信息 失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/写权限关闭)

”
能否优化升级速度、流程，同时完整进行不同状态、模式进行升级测试，保证comm tool、bms iap、app升级可靠性,用户可以傻瓜使用而不出问题,Project_Config.h每次你修改后都会乱码，keil可视化配置也乱码
ui上位机需要实时查看bms信息，每串电压、电流、soc、温度等等信息，可以单独开一个界面

bms app debug和release有什么区别
给bms app加入完整、重要的日志，用于debug长期检测，使用什么方式来显示日志？串口？效率太低了，是否有其他办法稳定、长期检测

1、完整review bms iap各种情况下是否会有bug，必须保证升级可靠性，iap绝对不能有问题。
2、comm tool加入iap，可以通过串口、can升级，串口使用和bms同样协议，可以复用之前的上位机升级，can协议可以使用目前通用协议，可以用其他comm tool给当前comm tool来升级

can总线上有多个设备，是否会有问题,ui上位机优化用户体验，例如升级过程加入升级进度等等

上位机单独加入一页，用于测试飞道can协议是否正确，对照飞道can文档，review bms app中的can协议实现是否有问题，同时实现上位机和comm tool



先不考虑can低功耗相关，完全重构can模块程序，先保证各功能正常、架构清晰简单、稳定，方便阅读


PROJECT_CFG_HOST_WRITE_ENABLE默认要为1，release也需要


帮我快速熟悉iap、app、comm tool三者架构、代码


review下目前的soc模块代码，怎么现在静置状态soc疯狂往下掉


同时重新看之前的串口上位机逻辑，保护参数界面有两种，不同afe的保护参数界面、逻辑不一样，目前是sh367309，界面是我截图这种，目前上位机界面是ti bq系列的界面


在不影响功能的前提下，保持功能稳定，能否优化整体的通讯速度、升级速度、用户体验等等，目前的速度限制在哪儿

仔细看一下串口上位机中的309参数列表怎么生成的，目前上位机有些参数列表生成的是错的同时要对应和bms app读写协议、单位等等是否正确，保证整个参数读、写不能出问题

comm tool的iap和app是两个独立的keil工程吗？怎么管理更好

上位机加入功能：电池信息长期监控记录到excel文件，参考之前的串口上位机

comm tool的iap闪灯频率设置500ms,app为200ms，方便看是什么状态，还有目前给comm tool串口升级会卡到中途停止


你现在作为我的嵌入式 AI 工程架构助手，请基于当前项目，为我设计并落地一套适合 BMS/STM32 项目的：

Codex + Skill + MCP + ST-Link 自动化开发/烧录/测试体系。

我的背景和偏好：
- 我是嵌入式/BMS 软件工程师。
- 常用 STM32F0/F1，尤其 STM32F103、STM32F0 系列。
- 优先使用 STM32 标准外设库 StdPeriph，不使用 HAL，除非当前项目已经使用 HAL。
- 常用 Keil、VS Code、Git、Codex。
- 项目常涉及 BMS、SOC、ADC、CAN、Modbus、RS485、低功耗、IAP/APP 升级、日志、LED、保护逻辑等模块。
- 我希望 AI 不只是写零散代码，而是可以逐步接管工程分析、修改、编译、烧录、测试、文档记录。
- 所有关键修改都必须同步生成文档，包括设计说明、变更记录、测试记录、风险说明。
- 不希望为了 AI 工程化而制造复杂度。
- 一切必须以当前项目源码和实际目录结构为准。

本次任务目标：
请你完整分析当前项目，并设计/创建一套可落地的 AI 工程化结构，使 Codex 可以通过 Skill 固化工作流，通过 MCP 或 Shell 调用外部工具，最终支持 ST-Link 烧录、复位、读芯片信息、读取 Flash、基础硬件验证和文档生成。

重要边界：
1. 不要直接大规模修改业务代码。
2. 不要默认执行擦全片、写 Option Bytes、修改 RDP 读保护、量产板烧录等高风险动作。
3. 所有危险动作必须设计为“需要人工确认”。
4. 本阶段优先生成架构、配置、Skill、文档、脚本框架。
5. 如果当前环境无法实际连接 ST-Link，也要生成可执行的本地脚本和使用说明。

请按以下步骤执行。

============================================================
一、先分析项目
============================================================

请先扫描当前项目目录，识别：

1. 源码结构
   - APP 代码
   - IAP/Bootloader 代码
   - BSP
   - Driver
   - Module
   - Communication
   - SOC
   - CAN
   - ADC
   - LED
   - LowPower
   - Storage/Flash
   - Log
   - Modbus/RS485

2. 构建方式
   - 是否是 Keil 工程
   - 是否有 .uvprojx / .uvoptx
   - 是否有 Makefile / CMakeLists.txt
   - 是否已有 build 输出目录
   - 是否已有 hex/bin/elf 输出文件

3. 文档结构
   - 是否已有 docs
   - 是否已有变更记录
   - 是否已有测试记录
   - 是否已有协议文档
   - 是否已有模块设计文档

4. AI 工程化文件
   - 是否已有 AGENTS.md
   - 是否已有 .codex/config.toml
   - 是否已有 .codex/skills
   - 是否已有 MCP 配置
   - 是否已有自动化脚本

分析完成后，先输出一份：
docs/ai_workflow/project_ai_readiness_report.md

内容包括：
- 当前项目结构概览
- 当前是否适合引入 Skill
- 当前是否适合引入 MCP
- 当前是否适合接入 ST-Link 自动烧录
- 当前主要风险
- 建议的第一阶段落地范围

============================================================
二、明确 MCP、Skill、ST-Link 的分工
============================================================

请在文档中明确：

1. Skill 的作用
Skill 用来沉淀固定工作流，例如：
- BMS 架构审查
- SOC 模块修改
- CAN 协议修改
- ADC 采样扩展
- LED 显示修改
- 低功耗检查
- IAP/APP 升级检查
- ST-Link 烧录测试流程
- 文档同步流程

2. MCP 的作用
MCP 用来连接外部工具和系统，例如：
- filesystem
- git
- shell
- build tool
- STM32_Programmer_CLI
- OpenOCD
- pyOCD
- serial tool
- CAN tool
- NAS
- 测试台

3. ST-Link 的作用
ST-Link 只作为实际硬件调试/烧录接口，不是 Skill，也不是 MCP。
它应由 MCP 或 Shell 工具间接调用。

推荐关系：

Codex
  ├─ Skill：定义流程、规范、风险控制、文档输出
  ├─ MCP/Shell：调用外部命令和设备工具
  └─ ST-Link：连接 STM32 硬件，执行烧录/复位/读取

请把这部分写入：
docs/ai_workflow/mcp_skill_stlink_design.md

============================================================
三、创建或更新 AGENTS.md
============================================================

请创建或更新项目根目录下的 AGENTS.md。

AGENTS.md 必须包含以下规则：

1. 项目定位
- 本项目是 STM32/BMS 嵌入式项目。
- 修改必须以稳定性、安全性、可维护性为优先。

2. 代码风格
- 优先遵循现有代码风格。
- 不要无依据引入复杂抽象。
- 不要把简单逻辑过度框架化。
- 模块应保持高内聚、低耦合。

3. STM32 约束
- 优先使用标准外设库 StdPeriph。
- 不主动引入 HAL。
- 避免破坏中断、定时器、低功耗、通信时序。
- 修改 SysTick、TIM、RTC、IWDG、CAN、USART、I2C、Flash 等底层模块时必须说明风险。

4. BMS 约束
- SOC、保护逻辑、MOS 控制、AFE 通信、CAN、Modbus、低功耗、日志是关键模块。
- 修改关键模块前必须先梳理现有逻辑。
- 保护逻辑不能只看单点代码，必须考虑状态机、滤波时间、恢复条件、通信上报、日志记录。

5. 文档要求
每次关键修改必须同步更新：
- docs/change_log/
- docs/module_design/
- docs/test_record/
- docs/risk_review/

6. ST-Link 安全要求
- 默认禁止擦全片。
- 默认禁止写 Option Bytes。
- 默认禁止修改 RDP。
- 默认禁止对量产板执行自动烧录。
- 烧录前必须确认目标芯片、固件路径、地址。
- 烧录后必须校验并复位。
- 每次烧录必须生成测试记录。

7. Codex 行为要求
- 不要直接大规模重构。
- 先分析，再提出方案，再局部修改。
- 修改后必须说明影响范围。
- 如果无法编译或无法连接硬件，必须如实说明，不得假装成功。

============================================================
四、设计 .codex 目录结构
============================================================

请创建以下结构，若已有则合并更新：

.codex/
  README.md
  config.example.toml
  prompts/
    project_review.md
    module_refactor.md
    stlink_flash_test.md
    doc_sync.md
  skills/
    bms-architecture-review/
      SKILL.md
    bms-module-refactor/
      SKILL.md
    bms-doc-sync/
      SKILL.md
    bms-communication/
      SKILL.md
    bms-iap-bootloader/
      SKILL.md
    bms-low-power/
      SKILL.md
    bms-stlink-flash-test/
      SKILL.md
  mcp/
    README.md
    recommended_servers.md
    stlink_mcp_design.md

注意：
- 不要强行写死与当前项目不匹配的路径。
- 如果项目已有类似结构，优先兼容。
- 所有文档用中文。

============================================================
五、创建 bms-stlink-flash-test Skill
============================================================

请重点创建：

.codex/skills/bms-stlink-flash-test/SKILL.md

这个 Skill 用来指导 Codex 进行 ST-Link 烧录和基础硬件验证。

内容必须包括：

1. 适用场景
- 编译后烧录 STM32F0/F1/F103 固件
- 读取芯片 ID
- 校验 Flash
- 复位目标板
- 生成烧录测试记录
- 结合串口/CAN 做基础验证

2. 默认工具优先级
优先使用：
- STM32_Programmer_CLI

备选：
- OpenOCD
- pyOCD
- ST-LINK_CLI，旧工具，仅在项目已有依赖时使用

3. 推荐命令模板
请生成跨平台示例，但不要假设路径一定存在。

示例一：连接 ST-Link
STM32_Programmer_CLI -c port=SWD

示例二：读取芯片信息
STM32_Programmer_CLI -c port=SWD -r32 0x1FFFF7E8 3

示例三：烧录 hex
STM32_Programmer_CLI -c port=SWD -w <firmware.hex> -v -rst

示例四：烧录 bin 到指定地址
STM32_Programmer_CLI -c port=SWD -w <firmware.bin> 0x08000000 -v -rst

示例五：读取 Flash 前 256 字节
STM32_Programmer_CLI -c port=SWD -r8 0x08000000 256

4. 风险动作
以下动作默认禁止，除非我明确要求：
- 全片擦除
- 写 Option Bytes
- 修改 RDP
- 写保护/解除写保护
- 对量产板烧录
- 自动批量烧录多块板

5. 烧录前检查清单
- 当前 Git 状态
- 当前分支
- 固件路径
- 固件类型 hex/bin
- 固件目标地址
- 目标芯片型号
- 是否测试板
- 是否已备份重要参数
- 是否涉及 IAP/APP 地址偏移

6. 烧录后检查清单
- CLI 返回结果
- 是否 verify 通过
- 是否 reset 成功
- 是否能重新连接
- 是否能通过串口/CAN看到启动日志或心跳
- 是否需要读取版本号
- 是否需要记录芯片 UID

7. 文档输出
每次烧录后生成：
docs/test_record/YYYY-MM-DD_stlink_flash_test.md

内容包括：
- 时间
- 操作者
- 项目
- Git commit
- 固件路径
- 固件大小
- 固件类型
- 芯片型号/UID
- 烧录命令
- 烧录结果
- 校验结果
- 复位结果
- 串口/CAN基础验证结果
- 问题和风险

============================================================
六、创建 ST-Link 脚本框架
============================================================

请根据当前项目情况创建 scripts/stlink/ 目录。

建议包含：

scripts/stlink/
  README.md
  stlink_flash.py
  stlink_read_id.py
  stlink_read_flash.py
  stlink_reset.py
  stlink_config.example.json

要求：

1. 使用 Python 编写跨平台脚本。
2. 不直接依赖固定安装路径。
3. 优先从环境变量或配置文件读取 STM32_Programmer_CLI 路径。
4. 支持 Windows/macOS/Linux。
5. 默认只执行低风险操作。
6. 高风险操作只生成提示，不自动执行。
7. 输出必须清晰，方便 Codex 解析。
8. 如果找不到 STM32_Programmer_CLI，应给出明确提示。

stlink_config.example.json 内容示例：
{
  "programmer_cli": "STM32_Programmer_CLI",
  "interface": "SWD",
  "default_address": "0x08000000",
  "firmware_dir": "build",
  "allow_mass_erase": false,
  "allow_option_bytes": false,
  "allow_rdp_change": false
}

stlink_flash.py 应支持：
- --firmware <path>
- --address <addr>
- --verify
- --reset
- --dry-run

示例：
python scripts/stlink/stlink_flash.py --firmware build/app.hex --verify --reset

如果是 bin 文件，必须要求地址参数；如果是 hex 文件，可以不要求地址。

============================================================
七、设计 MCP 落地方案
============================================================

请在：
.codex/mcp/stlink_mcp_design.md

中设计未来的 stlink-mcp-server，但本阶段可以不实现。

内容包括：

1. 为什么需要 ST-Link MCP
- 让 Codex 可以通过结构化工具调用烧录、复位、读取芯片信息。
- 避免 Codex 直接拼接危险命令。
- 统一做权限控制、参数校验、日志记录。

2. 建议工具接口
- stlink.connect
- stlink.read_chip_id
- stlink.read_uid
- stlink.flash_firmware
- stlink.verify_firmware
- stlink.reset_target
- stlink.read_memory
- stlink.generate_test_record

3. 不建议开放或必须强确认的接口
- stlink.mass_erase
- stlink.write_option_bytes
- stlink.set_rdp
- stlink.unlock_chip
- stlink.batch_flash

4. 权限策略
- 默认只读 + 单板烧录
- 高风险动作必须人工确认
- 所有写操作必须记录
- 所有失败必须保留日志

5. 第一阶段不做 MCP 的原因
- 先用 Skill + Shell/Python 脚本验证流程
- 等流程稳定后再封装 MCP
- 避免一开始过度设计

============================================================
八、设计其他 BMS Skill
============================================================

请分别创建以下 Skill：

1. bms-architecture-review
用途：
- 完整梳理项目架构
- 分析模块依赖
- 找潜在 bug
- 给出不影响功能的简化建议
- 输出架构审查文档

2. bms-module-refactor
用途：
- 重构 SOC、ADC、CAN、LED 等模块
- 保持功能不变
- 降低耦合
- 改善命名和模块边界
- 每次只做小步修改

3. bms-doc-sync
用途：
- 每次关键修改后同步生成文档
- 包括变更记录、设计说明、测试记录、风险清单

4. bms-communication
用途：
- 检查 CAN、Modbus、RS485、BLE 协议
- 生成协议映射表
- 检查周期帧、状态位、故障位、升级帧

5. bms-iap-bootloader
用途：
- 检查 APP/IAP 地址分区
- 检查升级流程
- 检查 SRAM mailbox
- 检查断电保护
- 检查向量表/MSP/复位逻辑

6. bms-low-power
用途：
- 检查 RTC、STOP/STANDBY、SysTick、TIM、IWDG、CAN 唤醒
- 分析低功耗风险
- 给出可测试方案

每个 SKILL.md 都必须包含：
- 适用场景
- 输入要求
- 执行步骤
- 禁止事项
- 输出文档路径
- 检查清单

============================================================
九、生成 Codex 日常使用 Prompt
============================================================

请创建：

.codex/prompts/stlink_flash_test.md

内容是我以后可以直接对 Codex 说的话，例如：

“请使用 bms-stlink-flash-test Skill，检查当前工程是否已经成功编译，找到最新 hex/bin 固件，使用 ST-Link 下载到 STM32F103 测试板。烧录前先 dry-run 并列出命令，确认固件路径和地址。不要擦全片，不要写 Option Bytes，不要修改 RDP。烧录后校验、复位，并生成 docs/test_record 下的测试记录。如果无法连接 ST-Link，请输出排查步骤。”

也请创建：

.codex/prompts/full_bms_review.md

内容是：

“请使用 bms-architecture-review Skill，完整梳理当前 BMS 项目，以源码为准，分析 SOC、ADC、CAN、LED、保护、低功耗、IAP、日志等模块，找潜在 bug 和复杂度问题，给出不影响功能的简化建议，并输出 docs/architecture 和 docs/risk_review 文档。不要直接大改代码。”

============================================================
十、最后输出总结
============================================================

完成后请输出总结，必须包含：

1. 创建或修改了哪些文件。
2. 每个文件的作用。
3. 当前已经能做什么。
4. 当前还不能做什么。
5. 后续如何接入真实 ST-Link。
6. 后续如何从 Skill + Shell 升级到 MCP。
7. 我以后怎么使用 Codex 来完成：
   - 项目审查
   - 模块重构
   - 编译
   - ST-Link 烧录
   - 串口/CAN 测试
   - 文档生成

执行原则：
- 先分析，后创建。
- 不确定的地方写入 TODO，不要编造。
- 不要假装硬件已连接。
- 不要假装烧录成功。
- 所有路径以当前项目实际结构为准。
- 所有文档用中文。
- 所有关键操作必须可追溯。

comm tool串口换成串口1了，我配置了BoardUart_Init_uart1，你把所有的都配置好，到底使用哪个串口你要预留，方便后续一键配置，目前使用串口1




















把老化模式剩余时间上传到can上位机，并can上位机增加写soc和开启老化模式和关闭老化模式和重置老化模式时间，老化模式三个功能需要单独实现。

给comm tool和bms app增加完整的日志，release版本

comm tool有时候灯会闪的不规则，暂停很久，然后日志经常会报“[15:27:01] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[15:27:07] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[15:28:13] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[15:29:47] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[15:30:13] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[15:30:51] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[15:32:25] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[15:33:26] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[16:09:10] 开始: 关闭老化模式
[16:09:10] 关闭老化模式 失败: comm tool 返回错误: UNSUPPORTED
[16:09:22] 开始: 开启老化模式
[16:09:22] 串口正在被实时监控占用，等待当前读数结束
[16:09:23] 开启老化模式 失败: comm tool 返回错误: UNSUPPORTED
” 老化模式时间在哪儿？没看到啊



同时考虑在不影响功能的前提下，把SystemStatus、LogRecord_Flag、System_OnOFF_Func、System_Func_StartUp、System_OnOFF_Func_StartUpRec变量都去掉，这些变量的原因，导致整个项目程序运行很复杂

梳理数码管模块逻辑并考虑简化、优化，为什么数码管模块代码这么多，重构代码不要再用s_ledbar_initialized这种宏了，数码管偶尔还会闪一下，先给出思路


















优化rtc逻辑
- 梳理rtc低功耗逻辑，html展示
- 清理重复功能变量，例如s_stLowPowerRuntime.mode又赋值给g_stLowPowerRtcStatus.u8SleepModeSelect，功能一样，为什么用几个变量，导致程序更复杂了，变量名要更简单、容易理解，例如s_stLowPowerRuntime.armed我就不懂什么功能





- rtc休眠前后时钟、外设配置分别是什么？是否有问题

完整review comm tool和bms app，给出建议，是否有bug，然后加上完整的日志，使用串口吗？还是有更好的方式，release不要打开，debug调试打开，运行流程、架构、重点等等生成html

rtc_sleep重构抽象，要兼容不同afe，mcu，方便移植，而不是通过宏来适配不同，这样太麻烦了，port层可以单独开一个文件或者有其他办法，rtc_sleep自身要高度抽象，不依赖底层mcu、afe驱动






完整梳理mcu资源使用，进入rtc低功耗前后配置，输出html

目前的can架构是否会丢帧

fd_T3Max_D009分支适配can通信升级、读写相关功能，参照当前分支，其他功能不要动

can上位机增加读取软、硬件版本、bms序列号功能，直接在实时监控界面的最底部边栏显示

“[09:55:22] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[09:57:45] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[09:58:52] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[09:59:48] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[09:59:59] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[10:01:19] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[10:01:30] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[10:04:10] 实时监控读取失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[10:04:15] 开始: 读取全部参数
[10:04:15] 串口正在被实时监控占用，等待当前读数结束
[10:04:15] 读取全部参数 失败: comm tool 返回错误: BMS_ERROR(板端拒绝/地址无效/参数越界/写权限关闭)
[10:04:18] 开始: 读取全部参数
[10:04:18] 串口正在被实时监控占用，等待当前读数结束
[10:04:20] 参数读取完成: 56 项
[10:04:20] 读取全部参数 完成”分析日志并优化



t3 can功耗，进入不了rtc


两个分支可以通过可视化配置，然后升级程序来重置老化时间吗

can-upgrade-host有这个问题吗？同步改了吗？

增加can上位机修改老化时间功能，单位小时，修改时间后重置老化时间，两个分支都需要实现


增加can上位机一键开启soc长期测试功能，先规划如何测试？如何保证soc测试没问题？can 上位机自动记录测试数据并分析？


comm tool串口还是升级失败，你直接使用串口3调试一遍看看问题


使用subagent，整体梳理comm tool、bms app、pc上位机架构，通讯时comm tool灯闪的不规律，能否优化，先分析三者通讯架构，给出优化建议


review bms iap，确认iap是否有bug，串口、can升级是否会有bug，必须保证


梳理can运行逻辑
优化can架构、功耗，是否会丢帧，结合stm32 can控制器原理说明，去掉心跳包,canbusoff是否有用，使用自身的

调研功耗调度逻辑，降低整体功耗


测试休眠，过放休眠，rtc

LedBar_ServiceMcuWakeFilter???

优化can模块，太复杂了，变量太多了，函数太多了,先给出想法规划
补充要求：
优化整体架构时，必须保持代码清晰、简单、直接，方便人阅读。
不要过度设计，不要搞复杂框架，不要增加太多抽象层，不要制造深层嵌套和多层 wrapper。
优先删除无用代码、重复代码、不必要变量，减少全局变量，简化数据流和控制流。
模块边界要清楚，但分层要轻量。
任何简化都不能影响功能、协议兼容、硬件行为、SOC、保护、低功耗和 IWDG。

打开App_Can功能后，功耗太高了，能否再不影响功能前提下，降低功耗




一直进不了rtc_sleep低功耗，使用stlink帮我调试原因，同时进入stop模式后，能否通过stlink机继续调试了，帮我持续监测、调试，优化、简化rtc_sleep逻辑，保证好用、用户体验好，功耗低，不要搞太复杂






can上位机中，关闭老化模式按钮功能：不仅仅是关闭老化模式，而是要提前结束老化模式时间




确认bms app各个io、外设配置，正常模式、rtc低功耗、低功耗唤醒后，对比commit：5d5564e0d706085e6d50a442588febdb8eaed21a


bms app can接收是否会休眠？会接收哪些帧？can通信不能唤醒？
















rtc休眠后，通过GPIO_SW中断唤醒后，需要数码管唤醒显示



















一键升级过程中，bms app还会进入rtc sleep导致升级不成功


修改rtc间隔、测试


bms app进入rtc sleep后，能否通过can通信唤醒bms app，不通过中断


单独开一个分支只用于量产出货，去掉不必要的测试代码、不必要的宏、没用的代码、变量，先和我确认哪些是不必要的

输出各模块完整功能、逻辑，需要非常细节，所有全局变量及功能，所有宏配置，输出文档并整理、更新、归纳当前已有的文档

确认清单中都不需要，全部删除，只要不影响当前功能的，全部去掉，单独开一个分支实现，专门用于量产出货的

输出本次修改的文档细节，然后提交git，并给出未来规划

调用keil，现在编译报错

Project_Config.h在keil中不能可视化了

目前发现本次删除修改后问题：1、Project_Config.h在keil中不能可视化了
2、上位机报错，实际上System_ErrFlag没有报错，应该是修改、删除Sci_Upper.c，导致协议匹配不上

Project_Config.h在keil可视化中，需要增加中文注释

放电mos开着，上位机显示却显示关着的，s_system_status,应该还是和Sci_Upper有关，仔细看下是否还有类似问题，一起解决

rtc是否能更新soc

codex审核从a53e77ef2dc72bdfdc0130abb8ec8f0287b1df9f到029907543e64966916c0bc0ddf1fb1839a8b0b06这些commit

充电的时候数码管又会闪了？充电时，闪电图标不亮，显示45%时，个位b段会偶尔闪一下

测试参数读写，升级更新参数等等

建立完整的log和系统状态，包含所有细节，例如每一个io的状态，每一个功能的状态，方便调试，先给出方案

g_dbg是否应该分结构体管理，然后能否继续完善该功能，给出计划，我的根本需求是完全掌握整个项目mcu使用的资源、监控所有状态，方便观察是否有bug


todo测试、梳理rtc休眠前后，外设、io状态
- GPIO_ADC_BUS_EN供电测试

不用宏，使用枚举，这样在keil watch中直接方便看到具体状态，先给出哪些需要修改

GPIO_MCU_WK 状态观察

修改进入rtc时间（60s），

优化、简化整个项目、各个模块，重构、清理重复功能变量，尽量减少重复、没用的变量，不要为了封装而封装，先给出方案


todo 完整测试
- 过放休眠、唤醒、rtc、充电唤醒测试
- soc测试
- 老化模式测试
- 参数读写测试
- 升级、参数更新、soc更新等等测试

能否全局去掉SOC_Enhance_Element.u16_SOC_InitOver这一类变量，不仅仅是这个变量，能明白我的意图吗

提交git，然后考虑能否进一步优化。
每次修改完代码，都需要通过keil编译成功

















对比feidao can协议和c073，去掉ERROR_CAN，去掉没用报错
能否g_dbg中增加更多调试信息，例如mcu各外设资源的使用，先给出想法






















增加更多方便调试、观察整个项目，各模块

先梳理低功耗rtc模块，然后优化、简化低功耗rtc模块，
测试低功耗rtc唤醒后，adc采样是否正常,typec电流、总压、温度


Can_RtcWakeService()

删掉PROJECT_CFG_DEBUG_WATCH_ENABLE，是否有用
梳理soc_apply_rtc_rest_ocv逻辑

升级时还是容易进入rtc 导致升级失败

NVIC_ClearPendingIRQ

static uint16_t s_u16IdleDelaySeconds = 0U;
static uint32_t s_u32RtcSleepElapsedSeconds = 0U;
static uint32_t s_u32RtcWakeCycles = 0U;
static uint8_t s_u8RtcSoc = 0U;

自适应rtc时间

adc逻辑为什么这么复杂，adc计算逻辑是怎么样的，能去掉滤波吗

SOC_GetTypeCBatEquivCurrentA10是否对时序有影响

加入System_ErrFlag.u8ErrFlag_Com_AFE1 afe通信异常恢复（本身就可以自动恢复)

加入typec soc测试 宏

rtc情况下，能否通过can唤醒

根据源码和文档梳理我的低功耗的需求和目前的实现功能，我需要和你对齐需求和功能，目前的实现太过复杂，需要简化，不方便阅读和维护，bms越简单越好，保证功能的前提下，降低功耗，稳定不出问题。


整个项目的很多时序是确定的，为什么还要用很多变量来记录状态、时序，来把问题、软件做复杂了，例如if (s_ledbar.initialized == 0u), if (g_stLowPowerRtcStatus.readyToSleep != 1U)
    {
        return;
    },

减少宏配置？

对于我的需求，can自动恢复busoff是否已经足够，目前软件上不会有bug，导致can通讯异常吧，先调研stm32官方和行业中是如何处理的

梳理整个项目的变量使用，减少不必要的变量、重复功能的变量，即使有变量，最好也不要用单独变量，最好用结构体，方便集中管理和keil在线watch调试，或者你有什么方案


sleep必须保证数码管熄灭

梳理项目中所有中断，并为每个中断增加中断计数，方便调试观察是否在不同状态、生命周期进入了不该进入的中断，例如rtc休眠前后，先给出具体方案并输出文档

g_stIrqDebug


rtc stop休眠关了哪些外设，唤醒后恢复了哪些，是否有没恢复的
- led？？？

不要用单独变量，例如s_u16IdleDelaySeconds，必须结构体模块化，而且变量名、函数名要简洁、清晰，不能太长


测试Can_PeekBusy是否有用，升级是否会进rtc


进一步简化low_power_select_sleep_mode，先给出方案


动了startup_stm32f10x_hd.s，是否有风险？

量产注意关掉

#ifndef PROJECT_CFG_DEBUG_MONITOR_ENABLE
#define PROJECT_CFG_DEBUG_MONITOR_ENABLE 1
#endif

// <q> Enable IRQ debug counters
// <i> Keeps lightweight interrupt counters for Keil watch and STOP wakeup debug.
#ifndef PROJECT_CFG_IRQ_DEBUG_ENABLE
#define PROJECT_CFG_IRQ_DEBUG_ENABLE 1
#endif

// <q> Enable IRQ debug event ring
// <i> High-rate interrupts are counted but not pushed into the event ring.
#ifndef PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE
#define PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE 1
#endifjj


can上位机中mos温度没有显示

整个bms app项目是否还有单个的变量，全部要统一到结构体整体中，要注意变量、函数命名规范，还有能否把所有结构体整合成一个全局结构体中，这样方便掌控全局所有变量和掌控整个项目，方便keil调试、观察


soc_apply_mid_tail


soc校准不太合适，还是要优化，静置校准太快

SOC_UpdateSampleData

led模块中key和mcu_wk的滤波是否有必要，可以去掉吗

MosStartup_ApplyInitialState运行时，老化模式是否已初始化，是否会有逻辑、时序问题

rtc唤醒后，adc的这几个采样结果是怎样的，adc还没完全采样、计算完成时，这几个值是多少，是否会有风险，adc的总压和温度采样值会对熔断保险丝逻辑有重大影响(GPIO_WriteBit(GPIO_RF_EN, PIN_RF_EN, Bit_SET);)

用keil调试时，目前很多变量都是static的，不方便加入到watch中，每次都要单步运行到对应模块才能加入，很不方便

测试一遍保护和对应的状态显示，飞道协议测试，短路与恢复

梳理目前所有的调试手段，分别怎么使用，是否有实际用处，能否优化调试手段，更方便在keil调试中，掌控状态、细节，方便项目优化、debug，有问题时，能快速定位、纠偏并解决

Keil 的 FD_Release 和 FD_Debug 两个 target 的 Source 分组现在有什么区别，实际有用吗，你是怎么配置的，我怎么确认你配置的没问题


FD_Release:
STM32F10X_MD,USE_STDPERIPH_DRIVER

FD_Debug:
STM32F10X_MD,USE_STDPERIPH_DRIVER,PROJECT_CFG_BUILD_PROFILE=1,PROJECT_CFG_DEBUG_WATCH_ENABLE=1,_DEBUG_


我想在FD_Debug中保留这些调试功能和代码，又不想在FD_Release中见到这些代码，在量产代码中只看到和真正有用的代码，不然影响阅读，给出方案，例如{
  /* event: fault detection */
	{
		uint16_t now_fault = g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBU;
		if ((now_fault != 0U) && (s_rt.fault == 0U)) {
			SystemDebug_Event(0x02, (uint8_t)now_fault, (uint8_t)(now_fault >> 8), 0U);
		} else if ((now_fault == 0U) && (s_rt.fault != 0U)) {
			SystemDebug_Event(0x07, 0U, 0U, s_rt.fault);
		}
		s_rt.fault = now_fault;
	}

	/* event: LP mode change */
	{
		uint8_t now_lp = g_stLowPowerRtcStatus.mode;
		if (now_lp != s_rt.lp) {
			SystemDebug_Event(0x03, now_lp, (uint8_t)g_stLowPowerRtcStatus.block, 0U);
			s_rt.lp = now_lp;
		}
	}

	section_start = SystemDebug_GetCycleCount();
}
这些我都不想见到


rtc休眠一次被唤醒后，会一直进入Can_IsBusy 的if (s_tx.count != 0U)
	{
		return 1U;
	}导致一直进不了rtc


g_dbg_watch中有很多重复的变量，直接搜索DebugWatch_BindAll函数
cell_report重复

去掉full_anchor，梳理静置soc校准






func

/****************init***************/
- DebugWatch_BindAll
- SleepDeal_HandleBootSleepStartup
  - afe sleep
  - mcu io\adc\can\ sleep conf
  - wakeup
  - soc backup dom store
- io conf
- uart conf
- flash param read and update
- afe init
  - param update
  - afe zero current cali
- can init
- adc init
- soc init
- timer sch irq init
- iwdg init

/*************************loop*****************/
- FactoryAging_Task
  - old task logi
- APP_LedBar
  - if(key press or typec wk) 5pin led disp task,disp window(5s??)
  - rtc sleep status:key/typec wk irq wakeup from sleep and disp led
  - if(key long press)  deep sleep
  - disp 1、soc 2、dsg mos
- App_AFEGet
  - 200ms led 
  - afe sample 1、vcell 2、temp 3、curr 4、309 status
  - adc sample 1、vbat 2、typec curr(neew to translate) 3、mos temp 4、one temp
  - _UL_RENZHENG_ENABLE_ logi
    - mos temp protect
    - if(afe comm err) close ctlc,if(vbat >= 4280 || temp >= 85 && 10s) 
    - if(!afe comm err) if(temp >> 80 || (vcell >= 4270 && vcellmin >= 2000) && 3s)->close ctlc->if((vcellmax >= 4280 || vbat || temp) && ichg)
  - if(5v chg) open_chg_close_dsg if(!5v chg) sleep;
- App_AnlogCal
- rtc_sleep
- App_Can












完全去掉SOC_Enhance_Element中间层



 有很多冗余或者说没意义的写法，例如if (cap_a10 == 0U)
	{
		cap_a10 = SOC_DEFAULT_CAP_A10;
	},首先你能证明cap_a10会为0的情况吗，还有{return (g_stCellInfoReport.u16VCellMax >= g_stCellInfoReport.u16VCellMin) ?
		(UINT16)(g_stCellInfoReport.u16VCellMax - g_stCellInfoReport.u16VCellMin) :
		(UINT16)(g_stCellInfoReport.u16VCellMin - g_stCellInfoReport.u16VCellMax);}vcellmax会有比vcellmin小的情况吗，类似很多这种冗余写法，除非你能证明会有这种情况，而且如果有这种情况，说明其他模块的判断有问题，应该从根本解决，而不是这种冗余复杂的写法，否则不应该这么写，你认为了















led模块太复杂了，不仅是本身逻辑复杂、LedBarRuntime结构体成员多，而且还涉及到各种状态，例如rtc休眠，深度休眠，休眠后led要配置好，保证低功耗，同时要保证体验，用户深度休眠唤醒后预览soc，数码管本身显示逻辑就很复杂，不方便调试，不清楚运行时序，帮我梳理清楚，然后看能否简化


宏太多了，不方便阅读和管理项目，只有必须用宏的地方使用宏，同时尽量减少Project_Config.h中的需要我配置的宏，尽量将宏配置放进对应模块，只有经常需要我修改的宏，例如soc体验、调试、升级相关需要我经常调试配置的房间Project_Config.h中，然后不是非必须使用宏的地方，尽量不要用宏，例如状态相关，可以用枚举或者更好的方案，例如#define SOC_MODE_RELAX               ((UINT8)0U)
#define SOC_MODE_CHG                 ((UINT8)1U)
#define SOC_MODE_DSG                 ((UINT8)2U)这几个宏

增加soc低电校准逻辑，目前逻辑应该是会往下校准，不会往上校准，这没问题，但是soc积分还是有用的，不能够确定表格是否合理，最好能加入当电压高于表格，但对应soc却低于表格，这个时候放电应该“卡住”soc，你认为怎么样

System_ErrorControlBase???

合并AppInit和Runtime，同时Runtime主循环实现合并为一个函数，不需要分这么多函数，然后rtc_sleep是否应该放在循环的末尾

修改老化模式，也允许进入rtc，但要保证老化模式时间的问题，先给出修改规划


看一下afe参数

屏蔽了FactoryAging_Task中的FactoryAging_ApplyRunningMos()是否有影响，还有整个项目中和mos动作相关的逻辑梳理出来

新上位机的bms直连串口通信方式，能否实现iap升级，升级具体流程参考：旧的串口上位机升级源码参考E:\sync\git\upper\BMS-upper - 副本 (4)中的分支ttt，bms iap参考E:\work\a002\new 030\IAP 103CB

梳理新上位机的完整功能、设计、架构文档，使用方式等等，例如直连串口和comm tool can桥接具体协议、流程，iap升级协议、流程，读、写bms参数协议等等，需要完整、具体的输出文档，方便后续维护


comm tool新增功能：1、当上位机选择串口直连模式时，comm tool的串口1直接和pc串口直连，pc发送modbus指令，comm tool要通过串口2透传modbus然后接收bms应答，然后串口1要透传modbus的应答。串口2的TX是PA2，RX是PA3。2、comm tool有按键PA6，当通过上位机下载完升级bin包到comm tool后，comm tool可以通过PA6按下（低电平）来脱离上位机，可以通过can接口实现一键批量给bms升级。



上位机主页面的连接检测按钮是否可以去掉？当通信异常时，实时监控的各种数据都归0,单体电压直接不显示。需要实现这两个功能

用户点击停止监控后，实时监控的各种数据也要都归0,单体电压直接不显示。

comm tool的iap和app是否需要加入看门狗

上位机中实时监控需要加入出厂容量显示


其他功能中选择升级文件功能，不要默认显示路径，升级文件路径让用户自己选择


上位机系统状态中的：充电mos、放电mos、加热、冷凝这4个状态在有些状况中显示有问题，明明通信上了，却全灰显示，不会显示绿色的on和红色的off

comm tool串口直连是否支持缓存升级


comm tool串口1的波特率是115200不会变，所以可以隐藏串口1的波特率选择。串口2的波特率目前写死了19200，但是需要兼容不同板子的波特率，因此波特率选择窗口用于选择comm tool串口2的波特率，而不是串口1的波特率，你认为这个方案怎么样，可行的话，直接实现

进入iap的条件是什么？以前的旧板子升级协议是和当前项目兼容的，但是进入不了iap，然后不能给旧板子升级

提取出把目前的和上位机can通信、升级最小代码，方便后续其他项目移植，快速实现，或者生成skill还是什么方法，方便改造其他项目也支持当前项目这种和上位机can通信和升级相关功能，想给出方案

soc_apply_rtc_rest_ocv逻辑是什么？rtc状态多久，会进行soc校准，怎么校准的  

增加rtc ocv校准条件，3700以上不校准？？？

FactoryAging_SaveStoredProgress

rtc休眠前和休眠唤醒后用的什么时钟

rtc休眠前后涉及到很多外设、io的配置，这些配置十分重要，特别是io，目前的软件写法感觉不太清晰，容易有问题，能否有什么更好的配置方式，清晰、简单、易维护，例如配置表？给出你的方案

修改

开发一个vscode插件，在使用go to def进行代码跳转时，能实时显示跳转路径图

帮我开发一个 VS Code 插件：Jump Path Tracker


增加一种休眠模式，除了过放休眠进入深度休眠，不进入rtc休眠，开关休眠等以后加入的其他各种休眠策略，只要不是过放、电压低，都进入rtc休眠，但是rtc休眠需要区分两种模式，平时待机rtc休眠不管mos，但开关休眠需要关mos，这种一直rtc休眠，就可以在用户关开关后，依然进行电池状态采样，进行预警、soc校准，特别是soc校准，重构、简化当前整个项目，状态机？















pa0 唤醒待确认，边沿
pa8 负载，完全去除，只使用afe自带
pa9 开关逻辑待确认
pa10 afe ship没用吧？
pa6  afe alm 没用？
pa7 afe mode 应该需要，待配置
pa1  vbus修改为测mos温度
pa4   cmnt en 待确认高低
pa5   加热逻辑，待加入
pb1   测温度？什么温度
串口2 需要支持？
cmnt en修改为ble en，给什么供电
pb5 sw供电？待确认


todo 
处理soc
梳理所有和mos相关逻辑，

打开_COMMOM_UPPER_SCI2宏，需要串口2功能使能，现在有问题：modbus主机一发送，bms就一直进入

上位机短路电流和短路延时列表目前的逻辑是怎样的，现在显示有一点点问题
soc100，均衡参数、休眠相关参数也不能通过升级修改，待梳理
