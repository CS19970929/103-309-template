# 主工程 AFE 集成流整理

## 1. 启动初始化链路

当前启动顺序：

```text
main()
  -> InitDevice()
      -> InitIO()
      -> InitE2PROM()
      -> InitCan()
      -> InitSci()
      -> InitMosRelay_DOx()
      -> InitData_SOC()
      -> bsp_InitSPIBus()
      -> sh36735_spi_sw_init()
      -> Init_IWDG()
      -> InitTimer()
  -> InitVar()
      -> InitSystemMonitorData_EEPROM()
      -> AFE3520_SyncSeriesNum(OtherElement.u16Sys_SeriesNum)
  -> InitAFE3520_Registers(0, 0)
```

关键点：AFE 初始化现在放在 `InitVar()` 之后，`SeriesNum` 会先从 EEPROM 参数恢复并经过 4..20 串范围规范化，再写入 AFE `SCONF4.CN`。

## 2. 采样链路

```text
App_AFEGet()
  -> UpdateVoltageFromBqMaximo()
      -> sh36735_read_regs(0x40..0x46)
      -> sh36735_read_regs(0x47..0x57)
      -> sh36735_read_regs(0x58..0x5C)
      -> sh36735_read_regs(0x5D..0x96)
      -> 转换 Cell/Temp/CADC/VTOP/VCHGR
  -> DataLoad_CellVolt()
  -> DataLoad_CellVoltMaxMinFind()
  -> DataLoad_Current()
  -> DataLoad_Temperature()
  -> DataLoad_TemperatureMaxMinFind()
  -> 保护恢复与清标志逻辑
```

当前 `UpdateVoltageFromBqMaximo()` 已经会返回错误 bitmask。任一 SPI 块读取失败时：

- `sys_time.crc_err = true`
- 置 `ERROR_SPI`
- 返回错误码
- `App_AFEGet()` 停止后续数据转换，避免半更新数据进入业务层

这是当前 AFE 链路里比较重要的可靠性改进。

## 3. 数据结构流向

```text
SH3673520 RAM 寄存器
  -> Registers_AFE1
      -> SH367309_Read_AFE1
          -> g_stCellInfoReport
              -> 保护、SOC、上位机、CAN、均衡
```

命名上仍保留大量 `SH367309` 和 `I2C_AFE1`，但当前实际数据来源是 SH3673520 SPI。后续整理时建议保留一层兼容输出结构，底层驱动和寄存器镜像改成 `sh36735xx` 命名。

## 4. 保护清除链路

保护恢复主要在 `DataDeal.c` 的 `App_AFEGet()` 后段触发：

- 单体过压恢复后清 `AFE_FLAG_OV`
- 单体欠压恢复后清 `AFE_FLAG_UV`
- 充电器移除后清 `AFE_FLAG_OCC`
- 负载移除后清 `AFE_FLAG_OCD1/OCD2`
- 温度恢复后清 `OTC/UTC/OTD/UTD`
- 短路释放流程会修改 `SCONF3.CRLD_EN`

实际清标志函数是 `SH_AFE_ClearProtectFlag()`：

```text
写 SCONF2.LTCLR=1
写 FLAG1 或 FLAG2 对应位为 0
```

当前剩余风险：

- 写 `SCONF2` 的返回值未检查。
- 没有清标志后的读回确认。
- `LTCLR` 会被 AFE 自动清，本地 `Registers_AFE1.sonf2` 可能与真实寄存器短暂不一致。

## 5. 均衡链路

均衡入口在 `App_CellBalance()`，最终调用 `CB_AfeWriteBalanceMaskU24()`：

```text
balance_mask
  -> BALANCEH = mask >> 16
  -> BALANCEM = mask >> 8
  -> BALANCEL = mask
  -> sh36735_write_reg_u8(0x55)
  -> sh36735_write_reg_u8(0x56)
  -> sh36735_write_reg_u8(0x57)
```

当前优点：

- 写寄存器有重试。
- 只有三字节全部写成功才更新软件均衡状态。

当前风险：

- 如果 H/M 写成功、L 写失败，最终又重试失败，AFE 内部可能留下部分新均衡位。
- 没有写后读回校验。
- `Registers_AFE1` 的均衡中/低字节镜像顺序目前不对，调试读回会误导。

## 6. 休眠与唤醒关联

休眠相关代码会写 `SCONF1=0xAA` 让 AFE 进入 SLEEP。PDF 说明 SLEEP 会关闭 CADC、WDT、电压电流保护和 MOS 输出，并保留部分唤醒检测能力。

需要确认：

- 进入休眠前是否关闭均衡。
- 唤醒后是否重新初始化 AFE `0x40..0x54`。
- 唤醒后 `FLAG1.WK_FLG` 是否被正确读取和清除。
- `SCONF3` 中充电器/负载唤醒配置是否与实际产品需求一致。

## 7. 建议模块边界

建议后续整理为：

| 模块 | 职责 |
| --- | --- |
| `sh36735xx_spi.*` | 只处理 SPI 帧、CRC、ACK、重试、错误统计 |
| `sh36735xx_reg.*` | 寄存器地址、位定义、读写封装、配置表 |
| `sh36735xx_afe.*` | 初始化、读回校验、采样、清标志、模式切换 |
| `afe_adapter.*` | 把新 AFE 数据转换到旧 `SH367309_Read_AFE1` 和业务结构 |
| `Cell_balance.c` | 只保留均衡策略，底层寄存器写交给 AFE 模块 |

这样可以保留上层业务不大动，同时把 SH3673520 的芯片细节从旧 I2C/SH367309 文件中拆出来。
