# LedBar 模块逻辑与原理梳理

## 1. 文档目的

本文基于当前工作区中的 `103 + 309/Project/Source/LedBar.c`、`LedBar.h` 以及主循环、时基、SOC/电流数据来源代码，对 `LedBar` 模块做一次面向维护的梳理。目标不是复述函数名，而是回答下面几个问题：

- 这个模块当前到底在驱动什么硬件。
- 显示数据从哪里来，如何被编码成段码。
- GPIO 是如何点亮每一个段的。
- 当前工程里它是否已经真正接通。
- 现阶段有哪些明显风险和未闭环点。

## 2. 结论摘要

先给结论，避免看完整篇后才发现关键点：

- 当前 `LedBar` 实现是 `5` 线 `Charlieplex` 扫描驱动，不是 `74HC595` 移位寄存器方案。
- 模块使用 `PC4~PC0` 五根 IO 线，理论上可形成 `5 x 4 = 20` 个有向发光通路，当前实际用了 `18` 个。
- 这 `18` 个通路被分配为：
  - 十位数码管 `7` 段：`LEDBAR_SEG_1A ~ LEDBAR_SEG_1G`
  - 个位数码管 `7` 段：`LEDBAR_SEG_2A ~ LEDBAR_SEG_2G`
  - 状态指示 `4` 段：`LEDBAR_SEG_H1 ~ LEDBAR_SEG_H4`
- 业务层入口是 `APP_LedBar()`，物理扫描入口是 `LedBar_Scan1ms()`。
- 当前主工程里没有找到 `LedBar_Scan1ms()` 的实际调度点，`main.c` 中的 `APP_LedBar()` 也被注释掉了，所以模块处于“核心算法已写、系统接线未完全闭环”的状态。
- 当前实现里至少有两个需要明确记录的问题：
  - `LedBar_SetNumber(100)` 会在 `LedBar_RebuildActiveMask()` 中访问 `s_digit_segments[10]`，存在数组越界。
  - `APP_LedBar()` 里最后无条件点亮了 `H1~H4`，导致前面的充电/放电/Fault/蓝牙条件判断基本被覆盖。

## 3. 模块文件与入口

### 3.1 主要文件

- `103 + 309/Project/Source/LedBar.h`
  - 定义段号、指示灯掩码、GPIO 管脚映射、对外接口。
- `103 + 309/Project/Source/LedBar.c`
  - 实现段路由表、数字段码表、GPIO 高阻/推挽切换、显示掩码构建和轮询扫描。

### 3.2 对外接口

模块当前对外暴露的核心接口如下：

- `LedBar_Init()`
- `LedBar_Scan1ms()`
- `LedBar_SetNumber(UINT8 value)`
- `LedBar_SetIndicators(UINT8 indicator_mask)`
- `LedBar_SetIndicatorState(UINT8 indicator_mask, UINT8 enable)`
- `LedBar_Clear()`
- `APP_LedBar()`

可以把它们分成两层理解：

- 业务层接口：
  - `APP_LedBar()`
  - `LedBar_SetNumber()`
  - `LedBar_SetIndicators()`
  - `LedBar_SetIndicatorState()`
  - `LedBar_Clear()`
- 物理驱动层接口：
  - `LedBar_Init()`
  - `LedBar_Scan1ms()`

## 4. 硬件原理：这是一个 5 线 Charlieplex 显示

### 4.1 五根控制线

`LedBar.h` 中把五根逻辑线定义为：

| 逻辑编号 | GPIO |
| --- | --- |
| `P1` | `PC4` |
| `P2` | `PC3` |
| `P3` | `PC2` |
| `P4` | `PC1` |
| `P5` | `PC0` |

也就是：

- `LEDBAR_PIN_P1 = GPIOC Pin 4`
- `LEDBAR_PIN_P2 = GPIOC Pin 3`
- `LEDBAR_PIN_P3 = GPIOC Pin 2`
- `LEDBAR_PIN_P4 = GPIOC Pin 1`
- `LEDBAR_PIN_P5 = GPIOC Pin 0`

### 4.2 Charlieplex 的本质

Charlieplex 不是“给某个段写 1 就亮”的静态显示，而是依赖三态 IO 的定向导通：

- 选中一段时：
  - 一根线输出低电平
  - 一根线输出高电平
  - 其余所有线全部切成输入高阻
- 因为 LED 具有方向性，所以“`A 线拉低 + B 线拉高`”与“`B 线拉低 + A 线拉高`”可以代表两个不同的发光段。

如果有 `N` 根线，理论上可以形成 `N x (N - 1)` 个有向通路。

对当前模块来说：

- `N = 5`
- 理论可用通路数 = `5 x 4 = 20`
- 当前实际使用了 `18` 个
- 还剩 `2` 个有向组合未使用

