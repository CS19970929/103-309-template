# LedBar 与系统状态收口重构说明 - 2026-05-25

## 目标

本次重构处理两个问题：

- `LedBar.c` 内部通过 `s_ledbar_initialized` 这类宏别名访问运行时状态，可读性差，也容易掩盖主循环和 TIM4 中断共享状态。
- `SystemStatus`、`LogRecord_Flag`、`System_OnOFF_Func`、`System_Func_StartUp`、`System_OnOFF_Func_StartUpRec` 作为跨模块全局变量被直接读写，导致状态来源不清晰。

本轮保持 RS485/CAN 协议布局、SOC 估算、低功耗策略、IAP/App 地址不变。

## LedBar 调整

- 删除 `LedBar.c` 中 `#define s_ledbar_* (...)` 兼容宏，统一直接访问 `s_ledbar.xxx`。
- 新增 `LedBar_EnsureInit()`，普通 API 入口仍可自动初始化。
- `LedBar_Scan1ms()` 不再在 TIM4 中断路径调用 `LedBar_Init()`；未初始化时直接返回。
- `frame_mask`、`scan_route`、扫描定时器状态字段标记为 `volatile`，明确这些字段由主循环和 TIM4 ISR 共享。
- 普通空帧只停止扫描并输出 Hi-Z；只有 `LedBar_SetSleep(1)` 和 `LedBar_PrepareForStop()` 才把 GPIO 进入 STOP 前低功耗确定态。
- LedBar 不再直接读取 `SystemStatus`，放电 MOS 图标来源改为 AFE `SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET`。

这减少了“业务刷新导致短暂 STOP GPIO 重配”的窗口，也避免了 ISR 里初始化 GPIO/TIM 的高风险路径。

## 系统状态收口

### 已移除外部直接变量访问

- `LogRecord_Flag` 改为 `LogRecord_RequestStartup()` / `LogRecord_RequestSleep()` 请求接口。
- `SystemStatus` 改为 `SystemRuntime_*` 接口：
  - `SystemRuntime_MarkBootReady()`
  - `SystemRuntime_SetProjectVersion()`
  - `SystemRuntime_SetAfeStatus()`
  - `SystemRuntime_SetMosStatus()`
  - `SystemRuntime_IsChargeMosOpen()`
  - `SystemRuntime_IsDischargeMosOpen()`
  - `SystemRuntime_GetStatusSnapshot()`
- `System_OnOFF_Func` 改为 `SystemFeature_*` 接口：
  - `SystemFeature_GetMask()`
  - `SystemFeature_SetById()`
  - `SystemFeature_IsSocFixed()`
  - `SystemFeature_IsSocZero()`
- `System_Func_StartUp` 和 `System_OnOFF_Func_StartUpRec` 的运行期写入已移除。

### 保留边界

`System_Monitor.h` 中的旧 union 类型暂时保留，原因是上位机协议位布局仍依赖这些 bitfield 的顺序。当前不再导出同名全局变量，协议上报通过 `SystemRuntime_GetStatusSnapshot()` 和 `SystemFeature_GetMask()` 获取兼容快照。

后续如果要继续清理，可以在确认上位机寄存器布局后，把旧 union 类型重命名为协议快照类型，例如 `SystemRuntimeStatusSnapshot` 和 `SystemFeatureMaskSnapshot`。

## 影响范围

主要修改文件：

- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/System_Monitor.c`
- `103 + 309/Project/Source/System_Monitor.h`
- `103 + 309/Project/Source/LogRecord.c`
- `103 + 309/Project/Source/LogRecord.h`
- `103 + 309/Project/Source/AppInit.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/SH367309_Func.c`
- `103 + 309/Project/Source/CanFeidaoFrames.c`

## 验证

已通过：

```powershell
py -3.9 tools\project_check.py --quiet
py -3.9 tools\soc_replay_test.py
.\tools\bms_dev_workflow.ps1 -Mode build -Target FD_Release
```

Keil `FD_Release` 构建结果：

- `0 Error(s), 0 Warning(s)`
- 生成 `103 + 309/Project/Users/Objects/FD_Release.bin`

未通过但与本次改动无直接关系：

```powershell
py -3.9 tools\run_soc_host_c_test.py
```

当前 `Project_Config.h` 为 `PROJECT_CFG_BAT_CHEMISTRY 1` 且 `PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE 0`，SOC host C 测试仍按三元锂 3835mV 期望 70% 编写；在当前编译配置下会使用磷酸铁锂表，3835mV 对应 100%，因此该测试失败。SOC Python replay 已按当前模型通过。

## 后续建议

- 下一轮继续把 `System_Monitor.h` 中旧 union 类型重命名为协议快照类型，降低旧变量名的误导。
- 重新整理 `tools/run_soc_host_c_test.py` 的电芯体系配置，使 host C 测试与 `PROJECT_CFG_BAT_CHEMISTRY` 保持一致。
- 上板观察短按显示窗口结束、MCU_WK 上升沿触发后到时熄屏、STOP 前后预览三种场景，重点确认数码管不再出现整屏短闪。
