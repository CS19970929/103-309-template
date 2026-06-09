#include "main.h"
#include "FactoryAging.h"
#include "Runtime.h"
#include "DebugHooks.h"
#include "DebugWatch.h"
#include "IrqDebug.h"
#include "RuntimeLog.h"
#include "debug_hub.h"

UINT8 SeriesNum = 10;

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

	EnableLowPowerDebug();

	Init_IWDG();
	RuntimeLog_Init();

	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	LedBar_Init();
	InitProID();
	LogRecord_RequestStartup();

	Init_RTC();
	DBG_Init();
	RuntimeLog_BootReady();
	/* RTC_WKTimeConfig(); */
}

void Runtime_RunOnce(void)
{
	uint32_t loop_start = DebugHooks_RuntimeLoopStart();
	uint32_t section_start;

	section_start = DebugHooks_RuntimeSectionStart();
	SysTime_LatchTaskFlags();
	DebugHooks_RuntimeAfterSysTime();

	FactoryAging_Task();
	DebugHooks_RuntimeAfterAging();

	APP_LedBar();
	DebugHooks_RuntimeAfterLed();

	App_AFEGet();
	DebugHooks_RuntimeAfterAfe();

	DebugHooks_RuntimeSnapshot();
	DebugHooks_RuntimeAfterFrontSection(section_start);

	section_start = DebugHooks_RuntimeSectionStart();
	App_CommonUpper();
	DebugHooks_RuntimeAfterSci();

	App_AnlogCal();
	DebugHooks_RuntimeAfterAdc();

	rtc_sleep();
	DebugHooks_RuntimeAfterLowPower();

	App_Can();
	DebugHooks_RuntimeAfterCan();
	DebugHooks_RuntimeAfterIoPowerSection(section_start);

	section_start = DebugHooks_RuntimeSectionStart();
	App_FlashUpdate();
	DebugHooks_RuntimeAfterFlash();

	App_LogRecord();
	DebugHooks_RuntimeAfterLog();
	RuntimeLog_Task1s();

	App_ProID_Deal();
	DebugHooks_RuntimeAfterProId();

	Feed_IWatchDog;
	DBG_Task();
	DebugHooks_RuntimeAfterBackgroundSection(section_start);

	section_start = DebugHooks_RuntimeSectionStart();
	DebugHooks_RuntimeDebugPrint();
	DebugHooks_RuntimeAfterDebugPrintSection(section_start);

	DebugHooks_RuntimeLoopDone(loop_start);
}
