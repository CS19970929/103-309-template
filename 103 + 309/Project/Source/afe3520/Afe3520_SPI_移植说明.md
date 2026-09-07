# SH3673520 配置、IO 与 SPI 说明

本次基线：`codex/afe-spi-refactor-debug`，提交 `78b4f5f` 中的 `InitAFE3520_Registers`。当前工程目标是 STM32F103C8，底层沿用工程 STM32F1 标准外设库。

## 配置入口

AFE 硬件配置集中在 `Afe3520Config.h`，支持 Keil Configuration Wizard。修改后重新编译。`AFE3520_CFG_WDT_ENABLE` 默认 `0`，改为 `1` 开启 AFE 看门狗；也可以通过编译宏覆盖。启用后，MCU 被调试器暂停并不会冻结外部 AFE 的看门狗。

配置写入后逐项回读验证；LTCLR 是命令位，不参与相等比较。非法串数或配置字节拒绝应用。串数来自现有持久化 `SeriesNum`，有效范围 5～20，缺少有效参数时工程默认 19 串。

| 地址 | 寄存器 | 当前默认写入值 / 意义 |
| --- | --- | --- |
| 40 | SCONF1 | 00，正常模式 |
| 41 | SCONF2 | 初始化 80，LTCLR；PUMP_EN、PD_EN、PDSG_CTL 为 0，充放电控制位由运行逻辑更新 |
| 42 | SCONF3 | 00，不周期触发 OWD |
| 43 | SCONF4 | SeriesNum，默认 13（19 串） |
| 44 | SCONF5 | 18；OCC、CADC 开启，MOS_EN=0，WDT 关闭；启用 WDT 后为 1C |
| 45 | SCONF6 | 7F；OV、UV、OCD、SC、TS1/2/3 开启，TS4 关闭 |
| 46 | SCONF7 | KEEP |
| 47 | OWV_ALARMH | KEEP |
| 48 | ALARML | KEEP |
| 49～4A | OV | 03 52；4250 mV，延时编码 0 |
| 4B～4C | UV | 02 12；2650 mV，延时编码 0 |
| 4D | OCD1 | KEEP |
| 4E | OCD2 | 03 |
| 4F | SC | KEEP |
| 50 | OCC | 07 |
| 51 | OTC | 86，参考 NTC 阻值 3.55 kΩ |
| 52 | OTD | 53，参考 NTC 阻值 1.935 kΩ |
| 53 | UTC | 77，参考 NTC 阻值 27.513 kΩ |
| 54 | UTD | D7，参考 NTC 阻值 116.11 kΩ |

表中寄存器值为十六进制。电压用 mV 修改；延时、电流、温度原始编码均在配置入口旁说明。温度对应的摄氏温度取决于板上 NTC 曲线，不能只凭阻值换成通用温度。

**KEEP 表示参考分支初始化没有写此寄存器，本工程也不写。** 可以改成 0x00～0xFF 显式配置。KEEP 不代表零，也不保证芯片默认值：若先前固件写过，单独复位 MCU 可能继续保留旧值。首次比较应使 AFE 完整掉电重新上电；本次没有新增强制复位或猜测这些寄存器的默认保护阈值。尤其 OCD1、SC 不能仅依据此配置表得出实际阈值，应读取芯片确认。

MCU 软件保护继续使用现有 `0x2400` 参数区及既有算法，协议含义不变；这些参数不再反向覆盖 AFE 硬件配置。例如软件单体过压阈值仍可能为 3750 mV，达到软件条件仍会报过压，即使尚未达到 AFE 的 4250 mV。排查时应区分 AFE FLAG 与软件故障来源。

## IO

有效板级宏统一在 `conf/conf_gpio.h`，不再有第二份 AFE 板级配置头。

| IO | 用途 / 配置 |
| --- | --- |
| PA4 / PA5 / PA6 / PA7 | SPI CS / SCK / MISO / MOSI；输出推挽，MISO 浮空输入；总线空闲 CS、SCK、MOSI 高 |
| PB14 | M_CCC 充电控制；启动低，运行跟随最终充电允许条件；不是充放电公共使能 |
| PA0 | MCU 唤醒，浮空输入，上升沿 EXTI0 |
| PB5 | 按键，浮空输入，下降沿 EXTI5；按键采样和中断统一 |
| PB12 | 通信唤醒，浮空输入，上升沿 EXTI12 |
| PA15 / PB3 / PB4 | M_STB / AD_EN / CMNT_EN，运行初始化高 |
| PB15 | 调试 LED，推挽输出 |
| PB6 / PB7 | USART1 TX / RX，沿用重映射 |
| PA1 | MOS 温度 ADC 输入 |

