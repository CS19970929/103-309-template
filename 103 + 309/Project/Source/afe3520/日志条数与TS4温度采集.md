# 日志条数与TS4温度采集

2026-09-07。默认日志容量500条；MOS温度改用参考分支的AFE第四路TS4。未烧录。

## 日志配置

只需修改 `Flash.h`：

```c
#define FLASH_STORAGE_LOG_RECORD_COUNT 500U /* 可改成100U */
```

支持100或500，其他值编译报错。该宏同时控制RAM环形记录数、读取窗口长度和缓存容量；默认500。日志Flash仍为原来的四页，格式和地址不变，不改IAP、App、SOC或配置分区。切换容量后只展示配置数量的最新记录；100条构建不会返回第101条。

上位机客户版和完整版均在“事件日志”页选择100/500条。默认100兼容旧固件；新500条固件也能读前100条。500条按每帧100字读取 `0xC008/0xC06C/0xC0D0/0xC134/0xC198`。顺序仍为最新在前，事件/间隔字节含义不变。短响应或任何一页失败均报错，不把部分结果当完整500条，不自动降级；读完全部页面才替换界面数据。多帧期间设备仍可能新增记录，原协议没有冻结快照机制。

## MOS温度与ADC删除

参考 `codex/afe-spi-refactor-debug` 的 `DataLoad_Temperature`，MOS来源为 `u16TempBat[3]`，即AFE TS4。当前使用 `g_afe3520Measurements.u16TempBat[AFE3520_MOS_TEMP_INDEX]`，通道常量在 `Afe3520Config.h` 中明确为3。

TS4继续经过原来的 `MDL_TEMP_MOS1` K/B校准、温度断线检查，并写入原 `MOS_TEMP1` 上报槽。编码仍为 `(℃+40)×10`，保持参考分支整数温度校准方式。TS1/TS2电池温度和电流采集不变。

删除应用 `ADC.c/ADC.h`、ADC1/DMA1_Channel1/TIM2采样配置、启动/循环/唤醒恢复调用、未使用的Type-C ADC回退逻辑；Keil两个目标及SPL配置头移除ADC/DMA模块引用。官方库原始驱动文件和芯片向量定义保留为库资料，不参与工程编译。

旧ADC错误字节保留原协议位置为零值保留槽，取消错误处理入口，避免移动其他错误字段。参考板PB3 `AD_EN` 等公共电源控制脚仍保持原时序；这些电源IO不再启动或调度MCU ADC。

## 验证与产物

ARMCC 5.06u7完整重建，0错误、3条既有未使用代码警告：

| 配置 | BIN字节 | App剩余 |
| --- | ---: | ---: |
| Release，500条，用户工作区O2 | 37252 | 1660 |
| Debug，500条，O3 | 37140 | 1772 |
| Release，100条，临时独立工程O2 | 37212 | 1700 |

100条独立构建仅覆盖日志宏，不改工作区默认500。App仍从 `0x08004800` 开始、最大38912字节，默认产物为 `Project/Users/Objects/FD_Release.bin` 和 `Project/Users/Objects_Debug/FD_Debug.bin`。

`run_flash_fit_host_test.py`验证真实TS4上报函数的-40..100℃编码、K/B校准和断线检查，以及两种日志容量的100字分页、最新在前、环形回绕和末尾越界拒绝；现有参数/CRC/串口/Flash差分通过。`run_afe3520_host_test.py`的24组合576项和休眠、分流器迁移、串数映射通过。Windows `test-event-log.ps1`分页及失败路径测试通过，两版EXE已重新发布：

- 客户版目录：`BmsTool.Windows/publish/customer-win-x64-log100-500-ts4-20260907/`
- 完整版目录：`BmsFactoryTest.Windows/publish/internal-full-win-x64-log100-500-ts4-20260907/`

目录相对于用户指定的 `bms-tool-windows` 项目。尚未实板测温、读取500条日志或测量休眠电流；本次没有烧录操作。
