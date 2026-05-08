# MCU 资源分布与架构优化评估

> 评估对象：`103 + 309/Project` 主工程  
> 评估时间：2026-04-25  
> 依据文件：Keil 工程、scatter 文件、map 文件、启动文件、Flash/EEPROM/外设初始化代码

## 1. 当前结论

### 1.1 重要更正：如果实际 MCU 是 STM32F103C8，当前 Flash 布局不成立

STM32F103C8 官方规格是 `64 KB Flash`、`20 KB SRAM`。因此其内部 Flash 地址范围应为：

```text
0x08000000 ~ 0x0800FFFF
```

当前工程中定义的参数区：

```text
0x0801C000 ~ 0x0801FFFF
```

只有在实际芯片至少具备 `128 KB Flash` 时才可能有效；如果板上真实 MCU 是标准 STM32F103C8，则这段地址已经超出片内 Flash，不能作为可靠参数存储区使用。

所以，本文后续提到的 `0x0801C000` 参数区，是“当前工程代码和链接配置中的规划”，不是 STM32F103C8 的安全物理地址。这个配置更像是按 128 KB Flash，或按部分 C8 板卡实际带 128 KB Flash 的情况设计的，但生产项目不能依赖这种不确定余量。

### 1.2 如果按当前工程地址假设 Flash >= 128 KB

在芯片实际 Flash 至少为 128 KB 的前提下，当前固件不会立即覆盖 IAP、APP 或参数 Flash 区。

当前 APP 从 `0x08004800` 启动，当前构建生成的 bin 文件大小为 `44112 B`，约 `43.08 KB`，镜像结束地址约为：

```text
0x08004800 + 44112 - 1 = 0x0800F44F
```

参数 Flash 规划从 `0x0801C000` 开始，因此当前 APP 距离参数区还有约 `52144 B`，约 `50.92 KB` 的安全增长空间。

但存在一个必须修正的结构性风险：

```text
当前链接脚本允许 APP 使用：
0x08004800 ~ 0x080247FF

当前参数 Flash 实际使用：
0x0801C000 ~ 0x0801FFFF
```

也就是说，链接器当前不会阻止后续 APP 增长覆盖参数区。当前体积没有撞上，不代表架构是安全的。

### 1.3 如果按 STM32F103C8 的 64 KB Flash 计算

如果 MCU 确认是 STM32F103C8，则可用 Flash 总范围为：

```text
0x08000000 ~ 0x0800FFFF
```

IAP 占用到 `0x080047FF` 后，APP 最大只能使用：

```text
APP: 0x08004800 ~ 0x0800FFFF
大小: 0xB800 = 47104 B = 46 KB
```

当前 APP bin：

```text
44112 B
```

因此在 64 KB C8 上，当前 APP 到 Flash 末尾仅剩：

```text
47104 - 44112 = 2992 B = 2.92 KB
```

这意味着：

- 当前 APP 已经非常接近 64 KB C8 的上限。
- 内部 Flash 几乎没有空间再单独划出可靠参数区。
- `0x0801C000` 参数区在标准 C8 上无效。
- 后续 APP 继续增大很容易超过 64 KB。

### 1.4 关于“STM32F103C8 后 64K”的调研结论

调研结论：

1. 官方规格上，`STM32F103C8` 只保证 `64 KB Flash`。
2. 官方同系列中，`STM32F103CB` 才是 `128 KB Flash`。
3. 确实有大量社区实测显示，部分标记为 `STM32F103C8` 的芯片可以访问 `0x08010000~0x0801FFFF` 后 64 KB。
4. 但这部分空间不是 C8 料号的官方保证资源，可能是同 die 降级、未完全测试、批次差异、渠道差异，甚至也可能是兼容/仿冒芯片行为。
5. 对生产固件，不能只因为某一批能写后 64K，就默认所有 C8 都可靠支持 128 KB。

官方资料依据：

