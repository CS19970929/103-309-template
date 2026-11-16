# 主 BMS 板级 IO 与外设配置

适用 `STM32F103CBT6 + SH367309` 主工程，两个 Keil 目标共用同一份配置。不包含独立的 `firmware/comm_tool_f103ret6`；未加入有效目标的 `Heat_Cool.c` 也不会因此启用。

工程开关、产品默认值与升级后按组更新参数的完整说明见 [主 BMS 配置与升级更新手册](configuration-and-upgrade.md)。本文说明板级映射与外设配置。

## 维护入口

硬件配置集中在 `103 + 309/Project/Source/conf/Board_Config.c`：

| 数据 | 内容 |
| --- | --- |
| `g_board_io` | 每个有效引脚的端口、脚号、模式、速度、初始化电平、初始化阶段、STOP 保留规则 |
| `g_board_wakeup` | 唤醒输入、触发沿及 IRQ 优先级；端口和 EXTI line 从 IO 项取得 |
| `g_board_uart1/2` | UART 实例、APB 时钟、重映射、IO 分组及 IRQ |
| `g_board_can` | 原有 CAN 时序、控制选项、IRQ、收发器供电极性 |
| `g_board_adc_inputs` | MOS 温度的 ADC 通道及采样时间；引脚属于 `BOARD_INIT_ADC` 分组 |
| `g_board_timer3` | 100 kHz 计数、999 重装值及 IRQ，维持现有 10 ms 调度 |
| `g_board_iwdg` | RTC 开启/关闭时的原有看门狗预分频和重装值 |

例如 PB5 的定义直接说明各字段含义：

```c
[BOARD_IO_SW_EN] = {
    .gpio = GPIOB, .pin_source = 5U,
    .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
    .initial_level = BOARD_HIGH_BEFORE_MODE,
    .init_group = BOARD_INIT_POWER, .stop_keep = BOARD_KEEP_NONE
}
```

`pin_source` 是 0～15 的脚号，GPIO mask 和 EXTI line 自动由脚号生成。现有 `GPIO_*`、`PIN_*`、`MCUO_*`、`MCUI_*` 名称保留在 `conf_gpio.h`，只引用配置表，业务代码无需另存一份引脚定义。表是 Flash 中的 `const` 数据，不增加可写状态。

## 当前引脚速查

| 引脚 | 软件功能 | RUN 配置 | STOP 准备阶段 |
| --- | --- | --- | --- |
| PA0 | `CHG_IN` | 浮空输入 | 模拟输入；Base/Deep 准备随后配置上升沿唤醒 |
| PA1 | MOS NTC | ADC1_IN1 | 模拟输入，ADC 停止 |
| PA2/PA3 | USART2 TX/RX | 10 MHz 复用推挽 / 浮空输入 | HICCUP 按配置启用 PA3 下降沿唤醒 |
| PA4 | `CMNT_EN` | 输出低，CAN 供电为低有效 | 模拟输入 |
| PA7 | `RF_EN` | 推挽输出，保留输出锁存值 | 模拟输入 |
| PA9 | 按键/开关 | 浮空输入 | 随后配置双沿唤醒 |
| PA11/PA12 | CAN RX/TX | 上拉输入 / 2 MHz 复用推挽 | 模拟输入 |
| PA15 | `M_STB` | 输出高 | 模拟输入 |
| PB0/PB14 | AFE PRO/CTL | 推挽输出；启动仅将 CTL 拉低 | RTC 路径保留原模式与电平 |
| PB3/PB4/PB5 | `AD_EN/BLE_EN/SW_EN` | 输出低 / 低 / 高 | PB4 仅在 HICCUP 路径保留；其余模拟输入 |
| PB6/PB7 | USART1 TX/RX | 重映射，10 MHz 复用推挽 / 浮空输入 | HICCUP 按配置启用 PB7 下降沿唤醒 |
| PB8/PB9 | AFE 软件 I2C SCL/SDA | 初始化为 2 MHz 推挽并共同拉高 | 模拟输入；周期读取或 RUN 恢复时重新初始化 |
| PB12 | `CMNT_WAKE` | 仅唤醒配置使用 | HICCUP 上升沿唤醒 |
| PB15 | DEBUG LED | 推挽输出，电平由既有业务维护 | RTC 路径保留原模式与电平 |

