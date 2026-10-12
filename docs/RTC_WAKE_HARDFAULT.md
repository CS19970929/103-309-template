# RTC 唤醒异常实板排查与修复

日期：2026-10-12。源码基线：`c91e93d2591b73ef3d0c153780c5cc4bb9337605`，另有用户尚未提交的配置修改。故障板用户标识为“103C8”；探针读到 Cortex-M3 r2p1、128 KB Flash，尚未确认芯片厂商。本结论针对当前板，不认定为 ST 官方勘误。

## 唤醒后停滞与 HardFault

板上原 AXF 与 Keil Objects 中的原 AXF 经 `verify_image` 全部一致。原 AXF SHA256 为 `afc3ea4d7f5fe2230ab063cd0a70dacd60c939d5e18f36943e59a2c463a216e2`。

在原 `PWR_EnterSTOPMode` 内对 `SCB->SCR` 写入设置硬件观察点：进入 STOP 的写入后 `SCR=4` 正常；RTC 唤醒后的清除写入后，却观测到 `R3=0xFFFFFFFF`、`SCR=0x16`。这一步尚无 Fault，之后中断返回无法正常继续。主动暂停时读到 `PC=0xFFFFFFF8`，再继续会出现 `CFSR=1`（IACCVIOL）、`HFSR=0x40000000`。

因此必须区分“STOP 返回路径已异常”与“调试恢复无效 PC 后进入 HardFault”，不能把最后的 Fault 堆栈直接当作最早出错指令。仅把异常 SCR 恢复为 0，就能正常返回 RTC 调用者；增加 Flash 等待周期、关闭预取、保留 SWD 或禁用一个 RTC IRQ 均未解决该异常。

控制实验只改变执行位置：把原函数完整 64 字节（含 literal pool）复制到 RAM，以相同参数执行，正常返回。重复对照结果：

| 执行方式 | RTC 唤醒结果 | SCR | CFSR |
| --- | --- | --- | --- |
| Flash，等待周期 1 | 返回断点未命中，暂停后 PC 无效 | `0x16` | 暂停时 0 |
| RAM，原函数原指令 | 正常返回 | 0 | 0 |
| Flash，原配置 | 再次失败 | `0x16` | 暂停时 0 |

这证明当前板上把这段 STOP 返回指令放到 RAM 可消除故障；尚不能仅凭该结果确定具体硅片内部原因。

正式修复通过 `Users/rtc_stop.sct` 将标准库 `stm32f10x_pwr.o(i.PWR_EnterSTOPMode)` 放到 RAM。继续调用同一标准库 API，不修改其寄存器实现。启动 scatter-loading 自动复制，链接器自动生成跨区调用 veneer；无需调试器临时复制。MAP 中函数执行地址为 `0x20000000`，长度 `0x40`。

工程关闭自动生成内存布局，指定此 scatter；设置含义参照 [Keil Arm Linker 文档](https://www.keil.com/support/man/docs/uv4/uv4_dg_adsld.asp)。

APP 起始地址仍为 `0x08004800`，RAM 范围、Flash/EEPROM 格式和 Bootloader 行为未改变。新增占用主要为 64 字节 RAM 代码。沿用原工程 ROM 上限配置；当前 APP 和原有 `0x0801FC00` 标志均依赖超过标称 64 KB 的地址空间，128 KB 的探针读值不能替代芯片厂商容量承诺。

## 3 秒后恢复 RUN

排除上述停滞后，第一次 RTC 唤醒正常；第二轮 GPIO 准备期间，抓到 `g_irq_t=5`（按键），PA9 实际仍为高。此前 `IOstatus_RTCMode()` 把 GPIOA 全部改为模拟输入，而上一轮 EXTI9 仍然使能，因此生成了假的按键下降沿。

修复为 RTC GPIO 准备时保留 PA0、PA9、PB12 唤醒输入，以及原有 PB14 CTLC 输出；同时保留 PA13/PA14 SWD 引脚。其他原先需要进入模拟模式的引脚保持原处理。无需为了掩盖假边沿而清除真实唤醒挂起位。

正常 RTC 每 3 秒唤醒一次 MCU 进行检查，再次进入 STOP；持续 RTC 模式不意味着 MCU 永不短暂唤醒。本次保留用户当前的 3 秒闹钟和 10 秒空闲准入配置。

## 验证

- `python -B tests/test_rtc_gpio.py`：直接编译生产 `IOstatus_RTCMode()`，GPIO 模型模拟已使能按键 EXTI；原代码触发假边沿并失败，修复后重复准备通过。此项为软件模型验证。
- `python -B tests/test_sleep.py`：RTC 备份域、当前配置的 3 秒闹钟、回绕、有界失败、休眠竞争与时钟恢复测试通过。
- `python -B tests/check_armcc_no_bin.py`：61 个生产对象编译链接通过，0 error、11 个既有 warning；Code 56520、RO-data 2948、RW-data 1376、ZI-data 6104。脚本使用工程指定 scatter，并检查 STOP 函数确实位于 RAM。没有生成 BIN。
- 下载并校验最终 AXF 59992 字节，SHA256 `489e81f626a0e8520b02e8745238dfffcee1a55ec10961230475d5d4d3d4bf34`。复位后由真实启动路径复制 RAM 函数。
- 20 次连续自然 RTC 唤醒，每次命中 STOP 返回处：`mode=HICCUP`、`g_irq_t=NO_IRQ`、RTC 计数递增、`SCR/CFSR/HFSR=0`。首轮清除历史复位标志，随后无新增复位标志。
- 软件触发 EXTI9 后正确进入 `sleep_restore_run`，`g_irq_t=5`；验证软件退出路径，未替代实体按键、PA0/PB12 电气与功耗测量。
- 撤掉全部断点并关闭 `DHCSR.C_DEBUGEN` 后运行约一分钟，重新使能调试并在下次自然返回处读取：RTC 计数由 21 增至 44，仍为 RTC 模式，`NO_IRQ`，`SCR/CFSR/HFSR=0`，无新增复位标志。STOP 期间关闭调试后的 AHB 读值会失真，验证采用回连后的返回断点数据，未把睡眠中的读值当作有效 RAM 数据。
- 烧录前后全 Flash 比较，Bootloader、APP 擦除页以外区域及 `0x0801FC00` 标志页完全一致。

临时原始记录位于 `%LOCALAPPDATA%\CodexTemp\103-309-template\rtc-fault-diagnosis`：`stop-comparison.json`、`fixed-validation.json`、`flash-preservation.json` 以及烧录前后 Flash 备份。编译产物位于 `%LOCALAPPDATA%\CodexTemp\103-309-template\armcc-no-bin\run-_0up40zp`。这些不是项目提交文件。

交付时清除全部断点/观察点、关闭 CPU 调试使能、释放 ST-Link，板上保留已校验的修复版本运行。匹配的 AXF 已同步至工程既有 `Users/Objects/CommomSH367309_16series_103RCT6_C.axf`，MAP 同步至 `Users/Listings`，原文件备份在上述用户临时区。Keil 打开了旧工程时需重新打开工程以载入新的 scatter 配置；本版本地址已变化，不应再沿用旧的固定地址断点。

未做充放电负载、全部外部唤醒源、长时间温度/电压边界或休眠电流验收；本次实板证据针对当前供电与接线条件下的 RTC 故障复现链。