- ST 官方产品页明确 `STM32F103C8` 是 `64 Kbytes of Flash memory`：`https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html`
- ST 数据手册订购信息明确：`8 = 64 Kbytes of Flash memory`，`B = 128 Kbytes of Flash memory`：`https://www.st.com/resource/en/datasheet/stm32f103c8.pdf`
- ST Flash 编程手册说明 medium-density Flash 组织到 `0x0801FFFF`，但这是 x8/xB medium-density 系列整体映射，不等于 C8 料号保证 128 KB：`https://www.st.com/resource/en/programming_manual/pm0075-stm32f10xxx-flash-memory-microcontrollers-stmicroelectronics.pdf`

社区资料依据：

- PlatformIO 板卡资料中存在 `BluePill F103C8 (128k)` 这类配置，但这反映的是 Blue Pill 市场上常见的实际板卡差异，不等于 ST 官方把 C8 定义为 128 KB：`https://docs.platformio.org/en/latest/boards/ststm32/bluepill_f103c8_128k.html`
- Stm32World 有实测案例：某块原装 STM32F103C8 Green Pill 在强制 `--flash=128k` 后，成功向 `0x08010000` 开始的后 64 KB 写入、读回并校验一致：`https://stm32world.com/wiki/STM32F103`

工程判断：

当前工程把参数区放在 `0x0801C000~0x0801FFFF`，本质上是在使用“后 64K”的尾部区域。它只有在实际芯片真的有可用后 64K 时才成立；如果某批 C8 只有官方保证的 64 KB，则该参数区无效。

建议：

- 研发阶段可以做“后 64K 探测和压力测试”。
- 生产阶段如果仍使用 C8，必须在产测中逐片验证 `0x08010000~0x0801FFFF` 擦写读回。
- 更稳妥方案是改用 `STM32F103CB` 或更大容量型号，或者把参数存回外部 EEPROM。
- 固件必须保留运行时 Flash 容量检查，检测不到 128 KB 时禁用后 64K 参数区，避免误写无效地址。
- 建议在启动早期读取 Flash 容量寄存器并记录到调试日志；同时产测执行后 64K 擦写读回，不要只依赖寄存器值。
- 对本工程而言，若继续使用 `0x0801C000` 参数区，还必须把 C8/CB medium-density 的 Flash page size 按 `1 KB` 重新核对，当前 `FLASH_STORAGE_PAGE_SIZE = 0x800` 的 2 KB 假设有风险。

## 2. Flash 空间分布

### 2.1 当前已识别布局

> 下表是当前工程代码和链接配置中的布局，不代表标准 STM32F103C8 的真实 64 KB Flash 上限。

| 区域 | 起始地址 | 结束地址 | 大小 | 说明 |
|---|---:|---:|---:|---|
| IAP / Boot | `0x08000000` | `0x080047FF` | 18 KB | IAP 固件区域 |
| APP 当前安全区 | `0x08004800` | `0x0801BFFF` | 94 KB | 建议 APP 链接区上限 |
| 参数 Flash 区 | `0x0801C000` | `0x0801FFFF` | 16 KB | 当前参数、日志、升级标志区 |
| 当前链接脚本 APP 上限 | `0x08004800` | `0x080247FF` | 128 KB | 与参数区重叠，存在风险 |

若 MCU 是标准 STM32F103C8，应改为按 64 KB 重新计算：

| 区域 | 起始地址 | 结束地址 | 大小 | 说明 |
|---|---:|---:|---:|---|
| IAP / Boot | `0x08000000` | `0x080047FF` | 18 KB | 当前 APP 起点反推 |
| APP 可用区 | `0x08004800` | `0x0800FFFF` | 46 KB | C8 下的极限 APP 区 |
| 可预留参数区 | 需要重新规划 | 需要压缩 APP 或使用外部存储 | 很小 | 当前 APP 仅剩约 2.92 KB |

相关定义：

- `FLASH_ADDR_IAP_START = 0x08000000`
- `FLASH_ADDR_APP_START = 0x08004800`
- 参数区从 `0x0801C000` 开始

