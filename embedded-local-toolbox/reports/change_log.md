# 嵌入式项目变更记录

- repo: `..`
- base: `HEAD`
- staged: `False`

## 文件变更

| 文件 | 新增行 | 删除行 | 分类 |
|---|---|---|---|
| embedded-local-toolbox/README.md | 14 | 0 | 文档 |
| embedded-local-toolbox/data/examples/README.md | 4 | 0 | 文档 |
| embedded-local-toolbox/docs/01_usage.md | 11 | 0 | 文档 |
| embedded-local-toolbox/docs/02_tool_list.md | 5 | 0 | 文档 |
| embedded-local-toolbox/docs/03_data_format.md | 42 | 0 | 文档 |
| embedded-local-toolbox/docs/04_safety_rules.md | 30 | 0 | 文档 |
| embedded-local-toolbox/docs/05_roadmap.md | 3 | 0 | 文档 |
| embedded-local-toolbox/tools/elt_common.py | 42 | 0 | 其他 |

## 回归建议

- 通信协议、参数表、Flash/IAP、低功耗、中断和 MOS 控制相关变更需要优先做板端回归。

- 本工具只读取本地 git diff，不联网，不上传项目数据。