现有实际使用的 USART2、CAN 复用接口继续由对应外设初始化。GPIO 输出使用 2 MHz 模式，SWD 保留、JTAG 释放。低功耗恢复重新配置 SPI 引脚。

删除 SHIP、PRO_EN、BLE_EN/SW_EN 等无实际用途或错误别名、未用 ADC PB1、未被业务读取的 PA8/PA9 检测定义，以及旧 MCUO/MCUI 宏。参考代码中的旧 PC12/PC13/PD2 和当前工程未用的 C/D/E 整口初始化不搬入 F103C8 工程。删除 UART RX 唤醒开关及未用 EXTI 分支；只保留实际板级唤醒及 RTC 中断。历史 `AFE_SHIP()` 兼容入口不再操作虚构引脚。

## 故障修复与通信评估

- 原采样函数成功返回 1，而 MonitorAFE 将非零当失败，会在正常通信时反复报 AFE1；现统一为成功 0、失败 1。
- 原启动过程通过旧 SH367309 地址别名写配置，实际可能写到 SH3673520 的模式命令寄存器；现经统一 MOS 请求处理。
- MOS 命令每轮重新落实，避免配置恢复关闭 MOS 后被旧缓存跳过；实际状态读取 BSTATUS1，充电还结合 PB14 输出状态，不把软件命令直接当硬件状态。
- FLAG2.bit3 按参考解释为 RST2，触发一次配置恢复，不再误判为断线故障。清保护标志逐寄存器重新授权 LTCLR，并保留无关标志位。
- 默认关闭 WDT，SCONF5 从原 3E 改为参考 18，SCONF2 不开启原 PUMP 配置。采样换算继续采用参考的 `raw × 5 / 32` mV。

SPI 保留参考的软件方式：Mode 3、MSB first、相同半周期延时循环；删除未使用的硬件 SPI 分支及单字节 CRC 宽松回退。读写均校验完整回显和 CRC，最多重试 5 次，帧结束释放 CS，并在重试前留出帧间隔。错误不会被当成有效采样或成功配置。

软件延时循环不是精确时基，实际频率受编译优化影响；中断以及调试器在帧内暂停会拉长事务。默认关闭 WDT 可消除看门狗超时这一来源，但不能保证任意暂停位置的当前帧仍有效，恢复后依赖完整校验和重试。主机测试不能证明真实 AFE 的时序裕量；仍需板上检查 PA4～PA7 波形、暂停恢复以及充放电实际状态。本次没有执行烧录或板级测试。

## 验证与构建

`tools/run_afe3520_host_test.py` 编译实际驱动和保护源码，通过逐位 GPIO 从设备模型检查配置、保留寄存器、CRC/NACK/回显错误、5 次重试、采样返回值、LTCLR、MOS 重配置恢复、硬件反馈、RST2 和 WDT 故障恢复。WDT 关闭/开启各 10 项，共 20 项通过。Windows 可在 Visual Studio Developer Shell 中执行，或使用 PATH 中的 gcc/clang；所有测试二进制写到用户临时区。

Keil ARMCC 的 FD_Release、FD_Debug 均完整编译验证。Debug 原 -O0 超出 App 分区，调整为 -O1 并保留调试符号，同时补全未处理向量记录函数；优化可能影响局部变量观察和单步顺序。没有扩大分区。

| 目标 | Code | RO | RW | ZI | Flash 合计 | RAM 合计 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| FD_Release | 34644 | 1796 | 544 | 8488 | 36984 | 9032 |
| FD_Debug | 37360 | 1796 | 544 | 8488 | 39700 | 9032 |

单位为字节。完整构建仍有 7 条既有未使用函数/变量警告，位于 ADC、SOC、Sci_Upper、SocEnhance；不影响链接。产物位于 `Project/Users/Objects/FD_Release.{axf,hex,bin}` 和 `Project/Users/Objects_Debug/FD_Debug.{axf,bin}`。

App 地址仍为 **0x08004800**，IAP 为 **0x08000000**。禁止将 App bin 裸写到 IAP 地址，后续烧录沿用仓库安全脚本。