`RF_EN`、`AD_EN` 等保留现有网络名；其外部电路用途仍需依据硬件确认。

## 初始化与休眠约束

- `BOARD_LEVEL_KEEP` 不写输出锁存值。其他值明确区分切换 GPIO 模式之前或之后写电平；供电分组先预置全部电平，再启用输出。AFE CTL 保持启动时先配置模式、再拉低。
- 初始化顺序仍由既有启动与恢复调用链决定：AFE 控制脚 → 共用 IO → 供电 → LED；UART/CAN/ADC/I2C 在原驱动入口初始化。STOP 恢复不重新初始化 AFE 控制脚和 LED。
- `stop_keep` 保留当前模式与电平，不重放启动电平。NORMAL 保留 PB0/PB14/PB15；HICCUP 额外保留 PB4。Deep 基础准备将所有端口设为模拟输入，再按原流程配置唤醒输入。
- 唤醒功能开关和串口角色仍由 `Project_Config.h` / `conf.h` 决定。UART 波特率继续由持久化参数决定，两路仍为 8N1。RTC、低功耗状态机、时钟树和外设停启时序继续由原模块维护。
- PB8/PB9 的软件 I2C 还需编译期常量才能保留直接寄存器操作的时序，因此其端口和脚号统一放在 `Board_Config.h` 的三个 `BOARD_AFE_I2C_*` 定义中。配置表和快速读写共用这些定义；当前实现只支持同一端口的两个不同 CRH 引脚（8～15），仍为 PB8/PB9。
- 修改硬件映射仍须核对 MCU 复用限制、AFIO remap、IRQ handler 和实际板线，不能仅根据功能名任意换脚。TIM3 的配置与调度/微秒换算绑定，不能单独修改频率或周期。

本次只集中现有配置，没有增加低功耗入口或启用新外设。普通 IO 和中断配置的执行代码留在 `conf.c`，驱动继续负责通信、采样与硬件停启。

## 验证

运行 `tools/test_board_config.py` 检查 21 个引脚、供电电平预置、恢复时 AFE/LED 保留、NORMAL/HICCUP/Deep 模拟输入范围、6 组唤醒与角色开关、I2C CRH 位操作。ADC/保护、UART/协议、休眠策略分别使用原有 `test_protection_refactor.py`、`test_uart_protocols.py`、`test_sleep_policy.py`。

使用 `tools/check_bms_no_bin.py --target FD_Release` / `FD_Debug` 验证编译与链接边界，所有验证文件保存在用户临时区，不生成 BIN/HEX。host 与编译结果不能替代实板上电、MOS 状态、波形、STOP 唤醒和功耗验证。


本次交付已验证当前工作区及排除原有脏修改后的独立暂存源码。独立提交使用 STM32F103CB、ARMCC 5.06 update 7；两个目标均为 0 错误、0 警告，编译各约 2 秒：

| 目标 | Code | RO-data | RW-data | ZI-data | App 结束地址 |
| --- | ---: | ---: | ---: | ---: | --- |
| FD_Release | 37260 | 3608 | 1016 | 5808 | 0x0800EB9C |
| FD_Debug | 45652 | 3640 | 1016 | 5808 | 0x08010C84 |

两目标 RAM 合计 6824 字节，App 均位于允许的 `[0x08004800, 0x0801C000)` 内。只产生用户临时区中的 AXF/MAP；未生成 BIN/HEX，未进行实板连接或烧录。四类 host 回归全部通过；原有 5 秒休眠设置、采样电阻档位、单体电压修正等未提交修改不纳入本次提交。