来源：

- `103 + 309/Project/Source/Flash.h`
- `103 + 309/Project/Users/Objects/CommomSH367309_16series_103RCT6_C.sct`
- `103 + 309/Project/Users/Listings/CommomSH367309_16series_103RCT6_C.map`

### 2.2 参数 Flash 详细分布

当前 `Flash.h` 中的参数页按 `0x800 = 2 KB` 分配：

> 注意：STM32F103C8 属于 medium-density，标准 Flash page 通常按 1 KB 组织。当前代码使用 2 KB page 的写法，更像是按 high-density 或更大容量器件习惯配置，也需要和真实芯片型号核对。

| 地址 | 用途 | 备注 |
|---:|---|---|
| `0x0801C000` | AFE 参数 Slot A | 双备份之一 |
| `0x0801C800` | AFE 参数 Slot B | 双备份之一 |
| `0x0801D000` | 日志记录 Slot A | journal/日志 |
| `0x0801D800` | 日志记录 Slot B | journal/日志 |
| `0x0801E000` | SOC 数据 Slot A | 原宏名 `FLASH_ADDR_SH367309_VALUE` |
| `0x0801E800` | SOC 数据 Slot B | 原宏名 `FLASH_ADDR_SH367309_FLAG` |
| `0x0801F000` | 升级参数策略标志 | `FLASH_ADDR_UPGRADE_PARAM_FLAG` |
| `0x0801F800` | IAP 更新标志 | `FLASH_ADDR_UPDATE_FLAG` |
| `0x0801FC00` | 休眠标志宏定义 | 当前代码实际已改为 BKP 寄存器保存休眠标志 |

### 2.3 当前 APP 大小

当前 map 文件统计：

```text
Total RO  Size: 43560 B = 42.54 KB
Total RW  Size:  7448 B =  7.27 KB
Total ROM Size: 44112 B = 43.08 KB
```

当前 bin 文件：

```text
CommomSH367309_16series_103RCT6_C.bin = 44112 B
```

当前 APP 占用范围：

```text
0x08004800 ~ 0x0800F44F
```

到参数区起点的剩余空间：

```text
0x0801C000 - 0x0800F450 = 52144 B = 50.92 KB
```

## 3. IAP 和参数区是否会互相影响

### 3.1 在 Flash >= 128 KB 的假设下，当前不会立即影响

当前 APP 结束地址约 `0x0800F44F`，远低于参数区 `0x0801C000`，因此当前构建不会覆盖参数区。

IAP 区固定在 `0x08000000~0x080047FF`，APP 从 `0x08004800` 开始，当前也不会覆盖 IAP。

如果 MCU 确认是 STM32F103C8，则 `0x0801C000` 参数区本身无效，不能按“不会覆盖”理解。正确说法应是：当前 APP 还在 64 KB 范围内，但参数区地址超出了 64 KB 物理 Flash。

### 3.2 后续 APP 增大会有影响

如果保持当前 scatter 文件：

```text
LR_IROM1 0x08004800 0x00020000
ER_IROM1 0x08004800 0x00020000
```

则链接器允许 APP 最大增长到：

```text
0x08004800 + 0x20000 - 1 = 0x080247FF
```

这会覆盖：

```text
0x0801C000 ~ 0x0801FFFF
```

也就是整个参数 Flash 区。

因此，当前最关键的问题不是“当前是否覆盖”，而是“链接边界没有保护参数区”。

### 3.3 IAP 升级擦写范围也必须同步确认

APP 侧仅能看到跳转和标志：

- `FLASH_ADDR_UPDATE_FLAG = 0x0801F800`
- 写入 `FLASH_TO_IAP_VALUE = 0x00AB` 后复位回 IAP
- `APP_To_IAP_Jump()` 跳转到 `0x08000000`

但当前仓库中没有看到完整 IAP 工程代码，因此还需要确认 IAP 擦写 APP 时的范围。

