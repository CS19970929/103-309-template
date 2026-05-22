# 项目资源与软件架构优化说明 2026-05-22

## 目标

在不改变产品功能和量产配置的前提下，降低 Release 固件资源占用，减少调试代码对量产镜像的影响，并把关键发布检查固化到脚本中。

## 本次优化内容

### 1. 启动 Flash 诊断打印改为配置隔离

新增配置：

```c
PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE
```

默认值为 0，Release 构建中 `Project_BuildGuard.h` 会强制禁止打开。

原来的 `StorageFlash_PrintBootCheck()` 在启动阶段无条件执行，内部使用多处 `printf`，会把 `printf`、`_printf_core`、浮点格式化相关库成员拉入 Release 镜像。现在该调用只在定义 `FLASH_BOOT_PRINT_ENABLE` 时进入：

```c
#ifdef FLASH_BOOT_PRINT_ENABLE
    StorageFlash_PrintBootCheck();
#endif
```

功能影响：

- 量产主功能不变。
- Release 默认不再串口打印启动 Flash 槽状态。
- 需要调试启动存储状态时，可在 Debug/Factory 构建中打开 `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE`。

### 2. NTC 温度表 const 化

`iSheldTemp_10K_NTC[141]` 是只读查表数据，当前只有读取使用。已从 RAM `.data` 移到 Flash `.constdata`：

```c
const UINT16 iSheldTemp_10K_NTC[141] = { ... };
extern const UINT16 iSheldTemp_10K_NTC[141];
```

收益：

- 释放约 282 字节 RAM。
- 减少启动时 RW Data 拷贝。
- 温度阈值计算逻辑不变。

### 3. 电流 K/B 校准关闭路径改为编译期隔离

原逻辑通过 `static const UINT8 s_u8AfeCurrentKbCalibEnable = 0U` 关闭 K/B 电流校准，编译器会把后续校准代码判定为不可达并产生告警。

现在改为：

```c
#define AFE_CURRENT_KB_CALIB_ENABLE 0U
```

并用 `#if AFE_CURRENT_KB_CALIB_ENABLE` 包住关闭状态下不可达的校准代码。当前默认行为仍然是不启用 K/B 电流校准，输出结果不变。

### 4. 清理无效编译告警

清理了以下不会影响功能的告警来源：

- 未使用的 `i32VCellTotle`
- JTAG 禁用函数中仅在 `#if 0` 代码块里使用的 `GPIO_InitStructure`
- 已注释逻辑对应的 `su8_IdischgOcp2_Flag`
- 未使用的旧电压唤醒辅助函数 `isVol_cuv()` / `isVol_cov()`

目的不是节省大量代码，而是让 Release 构建日志保持干净，后续新增告警更容易被发现。

### 5. 删除无效旧充电/负载与 IO 控制模块

删除以下旧模块：

```text
103 + 309/Project/Source/ChargerLoadFunc.c
103 + 309/Project/Source/ChargerLoadFunc.h
103 + 309/Project/Source/IO_Control.c
103 + 309/Project/Source/IO_Control.h
```

处理方式：
- `DataDeal.c` 中的 `App_DI1_Switch()` 调用被删除；该函数当前实现只有 `return`，无实际业务动作。
- `conf.c` 中的旧 `Init_ChargerLoad_Det()` 改为复用已有 `InitWakeUp_Base()`，继续保持 PA0 充电唤醒 EXTI 配置。
- `EXTI0_IRQHandler()` 保留清 pending 行为，去掉无人消费的 `ChargerLoad_Func` 标志位写入。
- `OPEN/CLOSE` 是仍被 `Sci_Upper.c` 和 `System_Monitor.c` 使用的通用状态枚举，已从旧 `IO_Control.h` 移到 `main.h`。
- Keil 工程文件已移除 `ChargerLoadFunc.c` 和 `IO_Control.c` 两个编译项。

未删除：
- `EEPROM.c/.h` 当前仍提供参数读写、Flash 参数持久化和事件记录接口。
- `Heat_Cool.c/.h` 当前仍提供加热/制冷参数、状态和运行控制。
- `main.h` 当前仍是大量源文件的公共聚合头文件，不能直接删除。

### 6. 发布检查脚本增强

`tools/project_check.py` 新增 Release map 检查：

- `FD_Release.map` 的 `Load Region LR_IROM1` 必须从 `0x08004800` 开始。
- `Execution Region ER_IROM1` 必须从 `0x08004800` 开始。
- ROM 超过 96KB 时给出告警。
- RAM 超过 14KB 时给出告警。
- map 比关键源码旧时给出告警。
- Release 最终链接组件表中出现 `printf` 库成员时给出告警。

