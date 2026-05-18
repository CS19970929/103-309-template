# AFE 初始化读回校验修复

日期：2026-05-18

## 背景

Review `60f90d6` 时发现，AFE 初始化函数名为 `AFE3520_WriteRegChecked()`，但旧提交只依赖 SPI ACK，不能确认 AFE RAM 寄存器最终值是否写入成功。当前主分支已经把初始化写入改为“写寄存器后立即读回比较”，本次继续修正读回比较方式，避免特殊位误判。

## 本次处理

- `AFE3520_WriteRegChecked()` 作为默认全字节读回校验入口。
- 新增 `AFE3520_WriteRegMaskedChecked()`，用于存在自清或不稳定读回位的寄存器。
- `AFE3520_ReadbackRegChecked()` 改为按 `mask` 比较：`(readback & mask) == (expect & mask)`。
- `SCONF2` 使用 `0x7F` mask，屏蔽 bit7 `LTCLR`。

## 原因

`SCONF2.LTCLR` 是清保护标志允许位，官方例程在寄存器巡检时也会屏蔽该位。该位写 1 后可能被 AFE 自清或表现为非稳定读回值。如果全字节比较，会把一次成功初始化误判为 SPI/AFE 配置失败。

## 当前初始化校验范围

以下寄存器初始化写入后会读回确认：

- `SCONF1`
- `SCONF2`，屏蔽 `LTCLR`
- `SCONF3`
- `SCONF4`
- `SCONF6`
- `OVT_OVH`
- `OVL`
- `UVT_UVH`
- `UVL`
- `OCD2V_OCD2T`
- `OCCV_OCCT`
- `OTC/UTC/OTD/UTD`

如果写入失败或读回不一致，`InitAFE3520_Registers()` 会置位 `ERROR_SPI`；全部成功则清除 `ERROR_SPI`。
