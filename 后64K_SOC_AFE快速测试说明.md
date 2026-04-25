# 后 64K SOC/AFE 参数快速测试说明

> 适用工程：`103 + 309/Project`
> 测试目标：快速确认当前应用实际使用的后 64K Flash 参数存储路径是否可写、可读、可校验。

## 1. 测试结论定位

当前包含两种测试模式：

- 快速压力测试：开机后主动循环写 SOC/AFE，测试完成后停机输出结果。
- 客户使用场景测试：正常运行 BMS，真实业务每次保存 SOC/AFE 后立即读回校验，并周期输出统计。

客户使用场景测试更适合判断“这个应用能不能在这颗 C8 的后 64K 上保存业务参数”。快速压力测试更适合快速暴露 Flash 页大小、双槽切换、journal 边界等问题。

这两种测试都不是完整的全后 64K Flash 物理压力测试。

它会直接调用当前工程的参数存储 API：

- `StorageFlash_SaveSocData()`
- `StorageFlash_LoadSocData()`
- `StorageFlash_SaveAfeData()`
- `StorageFlash_LoadAfeData()`

因此它能覆盖当前应用最关心的部分：

| 业务数据 | Slot A | Slot B |
|---|---:|---:|
| AFE 参数 | `0x0801C000` | `0x0801C800` |
| SOC 数据 | `0x0801E000` | `0x0801E800` |

如果这个测试失败，说明当前项目使用 `0x0801C000` 后 64K 参数区存在直接风险。

如果这个测试通过，只能说明当前板子在这些业务地址、当前温度/电压/循环次数下可用；还不能证明整片后 64K 长期可靠。

## 2. 如何启用

文件：`103 + 309/Project/Source/conf/conf.h`

### 2.1 客户使用场景测试

当前推荐打开客户使用场景加速测试：

```c
// #define FLASH64K_APP_QUICK_TEST_ENABLE
#define FLASH64K_APP_QUICK_TEST_CYCLES 96U

#define FLASH64K_APP_USE_TEST_ENABLE
#define FLASH64K_APP_USE_TEST_PRINT_PERIOD_SEC 10U
#define FLASH64K_APP_USE_TEST_ACCEL_ENABLE
#define FLASH64K_APP_USE_TEST_ACCEL_SOC_PERIOD_SEC 1U
#define FLASH64K_APP_USE_TEST_ACCEL_AFE_PERIOD_SEC 30U
```

运行效果：

- 固件继续正常跑主循环。
- SOC 由实际业务变化并保存时，测试模块校验 `StorageFlash_SaveSocData()` 写后是否能 `StorageFlash_LoadSocData()` 读回一致。
- AFE 参数由实际上位机/客户操作保存时，测试模块校验 `StorageFlash_SaveAfeData()` 写后是否能 `StorageFlash_LoadAfeData()` 读回一致。
- 每 `10s` 输出一次累计统计。
- 加速打开后，每 `1s` 模拟一次 SOC 保存，SOC 在 `0~100~0` 之间往返变化。
- 加速打开后，每 `30s` 读取当前已保存的 AFE 参数并原样重写一次；如果当前没有有效 AFE 参数，则跳过，不写测试假参数。

加速模式仍然走真实业务存储 API：

```c
StorageFlash_SaveSocData()
StorageFlash_SaveAfeData()
```

所以它比纯压力测试更接近客户使用，又比等待真实 SOC 慢慢变化更快。

### 2.2 快速压力测试

需要快速反复写 SOC/AFE 时，打开：

```c
#define FLASH64K_APP_QUICK_TEST_ENABLE
#define FLASH64K_APP_QUICK_TEST_CYCLES 96U
```

快速压力测试会在启动阶段运行，测试完成后停在结果输出循环中，不继续运行正常 BMS 业务。

## 3. Flash page size 修正

