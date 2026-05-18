# SH36735XX 寄存器配置整理

## 1. 关键寄存器表

| 地址 | 名称 | 作用 | 当前主工程状态 |
| --- | --- | --- | --- |
| `0x40` | `SCONF1` | 工作模式 | 初始化写 `0x00`，Normal |
| `0x41` | `SCONF2` | MOS、Pump、Powerdown、清标志允许 | 初始化由 `Registers_AFE1.sonf2` 拼出 |
| `0x42` | `SCONF3` | 唤醒、负载检测、断线检测 | 初始化清 `CRLD_EN` |
| `0x43` | `SCONF4` | 串数 `CN` | 写规范化后的运行期 `SeriesNum` |
| `0x44` | `SCONF5` | `MOS_EN/OCC_EN/CADC_EN/WDT` | 当前未显式写，依赖默认 `0x38` |
| `0x45` | `SCONF6` | 温度、电流、电压保护使能 | 初始化写 `0x7F` |
| `0x46` | `SCONF7` | 负载检测、IDLE CADC 周期 | 当前未显式写 |
| `0x47..0x54` | 阈值区 | ALARM、OV/UV/OCD/SC/OCC/温度 | 当前只写部分阈值 |
| `0x55..0x57` | `BALANCEH/M/L` | 均衡控制 | `Cell_balance.c` 直接写 |
| `0x58..0x5A` | `FLAG1/2/3` | 保护和转换标志 | 周期读取，清标志函数写 0 |
| `0x5B..0x5C` | `BSTATUS1/2` | MOS、负载、充放电状态 | 采样周期读取 |
| `0x5D..0x99` | ADC/断线数据 | 温度、电芯、电流、总压、C+ | 采样周期读取到 `Registers_AFE1` |

## 2. 当前初始化值

主工程入口在 `main.c` 的 `InitAFE3520_Registers()`。

当前写入逻辑：

| 寄存器 | 当前写法 | 说明 |
| --- | --- | --- |
| `SCONF1` | `0x00` | Normal |
| `SCONF2` | 设置 `LTCLR=1`，`PD_EN=0`，`PUMP_EN=0`，`CHGMOS/DSGMOS` 由参数传入 | 当前调用参数为 `0,0` |
| `SCONF4` | `SNum` | 当前 `SNum=19`，即 `0x13` |
| `SCONF3` | `CRLD_EN=0` | 关闭负载检测释放路径 |
| `SCONF6` | `0x7F` | 关闭 `TS4_EN`，其他保护位为 1 |
| `OVT/OVH + OVL` | `4250 / 5 = 0x352` | 单体过压阈值约 4250 mV |
| `UVT/UVH + UVL` | `2500 / 5 = 0x1F4` | 单体欠压阈值约 2500 mV |
| `OCD2V/OCD2T` | `0x03` | 原始配置值，未在代码中解码 |
| `OCCV/OCCT` | `0x07` | 原始配置值，未在代码中解码 |
| `OTC/UTC/OTD/UTD` | 由 NTC 阻值换算 | 使用固定浮点常量 |

当前每次写寄存器后都会读回同地址并比较写入值，不再只读回 `SCONF4`。

## 3. 与官方例程差异

官方例程 `SH_AFE_RegisterInit()` 连续写 `0x40..0x54`，共 21 个配置寄存器。官方 `SH_AFE_RegisterCheck()` 周期读回配置区，检查 RAM 配置是否被异常改写，并对 `LTCLR`、`OWD_TRG` 这类特殊位做 mask。

主工程当前差异：

- 初始化散落在 `main.c`，不是独立 AFE 配置模块。
- 未显式写 `SCONF5/SCONF7/OWV/ALARM/OCD1/SC` 等关键寄存器。
- 只读回 `SCONF4`，不能发现其他寄存器写失败。
- 初始化发生在 `InitVar()` 读取 EEPROM 串数之前，导致 AFE 串数与业务串数可能不同。

## 4. 已确认的配置风险

### 4.1 `BALANCEM/BALANCEL` 镜像顺序错，已修复

PDF 和官方例程顺序：

```text
0x55 BALANCEH
0x56 BALANCEM
0x57 BALANCEL
```

主工程 `AFEDATA` 原顺序：

```c
uint8_t BALANCEH;
uint8_t BALANCEL;
uint8_t BALANCEM;
```

连续读取 `0x47..0x57` 后，中、低均衡字节镜像会互换。本轮已修正为 `BALANCEH, BALANCEM, BALANCEL`。

### 4.2 `SCONF6=0x7F` 关闭 TS4

文档中 `SCONF6` bit7 是 `TS4_EN`。当前写 `0x7F` 会关闭 TS4 保护。若硬件实际只用 3 路温度，这是合理配置；若实际使用 4 路温度，则 TS4 保护被误关。

建议把这个值改成具名宏，例如：

```c
#define SH36735_SCONF6_ENABLE_TS1_TS3 0x7Fu
#define SH36735_SCONF6_ENABLE_ALL     0xFFu
```

并在产品配置里明确温度通道数量。

### 4.3 `SCONF5` 依赖默认值

`SCONF5` 默认 `0x38`，包含 `MOS_EN=1`、`OCC_EN=1`、`CADC_EN=1`、`WDT_EN=0`。当前主工程不写 `SCONF5`，正常上电后可能没问题，但软件复位或异常恢复后没有显式配置表做保证。

建议把 `SCONF5=0x38` 作为配置表的一项显式写入；如果后续启用 AFE WDT，必须同时设计 `WDT_FLG` 清除闭环。

## 5. 建议目标结构

建议建立 AFE 配置表：

```c
typedef struct {
    uint8_t addr;
    uint8_t value;
    uint8_t mask;
} sh36735xx_reg_cfg_t;
```

使用方式：

- 初始化时按表写 `0x40..0x54`。
- 写完后读回校验。
- 对 `LTCLR`、`OWD_TRG` 等自动变化位使用 mask。
- 串数、温度通道、阈值从统一产品参数生成，不再散落在 `main.c`。
