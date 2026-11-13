# 主 BMS 固件清理（2026-11-13）

适用 STM32F103CBT6、SH367309 主工程，保留 `FD_Release`、`FD_Debug` 两个目标。

## 保留范围

- `sys_time` 及全部既有普通成员；当前关闭的 IRQ/Flash 自测条件成员随对应实现移除，普通成员顺序不变。
- `__VIRTURE_CURRENT__`、`sys_time.isdebugenable/CHG/DSG`；默认不启用虚拟电流。
- AFE 每 200 ms 任务的 PB15 翻转及既有低功耗 LED 电平。
- `Monitor_TempBreak()` 当前工作区实现；加热源文件和参数头保持原样，加热仍未加入有效 Keil 目标。
- 真实采样、校零和 K/B、SOC、保护与反向放行、唤醒与恢复、IAP、日志和客户统计。

## 删除与简化

删除 EEPROM.h 的 29 个旧外置 EEPROM 宏，保留 4 个仍被序列化和协议使用的数量宏；清理未用延时/保护滤波/旧配置宏、bq 和第三串口分支、无调用接口、空包装和注释旧实现。删除旧 ShortFunc 模块，真实短路配置继续由 SH367309 参数应用路径负责。

移除 IRQ 插桩及默认向量观测钩子、Flash 压力自测和休眠阻塞、AFE raw 注入、V25～V32 诊断覆盖、0xC100～0xC10F 读区、无消费者的调度观测和缓存；删除 `__SOC_5_PROTECT_` 的低 SOC 关断/休眠策略。Cortex 异常保留两字备份原因，Release/Debug 均复位；启动时继续清除 DBGMCU 低功耗/看门狗调试位。

UART 删除无调用的唤醒帧暂存状态，正常帧仅在 RUN 解析；错误清除、坏帧丢弃、超时、TC 应答及 IAP 顺序保留。

Keil 移除不参与业务的标准外设 .c 条目，官方库文件继续保留。清理旧 Trace/Endurance、cppcheck 缓存和构建日志。project_check.py 只按当前主固件检查，不再要求已删除的调试模块、老化实现或历史文档；独立通信工具约束保留在显式 `--comm-tool` 检查中。默认 workflow 使用真实 C 的 SOC/电流测试，退出陈旧的 Python 镜像回放。

## 兼容边界

- 0xC100～0xC10F 返回非法地址；0x1102/0x1103 的功能号 7 返回非法数据。功能号 10 的 DEEP 保留。
- 不存在单体始终发送 61001。正式报告长度、身份信息、故障原因和保留字偏移保持原值。
- App 为 [0x08004800,0x0801C000)，SRAM 邮箱为 0x20004FE0；原测试区继续保留。
- 配置、SOC/老化共享状态、加热参数、日志、客户统计的长度、页地址、CRC、提交及掉电恢复不变。不因本次清理增加参数版本或触发新的清空。

## 验证入口

`project_check.py -q`、`test_storage.py`、`test_protection_refactor.py`、`test_uart_protocols.py`、`test_sleep_policy.py`、`run_soc_host_c_test.py`、`run_current_calibration_host_c_test.py`；均位于 tools。UART 用真实序列化验证 61001、D200 原因/保留区位置、退役接口和 DEEP10；电流用真实采样/校准函数验证虚拟电流关闭/开启。

`check_bms_no_bin.py --target FD_Release` / `FD_Debug` 在用户临时区生成 AXF/MAP，关闭 BIN/HEX 和构建钩子并校验链接边界。host 和构建不能替代板上充放电、MOS、STOP 唤醒、掉电和功耗验收。

## 本次验证结果

已分别验证当前工作区，以及排除原有未提交修改后的暂存版本：7 个相关检查/host 测试入口全部通过。两份源码的 Release/Debug 均为 0 错误、0 警告，编译器为 ARMCC 5.06 update 7，MCU 为 STM32F103CB。

| 源码 | 目标 | Code | RO | RW | ZI | App 结束地址 |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| 本次提交内容 | FD_Release | 37660 | 3184 | 1016 | 5808 | 0x0800EB84 |
| 本次提交内容 | FD_Debug | 46064 | 3216 | 1016 | 5808 | 0x08010C78 |
| 当前工作区 | FD_Release | 37780 | 3184 | 1016 | 5808 | 0x0800EBFC |
| 当前工作区 | FD_Debug | 46216 | 3216 | 1016 | 5808 | 0x08010D10 |

每次构建约 2～3 秒，只生成用户临时区中的 AXF/MAP；未生成 BIN/HEX，未连接或烧录实板。工作区既有串数、采样电阻档位、休眠时间、参数版本、电压转换、日志和温度断线修改仍保留在工作区，未纳入本次提交。