### 4.3 为什么要把未使用的脚切成输入高阻

这个点是整个模块最核心的物理原理。

如果未参与当前点亮的 IO 继续保持输出态，就可能形成旁路电流，导致：

- 串亮
- 鬼影
- 多段误点亮

所以 `LedBar_Scan1ms()` 在每次选择下一段前，第一件事就是调用 `LedBar_AllPinsHiZ()`，把五根线全部切成输入态。随后只把当前目标段对应的两根线切成输出：

- `low_pin` 输出低
- `high_pin` 输出高

这样才能保证同一时刻只有一个逻辑段真正导通。

## 5. 段资源分配与路由表

### 5.1 段资源分组

当前模块把 `18` 个逻辑段定义为三组：

| 分组 | 段 |
| --- | --- |
| 十位数码管 | `1A 1B 1C 1D 1E 1F 1G` |
| 个位数码管 | `2A 2B 2C 2D 2E 2F 2G` |
| 指示灯 | `H1 H2 H3 H4` |

### 5.2 路由表的含义

`s_ledbar_routes[]` 定义了“每个逻辑段由哪一对 IO 方向驱动”。这里的每一项都不是“某个引脚对应某个 LED”，而是：

- `low_pin`：这根线要被拉低
- `high_pin`：这根线要被拉高

### 5.3 当前段到 IO 的对应关系

#### 十位数码管

| 段 | 低电平线 | 高电平线 |
| --- | --- | --- |
| `1A` | `P3(PC2)` | `P2(PC3)` |
| `1B` | `P2(PC3)` | `P3(PC2)` |
| `1C` | `P3(PC2)` | `P4(PC1)` |
| `1D` | `P2(PC3)` | `P4(PC1)` |
| `1E` | `P2(PC3)` | `P5(PC0)` |
| `1F` | `P3(PC2)` | `P5(PC0)` |
| `1G` | `P4(PC1)` | `P5(PC0)` |

#### 个位数码管

| 段 | 低电平线 | 高电平线 |
| --- | --- | --- |
| `2A` | `P2(PC3)` | `P1(PC4)` |
| `2B` | `P1(PC4)` | `P2(PC3)` |
| `2C` | `P3(PC2)` | `P1(PC4)` |
| `2D` | `P1(PC4)` | `P3(PC2)` |
| `2E` | `P4(PC1)` | `P1(PC4)` |
| `2F` | `P1(PC4)` | `P4(PC1)` |
| `2G` | `P1(PC4)` | `P5(PC0)` |

#### 状态灯

| 段 | 低电平线 | 高电平线 |
| --- | --- | --- |
| `H1` | `P4(PC1)` | `P3(PC2)` |
| `H2` | `P4(PC1)` | `P2(PC3)` |
| `H3` | `P5(PC0)` | `P3(PC2)` |
| `H4` | `P5(PC0)` | `P2(PC3)` |

### 5.4 未使用的两个方向组合

5 线 Charlieplex 理论上有 20 个方向组合，当前只用掉 18 个。对照现有路由表，未使用的两个方向组合是：

- `P5 -> P1`
- `P5 -> P4`

这意味着后续如果要再扩展两个指示灯，硬件上仍有理论余量，但前提是 PCB 实物确实把这两条 LED 通路接出来了。

## 6. 数字显示编码逻辑

### 6.1 数字到 7 段的映射

`s_digit_segments[10]` 是一个标准 7 段码表，索引 `0~9` 分别表示数字 `0~9` 应点亮哪些段。

模块内部用下面这些位来表示段：

- `A B C D E F G`

例如：

- `0` 亮 `A B C D E F`
- `1` 亮 `B C`
- `8` 亮 `A B C D E F G`

### 6.2 显示数值如何转成段掩码

`LedBar_RebuildActiveMask()` 是显示逻辑的核心。它做了三件事：

1. 计算十位和个位：
   - `tens = number / 10`
   - `ones = number % 10`
2. 根据数字段码表，把十位/个位需要亮的段加入 `s_ledbar_active_mask`
3. 把 `H1~H4` 的状态指示位也 OR 进 `s_ledbar_active_mask`

最终 `s_ledbar_active_mask` 是一个 `32` 位掩码，哪一位为 `1`，表示那个逻辑段需要在扫描过程中被轮流点亮。

## 7. 软件执行流程

### 7.1 初始化

`LedBar_Init()` 的动作如下：

- 把 `PC4~PC0` 全部初始化成输入浮空
- 清零当前数字和指示灯状态
- 把 `s_ledbar_scan_index` 置为无效
- 置 `s_ledbar_initialized = 1`
- 把 `LedBar_Command` 设为 `LED_BAR_NORMAL`
- 调用 `LedBar_RebuildActiveMask()` 生成初始显示掩码

