#include "main.h"
#include "Flash64KAppTest.h"
#include "Runtime.h"

UINT8 SeriesNum = 10;

const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 9, 10, 11, 12, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}};

void InitVar(void);
void InitDevice(void);
void InitSci(void);
void App_Sci(void);
void InitSystemWakeUp(void);

UINT8 MosStartup_Is5vChargeActive(void)
{
	return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET);
}

void open_chg_close_dsg(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1;
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 1;
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 0;
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_SET);
}

void open_dsg_close_chg(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1;
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 0;
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1;
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_RESET);
}

void enter_fac_mode(bool on)
{
	if (MosStartup_Is5vChargeActive() != 0U)
	{
		open_chg_close_dsg();
		return;
	}

	if (on)
	{
		SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1;
		SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 1;
		SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1;
		MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
		GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_SET);
	}
	else
	{
		open_dsg_close_chg();
	}
}

void MosStartup_ApplyInitialState(void)
{
	if (MosStartup_Is5vChargeActive() != 0U)
	{
		open_chg_close_dsg();
	}
	else if (FactoryAging_ShouldStartOnBoot() != 0U)
	{
		enter_fac_mode(true);
	}
	else
	{
		open_dsg_close_chg();
	}
}

int main(void)
{
	InitDevice();
	InitVar();
	Init_RTC();
	/* RTC_WKTimeConfig(); */

	while (1)
	{
		Runtime_RunOnce();
	}
}

void InitDevice(void)
{
	SystemInit();

#if (defined _DEBUG_CODE)
	InitDelay();
	InitIO();
#else
	InitDelay();
	IsSleepStartUp();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	InitSci();
#ifdef ELOG_OUTPUT_ENABLE
	InitUSART_CommonUpper();
	elogInit();
#endif
	InitSystemWakeUp();
	StorageFlash_PrintBootCheck();
#ifdef FLASH64K_APP_QUICK_TEST_ENABLE
	StorageFlash_RunAppQuickTest();
#endif
	InitE2PROM();
	InitAFE1();
	InitCan();
	InitADC();

#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif
	/* Init_ChargerLoad_Det(); */
	/* InitMosRelay_DOx(); */
	InitData_SOC();

	InitTimer();
	__enable_irq();
	log_w("init over");

#ifdef _DEBUG_
	DBGMCU_Config(DBGMCU_STOP, ENABLE);
#endif

#ifdef wdog_enable
	Init_IWDG();
#endif

#endif
}

void InitVar(void)
{
	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemStatus.bits.b1StartUpBMS = 0;
	SystemStatus.bits.b1Status_ToSleep = 1;

	SystemStatus.bits.b4Status_ProjectVer = 1;
	LogRecord_Flag.bits.Log_StartUp = 1;
}

void InitSystemWakeUp(void)
{
}

void InitSci(void)
{
	InitUSART_CommonUpper();
}

void App_Sci(void)
{
	App_CommonUpper();
}