这把 App 地址、资源阈值、量产调试输出隔离这些规则固化到了脚本中，避免依赖单次对话记忆。

## 资源变化

优化前最新 Release map：

```text
Total RO  Size    63340 bytes (61.86KB)
Total RW  Size     7176 bytes ( 7.01KB)
Total ROM Size    63900 bytes (62.40KB)
```

优化后 Release map：

```text
Program Size: Code=55424 RO-data=3584 RW-data=984 ZI-data=5896
Total RO  Size    59008 bytes (57.63KB)
Total RW  Size     6880 bytes ( 6.72KB)
Total ROM Size    59284 bytes (57.89KB)
```

净变化：

```text
RO  减少 4332 bytes
RW  减少 296 bytes
ROM 减少 4616 bytes
```

map 证据：

```text
Removing flash.o(i.StorageFlash_PrintBootCheck), (672 bytes).
iSheldTemp_10K_NTC  0x08012d46  Data  282  sh367309_func.o(.constdata)
```

`printf` 库成员不再出现在最终 Library Totals 中。
`chargerloadfunc.o` 和 `io_control.o` 不再出现在最新 `FD_Release.map` 中。

## todo 优化补充 2026-05-22

本轮继续处理 `todo.md` 中“todo优化”一节的低风险项，详细规划和取舍见 `TODO_OPTIMIZATION_PLAN_2026-05-22.md`。

已落地内容：
- `PROJECT_CFG_SCI2_ROLE == 0` 时，SCI2 的运行态缓存、发送状态、错误计数和端口运行结构不再进入 Release 镜像。
- AFE 零电流校准的内部状态改为 `DataDeal.c` 文件内 `static`，从 `DataDeal.h` 删除外部暴露。
- `u32_ChgCur_mA`、`u32_DsgCur_mA` 改为 `DataLoad_Current()` 内部局部变量。
- `tools/project_check.py` 增加禁用 SCI2/SCI3 后运行态符号不得出现在 Release map 中的检查。
- `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE` 默认关闭，SOC 表由 `PROJECT_CFG_BAT_CHEMISTRY` 编译期决定。
- 固定三元锂配置下，`SOC_Table_Set`、`SOC_Table_Default`、`SOC_Table_CanSet` 和非当前电芯 SOC 表不再进入 Release 镜像。

最新 Release 结果：

```text
Program Size: Code=55040 RO-data=3332 RW-data=936 ZI-data=5464
Total RO  Size    58372 bytes (57.00KB)
Total RW  Size     6400 bytes ( 6.25KB)
Total ROM Size    58632 bytes (57.26KB)
```

相对上一轮优化后的基线，继续减少：

```text
RO  636 bytes
RAM 480 bytes
ROM 652 bytes
```

保留说明：
- `USART2_IRQHandler` 仍保留 2 字节空入口，因为启动向量强引用该符号，删除后禁用端口误触发中断可能落入启动文件默认死循环。
- 0x2200 的 SOC 表读取仍保留，但读取内容来自当前编译期电芯体系表；0x10 写 SOC 表在运行时表关闭时返回否定应答。
- 电压/温度 K/B 校准数组仍参与总压、温度和上位机校准寄存器路径；电流 K/B 当前已通过 `AFE_CURRENT_KB_CALIB_ENABLE 0U` 编译期关闭。

## 构建验证

构建命令：

```powershell
& 'C:\Keil_v5\UV4\UV4.exe' `
  -b 'E:\TODO\103 + 309 - 副本\103 + 309\Project\Users\CommomSH367309_16series_103RCT6_C.uvprojx' `
  -t 'FD_Release' `
  -j0 `
  -o 'E:\TODO\103 + 309 - 副本\logs\build_fd_release_optimization.log'
```

构建结果：

```text
0 Error(s), 0 Warning(s)
Build Time Elapsed: 00:00:02
```

项目检查：

```powershell
py -3 tools\project_check.py -q
```

结果：

```text
OK:       111
Warnings: 0
Errors:   0
```

## 后续建议

- `sci_upper.o` 仍是最大业务模块，后续可在完整协议回归前提下梳理 `0x03/0x06/0x10` 打包逻辑。
- `socenhance.o` 仍是第二大业务模块，若产品固定三元锂，可评估是否通过配置裁剪非当前电芯体系表。
- 当前 RAM 仍有余量，不建议盲目调小 3KB 栈；如需优化，应先做栈水位测试。
- 启动 Flash 槽状态建议逐步迁移到上位机只读寄存器，而不是恢复 Release 串口打印。
