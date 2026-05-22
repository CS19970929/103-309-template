# 项目架构重构说明 2026-05-22

## 目标

本轮重构先处理低风险、可验证的结构问题：入口文件职责过重、GPIO 初始化重复、MOS 上电策略散落在 `main.c`。不改变 AFE 初始化顺序、低功耗唤醒流程、上位机协议和量产配置。

## 已完成调整

### 1. MOS 上电控制独立成模块

新增：

```text
103 + 309/Project/Source/MosStartup.c
103 + 309/Project/Source/MosStartup.h
```

职责边界：

- `MosStartup_Is5vChargeActive()`：读取 5V 充电唤醒输入。
- `MosStartup_OpenChargeCloseDischarge()`：开充电管、关放电管。
- `MosStartup_OpenDischargeCloseCharge()`：开放电管、关充电管。
- `MosStartup_EnterFactoryMode()`：处理出厂老化时的 MOS 状态。
- `MosStartup_ApplyInitialState()`：供 AFE 初始化后应用上电初始 MOS 状态。

原 `open_chg_close_dsg()`、`open_dsg_close_chg()`、`enter_fac_mode()` 旧调用名仍通过 `MosStartup.h` 宏兼容，避免一次性改动 `DataDeal.c`、`FactoryAging.c`、`I2C_AFE1.c` 等业务模块。

收益：

- `main.c` 只保留启动入口、设备初始化和主循环。
- MOS 写寄存器动作集中到 `MosStartup_WriteMosState()`，减少重复写 `CADCON/CHGMOS/DSGMOS/MTPWrite/GPIO_MCC_C`。
- 后续如果调整 5V 充电、老化模式、MOS 上电策略，只需要优先检查 `MosStartup.c`。

### 2. GPIO 初始化公共化

`conf.c` 新增公共 helper：

```text
Conf_InitRunSharedIo()
Conf_InitMainPowerRails()
Conf_PrepareStopEntry()
Conf_InitAllPortsAnalog()
```

调整后：

- `InitIO()` 和 `InitIO_rtc()` 共享运行态 GPIO 初始化，不再各自维护一份充电输入、MCC、MCU_WK、SW、RF、ADC、主电源轨和调试 LED 初始化代码。
- `IOstatus_Base()` 复用 STOP 前处理、全端口模拟输入和主电源轨关闭逻辑。
- `IOstatus_RTCMode()` 保留原有 RTC 模式的特殊 pin 例外，只复用 STOP 前置处理，避免改变低功耗硬件边界。

收益：

- 删除重复 GPIO 配置块，后续新增或修正运行态 IO 时不需要同时改两处。
- 保持写 GPIO 电平再切输出模式的顺序，降低上电/休眠瞬间毛刺风险。

### 3. 自动检查固化

`tools/project_check.py` 增加检查：

- `FD_Release` 和 `FD_Debug` Keil 目标必须包含 `MosStartup.c`。
- `main.c` 不允许重新承载 `MosStartup_ApplyInitialState()` 或旧 MOS 控制函数实现。
- `conf.c` 必须保留共享 GPIO helper，防止后续回退到复制粘贴式初始化。

## 本轮未做的事

- 未改动当前已有未提交变更的文件：`DataDeal.c`、`EEPROM.h`、`Fault.*`、`RTC.c`、`Sci_Upper.c`、`todo.md`。
- 未调整 App 烧录地址、scatter、IAP 边界和 SOC 测试模式隔离规则。
- 未大规模改写协议、SOC 算法、AFE 采样和低功耗状态机；这些模块需要逐个配合构建、map 和硬件验证推进。

## 验证结果

Release 构建命令：

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' `
  -b 'E:\TODO\103 + 309 - 副本\103 + 309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx' `
  -t 'FD_Release' `
  -j0 `
  -o 'E:\TODO\103 + 309 - 副本\logs\build_fd_release_arch_refactor.log'
```

构建结果：

```text
Program Size: Code=46572 RO-data=2824 RW-data=808 ZI-data=5256
0 Error(s), 1 Warning(s)
```

说明：

- 相对本轮改前最近一次 Release 日志 `Code=46912 RO-data=2824 RW-data=808 ZI-data=5256`，Code 减少 340 字节。
- `FD_Release.bin` 输出大小为 49640 字节。
- 仍有 1 个 `Sci_Upper.c(377)` 未引用静态函数警告；该文件本轮开始前已有未提交改动，本轮未混入处理。
- `py -3 tools/project_check.py -q` 结果为 `Errors: 0, Warnings: 0`。

## 后续重构顺序建议

1. `Sci_Upper.c`：按 0x03/0x06/0x10 命令拆分读写分发和寄存器映射。
2. `SocEnhance.c`：按采样输入、OCV 校准、满电锚点、显示平滑拆分内部静态函数区域。
3. `conf.c`：继续把正常运行、RTC 唤醒、STOP 前 pin 策略整理成显式策略表，但需要先用示波器确认关键电源轨无毛刺。
4. `main.h`：逐步减少全局聚合 include，把模块公开接口移到各自 `.h`。
