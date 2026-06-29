#include "main.h"
#include "FactoryAging.h"
#include "Runtime.h"
#include "DebugHooks.h"
#include "DebugWatch.h"
#include "IrqDebug.h"
#include "debug_hub.h"

UINT8 SeriesNum = 7;

void Runtime_Boot(void)
{
	DebugWatch_BindAll();

	IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_BOOT);
	SystemInit();

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
	IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_RUN);


	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	InitProID();
	LogRecord_RequestStartup();

	Init_RTC();
	DBG_Init();
	/* RTC_WKTimeConfig(); */
	EnableLowPowerDebug();
	// Init_IWDG();
}

void Runtime_RunOnce(void)
{
	SysTime_LatchTaskFlags();
	// FactoryAging_Task();
	App_AFEGet();
	App_CommonUpper();
	App_AnlogCal();

	rtc_sleep();

	App_Can();
	App_FlashUpdate();

	App_LogRecord();

	App_ProID_Deal();

	Feed_IWatchDog;
}
