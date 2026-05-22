#include "main.h"
#include "FactoryAging.h"
#include "Flash64KAppTest.h"
#include "Runtime.h"

void App_Sci(void);

#if (defined _DEBUG_CODE)
static void Runtime_RunDebugOnce(void)
{
	App_AFEGet();
	App_Sci();
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
	App_Sci();
	App_AnlogCal();
	App_LowPowerProcess();
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