IAP 升级时必须只擦：

```text
0x08004800 ~ 0x0801BFFF
```

不能擦：

```text
0x0801C000 ~ 0x0801FFFF
```

否则升级会清空参数、日志、SOC 数据和升级策略标志。

## 4. RAM、堆、栈分布

### 4.1 RAM 配置

当前 scatter 文件配置：

```text
RW_IRAM1 0x20000000 0x00005000
```

即 RAM 总量按 `20 KB` 使用。

map 文件显示：

```text
RW + ZI = 7448 B = 7.27 KB
```

### 4.2 栈

启动文件配置：

```text
Stack_Size EQU 0x00000C00
```

即栈大小：

```text
0xC00 = 3072 B = 3 KB
```

map 文件中栈位于：

```text
0x20001118 ~ 0x20001D17
```

初始 SP：

```text
__initial_sp = 0x20001D18
```

### 4.3 堆

启动文件配置：

```text
Heap_Size EQU 0x00000200
```

即堆大小：

```text
512 B
```

但 map 文件显示：

```text
Removing startup_stm32f10x_hd.o(HEAP), (512 bytes)
```

说明当前没有实际使用 `malloc/free`，堆被链接器移除。

### 4.4 RAM 风险判断

当前 RAM 使用约 `7.27 KB`，按 20 KB 配置仍有约 `12.7 KB` 空间。

当前看 RAM 压力不大。主要风险不在 RAM，而在：

- APP Flash 链接范围没有排除参数区
- 中断和主循环共享状态较多，长期维护复杂
- Flash 写入 API 对独立半字擦页，需严格避免同页多标志互相覆盖

## 5. 当前外设使用情况

### 5.1 已启用或主路径使用的外设

| 外设 | 用途 | 说明 |
|---|---|---|
| GPIOA~GPIOE | 普通 IO、模拟输入、唤醒输入、控制输出 | 主初始化中统一启用 |
| AFIO | 引脚复用、JTAG 关闭、USART1 重映射 | `GPIO_Remap_SWJ_JTAGDisable` |
| USART1 | 上位机/通信口 | PB6/PB7，重映射，19200 |
| USART2 | 上位机/通信口 | PA2/PA3，19200 |
| USART3 | 代码支持但当前未启用 | `_COMMOM_UPPER_SCI3` 未定义 |
| CAN1 | CAN 通信 | PA11/PA12，目标波特率 250 kbit/s |
| ADC1 | 模拟采样 | 3 路 ADC |
| DMA1 Channel1 | ADC 数据搬运 | 环形模式，无实际 DMA 中断 |
| TIM2 | ADC 外部触发 | TIM2 CC2 触发 ADC |
| TIM3 | 系统 10 ms 软件节拍 | Update 中断 |
| TIM4 | LED 数码/74HC595 扫描 | 1 ms Update 中断 |
| SysTick | 阻塞延时 | 不作为系统 tick |
| RTC / BKP / PWR | 低功耗、休眠唤醒、BKP 标志 | RTC 主初始化未直接启用，低功耗路径使用 |
| IWDG | 看门狗代码存在 | 当前 Release 默认通过 `PROJECT_CFG_WDOG_ENABLE=1` 启用；`FD_Debug` 可按调试需要临时关闭 |
| DBGMCU | 调试低功耗配置 | `DBGMCU_STOP` 已限定在 `_DEBUG_` 下启用，Release 不应打开低功耗调试位 |

### 5.2 ADC 使用

当前 ADC 配置：

- ADC：`ADC1`
- 触发源：`TIM2_CC2`
- DMA：`DMA1_Channel1`
- 通道数量：`AD_Used_amount = 3`
- 通道：
  - `ADC_Channel_9`，PB1，`GPIO_ADC_NMOS`
  - `ADC_Channel_2`，PA2，`GPIO_ADC_CUR`
  - `ADC_Channel_1`，PA1，`GPIO_ADC_VBUS`

