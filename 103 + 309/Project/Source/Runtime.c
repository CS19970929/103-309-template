#include "main.h"
#include "FactoryAging.h"
#include "Runtime.h"
#include "DebugHooks.h"
#include "DebugWatch.h"
#include "IrqDebug.h"
#include "debug_hub.h"

void DataLoad_CellVolt(void);
void DataLoad_CellVoltMaxMinFind(void);

UINT8 SeriesNum = 10;

static UINT8 Runtime_SampleDeepSleepWakeVoltage(void)
{
	if ((SleepDeal_IsBootFromDeepSleepStartup() == 0U) ||
		(SleepDeal_IsBootFromSleepChargerWakeup() != 0U))
	{
		return 0U;
	}

	if (UpdateVoltageFromBqMaximo() != 0U)
	{
		return 0U;
	}

	DataLoad_CellVolt();
	DataLoad_CellVoltMaxMinFind();
	return 1U;
}

void Runtime_Boot(void)
{
	UINT8 deep_sleep_ocv_ready;

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
	deep_sleep_ocv_ready = Runtime_SampleDeepSleepWakeVoltage();
	InitCan();
	InitADC();

	InitData_SOC();
	if (deep_sleep_ocv_ready != 0U)
	{
		(void)SOC_ApplyDeepSleepWakeOcvCalibration();
	}

	InitTimer();
	__enable_irq();
	IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_RUN);


	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	LedBar_Init();
	InitProID();
	LogRecord_RequestStartup();

	Init_RTC();
	DBG_Init();
	/* RTC_WKTimeConfig(); */
	EnableLowPowerDebug();
	Init_IWDG();
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
