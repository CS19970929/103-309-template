#include "main.h"
#include "app_lowpower.h"
#include "AppInit.h"

UINT8 SeriesNum = 10;

static void AppInit_InitDevice(void)
{
	SystemInit();

	InitDelay();
	IsSleepStartUp();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	AppInit_InitSci();
	InitE2PROM();
	InitAFE1();
	InitCan();
	InitADC();

	InitData_SOC();

	InitTimer();
	__enable_irq();


	EnableLowPowerDebug();

	Init_IWDG();
}

static void AppInit_InitRuntimeState(void)
{
	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	LogRecord_RequestStartup();
}

void AppInit_Boot(void)
{
	AppInit_InitDevice();
	AppInit_InitRuntimeState();
	Init_RTC();
	LP_Init();
	/* RTC_WKTimeConfig(); */
	 
	
}
