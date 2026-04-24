
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


我不太熟悉adc和stm32的adc配置、模式，调研下adc怎么使用，有哪些配置方式，不同配置有什么区别，适合哪些场景，我目前的配置是怎样的，输出文档，我现在需要配置GPIO_ADC_VBUS，用于测总压，GPIO_ADC_NMOS测温度，GPIO_ADC_CUR测电流

低功耗模式是否会影响adc，比如从sleep、stop唤醒后

目前系统的时钟系统是怎样的，完整梳理mcu、各个外设的时钟，包括不同工作模式，是否会有问题，输出文档