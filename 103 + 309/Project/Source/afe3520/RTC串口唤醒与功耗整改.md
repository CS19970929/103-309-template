# RTC STOP串口唤醒与功耗整改

2026-09-07。用户反馈RTC后串口无法唤醒、开启RX EXTI后卡死，测得约3mA；测量点及调试器连接状态未确认。本次仅源码修复、模拟测试和编译，未烧录、未测量修复后电流。

## 已确认的代码问题

- USART1 RX为PB7/EXTI7，但共享EXTI9_5中断原来只清EXTI5；EXTI7挂起后不断重入，导致主程序无法运行。
- USART2 RX为PA3/EXTI3，原来只清pending，没有记录uart2_irq；两串口配置当前还被`#if 0`关闭。
- 唤醒退出清理漏掉EXTI3/7，运行期间串口数据边沿可能持续产生中断。
- RTC与串口同时触发时可能先走RTC采样分支，延迟外部唤醒；NORMAL模式只初始化基本唤醒源，没有串口。
- GPIO低功耗设置仅在RTC循环入口执行一次，每次AFE检查会重新初始化SPI，下一次STOP前未重新停用SPI/固定引脚；CS还可能浮空。
- 用户将RTC检查从10秒改为1秒，每秒恢复系统时钟、读AFE及检查保护，会增加平均电流；RTC模式不等于MCU永远处于STOP，也不等于AFE进入深度休眠。

## 修复行为

1. PB7/PA3配置下降沿EXTI、RX上拉，起始位唤醒；中断记录uart1_irq/uart2_irq，屏蔽本RX线并清pending，一次性退出睡眠。共享EXTI5/7分别处理，保留按键功能。
2. STOP退出同时禁用并清理EXTI3/7及NVIC pending，恢复原USART初始化。进入RTC前禁用USART和其NVIC，避免关闭时钟后的伪接收/错误中断；唤醒字节不交给Modbus解析器。
3. RTC与外部IRQ同时到达时优先退出RTC循环，恢复外设；记录外部通信事件，避免刚恢复又立即按空闲条件休眠。
4. 每次RTC STOP前重新关闭SPI、固定CS和SCLK高（匹配现有SPI mode 3）、MOSI低、MISO模拟输入。保留原M_CCC控制状态和用户新增的三路关电设置。
5. 清理停止的TIM3及SysTick挂起，使用内存屏障；保持“屏蔽IRQ检查唤醒条件 → WFI → 恢复系统时钟 → 恢复PRIMASK”的原子时序，不清除刚到达的外部唤醒事件。入睡前按既有低功耗调试宏应用DBGMCU设置，默认关闭低功耗调试保持。
6. `Project_Config.h/PROJECT_CFG_RTC_STOP_SECONDS`可选1..10秒，默认10；NORMAL和HICCUP统一使用，避免原20秒周期逼近开启IWDG时的超时时间。进入RTC前等待时间未改动。

底层依据本项目官方STM32F10x SPL V3.5.0的PWR_EnterSTOPMode、EXTI_Init/GetITStatus/ClearITPendingBit、USART_Cmd及CMSIS IRQ/SCB定义。芯片为STM32F103C8，非F030。

## 通信方式

STM32F103 STOP期间USART时钟停止，RX EXTI只负责唤醒，不能保证保存第一帧。建议发送一个唤醒字节后留出至少100ms，再发送正式Modbus请求；如时钟/AFE恢复耗时更长，应延长等待。直接发送完整第一帧时可能丢帧，应在恢复后重发；本次未修改上位机传输策略。

串口唤醒限于RTC/NORMAL模式；深度休眠仍保持原按键/充电唤醒策略，不给深度休眠增加通信唤醒。

## 3mA复测范围

先区分MCU 3.3V支路与整板电池输入，记录ST-Link、串口模块和其他外设是否连接；用电流波形区分STOP期间的底电流与每10秒的采样脉冲。源码无法将整板3mA直接归因于MCU。

若持续不进STOP，应检查PA0、按键电平、挂起IRQ及g_irq_t；若每10秒检查后直接恢复运行，应检查AFE通信和保护唤醒原因。SPI外部AFE、收发器、稳压器静态电流和外部IO回灌均需实测。当前RTC仍检查AFE并维持保护，不应以整个板子的电流直接对照单颗MCU数据手册STOP典型值。

保留的M_STB/AD_EN/CMNT_EN低电平设置来自用户修改，不能仅凭命名确定电源方向和覆盖范围。若其中断电了串口收发器，必须确保收发器支持低功耗唤醒且RX确有下降沿，否则MCU EXTI无法接收到通信。调试器与外部模块移除前后应分别测量。

## 验证

`tools/run_stop_wakeup_host_test.py`检查真实IRQ函数的两路唤醒来源、pending清除、RX一次性屏蔽、EXTI5/7共享及RTC/串口竞争；同时检查关闭/恢复路径顺序。既有AFE多组合、保护恢复、STOP边界、命令休眠和紧急休眠回归通过；测试模型补齐新增CMSIS调用的stub。

Release/Debug完整编译均0错误、3条既有警告。Release BIN为37120字节，App剩余1792字节，仍从0x08004800启动，不改IAP或存储分区。最终是否消除3mA及外部串口能否唤醒，待实板验证。