风险点：

- PA2 同时也是 USART2_TX，代码中 ADC 当前也配置 PA2 为模拟输入。若实际硬件 USART2 和 ADC_CUR 共用 PA2，需要确认是否为硬件复用冲突。
- ADC.c 中 DMA 中断配置代码被注释，实际没有 `DMA1_Channel1_IRQHandler`，这是合理的；不要误认为 DMA 中断已启用。

### 5.3 通信外设

USART：

- USART1：PB6/PB7，`GPIO_Remap_USART1`
- USART2：PA2/PA3
- 波特率：19200
- 中断：RXNE、IDLE、错误中断 EIE

CAN：

- CAN1 RX：PA11
- CAN1 TX：PA12
- 中断：`USB_LP_CAN1_RX0_IRQHandler`
- 接收 FIFO0 中断：`CAN_IT_FMP0`
- 当前 BusOff 自动恢复未启用，代码采用手动监控恢复

### 5.4 软件 I2C / AFE

SH367309 AFE 通过软件 I2C/TWI 访问：

- PB8：TWI_CLK
- PB9：TWI_DAT

原 EEPROM.h 中仍保留 PB10/PB11 访问外部 EEPROM 的宏，但当前 EEPROM.c 已将外部 EEPROM 读写接口空实现，实际参数已经转向内部 Flash 存储。

## 6. 当前中断使用情况

已实现的中断入口：

| 中断 | 入口函数 | 当前用途 |
|---|---|---|
| EXTI0 | `EXTI0_IRQHandler` | 充电/唤醒输入 |
| EXTI2 | `EXTI2_IRQHandler` | 清 pending，当前无业务 |
| EXTI3 | `EXTI3_IRQHandler` | 清 pending，当前无业务 |
| EXTI9_5 | `EXTI9_5_IRQHandler` | SW、USART1 RX 唤醒等 EXTI 线 |
| EXTI15_10 | `EXTI15_10_IRQHandler` | PB12 通信唤醒 |
| USART1 | `USART1_IRQHandler` | SCI1 通信 |
| USART2 | `USART2_IRQHandler` | SCI2 通信 |
| USART3 | `USART3_IRQHandler` | 代码存在，当前宏未启用 |
| USB_LP_CAN1_RX0 | `USB_LP_CAN1_RX0_IRQHandler` | CAN FIFO0 接收 |
| TIM3 | `TIM3_IRQHandler` | 10 ms 系统节拍 |
| TIM4 | `TIM4_IRQHandler` | LED 扫描 1 ms |
| RTC | `RTC_IRQHandler` | RTC 秒中断 |
| RTCAlarm | `RTCAlarm_IRQHandler` | RTC 闹钟/唤醒 |

当前 NVIC 分组：

```text
NVIC_PriorityGroup_1
```

典型优先级：

- TIM3：抢占 0，子优先级 3
- CAN RX0：抢占 1，子优先级 1
- TIM4：抢占 1，子优先级 3
- USART：抢占 3，子优先级 3
- EXTI 唤醒：抢占 1，子优先级 1

## 7. 架构现状评价

### 7.1 做得比较好的点

1. 参数 Flash 已经开始采用双 slot / journal 思路，不是单地址裸写。
2. AFE、SOC、日志已有 magic/version/length/sequence/crc 校验。
3. 休眠标志当前已迁移到 BKP 寄存器，避免频繁擦写 Flash。
4. TIM3 统一产生 10 ms 任务标志，主循环按任务标志执行，结构比纯阻塞轮询更可控。
5. ADC 使用 TIM2 触发 + DMA 环形搬运，采样路径比较稳定。

### 7.2 主要风险

#### 风险 1：APP 链接范围覆盖参数区

当前是最高优先级风险。

建议把 APP 链接区改为：

```text
LR_IROM1 0x08004800 0x00017800
ER_IROM1 0x08004800 0x00017800
```

这样 APP 最大只能到：

```text
0x0801BFFF
```

