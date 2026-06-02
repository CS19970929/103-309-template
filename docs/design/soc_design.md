# SOC 设计说明

文档状态：CURRENT
源码验证：PARTIAL
主要参考源码：`SOC.c`, `SOC.h`, `SocEnhance.c`, `SocEnhance.h`, `DataDeal.c`, `Flash.c`, `rtc_sleep_port.c`, `Project_Config.h`
最后更新时间：2026-06-02
未确认事项：测试虚拟电流入口必须继续和量产主路径隔离；Type-C 电流是否必须计入 SOC；初始 SOC 默认值策略。

## 1. SOC 输入

| 输入 | 来源 |
|---|---|
| 单体最大/最小电压 | `DataLoad_CellVolt()` -> `g_stCellInfoReport` |
| 充/放电电流 | 当前 `App_AFEGet()` 主路径调用 `DataLoad_Current()`，并通过 `g_u32AfeCurrentSampleSeq` 驱动 SOC 更新 |
| Type-C 等效电流 | `SOC_GetTypeCBatEquivCurrentA10()` |
| 电池容量/串数/V0/V100 | `OtherElement` |
| 保护/系统状态 | `System_ERROR_UserCallback()` 等 |
| RTC 休眠秒数 | `SOC_ApplyRtcRelaxationCompensation()` |
| Flash snapshot | `StorageFlash_LoadSocData()` |

## 2. SOC 输出

SOC 发布到：

- `g_stCellInfoReport.SocElement.u16Soc`
- `u16Soh`
- `u16CapacityNow`
- `u16CapacityFactory`
- `u16Cycle_times`

这些字段被 Modbus、CAN、LedBar 和测试工具消费。

## 3. 算法流程

```text
App_AFEGet()
  update sample sequence
  App_SOC()
    SOC_UpdateSampleData()
    SOC_IntEnhance_Ctrl()
      积分
      自耗补偿
      满电确认
      低压尾段
      sag holdoff
      静置 OCV
      显示平滑
      持久化
    SOC_PublishReportData()
```

## 4. 关键策略

| 策略 | 当前实现 |
|---|---|
| 满电锚点 | 电压/压差/持续时间满足后，按 1% 步进到 100 |
| 充电积分上限 | 未满电确认前不能自然超过 99 |
| 低压尾段 | Vmin 靠近 V0 时限制 SOC 虚高 |
| sag holdoff | 压降未恢复前避免激进 OCV 校准 |
| 静置 OCV | 静置稳定后记录目标，分步消化 |
| 显示平滑 | normal/chg/low 不同下降/上升节奏 |
| 自耗 | 配置 `PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA 30` |
| SOH | 当前按 cycle 简化估算，最低 80 |

## 5. 持久化

SOC snapshot 使用 Flash journal pair，支持 V1 -> V2 兼容。休眠前通过 `SOC_SaveSnapshotBeforeSleep()` 保存。

## 6. 当前问题

1. P0：必须保持真实 `DataLoad_Current()` 作为量产主路径；测试虚拟电流只能在测试 profile/测试固件中使用。
2. SOC runtime table 当前关闭，上位机写表会被拒绝。
3. 量产 `0xD300 supported=0` 是正常隔离，但测试固件规则必须保持文档化。
4. Type-C 输出电流是否扣 SOC 需要产品确认。

## 7. 后续重构建议

1. 先固化真实电流路径和测试电流隔离门禁，再谈算法优化。
2. 保持 1% 校准硬约束、满电/低压/静置策略不变。
3. 建立 SOC host 回放 + 上板实测双轨验证。
