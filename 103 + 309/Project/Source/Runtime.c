#include "main.h"
#include "FactoryAging.h"
#include "Flash64KAppTest.h"
#include "Platform_Port.h"
#include "Project_Features.h"
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
	Platform_LatchTaskFlags();
#if PROJECT_FEATURE_FACTORY_AGING
	FactoryAging_Task();
#endif
#if PROJECT_FEATURE_LEDBAR
	APP_LedBar();
#endif
	/* App_WarnCtrl(); */
#if PROJECT_FEATURE_AFE
	App_AFEGet();
#endif
}

static void Runtime_RunIoAndPowerTasks(void)
{
#if PROJECT_FEATURE_RS485
	App_Sci();
#endif
#if PROJECT_FEATURE_ANALOG_ADC
	App_AnlogCal();
#endif
#if PROJECT_FEATURE_RTC_LOW_POWER
	App_LowPowerProcess();
#endif
#if PROJECT_FEATURE_CAN
	App_Can();
#endif
	/* App_SOC(); */
}

static void Runtime_RunBackgroundTasks(void)
{
#if PROJECT_FEATURE_STORAGE
	StorageFlash_AppUseTest_Task();
#endif

#if defined(__FUNC__HEAT__) && PROJECT_FEATURE_HEAT
	App_Heat_Cool_Ctrl();
#endif

#if PROJECT_FEATURE_STORAGE
	App_FlashUpdate();
#endif
#if PROJECT_FEATURE_LOG_RECORD
	App_LogRecord();
#endif
#if PROJECT_FEATURE_PRODUCTION_ID
	App_ProID_Deal();
#endif
#if defined(wdog_enable) && PROJECT_FEATURE_WATCHDOG
	Platform_FeedWatchdog();
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
