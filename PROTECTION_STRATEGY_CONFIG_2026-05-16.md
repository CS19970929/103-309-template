# 软硬件保护策略配置说明 2026-05-16

本文以当前源码为准，说明本工程保护功能的归属、配置开关和后续移植到其他 AFE/MCU 项目时的一键切换方法。

## 1. 当前 309 项目的保护归属

当前项目使用 SH367309 AFE，真实电芯过压、欠压、充放电过流、短路、充放电温度等保护主要由 AFE 硬件完成。MCU 侧负责：

1. 配置 AFE 保护参数。
2. 周期读取 AFE 状态寄存器。
3. 把 AFE fault bit 镜像到 `g_stCellInfoReport.unMdlFault_Third`。
4. 记录故障历史，供 RS485、CAN、日志和显示读取。

因此当前量产默认不运行 `Fault.c` 中的 `App_WarnCtrl()` 软件保护轮询。`Fault.c` 仍保留，因为它还提供：

- `PRT_E2ROMParas` 保护参数结构和通信读写布局。
- `FaultWarnRecord2()` 故障记录入口。
- 未来移植到 bq769x0 等项目时可复用的软件保护判断逻辑。

## 2. 一键保护模式

保护模式只改一个宏：

```c
#define PROJECT_CFG_PROTECTION_MODE 0
```

位置：`103 + 309/Project/Source/conf/Project_Config.h`

模式定义在 `Project_Protection.h`：

| 值 | 模式 | AFE 硬件故障镜像 | MCU 软件保护轮询 | 当前用途 |
| --- | --- | --- | --- | --- |
| `0` | `PROJECT_PROTECTION_MODE_AFE_HARDWARE_ONLY` | 开启 | 关闭 | 当前 SH367309 量产默认 |
| `1` | `PROJECT_PROTECTION_MODE_MCU_SOFTWARE` | 关闭 | 开启 | bq769x0 或弱硬件保护 AFE 项目 |
| `2` | `PROJECT_PROTECTION_MODE_HYBRID` | 开启 | 开启 | AFE 硬件保护 + MCU 二次判定 |

`Project_Features.h` 会派生出：

```c
PROJECT_FEATURE_AFE_HARDWARE_PROTECTION
PROJECT_FEATURE_SOFTWARE_PROTECTION
```

业务代码不直接判断 `PROJECT_CFG_PROTECTION_MODE`，只读派生 feature。

如果 `PROJECT_CFG_PROTECTION_MODE` 填成非 `0/1/2` 的值，`Project_Protection.h` 会直接编译报错，不会静默回退到某个默认模式。

## 3. 当前源码运行关系

### 3.1 AFE 硬件保护路径

```text
App_AFEGet()
  -> App_SH367309()
     -> App_SH367309_Monitor()
        -> Fault_ChangeToMCU() [PROJECT_PROTECTION_USES_AFE_HARDWARE]
           -> g_stCellInfoReport.unMdlFault_Third
           -> FaultWarnRecord2()

rtc_sleep()
  -> rtc_monitor_sh367309()
     -> Fault_ChangeToMCU() [PROJECT_PROTECTION_USES_AFE_HARDWARE]
```

运行态采样路径和 RTC 唤醒监测路径都受同一个 `PROJECT_PROTECTION_USES_AFE_HARDWARE` 控制。该路径只镜像和记录 AFE 已经判定的硬件保护状态，不再由 MCU 重新计算保护阈值；切到 `PROJECT_PROTECTION_MODE_MCU_SOFTWARE` 后，低功耗唤醒也不会继续调用 AFE fault mirror。

`SH367309_DataDeal.c` 只负责 309 AFE 保护参数和 MTP 写入转换，已经改为显式依赖 `DataDeal.h`、`Flash.h`、`I2C_AFE1.h`、`SH367309_Func.h`、`Sci_Upper.h`、`System_Init.h` 和 `System_Monitor.h`，不再通过 `main.h` 间接获得全工程对象。后续替换为 bq769x0 等 AFE 时，这一层可以直接替换成新 AFE 的参数转换/寄存器写入模块。

### 3.2 MCU 软件保护路径

```text
Runtime_RunFrontTasks()
  -> App_WarnCtrl() [PROJECT_FEATURE_SOFTWARE_PROTECTION]
     -> App_*Check()
        -> g_stCellInfoReport.unMdlFault_*
        -> FaultWarnRecord2()
```

该路径使用 `Fault.c` 内的软件比较、滤波和恢复逻辑，适合后续其他 AFE 只提供采样值、不承担完整硬件保护的项目。`Fault.c` 已脱离 `main.h`，显式依赖 `DataDeal.h`、`Sci_Upper.h`、`System_Init.h`、`System_Monitor.h` 和 `PubFunc.h`，后续移植时可以按这些输入边界替换采样模型、10ms tick 和错误记录。

## 4. 移植建议

### 4.1 继续使用 SH367309

保持：

```c
#define PROJECT_CFG_PROTECTION_MODE 0
```

不要删除 `Fault.c`，因为通信参数、故障记录和日志仍依赖其中的数据结构和记录入口。

### 4.2 移植到 bq769x0 / 其他 AFE

建议先切换：

```c
#define PROJECT_CFG_PROTECTION_MODE 1
```

同时需要完成：

1. 新 AFE 驱动填充 `g_stCellInfoReport` 的电压、电流、温度采样值。
2. 核对 `PRT_E2ROMParas` 的阈值单位和通信地址是否沿用。
3. 验证 `App_WarnCtrl()` 的 10ms/滤波节拍是否满足目标产品。
4. 根据目标板 MOS 控制方式适配 `IO_Control.c` 或新的 driver port。

### 4.3 混合保护

当 AFE 有基础硬件保护，但 MCU 仍需要做更保守的策略保护时，使用：

```c
#define PROJECT_CFG_PROTECTION_MODE 2
```

注意混合模式下 AFE 状态镜像和 MCU 软件检查都会写 `g_stCellInfoReport.unMdlFault_*`，需要在上板测试中确认故障优先级和恢复时序符合产品需求。

## 5. 验收

`tools/project_check.py` 已固化以下检查：

1. `Project_Config.h` 必须有 `PROJECT_CFG_PROTECTION_MODE`。
2. `Project_Protection.h` 必须定义三种保护模式，并对非法模式编译期报错。
3. `Runtime.c` 必须通过 `PROJECT_FEATURE_SOFTWARE_PROTECTION` 调度 `App_WarnCtrl()`。
4. `SH367309_Func.c` 的 `Fault_ChangeToMCU()` 必须受 `PROJECT_PROTECTION_USES_AFE_HARDWARE` 控制。
5. `rtc_sleep.c` 的 RTC 唤醒故障镜像也必须受 `PROJECT_PROTECTION_USES_AFE_HARDWARE` 控制。
6. `Fault.c` 必须使用显式依赖，不再通过 `main.h` 间接获得软件保护输入。
7. Release 默认必须保持 `PROJECT_CFG_PROTECTION_MODE=0`，对应当前 309 AFE 硬件保护方案。
