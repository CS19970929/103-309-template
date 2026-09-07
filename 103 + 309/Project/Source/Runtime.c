#include "main.h"
#include "Runtime.h"

/* Fallback for a missing/invalid persistent image. The target pack is 19S. */
UINT8 SeriesNum = 19;

void Runtime_Boot(void)
{
	InitDelay();
	SleepDeal_HandleBootSleepStartup();
	Init_RTC();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	InitUSART_CommonUpper();
	InitE2PROM();
	Afe3520_AppInit();
	InitCan();
	InitADC();

	InitData_SOC();

	/* Seed requests once; the first protected sample decides actual MOS state. */
	MosStartup_ApplyInitialState();

	InitTimer();
	__enable_irq();

	InitSystemMonitorData_EEPROM();

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	InitProID();

	EnableLowPowerDebug();
	LogRecord_RequestStartup();
	// Init_IWDG();
}

void Runtime_RunOnce(void)
{
	SysTime_LatchTaskFlags();

	App_AFEGet();
	App_CommonUpper();
	App_AnlogCal();

	rtc_sleep();

	App_Can();
	App_FlashUpdate();

	App_LogRecord();

	Feed_IWatchDog;

	// __WFI();
}
