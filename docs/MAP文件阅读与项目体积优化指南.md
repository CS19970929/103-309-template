# MAP 文件阅读与项目体积优化指南

本文档用于说明如何阅读 Keil ARMCC 生成的 `.map` 文件，并结合当前工程最新的 `FD_Release.map` 给出体积和内存优化建议。

## 适用范围

- 工程：`103 + 309`
- 编译器：Keil ARMCC 5.x
- 目标 MCU：STM32F10x 系列
- 重点文件：
  - App map：`103 + 309/Project/Users/Listings/FD_Release.map`
  - Debug map：`103 + 309/Project/Users/Listings/FD_Debug.map`
  - IAP map：`103 + 309/Project/Users/Listings/IAP/FD_IAP.map`
  - App scatter：`103 + 309/Project/Users/Objects/FD_Release.sct`

## 先确认烧录和链接地址

阅读 App map 文件的第一步不是看大小，而是确认链接地址是否正确。

本工程固定规则：

```text
IAP / Bootloader 地址：0x08000000
正常 App 地址：        0x08004800
```

App 的 `.map` 中必须看到：

```text
Load Region LR_IROM1 (Base: 0x08004800, ...)
Execution Region ER_IROM1 (Exec base: 0x08004800, Load base: 0x08004800, ...)
```

如果 App 的 `Load Region` 或 `Execution Region` 出现 `0x08000000`，说明 App 链接到了 IAP 地址，直接烧录会覆盖 IAP。

App 烧录必须优先使用安全脚本：

```powershell
.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash
```

禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`。

## 当前最新 map 文件结论

当前最新 map 文件：

```text
103 + 309/Project/Users/Listings/FD_Release.map
生成时间：2026-05-22 10:46:55
```

关键地址检查：

```text
Image Entry point : 0x08004931

Load Region LR_IROM1
  Base: 0x08004800
  Size: 0x0000ea58
  Max : 0x00020000

Execution Region ER_IROM1
  Exec base: 0x08004800
  Size     : 0x0000e680
  Max      : 0x00020000

Execution Region RW_IRAM1
  Exec base: 0x20000000
  Size     : 0x00001ae0
  Max      : 0x00005000
