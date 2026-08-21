#include "main.h"
#include "Runtime.h"

UINT8 SeriesNum = PROJECT_CFG_DEFAULT_SERIES_NUM;

void Runtime_Boot(void)
{
	InitDelay();
	InitNVIC();

	/* Consume backup-domain sleep flags before RTC recovery can reset the domain. */
	SleepDeal_HandleBootSleepStartup();

#if PROJECT_CFG_RTC_ENABLE
	Init_RTC();
#endif

	jtag_disableAndConfIO();

	/* Core state must exist before AFE/config modules start publishing state. */
	InitSystemMonitorData_EEPROM();

	InitIO();
	InitUSART_CommonUpper();
	InitE2PROM();
	InitAFE1();
	InitCan();
	InitADC();
	InitData_SOC();

	InitTimer();
	__enable_irq();

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	InitProID();

	EnableLowPowerDebug();
	LogRecord_RequestStartup();
	Init_IWDG();
}

void Runtime_RunOnce(void)
{
	SysTime_LatchTaskFlags();

	App_AFEGet();
	App_CommonUpper();
	App_AnlogCal();

#if PROJECT_CFG_RTC_ENABLE
	rtc_sleep();
#endif

	App_Can();
	App_FlashUpdate();
	App_LogRecord();

	Feed_IWatchDog;
}