一旦代码增大超过安全边界，编译/链接阶段会直接报错，不会等到现场覆盖参数区。

#### 风险 2：目标芯片配置不一致

工程名和文件名显示 `103RCT6`，但 Keil 工程内看到：

```text
Device: STM32F103C8
Define: STM32F10X_MD
IRAM: 0x20000000, 0x5000
IROM: 0x08000000, 0x10000
```

同时启动文件使用：

```text
startup_stm32f10x_hd.s
```

这说明工程配置存在混用：

- 设备名像 C8
- RAM 按 20 KB
- 启动文件像 High Density
- 实际工程名像 RCT6

如果实际 MCU 是 STM32F103RCT6，应统一为 High Density 设备、`STM32F10X_HD`、正确 Flash/RAM 容量和启动文件。

#### 风险 3：Flash 写半字 API 擦整页

`FlashWriteOneHalfWord()` 当前逻辑是：

1. 解锁 Flash
2. 擦除 `StartAddr` 所在页
3. 写一个 halfword
4. 校验
5. 上锁

这适合独占一整页的标志位，不适合多个标志共用同一页。

当前：

- `0x0801F000`
- `0x0801F800`
- `0x0801FC00`

都在不同 2 KB page 或同页偏移边界附近，需要严格保持“不共享页写入”的原则。

#### 风险 4：IAP 工程擦写策略未知

APP 侧只能确认标志和跳转，不能确认 IAP 实际擦写范围。

必须补充确认 IAP：

- 是否擦到 `0x0801C000` 之后
- 是否把 `0x0801F800` 更新标志清回 `0xFFFF`
- 是否知道 APP 最大允许范围
- 是否有升级包长度校验，防止包过大覆盖参数区

#### 风险 5：PA2 可能存在 USART2_TX 与 ADC_CUR 复用冲突

代码中：

- USART2_TX 使用 PA2
- ADC_CUR 也定义为 PA2 / ADC Channel 2

如果硬件确实共用，必须重新确认当前产品型态下 USART2 和 ADC_CUR 是否会同时使用。

## 8. 优化建议

### 8.1 立即建议

1. 先确认真实 MCU Flash 容量；如果是标准 STM32F103C8，必须按 64 KB 重新规划，不能继续使用 `0x0801C000` 参数区。
2. 如果确认实际 Flash >= 128 KB，再修改 APP scatter 文件，限制 APP 不超过 `0x0801BFFF`。
3. 增加构建后检查脚本：C8 检查 bin 结束地址必须小于等于 `0x0800FFFF`；Flash >= 128 KB 且保留当前参数区时，检查 bin 结束地址必须小于 `0x0801C000`。
4. 明确芯片真实型号，统一 Keil Device、启动文件、宏定义、IROM/IRAM。
5. 补充 IAP 擦写范围文档，明确不能擦参数区。
6. 保留参数区地址集中定义，避免业务代码散落裸地址。

### 8.2 中期建议

1. 建立统一 Flash 分区表头文件，例如 `flash_layout.h`：
   - IAP 区
   - APP 区
   - 参数区
   - 每个 page 大小
   - 编译期边界检查
2. 参数存储继续统一到当前 `StorageFlash_*` 框架，减少 `FlashWriteOneHalfWord()` 裸写。
3. 把升级标志、策略标志也改成带 magic/version/crc 的小记录，避免单 halfword 状态不清晰。
4. 对中断共享变量做一次审计：
   - ISR 只置位或搬运数据
   - 主循环消费
   - 多字节共享变量读取时加临界区或快照
5. 对 USART/CAN/ADC/TIM 做资源表，后续新增功能必须先查资源表再分配外设。

### 8.3 如果确认 MCU 是 STM32F103RCT6

如果实际不是 C8，而是 256 KB Flash 的 RCT6，更推荐把参数区放到 Flash 尾部：

```text
参数区建议：
0x0803C000 ~ 0x0803FFFF
```

这样可得到：

