# Bootloader / IAP 设计

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`103 + 309/Project/Source/Flash.c`, `103 + 309/Project/Source/Flash.h`, `103 + 309/Project/Source/Sci_Upper.c`, `103 + 309/Project/Source/Can_HDX.c`, `tools/soc_flash_app_safe.ps1`, `tools/comm_tool_host.py`, `tools/can_bms_host.py`
最后更新时间：2026-05-26
未确认事项：最终 Keil scatter/map 是否稳定固定到 App `0x08004800` 仍需构建产物确认；独立 IAP 工程源码不在当前 BMS App 主链路内完整审计。

## 1. 地址约束

当前仓库规则和工具链约束：

| 项 | 当前值 | 证据 |
|---|---:|---|
| IAP / Bootloader 起始地址 | `0x08000000` | `AGENTS.md`, `tools/soc_flash_app_safe.ps1`, `tools/comm_tool_host.py` |
| BMS App 起始地址 | `0x08004800` | `tools/soc_flash_app_safe.ps1`, `tools/comm_tool_host.py`, `tools/can_bms_host.py` |
| App 误写保护 | 拒绝 `0x08000000` | `soc_flash_app_safe.ps1`, `comm_tool_host.py` |

注意：本轮没有修改 Keil 工程。`docs/review/module_map.md` 已记录当前仓库未找到源控下的 `FD_Release.sct`，最终 App 链接地址仍必须以 Release map/bin 复核。

## 2. App 到 IAP 请求路径

`Flash.c` 当前定义 SRAM mailbox：

- 地址：`0x20004FE0`
- magic：`0x49415031`
- request：`0x5AA55AA5`
- CRC：`magic ^ request ^ 0xA5A55A5A`

`AppUpgrade_RequestIap()` 写入 mailbox 并立即回读校验。`AppUpgrade_IsIapRequested()` 校验 magic、反码、request、反码和 CRC。

## 3. 进入 IAP 的触发入口

当前至少有三类入口：

| 入口 | 当前行为 | 风险 |
|---|---|---|
| Modbus `0xFFFD` 多寄存器写 | `Sci_WrRegs_0x10_FlashConnect()` 调用 `AppUpgrade_RequestIap()`，发送完成后置 `u8FlashUpdateFlag` | 上位机协议兼容和误触发风险 |
| CAN App `ENTER_IAP` | 校验 `0xC3 0x3C` 和节点后写 mailbox，并延迟置 `u8FlashUpdateFlag` | CAN 命令必须保留 guard 和 CRC |
| 内部 `APP_To_IAP_Jump()` / `InitAreaSelect()` | 如果 mailbox 有效则复位 | 与 IAP 交接行为相关 |

`App_FlashUpdate()` 在 `_IAP` 条件下检测 `u8FlashUpdateFlag`，先关闭 CHG/DSG MOS，延迟 10 ms，然后 `MCU_RESET()`。

## 4. 上位机工具约束

当前工具侧也有防误烧逻辑：

- `tools/soc_flash_app_safe.ps1` 默认地址 `0x08004800`，非该地址直接拒绝。
- `tools/comm_tool_host.py` 拒绝把 App 地址设为 `0x08000000`，并检查向量表 MSP/Reset。
- `tools/can_bms_host.py` 真实升级发送要求显式确认 `0x08004800`。
- `tools/comm_tool_upgrade_ui.py` 使用原 `BMS_CommTool_Upgrade_UI.exe` 规则，升级前会做 bin 向量表和大小检查。

## 5. 后续验证要求

1. 每次构建后检查 map：`LR_IROM1` 和 `ER_IROM1` 必须从 `0x08004800` 开始。
2. 安全烧录先 dry-run，再带 `-Flash` 执行。
3. IAP 相关变更必须回归串口 `0xFFFD` 和 CAN `ENTER_IAP` 两条入口。
4. 升级过程中不能进入 STOP 低功耗。
5. 禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`。