这里可以看出，当前初始化策略不是“上电立即点亮默认图案”，而是“所有线先进入高阻安全态，再等业务逻辑给内容”。

### 7.2 业务层更新

`APP_LedBar()` 是业务适配函数，当前逻辑是：

1. 如果 `SystemStatus.bits.b1StartUpBMS != 0`，直接返回
   - 也就是 BMS 启动阶段不显示
2. 如果尚未初始化，则先 `LedBar_Init()`
3. 从 `g_stCellInfoReport.SocElement.u16Soc` 取显示数值
4. 如果超过 `100`，截断为 `100`
5. 根据 `g_stCellInfoReport.u16Ichg`、`u16IDischg` 组装指示灯掩码
6. 调用：
   - `LedBar_SetNumber(display_value)`
   - `LedBar_SetIndicators(indicator_mask)`

### 7.3 物理扫描

`LedBar_Scan1ms()` 的单次扫描过程如下：

1. 若模块未初始化，直接返回
2. 先把五根引脚全部切成高阻输入
3. 如果 `s_ledbar_active_mask == 0`，说明当前无任何段需要亮，结束
4. 从上一次扫描位置之后开始，寻找下一个需要点亮的段
5. 找到后：
   - 读取该段对应的 `low_pin/high_pin`
   - `low_pin` 输出低
   - `high_pin` 输出高
   - 其余引脚保持高阻
6. 本次扫描结束，等下一次 `1ms` 再换下一段

这说明模块采用的是“单段轮询扫描”，而不是“整位刷新”。

如果未来按函数命名那样每 `1ms` 调用一次 `LedBar_Scan1ms()`，那么：

- 最坏情况下 `18` 个逻辑段都需要点亮
- 完整轮完一帧约需要 `18ms`
- 等效整屏刷新频率约为 `55Hz`

这个频率勉强可用，但已经接近人眼可感知闪烁的边界，所以扫描调度最好尽量稳定，不能再被主循环抖动进一步拉低。

## 8. 业务数据从哪里来

`LedBar` 自己不算 SOC，也不采电流，它只消费系统状态。

### 8.1 显示数值来源

`APP_LedBar()` 读取的是：

- `g_stCellInfoReport.SocElement.u16Soc`

这个字段在 `SOC.c` 的 `GetData_SOC()` 中被写回：

- `g_stCellInfoReport.SocElement.u16Soc = SOC_Enhance_Element.u8_SOC;`

因此 `LedBar` 上显示的数值，本质上是 SOC 模块最终给出的显示 SOC，而不是 `LedBar` 自己计算的电量。

### 8.2 充放电状态来源

`APP_LedBar()` 的指示灯判断使用：

- `g_stCellInfoReport.u16Ichg`
- `g_stCellInfoReport.u16IDischg`

这两个值在 `DataDeal.c` 中根据采样电流换算得到，并且做了一个低电流清零门限：

- `u16Ichg <= 2` 时清零
- `u16IDischg <= 2` 时清零

所以 `LedBar` 的充/放电指示本质上是“是否存在有效充电电流/放电电流”。

## 9. 当前工程中的真实接入状态

这一节非常关键，因为“代码能跑”和“代码已经接到主系统里”不是一回事。

### 9.1 `APP_LedBar()` 在主循环中当前是注释状态

`main.c` 里存在：

- `App_SysTime();`
- `// APP_LedBar();`

也就是说主线运行路径中，`APP_LedBar()` 当前并没有被执行。

### 9.2 `LedBar_Scan1ms()` 当前没有实际调度点

在当前 `Source` 目录下，`LedBar_Scan1ms()` 只有定义和声明，没有找到调用点。

这意味着即使业务层已经构建了 `s_ledbar_active_mask`，也没有谁在持续切换 GPIO 去完成动态扫描，显示就不可能稳定输出。

### 9.3 `conf.c`/`rtc_sleep.c` 中仍有旧显示路径残留

当前还能看到两处旧路径痕迹：

- `conf.c` 中在 `__FUNC__LED__` 条件下调用了 `APP_LedBar(); set_LED_state(LED_BAR_NORMAL, 4);`
- `rtc_sleep.c` 中在 `__FUNC__LED__` 条件下调用了 `set_LED_state(LED_BAR_NORMAL, 4);`

但在当前 `Source` 目录里没有找到 `set_LED_state()` 的实现。

这说明工程里同时存在：

- 新的 `Charlieplex LedBar` 实现
- 旧的显示控制接口残留

两套路径目前并没有完全统一。

### 9.4 旧文档与现代码已经不一致

