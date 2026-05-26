#include "main.h"
#include "AppInit.h"
#include "FactoryAging.h"
#include "Flash64KAppTest.h"
#include "Runtime.h"
#include "app_lowpower.h"

#if (defined _DEBUG_CODE)
static void Runtime_RunDebugOnce(void)
{
	App_AFEGet();
	AppInit_ServiceSci();
}
#else
static void Runtime_RunFrontTasks(void)
{
	SysTime_LatchTaskFlags();
	FactoryAging_Task();
	APP_LedBar();
	/* App_WarnCtrl(); */
	App_AFEGet();
}

static void Runtime_RunIoAndPowerTasks(void)
{
	AppInit_ServiceSci();
	App_AnlogCal();
	LP_Task();
	App_Can();
	/* App_SOC(); */
}

static void Runtime_RunBackgroundTasks(void)
{
	StorageFlash_AppUseTest_Task();

	App_FlashUpdate();
	App_LogRecord();
	App_ProID_Deal();
#ifdef wdog_enable
	Feed_IWatchDog;
#endif
}

static void Runtime_RunNormalOnce(void)
{
	Runtime_RunFrontTasks();
	Runtime_RunIoAndPowerTasks();
	Runtime_RunBackgroundTasks();
}
#endif

void Runtime_RunOnce(void)
{
#if (defined _DEBUG_CODE)
	Runtime_RunDebugOnce();
#else
	Runtime_RunNormalOnce();
#endif
}
