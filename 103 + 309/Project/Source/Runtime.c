#include "main.h"
#include "app_lowpower.h"
#include "AppInit.h"
#include "FactoryAging.h"
#include "Runtime.h"

static void Runtime_RunFrontTasks(void)
{
	SysTime_LatchTaskFlags();
	FactoryAging_Task();
	APP_LedBar();
	App_AFEGet();
}

static void Runtime_RunIoAndPowerTasks(void)
{
	AppInit_ServiceSci();
	App_AnlogCal();
	LP_Task();
	App_Can();
}

static void Runtime_RunBackgroundTasks(void)
{
	App_FlashUpdate();
	App_LogRecord();
	App_ProID_Deal();
	Feed_IWatchDog;
}

static void Runtime_RunNormalOnce(void)
{
	Runtime_RunFrontTasks();
	Runtime_RunIoAndPowerTasks();
	Runtime_RunBackgroundTasks();
}

void Runtime_RunOnce(void)
{
	Runtime_RunNormalOnce();
}
