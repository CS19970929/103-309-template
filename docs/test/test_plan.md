# 103-309 BMS 测试计划

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`Runtime.c`, `Sci_Upper.c`, `Can_HDX.c`, `Flash.c`, `SOC.c`, `SocEnhance.c`, `ADC.c`, `SH367309_*`, `rtc_sleep.c`, `LedBar.c`
最后更新时间：2026-05-27
未确认事项：硬件平台、COM 口、CAN 适配器、ST-Link、实际 Flash 容量和客户协议版本。

## 1. 编译测试

- Keil `FD_Release` 编译。
- 确认量产宏：`PROJECT_CFG_BUILD_PROFILE 0`, SOC test disabled。
- 检查 map/bin 起始地址，App 必须从 `0x08004800`。
- 检查 Flash 存储区不能越过真实容量。

## 2. 上位机 Modbus 协议回归

- `COM4/19200/slave=1` 读 `0xD000`。
- 读 `0xC002` 48 个寄存器，确认 SN/HW/SW。
- 读 `0xD300`，量产应 `supported=0`。
- 工装条件下测试保护/Other 参数写入和读回。
- 非法地址、只读地址、范围错误应返回异常码。

## 3. CAN 回归

- 抓取 `0x14F80200+index` 周期广播。
- 抓取 `0x14F80208` 老化状态和剩余分钟。
- 测试 CAN App `GET_STATUS`, `READ_REG`, `READ_BLOCK`, `WRITE_PREP/COMMIT`, `ENTER_IAP`, 老化控制。
- 模拟 bus-off 或 no-ack，确认恢复和队列清理。
- 不接 CAN 设备时，确认 `GPIO_CMNT_EN` 只在 10 s 探测窗口短时上电，且不持续发送完整 1 s/5 s 业务广播。
- 接入 CAN 设备后，确认任一 TX ACK 或 RX 报文会恢复 active 状态，并恢复完整 1 s/5 s 周期广播。
- 运行中断开 CAN 设备后，确认连续无 ACK/发送超时达到阈值后切换回 idle probe。
- RTC HICCUP STOP 下，active 总线确认 1 s 唤醒广播，idle 总线确认 10 s 轻量探测。

## 4. 参数存储测试

- RW 参数默认加载和保存。
- AFE 参数写入、verify、重启保持。
- SOC snapshot 断电恢复。
- 事件记录读写和重复限频。
- 老化状态 BKP/Flash 恢复。
- 写入中断电恢复。

## 5. SOC 测试

- 主机回放：充电、放电、满电、低压、静置、骑行。
- 上板真实电流积分。
- 满电确认前不提前发布 100。
- 低压尾段不虚高。
- RTC 休眠补偿不大跳。
- 量产 `0xD300` 测试入口关闭。

## 6. ADC / AFE 测试

- ADC PA1/PA2/PB1 三路采样。
- Type-C 输出电流折算。
- AFE I2C 46 bytes 读取和 CRC。
- AFE CADC 方向、零点、死区。
- AFE 保护参数写入和恢复。

## 7. 保护逻辑 / MOS 控制测试

- OVP/UVP/OCP/OTP/UTP fault。
- CBC/AFE status。
- 启动 MOS 状态。
- 充电器插拔。
- 深睡和唤醒 MOS 状态。

## 8. RTC 低功耗 / IWDG 测试

- 空闲进入 HICCUP STOP。
- 通信/Flash/fault/LED 阻塞 sleep。
- 启动/唤醒 SOC 显示窗口结束后，`LP_BLOCK_LED_ACTIVE` 必须释放，不能被 `startup_display_armed` 永久阻塞。
- Release 构建必须清除 `DBGMCU_CR_DBG_STOP`；功耗实测时不能用 Debug STOP 保持位。
- ST-Link 调试 STOP 只能作为逻辑定位手段，带 DBG_STOP 的功耗数据无效。
- 使用 `tools/stlink_bms_monitor.ps1 -Mode ReleaseProxy` 做真实低功耗长期监控。
- 使用 `tools/stlink_bms_monitor.ps1 -Mode DebugProbe` 做 RTC STOP 内部状态诊断，测试结果必须标注“非功耗实测”。
- RTC 周期唤醒恢复。
- IWDG 长稳。
- 过放 deep sleep 和接充恢复。
- STOP 电流实测。

## 9. LED 显示测试

- 0/1/9/10/99/100 显示。
- 上电显示窗口。
- 睡眠 SOC 预览。
- 长按进入 deep sleep。
- STOP 前 GPIO 泄漏。

## 10. IAP 测试

- `tools/soc_flash_app_safe.ps1` dry-run。
- 地址错误拒绝。
- CAN/Modbus 进入 IAP。
- IAP 后 App 不覆盖 bootloader。

## 11. 硬件实测清单

- MCU 丝印和 Flash size register。
- CHG_IN、MCU_WK、SW、RS485 wake、UART1 wake 电平。
- SH367309 CADC/保护/MOS。
- VBC 分压和 Type-C 电流。
- RTC LSE/LSI。
- CAN 250k 总线。
