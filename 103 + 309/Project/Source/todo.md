
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