```

结论：

- App 链接地址正确，当前没有链接到 `0x08000000`。
- App 区域最大 `0x20000`，即 128KB。
- 当前 Load Region 大小约 58KB，Flash 余量仍然充足。
- 当前 RAM 区域使用 `0x1ae0`，即 6880 字节，20KB RAM 内仍有明显余量。

当前 Release 总量：

```text
Total RO  Size (Code + RO Data)               59008 (57.63KB)
Total RW  Size (RW Data + ZI Data)             6880 ( 6.72KB)
Total ROM Size (Code + RO Data + RW Data)     59284 (57.89KB)
```

当前 Debug 对比：

```text
FD_Debug:
Total RO  Size    65260 (63.73KB)
Total RW  Size     7280 ( 7.11KB)
Total ROM Size    65820 (64.28KB)
```

Release 相比 Debug 大约少：

```text
RO  少 6252 字节
RW  少 400 字节
ROM 少 6536 字节
```

## Code、RO Data、RW Data、ZI Data 的含义

map 文件中最常见的几类体积如下。

```text
Code
```

函数机器码，放在 Flash 中。Cortex-M3 执行 Thumb/Thumb-2 指令，所以符号表中常显示为 `Thumb Code`。

```text
RO Data
```

只读数据，通常放在 Flash 中，例如：

```c
const UINT16 table[] = { ... };
const char version[] = "V1.0.0";
```

```text
RW Data
```

有初始值的全局变量或静态变量，运行时放 RAM，初始值放 Flash。它同时消耗 Flash 和 RAM。

```c
UINT16 g_u16Soc = 50;
UINT8 g_flag = 1;
```

```text
ZI Data
```

零初始化或未初始化的全局变量和静态变量，运行时放 RAM，不直接增加 bin 文件大小。

```c
UINT8 g_u8Buffer[512];
static UINT16 s_adc_cache[64];
```

常用估算：

```text
Flash 占用约等于 Code + RO Data + RW Data
RAM 占用约等于 RW Data + ZI Data + Stack + Heap
```

在 Keil map 的汇总里，`Total ROM Size` 通常已经按链接器压缩后的 ROM 实际需求统计，分析时优先看该值。

## Thumb Code 和 Data 的区别

map 中的符号可能类似：

```text
0x0800c459  Thumb Code  1060  Sci_ACK_0x03_ReadRegs_Data
0x08013624  Data         256  CRC8Table
0x20000b04  Data         251  g_u8SCITxBuff
```

含义：

- `Thumb Code` 是函数指令，主要占 Flash。
- `Data` 可能是 Flash 中的只读表，也可能是 RAM 中的变量，需要结合地址和 section 判断。
- `0x080xxxxx` 一般属于 Flash。
- `0x200xxxxx` 一般属于 SRAM。

示例：

```text
CRC8Table 0x08013624 Data 256 i2c_afe1.o(.constdata)
```

说明它是 `constdata`，在 Flash 中。

```text
g_u8SCITxBuff 0x20000b04 Data 251 sci_upper.o(.bss)
```

说明它在 RAM 的 `.bss` 中，属于 ZI/RAM 消耗。

## map 文件主要章节怎么读

### 1. Load Region / Execution Region

这部分用于确认链接布局：

```text
Load Region LR_IROM1
Execution Region ER_IROM1
Execution Region RW_IRAM1
```

重点检查：

- App 是否从 `0x08004800` 开始。
- `ER_IROM1` 是否超出 App 分区上限。
- `RW_IRAM1` 是否超出 RAM 上限。
- 栈是否被算入 RAM 区域。

当前工程：

```text
RW_IRAM1 Max: 0x00005000
RW_IRAM1 Size: 0x00001ae0
```

说明当前 RAM 使用约 6.72KB，未逼近 20KB 上限。

### 2. Image Symbol Table

该表可以看到函数和变量的符号级大小。

例如当前 Release 中较大的函数：

| 符号 | 大小 | 来源 |
| --- | ---: | --- |
| `Sci_ACK_0x03_ReadRegs_Data` | 1060 | `sci_upper.o` |
| `Refresh_Parameters` | 766 | `sh367309_datadeal.o` |
| `System_ERROR_UserCallback` | 748 | `system_monitor.o` |
| `Fault_ChangeToMCU` | 742 | `sh367309_func.o` |
| `Sci_Deal_WrRegs_0x10` | 692 | `sci_upper.o` |
| `InitIO` | 496 | `conf.o` |
| `AfeCurrent_StartupZeroCal` | 464 | `datadeal.o` |
| `InitIO_rtc` | 462 | `conf.o` |
| `new_todo_logi` | 458 | `datadeal.o` |

这些函数是 Flash 优化时的第一梯队候选，但是否值得优化要结合业务风险判断。

### 3. Memory Map of the image

该表按地址列出每个 section 的放置位置。

典型行：

```text
0x0800c458  0x00000424  Code  RO  i.Sci_ACK_0x03_ReadRegs_Data  sci_upper.o
0x20000b04  0x000000fb  Zero  RW  .bss                          sci_upper.o
```

读法：

- `Code RO`：代码，放 Flash。
- `Data RO`：只读数据，放 Flash。
- `Data RW`：有初始值变量，运行时放 RAM，初始值也要进 Flash。
- `Zero RW`：零初始化变量，运行时放 RAM。
- `PAD`：链接器填充，通常不是主要优化对象。

### 4. Image component sizes

这部分按 `.o` 文件统计各模块贡献，是定位优化方向最快的入口。

当前 Release 中代码量较大的对象：

| 对象文件 | Code | RO Data | RW Data | ZI Data | 建议 |
| --- | ---: | ---: | ---: | ---: | --- |
| `sci_upper.o` | 8764 | 716 | 84 | 902 | 协议读写函数是 Flash 与 RAM 重点 |
| `socenhance.o` | 5884 | 420 | 4 | 260 | SOC 增强逻辑是 Flash 重点 |
| `datadeal.o` | 3750 | 16 | 64 | 372 | 数据处理逻辑可关注重复计算 |
| `flash.o` | 2740 | 0 | 0 | 0 | 启动打印已隔离，后续关注存储日志逻辑 |
| `ledbar.o` | 3034 | 86 | 60 | 0 | 显示扫描逻辑体积中等 |
| `can_hdx.o` | 2736 | 0 | 57 | 44 | CAN 通信逻辑体积中等 |
| `conf.o` | 2626 | 0 | 88 | 0 | 初始化函数较大 |
| `i2c_afe1.o` | 1972 | 368 | 0 | 92 | AFE 驱动和查表 |
| `rtc_sleep.o` | 1932 | 0 | 26 | 12 | 休眠恢复逻辑 |

说明：

- `sci_upper.o` 是当前最大代码模块。
- `socenhance.o` 是第二大代码模块。
- RAM 侧 `sci_upper.o` 的 ZI Data 较高，主要来自通讯缓存和状态结构。

### 5. Removing unused sections

map 文件中会出现：

```text
Removing xxx.o(...)
```

这说明链接器已经移除了未引用的函数或数据。

当前 Release 中可以看到 easylogger 大量函数被移除，说明 `ELOG_OUTPUT_ENABLE` 未进入 Release 主路径，这是正确的。

当前 `StorageFlash_PrintBootCheck()` 已通过 `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE` 隔离，Release 默认关闭后，启动打印函数和 `printf` 库成员会被链接器移除。

## 当前 RAM 使用重点

当前 RAM 总使用约 6.72KB，暂时不紧张。但如果后续加入更多缓存、通信队列或日志记录，应优先关注以下符号：

| 符号 | 大小 | 位置 | 说明 |
| --- | ---: | --- | --- |
| `STACK` | 3072 | `startup_stm32f10x_hd.o` | 当前栈 3KB |
| `g_stCurrentMsgPtr_SCI1` | 262 | `sci_upper.o(.bss)` | SCI1 当前报文缓存 |
| `g_stCurrentMsgPtr_SCI2` | 262 | `sci_upper.o(.bss)` | SCI2 当前报文缓存 |
| `g_u8SCITxBuff` | 251 | `sci_upper.o(.bss)` | SCI 发送缓存 |
| `BMS_LOG_RECORD` | 200 | `logrecord.o(.bss)` | 事件记录 |
| `PRT_E2ROMParas` | 130 | `fault.o(.bss)` | 参数结构 |
| `SOC_Enhance_Element` | 122 | `socenhance.o(.bss)` | SOC 增强状态 |
| `ProductionInfor` | 108 | `productionid.o(.bss)` | 生产信息 |
| `g_u16CalibCoefK` | 94 | `datadeal.o(.bss)` | 校准系数 |
| `g_i16CalibCoefB` | 94 | `datadeal.o(.bss)` | 校准系数 |

当前最大的 RAM 项是栈。若后续 RAM 紧张，应先用栈水位法确认 3KB 是否必要，不建议只凭 map 直接减小栈。

## 当前 Flash 使用重点

当前 Flash 余量还可以，但 map 已经暴露出几个高收益优化点。

### 已优化 1：启动打印已按配置隔离

当前 `main.c` 中存在无条件调用：

```c
StorageFlash_PrintBootCheck();
```

该函数内部使用多处 `printf`：

```c
printf("\r\n[FLASH_BOOT] flash_size_reg=%uKB page=%lu\r\n", ...);
printf("[FLASH_BOOT] AFE A=%u seq=%lu B=%u seq=%lu selected=%c\r\n", ...);
printf("[FLASH_BOOT] RW_PARAM A=%u seq=%lu B=%u seq=%lu selected=%c\r\n", ...);
printf("[FLASH_BOOT] SOC A=%u seq=%lu next=0x%04lX B=%u seq=%lu next=0x%04lX selected=%c\r\n", ...);
```

map 中对应现象：

```text
_printf_core    1704 bytes
printfa.o       2218 bytes
mc_w.l          2732 bytes
mf_w.l          1066 bytes
```

优化前量产 Release 会拉入 `printf` 核心和部分格式化/浮点相关库代码。当前已新增 `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE`，Release 默认关闭后，map 中显示 `StorageFlash_PrintBootCheck()` 被移除。

后续建议：

- 如果启动 Flash 检查日志只用于调试，应保持编译开关隔离。
- 当前使用 `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE` 控制启动打印。
- Release 默认关闭启动打印。
- 需要保留板端诊断时，改为上位机寄存器读取或轻量串口输出，不走完整 `printf`。

预期收益：

- 可能减少 2KB 到 4KB 级别的 Flash。
- 减少格式化库带来的隐性链接依赖。
- 降低启动阶段串口输出对时序和低功耗流程的干扰。

风险：

- 关闭打印后，现场排查 Flash 存储槽状态不再直接从串口看到。
- 应确保上位机或 Modbus/RS485 寄存器仍能读到关键状态。

### 已优化 2：`iSheldTemp_10K_NTC` 已改为 const

当前 map 中：

```text
iSheldTemp_10K_NTC  282 bytes  0x08012d46  sh367309_func.o(.constdata)
```

源码中定义已改为：

```c
const UINT16 iSheldTemp_10K_NTC[141] = { ... };
```

使用处：

```c
extern const UINT16 iSheldTemp_10K_NTC[141];
temp = iSheldTemp_10K_NTC[AFE_TEMPERATURE[i]];
```

引用声明同步为：

```c
const UINT16 iSheldTemp_10K_NTC[141] = { ... };
extern const UINT16 iSheldTemp_10K_NTC[141];
```

收益：

- 释放约 282 字节 RAM。
- 减少启动时 RW Data 拷贝。
- 表会进入 Flash 的 RO Data。

风险：

- 必须确认没有任何代码写该表。
- 修改 extern 声明时需要全工程同步。

当前 RAM 不紧张，因此这是中低风险的小优化，不是必须立即做。

### 已优化 3：删除旧充电/负载与 IO 控制模块

本次已删除：

```text
ChargerLoadFunc.c/.h
IO_Control.c/.h
```

确认点：

- `App_DI1_Switch()` 当前只有 `return`，删除调用后行为不变。
- `ChargerLoadFunc` 的运行处理函数未进入主循环；PA0 充电唤醒 EXTI 已由 `InitWakeUp_Base()` 维护。
- `EXTI0_IRQHandler()` 仍清 pending，不再写无人消费的 `ChargerLoad_Func`。
- `OPEN/CLOSE` 通用枚举已移到 `main.h`，不再依赖旧 `IO_Control.h`。
- 最新 `FD_Release.map` 中已经没有 `chargerloadfunc.o` 和 `io_control.o`。
- `tools/project_check.py` 已增加旧模块文件、Keil 工程引用、源码引用检查，避免后续被重新引入。

### 建议 4：协议读写函数是 Flash 第二优化方向

当前 `sci_upper.o` 是最大对象：

```text
sci_upper.o Code: 8764 bytes
```

其中较大的函数：

```text
Sci_ACK_0x03_ReadRegs_Data      1060 bytes
Sci_Deal_WrRegs_0x10             692 bytes
Sci_ACK_0x03_ReadRegs_LCD        440 bytes
Sci_WrReg_0x06_Reset_CalibCoef   432 bytes
```

建议：

- 优先检查寄存器读取函数是否存在大量重复打包逻辑。
- 对连续寄存器块可考虑集中表驱动，但不要牺牲协议可读性。
- 对只在测试模式使用的寄存器入口，继续保持编译配置隔离。
- 对 Release 不支持的测试入口，应返回明确状态，不应保留完整测试逻辑。

注意：

该模块是上位机通讯核心，优化前必须准备回归用例，至少覆盖：

- `0x03` 读寄存器
- `0x06` 写单寄存器
- `0x10` 写多寄存器
- SOC 状态读取
- 日志读取
- 参数读取和写入

### 建议 4：SOC 增强逻辑可作为后续专项

当前 `socenhance.o`：

```text
Code:    5884 bytes
RO Data: 420 bytes
ZI Data: 260 bytes
```

其中 `soc_publish`、SOC 表和状态结构均有一定体积。

建议：

- 不建议为了省几百字节随意删 SOC 保护逻辑。
- 可以先整理 SOC 表是否重复，例如默认表、磷酸铁锂表、三元锂表是否都必须进入当前产品。
- 如果当前产品电芯类型固定，可通过编译配置只保留对应表。
- 若多化学体系需要同一固件支持，则保留多表是合理的。

### 建议 5：栈大小需要用运行数据验证

当前栈：

```text
STACK 0x00000c00 = 3072 bytes
```

当前 RAM 余量充足，不建议盲目下调栈。

如果后续 RAM 紧张，建议：

- 启动时填充栈区哨兵值。
- 运行充放电、通讯、休眠唤醒、异常保护、IAP 相关场景。
- 读取最深栈水位。
- 再决定是否从 3KB 下调。

### 建议 6：Release 配置隔离目前方向正确

当前 `FD_Release` Keil Define：

```text
STM32F10X_MD,USE_STDPERIPH_DRIVER
```

未显式定义 `PROJECT_CFG_BUILD_PROFILE`，因此会走默认：

```c
#define PROJECT_CFG_BUILD_PROFILE 0
```

`Project_BuildGuard.h` 中已有 Release 约束：

```c
#if defined(ELOG_OUTPUT_ENABLE)
#error "Release build: ELOG_OUTPUT_ENABLE must not be defined"
#endif
```

以及 SOC 测试模式约束：

```c
#if PROJECT_CFG_SOC_TEST_MODE_ENABLE && \
    (PROJECT_CFG_BUILD_PROFILE != PROJECT_BUILD_PROFILE_FACTORY_TEST)
