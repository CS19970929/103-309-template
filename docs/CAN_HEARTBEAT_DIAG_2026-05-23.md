# CAN 心跳诊断说明

## 目的

用于确认 comm tool、BMS IAP 和 CAN 通讯盒是否处在同一物理总线和同一位时序上。心跳帧只用于诊断，不参与升级协议状态机。

## 当前 CAN 配置

- 波特率：250000 bit/s。
- comm tool：CAN1 PA11/PA12，标准库，Normal mode，滤波全接收。
- comm tool 时钟：SYSCLK/PCLK1 为 8MHz，CAN 时序为 `SJW=1tq, BS1=5tq, BS2=2tq, Prescaler=4`，实际 250k。
- BMS App：CAN1 PA11/PA12，标准库，Normal mode，当前 App 工程同样按 8MHz PCLK1 和 `Prescaler=4` 工作。
- BMS IAP：CAN1 PA11/PA12，标准库，Normal mode，IAP 工程为 72MHz SYSCLK、36MHz PCLK1，CAN 时序应为 `SJW=1tq, BS1=5tq, BS2=2tq, Prescaler=18`，实际 250k。

## 心跳帧

### comm tool 心跳

- ID：标准帧 `0x05E`
- 周期：1000ms
- 数据：
  - Byte0：`0x43`，字符 `C`
  - Byte1：`0x54`，字符 `T`
  - Byte2：串口/CAN 协议版本
  - Byte3：目标 BMS node id
  - Byte4：comm tool 升级状态
  - Byte5：comm tool 最近升级错误码
  - Byte6：心跳递增序号
  - Byte7：comm tool 固件 patch 版本

示例：`43 54 01 01 00 00 2A 04` 表示 comm tool 0.1.4、node=1、空闲、无错误。

### BMS IAP 心跳

- ID：标准帧 `0x05F`
- 周期：1000ms
- 数据：
  - Byte0：`0x49`，字符 `I`
  - Byte1：`0x41`，字符 `A`
  - Byte2：CAN-IAP 协议版本
  - Byte3：IAP node id
  - Byte4：IAP 状态，`0=IDLE, 1=RECEIVING, 2=DONE, 3=ERROR`
  - Byte5：最近收到的 IAP 命令
  - Byte6：最近错误码，`0=OK`
  - Byte7：心跳递增序号

示例：`49 41 01 01 00 00 00 10` 表示 BMS IAP 已运行、node=1、空闲、无错误。

## 现场判断

1. 只上电 comm tool，通讯盒应看到 `0x05E` 每秒一帧。
2. 让 BMS 停在 IAP，通讯盒应看到 `0x05F` 每秒一帧。
3. 两个心跳都可见后再执行升级；如果只看到 `0x05E`，说明 BMS IAP 未运行、未接入总线、收发器未使能或 BMS IAP 位时序不对。
4. 如果通讯盒是 listen-only 模式，心跳可能因为无 ACK 发送失败。建议调试时把通讯盒设为 normal/active 模式，或保证总线上另有节点 ACK。

## 升级相关帧

- comm tool 请求 BMS App 进入 IAP：标准帧 `0x060`。
- BMS App 应答：标准帧 `0x061`。
- comm tool 请求 BMS IAP：扩展帧 `0x14F8F001`。
- BMS IAP 应答：扩展帧 `0x14F8F101`。

