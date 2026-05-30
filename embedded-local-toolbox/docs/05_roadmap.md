# 路线图

## 第一阶段：本地离线最小可用

- C 项目静态风险扫描。
- Keil map 分析与差异对比。
- Modbus 参数离线导出/对比和 dry-run 写调试。
- CAN 离线解码和周期分析。
- BMS 事件日志解码和统计。
- 参数表检查、C 生成、Markdown 生成。
- BMS 保护逻辑和 SOC PC 仿真。
- Cortex-M HardFault 解析。
- git diff 变更记录生成。
- 串口实时监控、BMS 命令行 dashboard、OpenOCD/STLink 只读探测和固件产物报告。

## 第二阶段：工程适配增强

- 适配更多 Keil map 格式和 ARM GCC map 格式。
- `embedded_project_doctor.py` 增加调用图、文件间依赖和中断共享变量交叉检查。
- Modbus 支持批量读窗口合并、异常码统计和寄存器快照归档。
- CAN 支持 DBC 子集导入或 JSON 自动转换。
- SOC 仿真增加 OCV 校准、静置判定、低压保持和显示平滑策略。
- OpenOCD/JLink 增加更多只读寄存器解码，例如 CPUID、UID、option bytes 只读快照。
- 串口 dashboard 增加寄存器窗口合并读取和异常码统计。

## 第三阶段：团队/项目资产化

- 每个项目维护自己的 `data/project_x/` 配置目录。
- CI 或本地 pre-commit 只运行只读检查。
- 报告长期归档到项目 `docs/reports/`。
- 针对 BMS 保护板形成参数表、Modbus 表、CAN 表、事件表的一体化配置源。
