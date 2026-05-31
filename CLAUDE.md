# BMS Firmware Project

## 项目性质
嵌入式 BMS（电池管理系统）固件，MCU: STM32F103RCT6，AFE: SH367309，使用标准外设库。

## 核心目录结构
```
103 + 309/Project/Source/   ← 主 BMS 固件源码（重点）
├── main.c — 主入口
├── AppInit.c / Runtime.c   — 启动与主循环
├── SocEnhance.c / SOC.c    — 电量算法
├── Sci_Upper.c             — Modbus 协议
├── Can_HDX.c               — CAN 协议
├── Flash.c / EEPROM.c      — 参数存储
├── app_lowpower.c / rtc_sleep.c — 低功耗
├── LedBar.c                — LED 显示
├── FactoryAging.c          — 老化
├── conf/                   — 产品配置宏
└── DataDeal.c              — 数据聚合

firmware/comm_tool_f103ret6/source/   ← 独立通信工具/IAP 工程（非主 BMS）
tools/               — Python 工具脚本（烧录、测试、检查）
docs/                — 文档（按需读取，不自动加载）
```

## 固件关键模块
- **SOC** — 电量算法（OCV 表、积分、满电确认、低压表、SOH）
- **CAN/Modbus/UART** — 通信协议栈
- **Flash** — 参数存储和 SOC 快照
- **RTC/低功耗** — 休眠唤醒、备份域
- **AFE 驱动** — SH367309 寄存器读写、MTP 写入
- **老化** — 出厂老化计时和控制

## 使用约定
- 全部用中文交流
- 固件编译：Keil MDK（`FD_Release` / `FD_Debug`），`PROJECT_CFG_BUILD_PROFILE 0` 为量产
- 烧录：使用 `tools/soc_flash_app_safe.ps1`，禁止裸写 `0x08000000`
- 修改上位机代码后立即编译更新 dist 目录下的 exe
- 设计文档在 `docs/` 子目录中，按需读取
- 提问时请指定具体文件路径或函数名
