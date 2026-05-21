# BMS Next 模块边界

日期：2026-05-21

## 1. 重构原则

BMS Next 目标是简单、稳定、代码体积小。新工程不以照搬旧代码为目标，但必须保留产品需要的功能。

保留规则：

1. 串口通信协议语义保持不变。
2. 保护参数地址和含义保持可追溯。
3. AFE、MOS、保护、SOC、参数持久化必须保留。
4. 数码管、出厂老化、低功耗逻辑必须保留，可以简化代码和优化框架，不能去掉功能。
5. IAP 完全重写，支持 CAN 升级。
6. 非必要调试功能默认删除或隔离到 Debug/Test Target。

## 2. 必须保留模块

| 模块 | 说明 |
|---|---|
| `bsp_clock` | 系统时钟、SysTick 或基础 tick |
| `bsp_gpio` | MOS、AFE 电源、CAN、串口、关键输入 |
| `bsp_uart` | 原串口协议 |
| `bsp_can` | BMS App 网关和 Bootloader 入口 |
| `afe_sh367309` | SH367309 初始化、采样、保护状态读取 |
| `bms_measure` | 电压、电流、温度汇总 |
| `bms_protect` | 必要保护判断和 MOS 控制 |
| `bms_soc` | 基础 SOC、SOH、容量 |
| `param_store` | 保护参数、校准参数、运行参数持久化 |
| `serial_regs` | 原 `0x03/0x06/0x10` 寄存器语义 |
| `can_gateway` | CAN 转寄存器读写 |
| `boot_request` | App 请求进入 Bootloader |
| `ledbar_display` | 数码管和 LED 显示，统一刷新入口和显示状态机 |
| `factory_aging` | 出厂老化计时、状态保持、老化模式控制 |
| `low_power` | 休眠判定、RTC/STOP、唤醒恢复、外设恢复 |

## 3. 精简或隔离模块

| 模块 | 处理 |
|---|---|
| 复杂事件日志 | 精简，只保留必要故障快照和升级诊断 |
| 出厂老化 | 保留功能，重构为独立状态机，避免散落在主循环和低功耗逻辑里 |
| 数码管/LED | 保留功能，重构显示状态机和扫描刷新入口 |
| 低功耗 | 保留功能，重构休眠准入、RTC 唤醒、外设恢复和 CAN 窗口 |
| 历史 IAP 兼容 | 删除 |
| 调试测试模式 | 默认不进量产 App |
| EasyLogger | 删除或只在 Debug 独立 Target 使用 |

## 4. 主循环建议

```text
init_clock()
init_gpio()
init_uart()
init_can()
init_param()
init_afe()
init_measure()
init_soc()
init_watchdog()

while (1) {
    scheduler_tick_latch()
    uart_service()
    can_service()
    afe_task_200ms()
    protect_task_200ms()
    soc_task_1s()
    ledbar_task_10ms()
    factory_aging_task_1s()
    low_power_task_1s()
    param_store_task()
    watchdog_feed()
}
```

第一版不引入 RTOS。所有任务必须可被主循环轮询，不允许长时间阻塞。数码管扫描可以由定时中断提供节拍，但显示决策必须收敛到 `ledbar_display` 模块。

## 5. 参数策略

参数分为四类：

| 类型 | 默认策略 |
|---|---|
| 工厂校准 | 默认保留，不随升级覆盖 |
| 保护阈值 | 可通过上位机写入，可按版本迁移 |
| 运行快照 | 默认保留 |
| 调试参数 | 不进 Release |

## 6. 串口协议保持不变

旧串口协议仍是外部兼容边界。BMS Next 可以重写内部数据结构，但 `0x03/0x06/0x10` 的地址含义必须可对照旧文档。

新增 CAN 网关时，不重新发明保护参数地址。CAN 只是承载层。

## 7. Bootloader 与 App 边界

Bootloader 只负责：

1. 上电检查 App。
2. CAN-IAP 升级。
3. 写 App 区。
4. 校验并跳转。

App 只负责：

1. 正常 BMS 业务。
2. 响应 Comm Tool 读写。
3. 收到合法命令后请求进入 Bootloader。

Bootloader 不读取保护参数，不控制业务 MOS 策略。App 进入 Bootloader 前负责让功率路径进入安全状态。
