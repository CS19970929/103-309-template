# 升级可靠性测试计划

日期：2026-05-21

## 1. 测试目标

验证 Comm Tool 与 BMS CAN-IAP 在异常情况下不会导致 BMS 永久死机。

## 2. Comm Tool 测试

| 编号 | 项目 | 预期 |
|---|---|---|
| CT-001 | 上电读取版本 | PC 能读取协议版本和 Flash 分区 |
| CT-002 | 下载空固件 | 拒绝 |
| CT-003 | 下载超大固件 | 拒绝且不擦写程序区 |
| CT-004 | 下载正常固件 | 写入缓存区并 CRC 通过 |
| CT-005 | 下载中断电 | 重启后缓存固件无效 |
| CT-006 | 重复下载 | 新固件覆盖旧缓存，索引正确 |
| CT-007 | 查询固件信息 | 返回 size、crc、valid |
| CT-008 | 无有效固件触发升级 | 拒绝 |

## 3. BMS Bootloader 测试

| 编号 | 项目 | 预期 |
|---|---|---|
| BL-001 | 空 App 上电 | 停留 Bootloader |
| BL-002 | App MSP 非 SRAM | 停留 Bootloader |
| BL-003 | App ResetHandler 越界 | 停留 Bootloader |
| BL-004 | 坏 CRC | 停留 Bootloader |
| BL-005 | START 后断电 | 重启后停留 Bootloader |
| BL-006 | 擦写中断电 | 重启后停留 Bootloader |
| BL-007 | 数据 seq 错误 | NACK 并返回期望 seq |
| BL-008 | 完整升级 | 写 valid 并跳 App |
| BL-009 | RUN 前断电 | 重启后校验 valid，合法则跳 App |

## 4. Comm Tool 到 BMS 联调

| 编号 | 项目 | 预期 |
|---|---|---|
| LINK-001 | BMS App 正常读寄存器 | PC 收到 BMS 数据 |
| LINK-002 | BMS App 写保护参数 | 参数写入并可回读 |
| LINK-003 | App 进入 Bootloader | Comm Tool 能 HELLO 成功 |
| LINK-004 | 一键升级 | Comm Tool 完成下载、擦除、发送、校验、运行 |
| LINK-005 | CAN 断开 | Comm Tool 超时并上报 |
| LINK-006 | 升级中断电 | BMS 不跳坏 App，可重新升级 |

## 5. BMS 必保功能回归

| 编号 | 项目 | 预期 |
|---|---|---|
| BMS-LED-001 | 上电数码管显示 | 显示状态与旧产品规则一致或有明确新规则 |
| BMS-LED-002 | 休眠唤醒显示 | 按键/唤醒后的 SOC 显示窗口正常 |
| BMS-LED-003 | 充电显示 | 充电图标和 SOC 刷新无误亮、无闪烁 |
| BMS-AGING-001 | 老化模式进入 | 触发条件、MOS 状态和计时起点正确 |
| BMS-AGING-002 | 老化计时保持 | 断电/复位后的计时策略符合文档 |
| BMS-AGING-003 | 老化结束 | 状态退出、参数保持、通信读取正确 |
| BMS-LP-001 | 普通休眠 | 休眠准入条件正确，功耗符合目标 |
| BMS-LP-002 | RTC/STOP 唤醒 | 唤醒后 AFE、ADC、CAN、串口恢复正常 |
| BMS-LP-003 | 充电/按键唤醒 | 唤醒源判断正确，不误入死循环 |
| BMS-LP-004 | 低功耗与 CAN | CAN 窗口不长期阻塞休眠，也不丢关键升级命令 |

## 6. 发布前门禁

1. Comm Tool Release 构建通过。
2. BMS Bootloader Release 构建通过。
3. BMS App Release 构建通过。
4. 地址检查脚本确认 App 不覆盖 Bootloader。
5. PC 工具 dry-run 能显示固件长度、CRC、目标 App 地址。
6. 手工断电测试至少覆盖 START 后、擦除后、数据中段、VERIFY 前。
7. 数码管、老化、低功耗回归测试通过。
