# 后 64K SOC/AFE 参数快速测试说明

> 适用工程：`103 + 309/Project`
> 测试目标：快速确认当前应用实际使用的后 64K Flash 参数存储路径是否可写、可读、可校验。

## 1. 测试结论定位

这个测试不是全后 64K Flash 压力测试，而是业务路径快速测试。

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

默认关闭：

```c
// #define FLASH64K_APP_QUICK_TEST_ENABLE
#define FLASH64K_APP_QUICK_TEST_CYCLES 96U
```

测试时打开：

```c
#define FLASH64K_APP_QUICK_TEST_ENABLE
#define FLASH64K_APP_QUICK_TEST_CYCLES 96U
```

`FLASH64K_APP_QUICK_TEST_CYCLES` 是循环次数。默认 `96` 次，目的是让 SOC journal 页有机会进入更接近真实使用的滚动写入，同时让 AFE 双槽反复擦写切换。

## 3. 运行位置

测试入口在 `InitDevice()` 中：

```c
InitSystemWakeUp();
#ifdef FLASH64K_APP_QUICK_TEST_ENABLE
StorageFlash_RunAppQuickTest();
#endif
InitE2PROM();
```

也就是说，测试发生在正常读取 EEPROM/Flash 参数之前。测试程序会停在结果输出循环中，不继续执行正常 BMS 业务逻辑。

## 4. 串口输出

测试使用当前工程 `printf()` 重定向，默认走 `USART1`：

- `USART1`
- `19200 bps`
- `8N1`

典型输出：

```text
[FLASH64K_APP_TEST] quick storage test start
[FLASH64K_APP_TEST] flash_size_reg=64KB cycles=96
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

## 5. 结果解释

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

## 6. 重要风险

这是破坏性测试。它会重写：

- `0x0801C000`
- `0x0801C800`
- `0x0801E000`
- `0x0801E800`

测试开始时会尝试备份原 SOC/AFE 数据，结束时如果原数据有效，会尝试恢复。但如果原数据本身无效，或者测试中途失败，Flash 中可能会留下测试数据。

正式固件中必须保持：

```c
// #define FLASH64K_APP_QUICK_TEST_ENABLE
```

## 7. 建议测试流程

1. 打开 `FLASH64K_APP_QUICK_TEST_ENABLE`。
2. 编译并烧录测试固件。
3. 串口观察是否输出 `PASS`。
4. 断电重启 3 次，重复确认仍可 `PASS`。
5. 改大 `FLASH64K_APP_QUICK_TEST_CYCLES`，例如 `1000U`，做更长循环。
6. 多拿几块板重复测试，不要只测一块样机。
7. 测试完成后关闭 `FLASH64K_APP_QUICK_TEST_ENABLE`，重新编译正式固件。

## 8. 后续仍需补充的可靠性测试

这个快速测试通过后，仍建议追加：

- `0x08010000~0x0801FFFF` 全后 64K 裸擦写读回测试
- 高低温测试
- 低电压擦写测试
- 掉电恢复测试
- 至少数千次循环擦写测试

这样才能判断“后 64K”是否足够支撑你的应用长期保存参数。
