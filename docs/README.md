# BMS 项目文档

> 项目: 103-309 BMS (STM32F103 + SH367309)
> 最后更新: 2026-06-04

---

## 入口

📖 **[INDEX.md](INDEX.md)** — 文档总索引，链接到所有分文档。

当前专项入口：

- [SOC 模块设计与源码审查](design/soc_design.md) - 当前唯一活跃 SOC 逻辑入口，已合并历史 SOC review/devlog 的有效内容
- [SOC 函数粒度审查与净删减记录](review/soc_function_granularity_review_2026-06-04.md) - 记录 SOC 小函数保留/合并边界
- [其他模块简化审查与 LedBar 净删减记录](review/module_simplification_review_2026-06-04.md) - 记录 SOC 之外的模块简化判断和本轮 LedBar 删除项
- [状态变量净删减专项审计](review/state_variable_audit.md)
- [顶层变更记录](change_log.md)
- [顶层测试计划](test_plan.md)

---

## 目录结构

```
docs/
├── INDEX.md         ← 主入口，链接所有文档
├── README.md        ← 你在这里
├── reference/       ← 核心参考（长期维护）
├── design/          ← 模块设计文档
├── protocol/        ← 协议文档
├── changelog/       ← 变更记录
├── devlog/          ← 历史开发日志
├── guides/          ← 使用指南
├── review/          ← 审查文档
├── test/            ← 测试
└── archive/         ← 归档
```

## 使用原则

1. 源码是第一可信来源。
2. 文档与源码冲突时，以源码为准。
3. 修改关键模块（协议、Flash、IAP、低功耗、SOC、AFE）时，同步更新 `reference/` 和 `design/` 下对应文档。
4. `devlog/` 下为历史记录，完成后不再更新。
5. 新文档开头标注：状态、源码验证、参考源码、更新时间、未确认事项。