当前工程定义 `STM32F10X_MD`，按 STM32F103C8/CB medium-density 处理，参数 Flash page size 使用：

```text
FLASH_STORAGE_PAGE_SIZE = 0x400 = 1 KB
```

之前按 `0x800 = 2 KB` 处理时，在 C8/CB 上会把两个物理 1KB 页当成一个页使用，容易在 SOC journal 写到页中后段时暴露擦除/恢复异常。

## 4. 运行位置

快速压力测试入口在 `InitDevice()` 中：

```c
InitSystemWakeUp();
#ifdef FLASH64K_APP_QUICK_TEST_ENABLE
StorageFlash_RunAppQuickTest();
#endif
InitE2PROM();
```

客户使用场景测试入口在主循环中：

```c
App_SOC();
StorageFlash_AppUseTest_Task();
```

真实写后读回校验接在底层保存 API 中：

```c
StorageFlash_SaveSocData()
StorageFlash_SaveAfeData()
```

## 5. 串口输出

测试使用当前工程 `printf()` 重定向，默认走 `USART1`：

- `USART1`
- `19200 bps`
- `8N1`

### 5.1 开机诊断日志

每次启动都会在 `InitE2PROM()` 之前打印只读诊断：

```text
[FLASH_BOOT] flash_size_reg=128KB page=1024
[FLASH_BOOT] AFE A=1 seq=12 B=1 seq=11 selected=A
[FLASH_BOOT] SOC A=1 seq=205 next=0x01A0 B=1 seq=204 next=0x0180 selected=A
[FLASH_BOOT] flag update=0xFFFF upgrade_param=0xFFFF
```

断电测试时重点看：

| 日志 | 含义 |
|---|---|
| `flash_size_reg=128KB` | 当前芯片容量寄存器报告 128KB |
| `page=1024` | 当前工程按 1KB Flash page 工作，适合 F103C8/CB medium-density |
| `AFE A/B=1` | AFE 对应备份槽 CRC/header 有效 |
| `SOC A/B=1` | SOC 对应 journal 页内至少有一条有效记录 |
| `selected=A/B` | 当前启动会选择的有效数据源 |
| `next=0x....` | 下一条 SOC journal 记录写入偏移 |

如果你中途断电后看到：

```text
[FLASH_BOOT] AFE A=1 seq=20 B=0 seq=0 selected=A
```

说明其中一个 AFE 备份可能被断电打坏，但另一个备份仍然有效，可以继续恢复。

如果看到：

```text
[FLASH_BOOT] SOC A=0 seq=0 ... B=0 seq=0 ... selected=-
```

说明 SOC 两个备份页都没有可用记录，这才是严重问题。

### 5.2 客户场景测试日志

客户使用场景测试典型输出：

```text
[FLASH64K_USE_TEST] start
[FLASH64K_USE_TEST] flash_size_reg=128KB page=1024 print=10s
[FLASH64K_USE_TEST] accel on: soc=1s afe=30s
[FLASH64K_USE_TEST] monitor real SOC/AFE save->load verify while app is running
[FLASH64K_USE_TEST] SOC save=3 fail=0 verify=3 vfail=0 last=80/80/12
[FLASH64K_USE_TEST] AFE save=1 fail=0 verify=1 vfail=0 last=0x1234/0x5678
[FLASH64K_USE_TEST] accel soc_write=10 afe_write=0 afe_skip=0 soc_now=10
```

快速压力测试典型输出：

```text
[FLASH64K_APP_TEST] quick storage test start
[FLASH64K_APP_TEST] flash_size_reg=128KB cycles=96 page=1024
[FLASH64K_APP_TEST] AFE: 0x0801C000 0x0801C800, SOC: 0x0801E000 0x0801E800
[FLASH64K_APP_TEST] destructive: SOC/AFE slots will be rewritten
[FLASH64K_APP_TEST] cycle 8/96 ok
...
[FLASH64K_APP_TEST] restore old_soc=1 old_afe=1
[FLASH64K_APP_TEST] finished result=PASS cycle=96
```