#error "SOC test mode requires Factory/Test build profile"
#endif
```

这符合量产隔离要求。

仍建议定期通过 map 检查：

- `Flash64KAppTest` 是否只剩空桩或被完全移除。
- `ELOG_OUTPUT_ENABLE` 是否未进入 Release。
- `PROJECT_CFG_SOC_TEST_MODE_ENABLE` 是否为 0。
- `0xD300 supported=0` 是否符合量产预期。

## 结合当前项目实际情况的多维优化建议

本节不是泛泛而谈，而是基于当前 `Project_Config.h`、`Project_BuildGuard.h`、`main.c`、`Runtime.c` 和最新 `FD_Release.map` 给出的项目级建议。

当前关键配置快照：

| 配置项 | 当前值 | 含义 |
| --- | ---: | --- |
| `PROJECT_CFG_BUILD_PROFILE` | 0 | 量产 Release |
| `PROJECT_CFG_BAT_TYPE` | 1 | 从包 BAT_SLAVE，40A |
| `PROJECT_CFG_BAT_CHEMISTRY` | 0 | 三元锂 |
| `PROJECT_CFG_LEVEL_CURR` | 2 | 150A 档 |
| `PROJECT_CFG_AFE_TYPE` | 1 | SH36xx AFE |
| `PROJECT_CFG_WDOG_ENABLE` | 1 | 看门狗开启 |
| `PROJECT_CFG_RTC_ENABLE` | 1 | RTC 开启 |
| `PROJECT_CFG_HEAT_ENABLE` | 0 | 加热关闭 |
| `PROJECT_CFG_RS485_WAKEUP_ENABLE` | 1 | RS485 唤醒开启 |
| `PROJECT_CFG_IAP_ENABLE` | 1 | IAP 支持开启 |
| `PROJECT_CFG_FACTORY_AGING_ENABLE` | 1 | 出厂老化开启 |
| `PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE` | 0 | Flash 破坏性测试关闭 |
| `PROJECT_CFG_FLASH64K_USE_TEST_ENABLE` | 0 | Flash 运行测试关闭 |
| `PROJECT_CFG_SOC_TEST_MODE_ENABLE` | 0 | SOC 注入测试关闭 |
| `PROJECT_CFG_LEDBAR_SLEEP_ENABLE` | 1 | 休眠前备份灯条 SOC |
| `PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE` | 1 | 升级参数策略开启 |

### 1. 量产构建和发布安全

当前方向是正确的：

- `FD_Release` 没有定义 `_DEBUG_`。
- `PROJECT_CFG_BUILD_PROFILE` 默认是 0。
- `Project_BuildGuard.h` 已经限制 Release 中不能打开调试代码、Flash 测试、SOC 测试和 `ELOG_OUTPUT_ENABLE`。
- map 中 App 入口和执行区域均为 `0x08004800`。

建议继续加强：

- 增加一个自动 map 检查脚本，构建后固定检查 `LR_IROM1 Base == 0x08004800`。
- 增加 `Total ROM Size` 和 `Total RW Size` 阈值检查，例如 ROM 超过 96KB 或 RAM 超过 14KB 时给出告警。
- 增加 Release 禁止项扫描，例如 `_printf_core`、`ELOG_OUTPUT_ENABLE`、`FLASH64K_APP_QUICK_TEST_ENABLE`、`PROJECT_CFG_SOC_TEST_MODE_ENABLE`。
- 将 map 检查作为发布前固定步骤，避免靠人工记忆判断。

建议阈值起点：

```text
App Base 必须等于 0x08004800
Total ROM Size 建议告警线 96KB
Total RW Size 建议告警线 14KB
Release 中出现 _DEBUG_ 必须失败
Release 中出现 SOC 测试入口必须失败
Release 中出现裸 printf 建议告警
```

### 2. Flash/ROM 体积优化

当前 ROM 约 57.89KB，距离 128KB App 分区还有余量，因此不建议做大规模重构。

当前最值得处理的是“调试输出进入量产路径”：

```c
StorageFlash_PrintBootCheck();
```

它曾在 `InitDevice()` 中无条件执行，内部使用 `printf`。当前已改为 `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE` 控制，量产默认关闭，避免启动阶段默认输出大量串口诊断。

建议：

- 新增 `PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE`，Release 默认 0。
- 或复用 `PROJECT_CFG_DEBUG_CODE_ENABLE` 控制该函数调用。
- 保留上位机寄存器读取入口，用来替代串口启动打印。

当前 `FD_Release.map` 中也可以看到 `sci_upper.o` 和 `socenhance.o` 是主要代码大户。对于这两个模块，建议先做结构化审查，不要直接删逻辑：

- `sci_upper.o` 优先查寄存器读取和写入分支是否重复。
- `socenhance.o` 优先查当前三元锂产品是否还需要携带多套 SOC 表。
- `flash.o` 优先查启动打印和 Flash 诊断字符串。
- `conf.o` 优先查初始化路径是否有当前硬件不使用的分支。

当前 `SeriesSelect_AFE1` 是 `16 x 16` 表，占 256 字节 RO Data。当前 `SeriesNum = 10`，如果这个固件只服务固定 10 串硬件，可以评估是否改成更小的映射表或按公式计算。但如果后续同一固件需要覆盖多串数产品，则保留该表更稳。

### 3. RAM 使用优化

当前 RAM 约 6.72KB，20KB SRAM 余量较好。RAM 不是当前第一瓶颈。

可做的小优化：

- `iSheldTemp_10K_NTC` 已进入 `.constdata`，不再占用 282 字节 RAM。
- `sci_upper.o` 中 SCI1/SCI2 当前报文缓存和发送缓存合计较明显。当前配置 `SCI2_ROLE = 0`、`SCI3_ROLE = 0`，后续可以评估是否通过编译配置裁剪未使用通道的运行时状态。
- `STACK` 当前 3KB，不建议直接下调，应先做栈水位测试。

RAM 优化顺序：

```text
1. 大表加 const
2. 裁剪未使用 SCI 通道缓存
3. 复用临时 buffer
4. 栈水位验证后再调整栈
```

不建议当前就压缩：

- 故障记录结构
- SOC 运行状态
- AFE 参数结构
- Flash 双槽存储缓冲

这些数据体积不算大，且对稳定性有直接影响。

### 4. 启动流程优化

当前 `main()` 流程：

```c
InitDevice();
InitVar();
Init_RTC();

