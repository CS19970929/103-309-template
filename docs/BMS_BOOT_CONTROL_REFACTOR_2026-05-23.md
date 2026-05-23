# BMS IAP 启动与升级可靠性重构

日期：2026-05-23

## 结论

App 主动进入 IAP 不再写 Flash 标志，也不再占用 `0x0801F800` 控制页。新的方式是：

- App 收到串口或 CAN 的“进入升级”请求后，在 SRAM mailbox 写入带反码和 CRC 的请求。
- App 复位，IAP 上电最早阶段读取并清除 mailbox。
- mailbox 有效时 IAP 留在升级模式；无 mailbox 且 App 向量表有效时跳 App。

mailbox 固定地址为 `0x20004FE0`，占用 `0x20004FE0-0x20004FFF` 32 字节。App Release scatter 已把 RAM 从 `0x5000` 收缩到 `0x4FE0`，避免正常变量或栈覆盖这段复位保持区。

## 为什么不再用 Flash 作为进入 IAP 请求

旧方式用 `FLASH_ADDR_UPDATE_FLAG` 的半字做启动门闩：

- `0xFFFF`：跳 App。
- `0x00AB`：留 IAP。

问题是 `0xFFFF` 本身就是擦除态，而且每次进入升级都要擦控制页，浪费 Flash 寿命和空间。进入 IAP 是“立即复位后消费”的瞬时请求，不需要跨掉电保存，用 SRAM mailbox 更合适。

## 防断电策略

不使用 Flash 标志后，升级中断保护改由写 App 镜像顺序保证：

1. 串口或 CAN 升级开始时，IAP 先擦 App 第 1 页，使旧 App 向量表失效。
2. IAP 接收镜像时，把 App 第 1 页暂存在 RAM，不立即写入 Flash。
3. 其他页正常按地址写入。
4. 完成命令到达并完成长度/CRC/向量检查后，最后写 App 第 1 页。
5. 第 1 页写入顺序为：先写 8 字节之后的数据，再写 Reset Handler，最后写 MSP。

这样任意时刻断电：

- 升级未完成：App 向量表无效，下次上电留在 IAP。
- 第 1 页已经写完：镜像已经接收完成并通过检查，下次上电可以跳 App。

## 串口和 CAN 独立升级

- 串口升级协议保持原样，仍兼容旧上位机命令。
- 串口升级不依赖 CAN：串口连接/写块/完成即可单独完成升级。
- CAN 升级不依赖串口：CAN `START/DATA/COMMIT/END` 即可单独完成升级。
- 两个入口共享底层 `IapFlash` 写入策略，避免两套升级流程出现不同的断电行为。

## 地址规则

- IAP/Bootloader 地址：`0x08000000`。
- App 地址：`0x08004800`。
- App 安全烧录仍使用：
  `.\tools\soc_flash_app_safe.ps1 -Bin "103 + 309\Project\Users\Objects\FD_Release.bin" -Flash`
- 禁止把 App bin 裸写到 `0x08000000`。

