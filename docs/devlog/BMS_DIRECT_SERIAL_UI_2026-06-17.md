# BMS 直连串口上位机模式

日期：2026-06-17

## 背景

原 `BMS_CommTool_Upgrade_UI.exe` 主要面向 `comm tool/CAN桥`：PC 串口连接 comm tool，comm tool 再通过 CAN 访问 BMS App。现场有时只需要直接接 BMS App 串口，因此在同一个上位机内增加 `BMS直连串口` 通信方式，避免维护第二套界面。

## 实现范围

- UI 连接区新增 `通信方式`：
  - `comm tool/CAN桥`：保留原有 CAN 桥接、缓存下载、CAN-IAP 升级能力。
  - `BMS直连串口`：通过 BMS App Modbus RTU 直接访问板端寄存器。
- 直连串口默认参数：
  - 波特率：`19200`
  - Modbus 地址：`1`
  - 功能码：`0x03` 读寄存器、`0x06` 单寄存器写、`0x10` 多寄存器写。
- 直连模式已接入：
  - 实时监控、实时数据、长期 CSV 记录。
  - BMS 日志窗口 `0xC008`。
  - 参数读取、参数写入、SH309 默认值/保护点相关操作。
  - 高级寄存器读写。
  - 写一次 SOC：`0x1005`。
  - 进入 IAP：`0xFFFD` 使用 `0x10` 写 1 个寄存器。
  - BMS App 串口 IAP 一键升级。
  - 老化开始/停止：`0x1102=7`、`0x1103=7`。

## 保留在 comm tool/CAN 桥的能力

以下能力依赖 comm tool 固件或 CAN App 命令，BMS App 直连 Modbus 没有等价接口：

- comm tool 固件信息、缓存信息、缓存写入。
- 使用 comm tool 缓存升级。直连模式的“一键升级”改为通过 BMS 串口 IAP 直接写入。
- CAN 诊断。
- 读取老化剩余时间：来源为 `0x14F80208` CAN 广播。
- 重置老化时间、修改老化总时长。

直连模式下点击上述功能会明确提示需要切换到 `comm tool/CAN桥`。

## 直连串口 IAP 流程（2026-06-22）

1. 上位机校验 bin 向量表，App 基地址固定为 `0x08004800`，镜像结束地址不得超过 `0x0801F800`。
2. 按当前 BMS Modbus 地址写 `0xFFFD`，请求 App 通过 SRAM mailbox 复位进入 IAP。
3. 等待复位后切换到 IAP 固定 Modbus 地址 `1`，再写 `0xFFFD` 初始化升级会话。
4. 通过 `0xFFFE` 按 1024 字节分块写入；非最后块必须是 1024 字节，`byte_count=0`，长度字段表示原始字节数。
5. 每块收到正 ACK 后才发送下一块。块 ACK 超时时不自动重发，因协议没有块序号，重发可能使数据写入错位；必须重新从 `0xFFFD` 和第 0 块开始。
6. 写 `0xFFFF` 完成升级。IAP 校验 MSP/ResetHandler，最后写入首页向量并在 ACK 发送完成后复位。
7. 上位机恢复原 App Modbus 地址，等待 `0xD034` 状态寄存器恢复响应。

直连串口 IAP 不使用 comm tool 缓存。传输完整性依赖每帧 Modbus CRC、Flash 半字回读和最终向量校验；当前串口 IAP 协议没有对外提供整包 CRC 比对。

## 验证

- `py -3 tools\comm_tool_upgrade_ui.py --self-test`
- `py -3 -m py_compile tools\comm_tool_upgrade_ui.py`
- `powershell -ExecutionPolicy Bypass -File tools\build_comm_tool_upgrade_ui_exe.ps1`

编译产物覆盖：`dist\BMS_CommTool_Upgrade_UI.exe`。
