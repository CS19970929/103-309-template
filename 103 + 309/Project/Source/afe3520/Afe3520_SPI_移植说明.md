# SH3673520 SPI 移植说明

## 当前默认配置

`AFE3520_USE_SOFTWARE_SPI` 在 `Afe3520Board.h` 中默认为 `1U`，使用参考分支已验证的 GPIO 模拟 SPI：

- `PA4`：CS，推挽输出；
- `PA5`：SCK，推挽输出；
- `PA6`：MISO，浮空输入；
- `PA7`：MOSI，推挽输出；
- SPI Mode 3，MSB first；
- 每个半周期约 1us。

将宏改为 `0U` 时，切换到 SPI1 硬件收发；硬件路径使用 STM32F1 标准库的 `SPI_I2S_SendData/ReceiveData` 访问 `DR`。

## IO 初始条件

正常启动按照参考板配置打开 `M_STB(PA15)`、`AD_EN(PB3)` 和 `CMNT_EN(PB4)`，保持 `CTLC(PB14)` 低电平，并初始化 CS、检测输入、唤醒输入和调试 LED。`BLE_EN`、`SW_EN` 仅为历史兼容别名，不再作为独立输出驱动，避免覆盖 `CMNT_EN(PB4)` 或按键输入 `PB5`。

`SHIP(PA10)` 不在正常启动时强制驱动，保持参考分支的外部默认状态；只有明确执行 `AFE_SHIP()` 时才配置并拉低该引脚。

## 本次验证

- Keil `FD_Release`：`0 Error(s), 0 Warning(s)`；
- 软件 SPI 固件已通过 ST-Link 下载，`Verify OK`；
- `COM17@19200` 的 Modbus 通信正常；
- 当前板端 AFE 仍未返回有效帧，诊断为 `AFE3520_ERR_SPI`，MISO 读值持续为低。该结果说明还需要对板端 AFE 供电、SHIP 默认电平和 PA4~PA7 波形进行示波器或逻辑分析仪确认，不能将当前结果判定为协议层已打通。