```text
IAP:   0x08000000 ~ 0x080047FF
APP:   0x08004800 ~ 0x0803BFFF
PARAM: 0x0803C000 ~ 0x0803FFFF
```

优点：

- APP 可增长空间显著增加
- 参数区位于芯片尾部，分区更直观
- IAP 擦 APP 时可按区间擦，不容易误伤参数

但前提是必须确认：

- 实际 MCU Flash 容量确实为 256 KB
- IAP 支持写入更高地址
- Keil 工程目标和下载算法同步更新

## 9. 建议的安全边界

### 9.1 如果 MCU 是标准 STM32F103C8

```text
IAP_START      = 0x08000000
APP_START      = 0x08004800
FLASH_END      = 0x0800FFFF

APP_MAX_SIZE   = FLASH_END + 1 - APP_START
               = 0x0000B800
               = 47104 B
               = 46 KB
```

当前 APP：

```text
44112 B / 47104 B = 93.6%
```

当前剩余：

```text
2992 B = 2.92 KB
```

这种情况下不建议继续把内部 Flash 当参数区使用，除非压缩 APP 并从尾部预留 1~2 KB page，或者改回外部 EEPROM / BKP / 更大 Flash 型号。

### 9.2 如果实际 Flash >= 128 KB 并保留当前参数区

在不搬迁参数区的情况下，建议采用：

```text
IAP_START       = 0x08000000
APP_START       = 0x08004800
PARAM_START     = 0x0801C000
FLASH_END_USED  = 0x0801FFFF

APP_MAX_SIZE    = PARAM_START - APP_START
                = 0x00017800
                = 96256 B
                = 94 KB
```

当前 APP：

```text
44112 B / 96256 B = 45.8%
```

当前剩余：

```text
52144 B = 50.92 KB
```

## 10. 优先级清单

| 优先级 | 动作 | 原因 |
|---|---|---|
| P0 | 修正 APP scatter 上限到 `0x17800` | 防止后续代码增长覆盖参数区 |
| P0 | 确认 IAP 擦写范围 | 防止升级擦掉参数区 |
| P1 | 统一真实 MCU 型号配置 | 当前 Device/宏/启动文件存在混用迹象 |
| P1 | 增加 bin 地址边界检查 | 让风险在构建阶段暴露 |
| P2 | 整理 Flash layout 文档和头文件 | 后续维护更安全 |
| P2 | 审计 PA2 复用冲突 | ADC_CUR 与 USART2_TX 可能冲突 |
| P2 | 将裸 halfword Flash 标志迁移到记录结构 | 提高掉电和误擦容错 |

## 11. 总结

如果按当前工程的 128 KB 地址规划看，当前固件资源余量是够的：

- Flash：当前 APP 约 `43.08 KB`，按当前参数区前安全上限还有约 `50.92 KB` 可增长。
- RAM：当前约 `7.27 KB`，按 20 KB 配置仍有约 `12.7 KB` 可用。
- 栈：3 KB，当前未看到明显不足证据。
- 堆：源码配置 512 B，但当前未实际使用。

但如果实际 MCU 是 STM32F103C8，则 Flash 资源已经很紧：

- C8 Flash 范围只有 `0x08000000~0x0800FFFF`。
- IAP 保留 18 KB 后，APP 最大约 46 KB。
- 当前 APP 已经 43.08 KB，仅剩约 2.92 KB。
- 当前 `0x0801C000` 参数区无效，必须重新规划。

真正需要优先处理的是确认真实芯片容量、Flash 分区边界和工程目标配置一致性。

当前建议先不要继续盲目增加 APP 功能体积，先把真实 MCU 容量、链接边界、IAP 擦写范围和 MCU 型号配置修正。若实际是 C8，APP 超过 46 KB 就会越界；若实际 Flash >= 128 KB 且仍使用当前参数区，APP 超过 94 KB 会覆盖参数区。两种情况都应该让链接器或构建脚本提前报错。
