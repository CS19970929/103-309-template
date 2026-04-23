
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