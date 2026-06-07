#include "main.h"
#include "AppInit.h"
#include "DebugWatch.h"
#include "IrqDebug.h"
#include "debug_hub.h"

UINT8 SeriesNum = 10;

static void AppInit_InitDevice(void)
{
	IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_BOOT);
	SystemInit();

	InitDelay();
	SleepDeal_HandleBootSleepStartup();

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
	IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_RUN);


	EnableLowPowerDebug();

	Init_IWDG();
}

static void AppInit_InitRuntimeState(void)
{
	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	LedBar_Init();
	InitProID();
	LogRecord_RequestStartup();
}

void AppInit_Boot(void)
{
	DebugWatch_BindAll();
	AppInit_InitDevice();
	AppInit_InitRuntimeState();
	Init_RTC();
	DBG_Init();
	/* RTC_WKTimeConfig(); */
	 
	
}
