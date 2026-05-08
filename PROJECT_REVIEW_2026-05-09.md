# 项目级代码与文档审查记录 2026-05-09

本文记录本轮对 `103 + 309` 工程的项目级审查结论。范围覆盖源码、Keil 工程配置、通信/存储/SOC 相关工具脚本和根目录旧文档。

## 1. 审查范围

- Keil 工程：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- 主要源码目录：`103 + 309/Project/Source`
- 自动化脚本：`tools/project_check.py`、`tools/soc_replay_test.py`
- 重点模块：RS485/SCI、SOC、AFE MTP 写入、热管理、MOS 驱动、项目构建保护、文档链接

## 2. 已修复问题

### 2.1 发布配置保护

- `PROJECT_CFG_BUILD_PROFILE` 默认值恢复为 `0`，Release 档作为默认发布配置。
- `PROJECT_CFG_WDOG_ENABLE` 默认值恢复为 `1`，Release 档默认启用 IWDG。
- 旧文档中关于 Release 看门狗关闭状态的过期描述已同步为当前配置事实。

### 2.2 RS485 `0x03` 读寄存器边界

原实现只计算起始偏移和读字节数，没有校验读窗口长度。异常起始地址或过大寄存器数量可能导致 `Sci_ACK_0x03()` 从 `g_u8SCITxBuff` 越界取数。

本轮处理：

- 增加 `Sci_GetReadWindowWordCount()`，按真实读区返回窗口长度。
- 增加 `Sci_RangeFits()` 校验读偏移和寄存器数量。
- 对 `reg_count == 0`、响应长度超过 `RS485_MAX_BUFFER_SIZE`、非法地址、跨区读取返回负响应。
- 对 `0xC000 / 0xC001 / 0xC002 / 0xC008` 这类独立只读子块按子块长度校验。

### 2.3 AFE MTP 写入指针错误

`SH367309_SC_DelayT_Set()` 中原代码把数值强制转换成 `UINT8 *` 传给 `MTPWriteROM()`，属于高风险指针错误。

本轮处理：

- 改为使用局部 `UINT8 u8temp_write` 保存待写字节。
- 将 `&u8temp_write` 传入 `MTPWriteROM()`。
- 写入成功后再同步 `SH367309_Reg_Store.u8_MTP_SCV_SCT`。

### 2.4 热管理 3 小时超时计数

`Heat_Control()` 中 `heat_Count` 原为 `UINT8`，但判断阈值是 `60 * 60 * 3 = 10800`，计数器永远达不到目标。

本轮处理：

- `heat_Count` 改为 `UINT16`。
- 超时判断改为 `>= 60U * 60U * 3U`。
- 在加热关闭、功能禁用、超时错误、重新启动加热等路径重置相关静态计数器，避免状态残留。

### 2.5 MOS 驱动状态字段误写

`IODrivers.c` 中一处将 `FORCE_KEEP_MODE` 写入 `MosRelay_Status.bits.b1Status_MOS_DSG`，类型和语义都不匹配。

本轮处理：

- 改为写入 `DriverForceExt.bits.b2_Force_MOS_DSG`，避免状态位和强制控制位混用。

### 2.6 缺失函数声明

部分跨文件调用缺少声明，容易在 ARMCC/C89 风格编译下形成隐式声明风险。

本轮处理：

- 在 `main.h` 增加 `open_chg_close_dsg()`、`open_dsg_close_chg()`、`enter_fac_mode()` 声明。
- 在 `IO_Control.h` 增加 `App_DI1_Switch()` 声明。

## 3. 架构评估

### 3.1 当前主要结构性风险

1. MOS 控制权仍未完全收口。`Heat_Cool.c`、`main.c`、`DataDeal.c`、`SH367309_Func.c` 等路径仍存在直接或间接写 AFE MOS 配置的逻辑。
2. `App_MOS_Relay_Ctrl()` 目前没有在主循环中实际运行，统一 MOS/继电器控制框架和当前产品路径存在双轨。
3. 热管理已通过 `__FUNC__HEAT__` 参与编译，但仍会直接调用 `SH367309_DriverMos_Ctrl(GPIO_CHG, ...)`，需要上板验证不会覆盖保护链路。
4. 低功耗存在 `rtc_sleep.c`、`SleepDeal.c`、主循环 idle sleep 等多套概念，长期建议收敛入口和状态命名。
5. 文档体系较完整，但历史副本文档、混合编码和旧绝对路径会降低长期维护效率。

### 3.2 建议的长期收口方向

- `Protect Layer` 只负责故障置位、恢复条件和保护等级。
- `Policy Layer` 统一计算 `chg_allow`、`dsg_allow`、`sleep_request`、`heat_request`、`can_wakeup_request`。
- `Driver Layer` 作为唯一硬件动作出口，集中调用 `SH367309_DriverMos_Ctrl()`、继电器 GPIO 和低功耗入口。
- 通信写参数只更新 RAM + 设置 dirty/commit 请求，持久化由后台统一处理，并对 EEPROM/AFE/Flash 写后回读校验。
- 旧文档保留历史判断时必须增加“当前复核状态”，避免已过期结论继续误导。

## 4. 剩余风险

- 尚未在 Windows + Keil MDK 下做真实 ARMCC 编译。
- 尚未上板验证 RS485 负响应、AFE MTP 写入、热管理超时、IWDG Release 行为。
- 热管理直接 MOS 控制属于架构级风险，本轮只修复计数器 bug，没有做完整 MOS 控制权收口。
- `Source/todo.md` 中的 SOC 100%、SOC 到 0、RTC 唤醒 CAN 发送、Flash 回读校验仍是后续高优先级专项。

## 5. 本轮验证

- `tools/project_check.py`：通过，`OK 65 / Warnings 0 / Errors 0`。
- `tools/soc_replay_test.py`：通过，`16 / 16`。
- Host `clang -fsyntax-only` 辅助检查：`FD_Debug`、`FD_Release` 涉及的 29 个源码文件均为 `errors=0`。
- Keil 工程解析：可识别 `FD_Release`、`FD_Debug` 两个 target。

## 6. 后续优先级

1. 在 Windows + Keil MDK 下完整编译 `FD_Release` 和 `FD_Debug`。
2. 上板验证 RS485 `0x03` 非法地址、越界长度、正常读区兼容性。
3. 上板验证 `SH367309_SC_DelayT_Set()` 对 MTP `0x0E` 写入成功和失败路径。
4. 梳理热管理对 CHG MOS 的控制权，先改成写意图，再由统一驱动层执行。
5. 继续处理 SOC 末端体验：充满能到 `100%`，低压能到 `0%`，同时避免显示和真实容量严重背离。
