#ifndef PROJECT_BUILD_GUARD_H
#define PROJECT_BUILD_GUARD_H

#include "Project_Config.h"

#define PROJECT_BUILD_PROFILE_RELEASE       0
#define PROJECT_BUILD_PROFILE_DEBUG         1
#define PROJECT_BUILD_PROFILE_FACTORY_TEST  2

#endif

/* Checks stay outside the include guard so late-defined macros are still caught. */
#if (PROJECT_CFG_BUILD_PROFILE != PROJECT_BUILD_PROFILE_RELEASE) && \
    (PROJECT_CFG_BUILD_PROFILE != PROJECT_BUILD_PROFILE_DEBUG) && \
    (PROJECT_CFG_BUILD_PROFILE != PROJECT_BUILD_PROFILE_FACTORY_TEST)
#error "Invalid PROJECT_CFG_BUILD_PROFILE"
#endif

#if (PROJECT_CFG_BUILD_PROFILE == PROJECT_BUILD_PROFILE_RELEASE)

#if !PROJECT_CFG_WDOG_ENABLE
#error "Release build: PROJECT_CFG_WDOG_ENABLE must be 1"
#endif

#if PROJECT_CFG_DEBUG_CODE_ENABLE
#error "Release build: PROJECT_CFG_DEBUG_CODE_ENABLE must be 0"
#endif

#if PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE
#error "Release build: PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE must be 0"
#endif

#if PROJECT_CFG_FLASH64K_USE_TEST_ENABLE
#error "Release build: PROJECT_CFG_FLASH64K_USE_TEST_ENABLE must be 0"
#endif

#if PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST
#error "Release build: PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST must be 0"
#endif

#if PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY
#error "Release build: PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY must be 0"
#endif

#if defined(_DEBUG_)
#error "Release build: remove _DEBUG_ from Keil defines or select Debug profile"
#endif

#if defined(_DEBUG_CODE)
#error "Release build: _DEBUG_CODE must not be defined"
#endif

#if defined(FLASH64K_APP_QUICK_TEST_ENABLE)
#error "Release build: FLASH64K_APP_QUICK_TEST_ENABLE must not be defined"
#endif

#if defined(FLASH64K_APP_USE_TEST_ENABLE)
#error "Release build: FLASH64K_APP_USE_TEST_ENABLE must not be defined"
#endif

#if defined(ELOG_OUTPUT_ENABLE)
#error "Release build: ELOG_OUTPUT_ENABLE must not be defined"
#endif

#endif
