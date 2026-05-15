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
static void Runtime_RunNormalOnce(void)
{
	SysTime_LatchTaskFlags();
	FactoryAging_Task();
	APP_LedBar();
	/* App_WarnCtrl(); */
	App_AFEGet();

	App_Sci();
	App_AnlogCal();
	App_LowPowerProcess();
	App_Can();
	/* App_SleepDeal(); */
	/* App_SOC(); */
	StorageFlash_AppUseTest_Task();

#ifdef __FUNC__HEAT__
	App_Heat_Cool_Ctrl();
#endif

	App_FlashUpdate();
	App_LogRecord();
	App_ProID_Deal();
#ifdef wdog_enable
	Feed_IWatchDog;
#endif
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
