# SCI 协议体积优化说明 2026-05-22

## 目标

结合 `103 + 309/Project/Users/Listings/FD_Release.map`，优先处理当前 Release 镜像中最大的协议模块 `sci_upper.o`，在不改变寄存器协议和量产配置的前提下减少 Flash 占用，并降低后续阅读和维护成本。

## map 基线

优化前基线来自 `logs/FD_Release_before_sci_pack_refactor.map`：

```text
Program Size: Code=53676 RO-data=2744 RW-data=808 ZI-data=5256
Total RO  Size: 56420
Total RW  Size: 6064
Total ROM Size: 56664
sci_upper.o Code: 7610
Sci_ACK_0x03_ReadRegs_Data: 1052
Sci_Deal_WrRegs_0x10: 654
```

优化后重新构建 `FD_Release`：

```text
Program Size: Code=52796 RO-data=2744 RW-data=808 ZI-data=5256
Total RO  Size: 55540
Total RW  Size: 6064
Total ROM Size: 55784
sci_upper.o Code: 6738
Sci_ACK_0x03_ReadRegs_Data: 546
Sci_Deal_WrRegs_0x10: 212
```

收益：

- Code 减少 880 字节。
- ROM 减少 880 字节。
- RAM 不变，仍为 6064 字节。
- `sci_upper.o` 代码量减少 872 字节。
- `Sci_ACK_0x03_ReadRegs_Data` 从 1052 字节降到 546 字节。
- `Sci_Deal_WrRegs_0x10` 从 654 字节降到 212 字节。

## 修改内容

1. 新增 `Sci_PutWordBE()`，统一 Modbus 大端 word 写入，替代多个函数里重复的：

```c
t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
```

2. 新增 `Sci_PutZeroWordsBE()`，用于保留协议预留字时批量写入 0，避免展开多段重复代码。

3. 新增 `Sci_RecordBackIndex()` 和 `Sci_PutLatestFaultWords()`，将故障环形记录的倒序索引和打包逻辑集中到一个入口。输出顺序仍保持原来的最近 4 条记录组合方式。

4. 将 `Sci_Deal_WrRegs_0x10()` 中的校准寄存器大段 `case` 列表改为 `Sci_IsCalibPairStart()` 判断：

```text
0x2000 起始、偶数偏移、且不超过 RS485_CMD_ADDR_TEMP_MOS_CALIB_K
```

这与原来只允许 `*_CALIB_K` 起始地址写入 K/B 两个 word 的规则一致。

5. 删除 `Sci_Deal_WrRegs_0x10()` 中已经被前置范围判断覆盖的重复分支。保护参数区和 OtherElement 区仍先走范围判断，协议入口不变。

6. 当前源码中保护参数写入路径已被 `#if 0` 隔离，配套的 `Sci_ApplyProtectSideEffects()` 也保持在同一禁用区间内，避免 Release 构建出现未引用 warning。

## 协议兼容性

- `0x03` 读寄存器响应仍按原有顺序填充 `g_u8SCITxBuff`。
- `0xD000` 基础状态、`0xD100` RTC/故障记录、`0xD200` Cortex fault snapshot、`0xD300` SOC 测试状态窗口位置保持不变。
- `0x10` 写多个寄存器仍保留 SOC 测试、AFE 参数、保护参数、OtherElement、SOC 表、铜损、RTC、SN/版本、IAP 跳转等原有入口。
- 量产配置保持 `PROJECT_CFG_BUILD_PROFILE 0`、`PROJECT_CFG_SOC_TEST_MODE_ENABLE 0`。
- App 链接基址仍为 `0x08004800`，未触碰 IAP/Bootloader 地址规则。

## 验证

执行 Keil Release 构建：

```powershell
& "C:\Keil_v5\UV4\UV4.exe" -b ".\CommomSH367309_16series_103RCT6_C.uvprojx" -t "FD_Release" -o ".\codex_FD_Release_build.log"
```

结果：

```text
".\Objects\FD_Release.axf" - 0 Error(s), 0 Warning(s).
Program Size: Code=52796 RO-data=2744 RW-data=808 ZI-data=5256
```

执行仓库静态检查：

```powershell
py -3.9 tools\project_check.py
```

结果：

```text
OK: 120
Warnings: 0
Errors: 0
```

关键安全检查：

```text
Load Region LR_IROM1 Base: 0x08004800
Execution Region ER_IROM1 Exec base: 0x08004800
Total ROM Size: 55784
Total RW Size: 6064
```
