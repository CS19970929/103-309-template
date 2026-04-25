#include "main.h"

UINT8 SeriesNum = 16;

void IOstatus_RTCMode_test(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // ??GPIOA??
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ??GPIOB??
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // ??GPIOC??
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // ??GPIOD??
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // ??GPIOE??

	ADC_DeInit(ADC1); // ????????????????

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_3) & (~GPIO_Pin_2);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_14) & (~GPIO_Pin_7) & (~GPIO_Pin_15) & (~GPIO_Pin_6);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	// ??????
	// ???
#if 1
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	// GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	// GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);

#endif
}
// ²»Í¬´®ÊýÎ¬»¤µÄ±í¸ñ
// ÖÐÓ±
const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 1´®
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 2´®
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 3
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 4
	{0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 5
	{0, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 6
	{0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 7
	{0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0},	   // 8
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0},	   // 9
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0},	   // 10
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 0, 0, 0, 0},	   // 11
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0},	   // 12
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 9, 10, 11, 12, 0, 0, 0},   // 13
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0},  // 14
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0}, // 15
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} // 16
};

void InitVar(void);
void InitDevice(void);
void InitSci(void);
void App_Sci(void);
void InitSystemWakeUp(void);
static UINT8 MainLoop_HasPendingWork(void);
static void MainLoop_EnterIdleSleep(void);

static UINT8 MainLoop_HasPendingWork(void)
{
	if ((g_st_SysTimeFlag.all != 0U) ||
		(gu8_200msAccClock_Flag != 0U) ||
		(gu8_1000msAccClock_Flag != 0U))
	{
		return 1U;
	}

	if (Sci_IsAnyPortBusy() != 0U)
	{
		return 1U;
	}

	return 0U;
}

static void MainLoop_EnterIdleSleep(void)
{
	if (!System_OnOFF_Func.bits.b1OnOFF_Sleep)
	{
		return;
	}

	if (MainLoop_HasPendingWork())
	{
		return;
	}

	if (__get_PRIMASK() != 0U)
	{
		return;
	}

	/* 空闲等待只允许进入 Sleep，避免残留的 SLEEPDEEP 影响主循环。 */
	SCB->SCR &= (uint32_t)(~SCB_SCR_SLEEPDEEP_Msk);
	__DSB();
	__WFI();
	__ISB();
}

void open_chg_close_dsg(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1; // å¯?¿½éšç–ŒADC
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 1; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 0; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_SET);
}
void open_dsg_close_chg(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1; // å¯?¿½éšç–ŒADC
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 0; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_RESET);
}
void enter_fac_mode(bool on)
{
#if 1
	if (on)
	{
		SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1; // å¯?¿½éšç–ŒADC
		SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 1; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
		SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
		MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
		GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_SET);
	}
	else
	{
		open_dsg_close_chg();
	}
#endif
}

extern void new_todo_logi(void);

int main(void)
{
	InitDevice(); // ³õÊ¼»¯ÍâÉè
	InitVar();	  // ³õÊ¼»¯±äÁ¿
	// Init_RTC();
	// RTC_WKTimeConfig();
	// while (1)
	// {
	// 	App_AFEGet();
	// 	App_Sci();
	// 	/* Low power: WFI between tasks */
	// 	IOstatus_RTCMode_test();
	// 	// if (1 == gu8_TxEnable_SCI1 || 1 == gu8_TxEnable_SCI2 || 1 == gu8_TxEnable_SCI3)
	// 	if (0 == gu8_TxEnable_SCI1 && 0 == gu8_TxEnable_SCI2 && 0 == gu8_TxEnable_SCI3)
	// 		__WFI();
	// }

	while (1)
	{
#if (defined _DEBUG_CODE)
		App_AFEGet();
		App_Sci();
#else
		App_SysTime();
		APP_LedBar();
		// App_WarnCtrl();
		charger_detect_and_keyLogi_200ms();
		App_AFEGet();

		App_Sci();
		App_AnlogCal();
		App_E2promDeal();
		// App_CellBalance();
		App_Can();
		// App_SleepDeal(); // 关闭这个功能的话，在InitVar()中System_OnOFF_Func相关置零，或者直接屏蔽
		App_LowPowerProcess();
		App_SOC();

#ifdef __FUNC__HEAT__
		App_Heat_Cool_Ctrl();
#endif
		// App_ChargerLoad_Det();

		App_FlashUpdate();
		App_LogRecord();
		App_ProID_Deal();
#ifdef wdog_enable
		Feed_IWatchDog;
#endif
		MainLoop_EnterIdleSleep();

#endif
	}
}

void InitDevice(void)
{
	SystemInit(); // HSEÄ¬ÈÏ±¶Æµµ½72MHz£¬Èç¹ûÃ»HSEÇÐ»ØHSIÔõÃ´´¦ÀíÄ¿Ç°»¹Ã»ÁË½â

#if (defined _DEBUG_CODE)
	InitDelay();
	InitIO();
#else
	InitDelay();
	IsSleepStartUp();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
#ifdef ELOG_OUTPUT_ENABLE
	InitUSART_CommonUpper();
	elogInit();
#endif
	InitSystemWakeUp();
	InitE2PROM(); // ¾ö¶¨°ÑÕâ¸ö·ÅÔÚÇ°Ãæ£¬ÓÅÏÈ¼¶Ìá¸ß£¬ÒòÎª¿Í»§´®¿Ú³õÊ¼»¯£¬ÓÐ¿ÉÄÜÒª¶ÁÆä×Ô¼ºµÄÊý¾Ý
	InitAFE1();
	InitCan();
	InitADC();
	InitSci();

#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif
	// Init_ChargerLoad_Det();

	InitMosRelay_DOx();
	InitData_SOC(); // ±ØÐë·ÅÔÚ¶ÁÍêeepromÊý¾ÝºóÃæ

	InitTimer();
	log_w("init over");

	// EnableLowPowerDebug();
	DBGMCU_Config(DBGMCU_STOP, ENABLE);

#ifdef wdog_enable
	Init_IWDG();
#endif // !1

#endif
}

void InitVar(void)
{
	// SystemMonitorResetData_EEPROM();							//Õâ¸öº¯ÊýµÄ³õÊ¼»¯Ä¬ÈÏÐèÇó¹¦ÄÜÐÞ¸ÄÁË£¬ÒªÐÞ¸ÄEEPROMµÄÉÏµç±êÖ¾Î»
	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemStatus.bits.b1StartUpBMS = 0; // È¥µô¿ª»úÊ±Ðò
	SystemStatus.bits.b1Status_ToSleep = 1;

	SystemStatus.bits.b4Status_ProjectVer = 1;
	// LogRecord_Flag.bits.Log_StartUp = 1;
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
