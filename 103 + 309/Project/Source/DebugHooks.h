#ifndef DEBUG_HOOKS_H
#define DEBUG_HOOKS_H

#include "Project_Config.h"
#include <stdint.h>

#if defined(PROJECT_CFG_DEBUG_MONITOR_ENABLE) && (PROJECT_CFG_DEBUG_MONITOR_ENABLE != 0)

void DebugHooks_RuntimeAfterSysTime(void);
void DebugHooks_RuntimeAfterAging(void);
void DebugHooks_RuntimeAfterLed(void);
void DebugHooks_RuntimeAfterAfe(void);
void DebugHooks_RuntimeSnapshot(void);
void DebugHooks_RuntimeAfterSci(void);
void DebugHooks_RuntimeAfterAdc(void);
void DebugHooks_RuntimeAfterLowPower(void);
void DebugHooks_RuntimeAfterCan(void);
void DebugHooks_RuntimeAfterFlash(void);
void DebugHooks_RuntimeAfterLog(void);
void DebugHooks_RuntimeAfterProId(void);
void DebugHooks_RuntimeDebugPrint(void);

uint32_t DebugHooks_RuntimeLoopStart(void);
uint32_t DebugHooks_RuntimeSectionStart(void);
void DebugHooks_RuntimeAfterFrontSection(uint32_t section_start);
void DebugHooks_RuntimeAfterIoPowerSection(uint32_t section_start);
void DebugHooks_RuntimeAfterBackgroundSection(uint32_t section_start);
void DebugHooks_RuntimeAfterDebugPrintSection(uint32_t section_start);
void DebugHooks_RuntimeLoopDone(uint32_t loop_start);

#else

#define DebugHooks_RuntimeAfterSysTime()              ((void)0)
#define DebugHooks_RuntimeAfterAging()                ((void)0)
#define DebugHooks_RuntimeAfterLed()                  ((void)0)
#define DebugHooks_RuntimeAfterAfe()                  ((void)0)
#define DebugHooks_RuntimeSnapshot()                  ((void)0)
#define DebugHooks_RuntimeAfterSci()                  ((void)0)
#define DebugHooks_RuntimeAfterAdc()                  ((void)0)
#define DebugHooks_RuntimeAfterLowPower()             ((void)0)
#define DebugHooks_RuntimeAfterCan()                  ((void)0)
#define DebugHooks_RuntimeAfterFlash()                ((void)0)
#define DebugHooks_RuntimeAfterLog()                  ((void)0)
#define DebugHooks_RuntimeAfterProId()                ((void)0)
#define DebugHooks_RuntimeDebugPrint()                ((void)0)
#define DebugHooks_RuntimeLoopStart()                 ((uint32_t)0U)
#define DebugHooks_RuntimeSectionStart()              ((uint32_t)0U)
#define DebugHooks_RuntimeAfterFrontSection(start)    ((void)(start))
#define DebugHooks_RuntimeAfterIoPowerSection(start)  ((void)(start))
#define DebugHooks_RuntimeAfterBackgroundSection(start) ((void)(start))
#define DebugHooks_RuntimeAfterDebugPrintSection(start) ((void)(start))
#define DebugHooks_RuntimeLoopDone(start)             ((void)(start))

#endif

#endif
