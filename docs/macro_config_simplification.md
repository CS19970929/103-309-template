# 宏配置分层简化说明

## 目标

当前工程的宏分成三层维护，避免每次发布或调试都在大量开关里人工确认。

- 日常必须配置的内容放在 `103 + 309/Project/Source/conf/Project_Config.h`
- 调试、破坏性测试、SOC 调参、LedBar 时序等默认值放在 `103 + 309/Project/Source/conf/Project_AdvancedConfig.h`
- 发布保护和合法性检查放在 `103 + 309/Project/Source/conf/Project_BuildGuard.h`

## 日常只改这些

`Project_Config.h` 是唯一建议经常打开的配置入口。它保留以下类型的配置：

- 构建档位：`Release`、`Debug`、`Factory/Test`
- 产品信息：电池类型、电芯体系、固件日期、版本号、电流档位、AFE 类型
- 基础功能：看门狗、RTC、加热、唤醒源、DI 开关、SCI 角色、LedBar 驱动选择
- 升级策略：是否强制刷新 AFE、保护、SOC、事件记录等参数

Keil 中正式包选 `FD_Release` target，在线调试选 `FD_Debug` target。不要为了调试反复手改 `PROJECT_CFG_BUILD_PROFILE`，因为 target 已经负责传入 profile。

## 默认不要手动配置的内容

以下内容已经移到 `Project_AdvancedConfig.h`，平时不需要在 Keil Wizard 里逐项确认：

- `_DEBUG_CODE` 派生开关
- 电动车骑行 SOC 模拟
- SOC 自动测试
- Flash 64K 破坏性和运行期存储测试
- SOC 在线 OCV、满充确认、校准有效电压等算法阈值
- LedBar 长按 GPIO 测试、显示时长、扫描定时、滤波参数

如果确实要做专项验证，可以临时改 `Project_AdvancedConfig.h` 或在专用 Keil target 中覆盖对应 `PROJECT_CFG_*` 宏。验证结束后必须回到默认值，`FD_Release` 下打开危险测试项会被 `Project_BuildGuard.h` 编译期拦截。

## Release 和 Debug 规则

- `FD_Release` 只保留 `STM32F10X_MD,USE_STDPERIPH_DRIVER`，默认走 `PROJECT_CFG_BUILD_PROFILE=0`
- `FD_Debug` 额外定义 `PROJECT_CFG_BUILD_PROFILE=1,_DEBUG_`
- `Factory/Test` 用于产测或破坏性验证，不作为现场正式包
- Release 下强制要求看门狗开启，并禁止 `_DEBUG_`、`_DEBUG_CODE`、Flash 64K 测试、SOC 自动测试、骑行模拟、LedBar GPIO 测试、EasyLogger 输出等危险项

## 自动检查

运行：

```sh
python tools/project_check.py
```

检查内容包括：

- `FD_Release` 和 `FD_Debug` target 的宏差异是否正确
- release 默认值是否安全
- `Project_Config.h` 是否只暴露日常配置
- `Project_AdvancedConfig.h` 是否包含内部默认项
- `Project_BuildGuard.h` 是否覆盖 release 禁用项

启用 `.githooks` 后，提交和推送前也会自动运行该检查。