while (1)
{
    Runtime_RunOnce();
}
```

`InitDevice()` 内部又会调用：

```c
IsSleepStartUp();
...
StorageFlash_PrintBootCheck();
...
InitCan();
InitADC();
...
InitTimer();
__enable_irq();
```

其中 `IsSleepStartUp()` 在休眠唤醒路径中可能涉及 RTC 初始化和 IO 恢复，`main()` 后面又固定调用一次 `Init_RTC()`。建议专项梳理 RTC 初始化顺序：

- 冷启动时 RTC 初始化一次即可。
- STOP/休眠唤醒时避免重复配置 RTC 中断和备份域。
- `RTC_WaitForLastTaskSafe()` 已经是好方向，应继续保持超时保护。
- 启动阶段不要默认打印 Flash 状态，避免拉长上电时间。

建议目标：

```text
冷启动：SystemInit -> IO/AFE/ADC/CAN/RTC 初始化清晰有序
休眠唤醒：只恢复必要外设，不重复清空状态
IAP 跳转：保持时基、NVIC、外设状态可控
```

### 5. 主循环和任务调度优化

当前 `Runtime_RunOnce()` 分成三段：

```c
Runtime_RunFrontTasks();
Runtime_RunIoAndPowerTasks();
Runtime_RunBackgroundTasks();
```

这是比较清晰的结构。建议继续把“快任务”和“可能阻塞任务”分开。

当前前台任务：

```c
FactoryAging_Task();
APP_LedBar();
App_AFEGet();
```

当前 IO 和低功耗任务：

```c
App_Sci();
App_AnlogCal();
App_LowPowerProcess();
App_Can();
```

当前后台任务：

```c
StorageFlash_AppUseTest_Task();
App_FlashUpdate();
App_LogRecord();
App_ProID_Deal();
Feed_IWatchDog;
```

建议：

- 给每类任务建立最大耗时预算。
- Flash 写入、日志保存、参数升级策略应保持在后台任务。
- `App_Sci()` 和 `App_Can()` 不应长时间等待发送完成。
- 低功耗判断前应确保 CAN、SCI、Flash 写入状态不会被中途打断。
- 看门狗喂狗点当前在后台任务，若后台任务前面存在潜在阻塞，需确保不会误复位。

可以加入轻量运行监控：

```text
Runtime 前台任务最大耗时
Runtime 通讯任务最大耗时
Runtime 后台任务最大耗时
最近一次看门狗喂狗 tick
最近一次 Flash 写入耗时
```

这些数据可以映射到上位机只读寄存器，调试价值高，ROM/RAM 成本低。

### 6. 低功耗和 RTC 优化

当前项目已经有较多低功耗状态观测变量，例如：

```c
g_stLowPowerRtcStatus
g_stCanLowPowerStatus
```

这是好方向。低功耗优化不应只看代码体积，重点应看“为什么没睡”和“睡了以后是否按预期醒”。

建议从四个指标优化：

```text
1. 进入低功耗前的阻塞原因
2. RTC 实际睡眠秒数
3. CAN 是否因总线设备存在切换 1s / 10s 周期
4. 唤醒后 AFE、ADC、CAN、SCI 是否恢复完整
```

当前配置中：

- RTC 开启。
- RS485 唤醒开启。
- 出厂老化开启。
- CAN 有低功耗状态机。
- LedBar 休眠 SOC 备份开启。

建议：

- 量产版本保留低功耗 block reason 上报。
- 出厂老化期间如果阻止睡眠，应明确记录 block reason。
- RTC 唤醒后恢复 CAN 供电和 CAN 报文节奏时，继续使用当前“有设备 1s，无设备 10s”的策略。
- 禁止启动阶段无条件串口打印，因为它会干扰低功耗功耗测量和启动时序。
- 休眠前保存 SOC、出厂老化进度、CAN 状态的顺序要固定，不要分散在多个入口里隐式调用。

### 7. 通讯协议优化

当前 `sci_upper.o` 是最大对象，并且 RAM 中也有较多 SCI 状态。

当前配置：

```text
SCI1_ROLE = 通用上位机
SCI2_ROLE = 禁用
SCI3_ROLE = 禁用
RS485_WAKEUP = 开启
```

建议：

- 如果量产只使用 SCI1，则评估是否完全裁剪 SCI2/SCI3 的状态变量和处理分支。
- 上位机寄存器读取应优先保留，作为替代串口 debug print 的诊断通道。
- `0x03` 读寄存器函数体积最大，应检查是否可以把重复字段打包逻辑集中。
- `0x06` 和 `0x10` 写寄存器必须保持边界检查，不要为了省代码删除校验。
- SOC 测试相关寄存器必须继续由 `PROJECT_CFG_SOC_TEST_MODE_ENABLE` 隔离。

建议保留或增强的只读诊断寄存器：

```text
构建档位
SOC 测试入口是否 supported
最近低功耗阻塞原因
最近 RTC 睡眠秒数
CAN 当前是否检测到设备
Flash 存储槽选择状态
最近一次升级参数策略执行结果
```

这样可以减少串口 printf 依赖，同时提高现场诊断能力。

### 8. CAN 低功耗优化

当前 CAN 模块已有状态：

```c
g_stCanLowPowerStatus
```

并且有 RTC 周期策略：

```text
有 CAN 设备：1s 周期
无 CAN 设备：10s 周期
```

建议：

- 继续以 ACK/RX/发送失败作为“设备存在”判断依据。
- 无设备时只保留轻量探测帧，避免低功耗期间完整业务帧持续发送。
- RTC 唤醒后 CAN 供电、GPIO、CAN 外设、过滤器恢复顺序要固定。
- CAN 进入低功耗前调用 `Can_PrepareSleep()` 是合理的，应确保所有睡眠入口都经过它。
- 将 `g_stCanLowPowerStatus` 关键字段暴露给上位机，减少现场依赖示波器或串口日志。

体积层面：

- `can_hdx.o` 当前 Code 约 2736 字节，不是首要体积瓶颈。
- 不建议为了省几百字节削弱 CAN 设备探测和低功耗恢复逻辑。

### 9. SOC 算法和显示优化

当前 SOC 配置较完整，包括满电确认、静置 OCV、压降延迟、板端自耗、尾段显示等。

当前产品配置为三元锂：

```text
PROJECT_CFG_BAT_CHEMISTRY = 0
```

建议：

- 如果量产固件只面向三元锂，可评估是否通过编译配置裁剪 LIFEPO 表。
- 如果同一个固件要兼容多体系电芯，则保留多表更合适。
- `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA = 30` 必须用实测静态电流校准，不能长期只靠经验值。
- 满电确认时间当前普通 15s、快速 5s，体验较快，需通过充电末端数据确认不会过早跳 100%。
- 低压显示下降速度当前 1s/1%，体验上比较直接，需结合实车放电曲线确认最后 20% 不会掉得过快。
- SOC 注入测试入口当前关闭，量产读到 `0xD300 supported=0` 是正确预期。

SOC 优化优先级：

```text
1. 用实际充放电日志调 OCV 表和尾段参数
2. 用静置数据验证 OCV deferred target
3. 用低温和高负载数据验证压降 holdoff
4. 用长时间静置数据校准板端自耗
5. 最后再考虑裁剪表和代码体积
```

### 10. Flash 存储和磨损优化

当前配置：

```text
PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC = 3600
PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE = 1
PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD = 0
PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT = 0
```

这是比较稳妥的量产策略。

建议：

- 继续保留事件记录重复限频，避免故障抖动反复写 Flash。
- SOC 快照、AFE 参数、事件记录继续使用双槽或带序号策略。
- 升级参数策略版本变化时，必须明确哪些参数覆盖、哪些参数保留。
- 不建议升级时默认清事件记录，售后需要历史故障。
- `StorageFlash_PrintBootCheck()` 的信息可以改成寄存器读取，减少启动打印。

建议新增的 Flash 诊断指标：

```text
AFE 槽 A/B valid 和 seq
SOC 槽 A/B valid 和 seq
RW 参数槽 A/B valid 和 seq
最近一次保存失败原因
最近一次保存耗时
升级参数策略版本和执行结果
```

### 11. AFE/ADC 采样优化

当前 AFE 为 SH36xx，ADC/AFE/SOC 逻辑已经有较多校准和低功耗恢复路径。

建议：

- AFE 电流零点校准不要只靠启动一次，应区分冷启动、休眠唤醒、充电器唤醒、负载存在等场景。
- Type-C 或外部 ADC 电流采样继续保持整数计算，避免引入浮点。
- ADC 平滑和 AFE 电流滤波应以响应时间和保护误判为目标，不要单纯为了平滑拉长时间。
- 休眠前后要验证 CADC、OCD/SC 状态和电流恢复逻辑。
- AFE 采样异常时进入休眠的路径应继续保留延时和 block reason，避免误睡。

体积层面：

- `i2c_afe1.o` 的 `CRC8Table` 是 256 字节 RO Data，若通信频率高，查表 CRC 是合理的。
- `iSheldTemp_10K_NTC` 若只读，应优先改 `const`。

### 12. LedBar 和人机显示优化

当前配置：

```text
PROJECT_CFG_LEDBAR_SLEEP_ENABLE = 1
PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS = 500
PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS = 1000
PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS = 50
```

建议：

- 当前 0.5ms 扫描节拍应关注 ISR 占用，显示稳定优先于体积。
- 休眠唤醒显示 10s 是用户体验逻辑，应保留。
- 充电图标仅由放电 MOS 状态决定，不再保留充电插入/拔出滤波配置项。
- 如果低功耗功耗偏高，先确认 LedBar 是否彻底关闭扫描定时器和相关 GPIO，而不是先改显示逻辑。

体积层面：

- `ledbar.o` Code 约 3034 字节，属于中等体积。
- 不建议当前优先优化 LedBar 代码体积，除非出现显示 ISR 占用或低功耗漏电问题。

### 13. IAP 和升级可靠性优化

当前 IAP 支持开启，App 地址必须继续固定在 `0x08004800`。

建议：

- 每次发布都检查 map 的 `LR_IROM1 Base`。
- 每次烧录都通过 `tools/soc_flash_app_safe.ps1`。
- App scatter 和烧录脚本必须保持 `0x08004800` 检查。
- IAP 跳 App 前后，SysTick、NVIC、外设中断和向量表必须有明确交接。
- 升级参数策略应在文档中记录版本和覆盖字段，避免现场升级后参数变化不可追溯。

map 层面：

- `FD_IAP.map` 应独立检查，不能只看 App map。
- App ROM 接近上限前，需要确认 IAP 分区、App 分区和后 64K 存储区互不重叠。

### 14. 编译选项和链接器优化

当前 `FD_Release` 的 Keil `Optim` 为 1。当前 ROM 余量足够，不建议立即因为体积去提高优化等级。

建议：

- 如果后续 ROM 接近 96KB，可单独建立一次 size-optimized 对比构建。
- 对比前后必须跑通讯、保护、低功耗、IAP、SOC 显示回归。
- 保持 per-function section 和 unused section remove，让未用测试代码能被链接器移除。
- 不要为了减少体积关闭必要的断言式构建检查，`Project_BuildGuard.h` 的价值高于节省的少量代码。

### 15. 测试和验证优化

建议把验证分成四类：

```text
1. 静态检查：map 地址、ROM/RAM、Release 禁止项
2. 上位机检查：COM4/19200/slave=1 读取关键寄存器
3. 功耗检查：正常运行、RTC 休眠、有 CAN 设备、无 CAN 设备
4. 功能回归：SOC、AFE、CAN、RS485、LedBar、IAP、Flash 存储
```

结合当前项目，发布前最小验证集建议：

```text
读取 0xD000：确认基础运行状态
读取 0xD300：量产应 supported=0
读取低功耗状态：确认最近 block reason
读取 CAN 状态：确认有/无设备判断
执行一次睡眠唤醒：确认 LedBar SOC 显示和 AFE/ADC 恢复
执行一次参数读取和写入：确认 0x03/0x06/0x10 正常
执行一次安全 App 烧录 dry-run：确认地址 0x08004800
```

## 优化优先级建议

按当前 map 的收益和风险排序：

| 优先级 | 建议 | 主要收益 | 风险 |
| --- | --- | --- | --- |
| 已完成 | 隔离 `StorageFlash_PrintBootCheck()` 的 `printf` | Flash 4KB 级别 | Release 默认关闭，调试可配置开启 |
| 已完成 | `iSheldTemp_10K_NTC` 改 `const` | RAM 282B | 已确认当前只读使用 |
| P2 | 梳理 `sci_upper.o` 大函数 | Flash 几百到 1KB+ | 协议回归成本较高 |
| P3 | 检查 SOC 多套表是否都必要 | Flash 数百字节 | 产品配置需要确认 |
| P3 | 栈水位验证后再调栈 | RAM 几百字节到 1KB | 栈溢出风险高 |

当前并不存在 Flash 或 RAM 即将耗尽的问题。因此优化应优先围绕“隔离调试输出、保持量产干净、降低隐性库依赖”展开，而不是大规模重构。

## 推荐 map 分析流程

每次优化前后按以下流程执行：

1. 保存当前 map 作为基线。

```powershell
Copy-Item "103 + 309\Project\Users\Listings\FD_Release.map" "logs\FD_Release_before.map"
```

2. 检查 App 地址。

```powershell
Select-String -Path "103 + 309\Project\Users\Listings\FD_Release.map" `
  -Pattern "Load Region|Execution Region|Image Entry point"
```