最终会每秒重复输出：

```text
[FLASH64K_APP_TEST] result=PASS cycle=96
```

如果失败，会输出类似：

```text
[FLASH64K_APP_TEST] result=VERIFY_AFE_FAIL cycle=3
```

## 6. 结果解释

| 结果 | 含义 |
|---|---|
| `PASS` | SOC/AFE 写入、读回、CRC/双槽选择流程在当前板子上通过 |
| `SAVE_SOC_FAIL` | SOC 写入失败，重点查 `0x0801E000/0x0801E800` |
| `LOAD_SOC_FAIL` | SOC 写后读记录失败，重点查 header/CRC/journal |
| `VERIFY_SOC_FAIL` | SOC 读回内容与写入内容不一致 |
| `SAVE_AFE_FAIL` | AFE 写入失败，重点查 `0x0801C000/0x0801C800` |
| `LOAD_AFE_FAIL` | AFE 写后读记录失败 |
| `VERIFY_AFE_FAIL` | AFE 读回内容与写入内容不一致 |
| `RESTORE_SOC_FAIL` | 测试完成后恢复原 SOC 数据失败 |
| `RESTORE_AFE_FAIL` | 测试完成后恢复原 AFE 数据失败 |

客户使用场景统计说明：

| 字段 | 含义 |
|---|---|
| `save` | 业务保存成功次数 |
| `fail` | 业务保存失败次数 |
| `verify` | 保存后读回一致次数 |
| `vfail` | 保存后读回失败或数据不一致次数 |
| `last` | 最近一次校验通过的数据摘要 |
| `accel soc_write` | 加速模式主动写入 SOC 的次数 |
| `accel afe_write` | 加速模式主动重写 AFE 参数副本的次数 |
| `accel afe_skip` | 没有有效 AFE 参数可重写时的跳过次数 |

## 7. 重要风险

快速压力测试是破坏性的。它会重写：

- `0x0801C000`
- `0x0801C800`
- `0x0801E000`
- `0x0801E800`

测试开始时会尝试备份原 SOC/AFE 数据，结束时如果原数据有效，会尝试恢复。但如果原数据本身无效，或者测试中途失败，Flash 中可能会留下测试数据。

正式固件中必须保持：

```c
// #define FLASH64K_APP_QUICK_TEST_ENABLE
```

客户使用场景测试会在真实业务保存时做额外读回校验；加速模式还会主动增加 Flash 擦写次数。正式出货前必须关闭：

```c
// #define FLASH64K_APP_USE_TEST_ENABLE
// #define FLASH64K_APP_USE_TEST_ACCEL_ENABLE
```

## 8. 建议测试流程

1. 打开 `FLASH64K_APP_USE_TEST_ENABLE` 和 `FLASH64K_APP_USE_TEST_ACCEL_ENABLE`，关闭 `FLASH64K_APP_QUICK_TEST_ENABLE`。
2. 编译并烧录测试固件。
3. 先静置运行，观察 SOC 是否每秒写入并读回通过。
4. 用上位机按客户实际流程读写 AFE 参数；随后观察 AFE 是否每 30 秒原样重写并校验通过。
5. 观察 `SOC save/fail/verify/vfail` 和 `AFE save/fail/verify/vfail`。
6. 断电重启后继续测试，确认历史参数还能读出，并且后续保存校验继续通过。
7. 多拿几块板重复测试，不要只测一块样机。

## 9. 后续仍需补充的可靠性测试

这个快速测试通过后，仍建议追加：

- `0x08010000~0x0801FFFF` 全后 64K 裸擦写读回测试
- 高低温测试
- 低电压擦写测试
- 掉电恢复测试
- 至少数千次循环擦写测试

这样才能判断“后 64K”是否足够支撑你的应用长期保存参数。