仓库根目录下已有一份 `LEDBAR_74HC595_OE_FIXED_INTEGRATION.md`，但从当前 `LedBar.c` 的实现看，模块已经不是 `74HC595` 方案，而是直接 GPIO Charlieplex 驱动。

因此后续维护应优先以当前代码为准，不能再把那份文档当作现行实现说明。

## 10. 当前实现中的关键逻辑细节

### 10.1 百位显示的设计意图

代码试图在 `number >= 100` 时：

- 点亮 `H1`
- 点亮 `H2`

也就是说，`H1/H2` 不只是普通状态灯，还承担了“显示 100”时的百位提示作用。

这是一种节省显示资源的做法：不用真正增加第三位数码管，而是用两个额外图标表达“100%”。

### 10.2 指示灯和百位提示共用资源

`LedBar_RebuildActiveMask()` 的逻辑是：

- 若 `number >= 100`，先把 `H1/H2` 点亮
- 然后再把 `s_ledbar_indicator_mask` 中的 `H1~H4` OR 进去

因此：

- `H1/H2` 同时承担“百位提示”和“状态灯”两种语义
- 这会让显示语义变得比较耦合

如果后续要把 `H1/H2` 改成纯状态灯，那么 `100%` 的显示方式也要跟着改。

### 10.3 GPIO 切换顺序是有意设计的

`LedBar_PinToOutput()` 里是先写电平，再切输出模式，而不是先切输出再写电平。

这个顺序是合理的，因为可以降低输出瞬间的毛刺风险，避免在切换到推挽输出的一瞬间把错误电平打到 LED 网络上。

## 11. 已识别问题与风险

### 11.1 `100` 显示存在数组越界

这是当前最明确的功能性问题。

`LedBar_RebuildActiveMask()` 中：

- `tens = s_ledbar_number / 10`
- 当 `s_ledbar_number == 100` 时，`tens == 10`
- 随后代码直接访问 `s_digit_segments[tens]`

但 `s_digit_segments` 只有 `0~9` 十个元素。

因此：

- `100` 这个合法输入值会触发越界访问
- 当前“百位提示”逻辑并没有真正把 `100` 的显示处理完整

### 11.2 指示灯当前被无条件点亮

`APP_LedBar()` 里虽然先根据充电/放电状态设置了 `H1/H2`，并保留了 `H3/H4` 的 Fault/蓝牙注释逻辑，但在后面又直接执行了：

- `indicator_mask |= LEDBAR_H1_MASK;`
- `indicator_mask |= LEDBAR_H2_MASK;`
- `indicator_mask |= LEDBAR_H3_MASK;`
- `indicator_mask |= LEDBAR_H4_MASK;`

结果就是：

- `H1~H4` 会一直亮
- 充/放电/Fault/蓝牙的条件判断目前没有真正起作用

从代码形态上看，这更像是调试阶段的强制点亮遗留。

### 11.3 扫描时基没有真正接入

`LedBar_Scan1ms()` 的命名和实现都表明它需要稳定周期调用，但当前工程没有实际调用点。

直接后果是：

- 显示不会刷新
- 就算业务层设置了段掩码，也无法完成轮询点亮

### 11.4 旧接口残留，维护时容易误判

当前同时存在：

- 新接口：`LedBar_SetNumber / LedBar_SetIndicators / LedBar_Scan1ms`
- 旧接口痕迹：`set_LED_state(LED_BAR_NORMAL, 4)`
- 旧状态枚举：`LedBar_Command`

但 `LedBar_Command` 当前除了初始化赋值外，没有真正参与显示流程。

这说明模块仍处于重构过渡态，维护者如果只看宏名和枚举名，容易误以为还存在一套更高层的状态机。

## 12. 维护建议

如果后续要把 `LedBar` 真正接回主工程，建议按下面顺序处理：

1. 先修正 `100` 的显示越界问题
   - 这是明确 bug，不应继续带着接入
2. 明确 `H1~H4` 的真实产品语义
   - 哪些是充/放电
   - 哪些是 Fault/蓝牙
   - `100%` 是否继续复用 `H1/H2`
3. 统一显示入口
   - 删除或替换旧的 `set_LED_state()` 路径
   - 明确只保留一套新接口
4. 给 `LedBar_Scan1ms()` 增加稳定调度点
   - 最好接到可靠的 `1ms` 周期任务中
5. 再决定是否恢复 `main.c` 中的 `APP_LedBar()` 主循环调用

## 13. 一句话总结

当前 `LedBar` 模块的核心原理已经很清楚：它是基于 `PC4~PC0` 五根 IO 的 `Charlieplex` 单段轮询显示驱动，业务上用来显示 SOC 和状态灯；但从工程接入角度看，它还没有完全闭环，至少还存在“`100` 越界、指示灯全亮、扫描未接入、旧接口残留”四个必须先澄清的问题。
