# 重复文档分析

> 文档状态：CURRENT
> 源码验证：PARTIAL
> 主要参考源码：同 `document_source_consistency.md`
> 最后更新时间：2026-05-26
> 未确认事项：本表只提出合并/归档建议，不删除旧文档。

| 重复组 ID | 涉及文档 | 重复内容 | 冲突内容 | 建议保留主文档 | 建议合并到 | 建议归档 |
|---|---|---|---|---|---|---|
| DUP-ARCH-001 | `PROJECT_REVIEW_2026-05-09.md`, `项目逻辑完整梳理与架构简化建议_2026-05-24.md`, `项目运行流程与时序源码梳理_2026-05-16.md`, `docs/review/full_project_review.md` | 项目总览、主循环、模块风险、重构建议 | 旧文档未覆盖当前虚拟电流 P0 风险和最新 comm/CAN 老化 | `docs/review/full_project_review.md` | `docs/architecture.md`, `docs/module_map.md` | 旧 review 文档建议归档 |
| DUP-SOC-001 | `SOC_MODULE_LOGIC.md`, `SOC完整运行流程说明.md`, `SOC逻辑与参数影响梳理.md`, `BMS_SOC_STRATEGY_COMPARISON.md`, `SOC_CALIBRATION_STRATEGY.md`, `docs/design/soc_design.md` | SOC 输入、积分、满电、低压、静置、平滑、测试 | 部分旧文档描述简化策略，与当前 `SocEnhance.c` 增强逻辑不完全一致 | `docs/design/soc_design.md` | `docs/design/soc_design.md`, `docs/test/test_plan.md` | 旧 SOC 修复/阶段文档归档 |
| DUP-SOC-TEST-001 | `SOC_TEST*.md`, `SOC_HOST_VALIDATION_PLAN.md`, `SOC_RIDE_SIM_REPORT.md`, `TEST_PENDING.md`, `docs/test/test_plan.md` | SOC 测试 UI、host 回放、板端监控、待测项 | 部分命令依赖 Windows/COM4，需按当前脚本复核 | `docs/test/test_plan.md` | `docs/test/test_plan.md` | 旧测试报告按历史保留 |
| DUP-LP-001 | `docs/current/*.md`, `docs/design/low_power_*.md`, `docs/low_power_rtc_*.md`, `RTC_*.md`, `LOW_POWER_*.md`, `休眠*.md` | RTC/STOP/IWDG/时钟/外设恢复/阻塞原因 | 旧 CAN 阻塞、旧状态机、旧阶段设计与当前 `app_lowpower.c` 不一致 | `docs/design/low_power_design.md` | `docs/design/low_power_design.md`, `docs/test/test_plan.md` | 阶段设计/迁移计划归档 |
| DUP-STORAGE-001 | `EEPROM_LAYOUT_OPTIMIZATION.md`, `EEPROM_WRITEFLAG_CLEANUP.md`, `RW_PARAMETER_FLASH_STORAGE.md`, `STORAGE_LAYOUT_REPORT.md`, `Flash磨损寿命*.md`, `后64K_SOC_AFE快速测试说明.md` | EEPROM/Flash/RW 参数/SOC snapshot/日志/后 64K | 外部 EEPROM 旧布局与当前内部 Flash 实现冲突 | `docs/design/storage_design.md` | `docs/design/storage_design.md` | 外部 EEPROM 布局归档 |
| DUP-COMM-001 | `COMMUNICATION_*.md`, `docs/BMS_CAN_SERVICE_PROTOCOL.md`, `docs/CAN_FACTORY_AGING_SOC_CONTROL_2026-05-25.md`, `docs/COMM_ARCH_OPTIMIZATION_2026-05-26.md` | Modbus/CAN 地址、读写、老化、产品信息 | 旧文档称 CAN 不承担寄存器映射，与当前 CAN App read/write 冲突 | `docs/design/protocol_design.md` | 后续 `docs/protocol/modbus_register_map.md`, `docs/protocol/can_protocol.md` | 旧通信布局文档归档 |
| DUP-IAP-001 | `docs/BMS_BOOT_CONTROL_REFACTOR_2026-05-23.md`, `docs/BMS_CAN_IAP_PROTOCOL.md`, `docs/BMS_SERIAL_IAP_REFACTOR_2026-05-22.md`, `CAN_IAP_UPPER_COMPUTER_PLAN.md`, `IAP_APP_TIM3_HANDOFF_FIX_2026-05-18.md` | App/IAP 地址、进入 IAP、升级可靠性 | 主 App 只确认入口；IAP/comm tool 独立工程未本轮核完 | 后续 `docs/design/bootloader_iap_design.md` | 当前先在 `project_overview.md` 和 `protocol_design.md` 说明 App 入口 | IAP 旧实施记录归档 |
| DUP-LED-001 | `LEDBAR_*.md`, `LED软件框架与时序梳理.md`, `数码管*.md`, `休眠唤醒数码管显示10秒说明.md`, `docs/LEDBAR_SYSTEM_STATE_REFACTOR_2026-05-25.md` | Charlieplexing、显示窗口、按键、低功耗引脚 | 长按 3 秒 vs 当前约 500ms；charge icon 语义待确认 | 后续 `docs/design/led_display_design.md` | 本轮先在 `docs/module_map.md` 和 review 中记录 | 旧 LED 计划归档 |
| DUP-CAN-OLD-001 | `CAN低功耗发送调度说明.md`, `CAN_RTC低功耗重构说明_2026-05-14.md`, `CAN通信逻辑与低功耗策略分析.md`, `docs/BMS_CAN_MODULE_REFACTOR_2026-05-23.md` | CAN 周期调度、低功耗、请求处理 | 多份文档描述不同阶段；以当前 `Can_HDX.c` 为准 | `docs/design/protocol_design.md` | `docs/design/protocol_design.md` | 旧 CAN 低功耗调度文档归档 |
| DUP-CHANGE-001 | 大量 `*_2026-05-xx.md`, `docs/low_power_rtc_change_log.md`, `docs/implementation/*` | 阶段性变更记录 | 不应作为当前行为依据 | `docs/changelog/change_log.md` | `docs/changelog/change_log.md` 摘要 | 原记录归档保留 |
