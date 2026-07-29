#include "main.h"
#include "Runtime.h"

UINT8 SeriesNum = 7;

void Runtime_Boot(void)
{
	SystemInit();
	Init_RTC();

	InitDelay();
	SleepDeal_HandleBootSleepStartup();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	InitUSART_CommonUpper();
	InitE2PROM();
	InitAFE1();
	InitCan();
	InitADC();

	InitData_SOC();

	InitTimer();
	__enable_irq();


	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	InitProID();

	EnableLowPowerDebug();
	// Init_IWDG();
	LogRecord_RequestStartup();
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

	__WFI();
}
