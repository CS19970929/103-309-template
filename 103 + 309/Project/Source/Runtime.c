#include "main.h"
#include "AppInit.h"
#include "FactoryAging.h"
#include "Runtime.h"
#include "DebugHooks.h"

static void Runtime_RunFrontTasks(void)
{
	SysTime_LatchTaskFlags();
	DebugHooks_RuntimeAfterSysTime();

	FactoryAging_Task();
	DebugHooks_RuntimeAfterAging();

	APP_LedBar();
	DebugHooks_RuntimeAfterLed();

	App_AFEGet();
	DebugHooks_RuntimeAfterAfe();

	DebugHooks_RuntimeSnapshot();
}

static void Runtime_RunIoAndPowerTasks(void)
{
	AppInit_ServiceSci();
	DebugHooks_RuntimeAfterSci();

	App_AnlogCal();
	DebugHooks_RuntimeAfterAdc();

	rtc_sleep();
	DebugHooks_RuntimeAfterLowPower();

	App_Can();
	DebugHooks_RuntimeAfterCan();
}

static void Runtime_RunBackgroundTasks(void)
{
	App_FlashUpdate();
	DebugHooks_RuntimeAfterFlash();

	App_LogRecord();
	DebugHooks_RuntimeAfterLog();

	App_ProID_Deal();
	DebugHooks_RuntimeAfterProId();

	Feed_IWatchDog;
}

static void Runtime_RunNormalOnce(void)
{
	uint32_t loop_start = DebugHooks_RuntimeLoopStart();
	uint32_t section_start;

	section_start = DebugHooks_RuntimeSectionStart();
	Runtime_RunFrontTasks();
	DebugHooks_RuntimeAfterFrontSection(section_start);

	section_start = DebugHooks_RuntimeSectionStart();
	Runtime_RunIoAndPowerTasks();
	DebugHooks_RuntimeAfterIoPowerSection(section_start);

	section_start = DebugHooks_RuntimeSectionStart();
	Runtime_RunBackgroundTasks();
	DebugHooks_RuntimeAfterBackgroundSection(section_start);

	section_start = DebugHooks_RuntimeSectionStart();
	DebugHooks_RuntimeDebugPrint();
	DebugHooks_RuntimeAfterDebugPrintSection(section_start);

	DebugHooks_RuntimeLoopDone(loop_start);
}

void Runtime_RunOnce(void)
{
	Runtime_RunNormalOnce();
}