3. 查看总体大小。

```powershell
Select-String -Path "103 + 309\Project\Users\Listings\FD_Release.map" `
  -Pattern "Total RO|Total RW|Total ROM"
```

4. 查看对象文件大小表。

```powershell
Select-String -Path "103 + 309\Project\Users\Listings\FD_Release.map" `
  -Pattern "Image component sizes" -Context 0,130
```

5. 查找 printf、浮点和日志相关链接。

```powershell
Select-String -Path "103 + 309\Project\Users\Listings\FD_Release.map" `
  -Pattern "printf|snprintf|vsnprintf|dadd|ddiv|dmul|elog"
```

6. 修改后重新编译，对比：

```text
Total RO Size
Total RW Size
Total ROM Size
sci_upper.o
flash.o
printfa.o
mf_w.l
```

## 常见判断规则

看到：

```text
Thumb Code
```

说明是函数指令，主要优化 Flash。

看到：

```text
.constdata
.conststring
```

说明是只读常量或字符串，主要优化 Flash。

看到：

```text
.data
```

说明是有初始值的可写数据，通常同时占 Flash 和 RAM。大表如果不修改，应优先考虑 `const`。

看到：

```text
.bss
Zero RW
```

说明是 ZI Data，只占 RAM，不直接增加 bin 文件。

看到：

```text
printfa.o
_printf_core
__0printf
__0snprintf
vsnprintf
```

说明格式化输出库被拉入。嵌入式量产固件应确认是否必要。

看到：

```text
mf_w.l
dadd.o
ddiv.o
dmul.o
_fp_digits
```

说明浮点或格式化相关库被拉入。应确认是否来自业务计算，还是被 `printf` 间接带入。

看到：

```text
Removing xxx
```

说明链接器已经移除未使用段。该信息可以证明某些测试代码没有真正进入最终镜像。

## 结论

当前最新 `FD_Release.map` 显示：

- App 链接地址正确，仍从 `0x08004800` 开始。
- 当前 ROM 约 57.89KB，App 区域余量充足。
- 当前 RAM 约 6.72KB，20KB SRAM 余量充足。
- Release 配置隔离整体正确，easylogger 大量代码已被链接器移除。
- `StorageFlash_PrintBootCheck()` 已改为配置隔离，Release 默认移除启动打印和 `printf` 库成员。
- `iSheldTemp_10K_NTC` 已改为 `const`，进入 Flash 的 `.constdata`。
- 若需要继续节省 Flash，应专项审查 `sci_upper.o` 和 `socenhance.o`，但这两个模块业务风险更高，需要配套回归测试。
