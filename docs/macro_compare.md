# 宏定义迁移对照

记录日期：2026-05-31

## `.uvprojx` 宏提取

| Keil 目标 | Keil Define | GCC/CMake 迁移 |
|---|---|---|
| `FD_Release` | `STM32F10X_MD,USE_STDPERIPH_DRIVER` | `STM32F10X_MD;USE_STDPERIPH_DRIVER;PROJECT_CFG_BUILD_PROFILE=0` |
| `FD_Debug` | `STM32F10X_MD,USE_STDPERIPH_DRIVER,PROJECT_CFG_BUILD_PROFILE=1,PROJECT_CFG_DEBUG_WATCH_ENABLE=1,_DEBUG_` | `STM32F10X_MD;USE_STDPERIPH_DRIVER;PROJECT_CFG_BUILD_PROFILE=1;PROJECT_CFG_DEBUG_WATCH_ENABLE=1;_DEBUG_` |

Release 中显式加入 `PROJECT_CFG_BUILD_PROFILE=0` 是为了固化量产隔离规则；Keil 原本依赖 `Project_Config.h` 默认值 `0`，行为应等价。

## 关键库宏

| 宏 | 当前来源 | 迁移策略 | 风险 |
|---|---|---|---|
| `STM32F10X_MD` | Keil Define | CMake target compile definitions | 高：与 HD startup/Flash 使用范围存在不一致 |
| `STM32F10X_HD` | 未定义 | 不新增 | 高：除非确认实际芯片/库配置需要，不得猜测改宏 |
| `STM32F0XX` | 未定义，`conf.h` 中 F0 include 被注释 | 不新增 | 低：当前目标是 F1 工程 |
| `USE_STDPERIPH_DRIVER` | Keil Define | CMake target compile definitions | 低 |
| `HSE_VALUE` | `.uvprojx` 未定义，`stm32f10x.h` 默认非 CL 为 `8000000` | 不覆盖 | 中：需要实物晶振确认 |

## 项目功能宏链

项目功能宏主要来自 `Source/conf/Project_Config.h`，再由 `Source/conf/conf.h` 派生：

- `PROJECT_CFG_BUILD_PROFILE`
- `PROJECT_CFG_AFE_TYPE`
- `PROJECT_CFG_WDOG_ENABLE`
- `PROJECT_CFG_RTC_ENABLE`
- `PROJECT_CFG_IAP_ENABLE`
- `PROJECT_CFG_FACTORY_AGING_ENABLE`
- `PROJECT_CFG_SCI1_ROLE`
- `PROJECT_CFG_SCI2_ROLE`
- `PROJECT_CFG_SCI3_ROLE`
- `PROJECT_CFG_SOC_TEST_MODE_ENABLE`
- `PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX`
- `PROJECT_CFG_FLASH64K_*`
- `PROJECT_CFG_UPGRADE_PARAM_*`
- `PROJECT_CFG_LEDBAR_*`

派生出的关键功能宏包括：

- `_IAP`
- `_COMMOM_UPPER_SCI1`
- `_COMMOM_UPPER_SCI2`
- `_COMMOM_UPPER_SCI3`
- `wdog_enable`
- `__FUNC_RTC__`
- `UART1_WAKEUP_ENABLE`
- `RS485_WAKEUP_ENABLE`
- `SOC_TEST_MODE_ENABLE`
- `AFE_TYPE`

## 迁移原则

- CMake 只迁移 Keil 工程中的编译宏和构建 profile 宏，不把 `Project_Config.h` 中的业务配置展开到 CMake。
- 业务功能开关继续由仓库源码配置文件管理，避免 CMake 与源码双配置漂移。
- SOC 测试模式不得在 Release preset 中开启。

## TODO

- TODO：确认实际芯片型号和密度后，决定是否继续使用 `STM32F10X_MD`，或需要另建目标配置。
- TODO：确认晶振频率是否确实为默认 `HSE_VALUE=8000000`。
