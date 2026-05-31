# 本次迁移变更记录

记录日期：2026-05-31

## 新增

- `docs/keil_build_baseline.md`：Keil 构建基准。
- `docs/keil_to_gcc_config_compare.md`：Keil 到 GCC/CMake 总配置对照表。
- `docs/scatter_to_ld_compare.md`：scatter 到 linker script 对照。
- `docs/startup_migration_check.md`：startup 迁移检查。
- `docs/macro_compare.md`：宏定义对照。
- `docs/include_path_compare.md`：include 路径对照。
- `docs/optimization_migration_check.md`：优化等级和时序风险检查。
- `docs/compiler_compatibility.md`：ARMCC/GCC 兼容层说明。
- `docs/flash_layout_and_storage_check.md`：Flash 参数区和日志区检查。
- `docs/iap_bootloader_check.md`：IAP/Bootloader 检查。
- `docs/bms_gcc_migration_risk.md`：BMS 功能迁移风险。
- `docs/migration_blockers.md`：当前阻塞项。
- `docs/keil_to_cmake_migration.md`：迁移执行记录。
- `docs/toolchain_install.md`：工具链安装说明。
- `docs/build_debug_guide.md`：构建、烧录与调试指南。
- `docs/migration_risk_list.md`：汇总风险清单。
- `cmake/`：GCC toolchain、公共 flags、STM32F103 配置和 STM32F030 占位配置。
- `linker/stm32f103_app_0x08004800.ld`：GCC linker script，保留 IAP 与参数区边界。
- `startup/startup_stm32f10x_hd_gcc.s`：GCC startup，不覆盖 Keil ARMASM startup。
- `scripts/`：跨平台环境检查、构建、清理、size、烧录脚本。
- `.vscode/`：插件推荐、CMake/调试 settings、tasks、launch 配置。

## 修改

- 未修改业务逻辑文件。
- 未删除 Keil 工程文件。
- 未修改通信协议、BMS 保护逻辑、SOC 逻辑、低功耗逻辑。
- 新增 `cmake/compat/compiler_port.h` 作为 ARMCC/GCC 兼容集中入口，避免分散修改。

## 未完成

- 当前机器缺少 CMake、Ninja 和 Arm GNU Toolchain，GCC 构建尚未完成。
- 实际 MCU 型号和 Flash 容量仍需硬件/BOM/读芯片确认。
- GCC 编译阶段尚未验证 ARMCC 汇编函数、retarget、section 属性等兼容性。
- 尚未完成上板烧录、调试和 BMS 功能回归。

## 已提交记录

| Commit | 说明 |
|---|---|
| `e7d0657` | `docs: 记录 Keil 构建基准` |
| `8d50142` | `docs: 审查 Keil 到 GCC 配置对照` |
| `82a9685` | `docs: 审查 scatter 与 Flash 布局迁移` |
| `44dd913` | `docs: 审查 startup 迁移` |
| `64d3e0a` | `docs: 审查宏定义与 include 迁移` |
| `71e4457` | `docs: 审查优化兼容与 BMS 风险` |
| `21e42f3` | `build: 新增 CMake GCC 构建骨架` |
| `fe55eff` | `tools: 新增跨平台构建脚本` |
| `3b8dd55` | `vscode: 新增 VS Code 调试配置` |
| `5909696` | `tools: 改进构建脚本缺失工具提示` |
