# todo.md 优化规划与推进记录 2026-05-22

## 范围

依据 `103 + 309/Project/Source/todo.md` 中“todo优化”一节，本轮目标是在不影响量产功能、通信协议和参数持久化布局的前提下，继续推进资源占用和软件架构优化。

## 执行规划

1. 先处理低风险裁剪：已经由配置关闭的模块，其运行态变量也必须跟随配置关闭，避免“功能禁用但 RAM 仍占用”。
2. 收敛 AFE 零电流校准内部状态：不改零点学习算法、不改电流单位换算、不改充放电方向，只把外部无需访问的变量收为文件内状态。
3. 用 Release map 固化验证：禁用 SCI2/SCI3 后，map 中不允许再出现对应端口的运行态缓存和状态变量。
4. 对 SOC 表、K/B 校准等牵涉上位机协议和 EEPROM 数据布局的项先做引用分析，再决定是否引入编译配置隔离。
5. 每次优化后执行 `FD_Release` 构建、`tools/project_check.py -q` 和 `git diff --check`。

## 本轮已完成

### 0. SOC 表改为编译期固定表

新增配置：

```c
PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
```

当前默认值为 0。关闭后：

- SOC OCV 表由 `PROJECT_CFG_BAT_CHEMISTRY` 在编译期决定。
- 当前三元锂配置只保留 `SocTable_TernaryLi`。
- `SOC_Table_Set`、`SOC_Table_Default`、`SOC_Table_CanSet` 不再进入 Release 镜像。
- 0x2200 的 SOC 表读取仍返回当前编译期表，保持上位机可读。
- 0x2200 的 0x10 写表返回否定应答，避免现场写入一个固件不再使用的运行时表。

这一步兑现了 todo 中“SOC 校准表格在编译时确定，磷酸铁锂和三元锂宏定义决定”的方向，同时保留读表兼容，不改变默认三元锂量产 SOC 表。

### 1. SCI2 禁用后同步裁剪运行态变量

当前量产配置：

```c
#define PROJECT_CFG_SCI2_ROLE 0
#define PROJECT_CFG_SCI3_ROLE 0
```

SCI3 原先已经基本按 `_COMMOM_UPPER_SCI3` 隔离；SCI2 仍保留了这些 RAM/状态：

```text
g_stCurrentMsgPtr_SCI2
g_stSciPort2
gu16_CommuErrCnt_SCI2
gu8_TxEnable_SCI2
gu8_TxFinishFlag_SCI2
```

本轮将 SCI2 的运行态结构、发送状态、错误计数、busy 判断和初始化调用全部挂到 `_COMMOM_UPPER_SCI2` 下。禁用端口时，业务运行态不再进入 Release 镜像。

保留项：

```text
USART2_IRQHandler
```

原因是启动向量会强引用该中断入口。保留 2 字节空处理函数，比删除后落到启动文件弱符号默认死循环更稳妥。

### 2. AFE 零电流校准内部状态收敛

以下变量只在 `DataDeal.c` 内部使用，外部模块不需要直接读写：

```text
g_i32AfeCurrentZeroOffsetRawQ4
g_i32AfeCurrentLastRawSigned
g_u8AfeCurrentZeroStableCnt
g_u8AfeCurrentZeroReady
g_u8AfeCurrentZeroState
```

本轮改为文件内 `static` 状态，并从 `DataDeal.h` 删除 extern 暴露。外部仍通过已有接口和观察结构获取必要诊断信息：

```text
AfeCurrent_PrepareStartupZero()
AfeCurrent_IsStartupZeroDone()
AfeCurrent_StartupZeroCal()
g_stAfeCurrentObserve
g_u32AfeCurrentSampleSeq
```

`u32_ChgCur_mA` 和 `u32_DsgCur_mA` 只在 `DataLoad_Current()` 内用于临时承接计算结果，已改为局部变量，减少全局 RAM 占用。

### 3. Release 检查脚本增强

`tools/project_check.py` 增加 map 检查：

- `PROJECT_CFG_SCI2_ROLE == 0` 时，Release map 不允许出现 SCI2 运行态符号。
- `PROJECT_CFG_SCI3_ROLE == 0` 时，Release map 不允许出现 SCI3 运行态符号。

这样后续新增串口功能时，如果只关了宏但忘记裁剪 RAM，发布检查会直接失败。

## 本轮验证结果

构建命令：

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' `
  -b 'E:\TODO\103 + 309 - 副本\103 + 309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx' `
  -t 'FD_Release' `
  -j0 `
  -o 'E:\TODO\103 + 309 - 副本\logs\build_fd_release_todo_opt.log'
```

构建结果：

```text
Program Size: Code=55040 RO-data=3332 RW-data=936 ZI-data=5464
0 Error(s), 0 Warning(s)
```

map 汇总：

```text
Total RO  Size    58372 bytes (57.00KB)
Total RW  Size     6400 bytes ( 6.25KB)
Total ROM Size    58632 bytes (57.26KB)
```

相对上一轮优化后的基线：

```text
RO  减少 636 bytes
RAM 减少 480 bytes
ROM 减少 652 bytes
```

自动检查：

```text
Project check summary:
  OK:       113
  Warnings: 0
  Errors:   0
```

## 暂不直接删除的项

### 1. 运行时 SOC 写表功能

已通过 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE` 隔离。默认关闭后，运行时表不再进入 Release 镜像。

保留的兼容行为：
- 上位机仍可通过 0x03 读取 0x2200 的 SOC 表内容。
- 读取内容来自当前编译期电芯体系表。

关闭后的变化：
- 上位机 0x10 写 0x2200 SOC 表会返回否定应答。
- 历史 Flash/EEPROM 中的 `OtherElement.u16Soc_TableSelect` 不再决定实际 OCV 表，实际表由 `PROJECT_CFG_BAT_CHEMISTRY` 决定。

### 2. 电压/温度 K/B 校准数组

当前 `g_u16CalibCoefK[]`、`g_i16CalibCoefB[]` 不只服务电流，还参与总压、温度和上位机校准寄存器读写。直接删除会改变现场校准能力和 EEPROM 参数布局。

电流 K/B 路径当前已经通过编译期宏关闭：

```c
#define AFE_CURRENT_KB_CALIB_ENABLE 0U
```

若要进一步裁剪全部 K/B 校准，应先定义新的量产策略，明确是否放弃上位机校准寄存器兼容和历史 EEPROM 参数兼容。

## 后续建议

1. 串口协议继续按角色拆分：SCI1/SCI2/SCI3 的 common upper、bootloader、SOC 测试角色已经有宏基础，后续应继续把对应寄存器表和运行态按角色隔离。
2. SOC 表已完成默认固定表裁剪；如后续确需现场写表，可把 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE` 改为 1 并重新验证协议行为。
3. K/B 校准裁剪应先确定产品策略：如果量产完全不允许现场校准，再统一裁剪数组、EEPROM 地址、0x03/0x06/0x10 处理。
4. AFE 零电流后续优化应保持算法可观测：保留 `g_stAfeCurrentObserve`，不要为了少量 RAM 删除关键诊断字段。
