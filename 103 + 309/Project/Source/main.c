#include "main.h"

UINT8 SeriesNum = 16;

#define AFE3520_SERIES_MIN         ((UINT8)4u)
#define AFE3520_SERIES_MAX         ((UINT8)20u)
#define AFE3520_SCONF6_PROTECT_EN  ((UINT8)0x7fu)
#define AFE3520_READBACK_MASK_ALL  ((UINT8)0xffu)
#define AFE3520_SCONF2_CHECK_MASK  ((UINT8)0x7fu)

const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 1�?
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 2�?
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
static UINT8 AFE3520_WriteRegChecked(UINT8 reg, UINT8 val);
static UINT8 AFE3520_WriteRegMaskedChecked(UINT8 reg, UINT8 val, UINT8 mask);
static UINT8 AFE3520_ReadbackRegChecked(UINT8 reg, UINT8 val, UINT8 mask);
static UINT8 InitAFE3520_Registers(UINT8 chgmos, UINT8 dsgmos);
static UINT8 AFE3520_NormalizeSeriesNum(UINT16 series);
static void AFE3520_SyncSeriesNum(UINT16 series);

int main(void)
{
	InitDevice(); // 初始化外�?
	InitVar();	  // 初始化变�?
	InitAFE3520_Registers(0, 0);
	while (1)
	{
		App_SysTime();
		App_AFEGet();
		App_WarnCtrl();

		App_Sci();
		// App_AnlogCal();
		App_E2promDeal();
		App_CellBalance();
#ifdef __FUNC__CAN__
		App_Can();
#endif // __FUNC__CAN__
		App_SleepDeal(); // 关闭这个功能的话，在InitVar()中System_OnOFF_Func相关置零，或者直接屏�?
		// sleep();
		App_SOC();

#ifdef __FUNC__HEAT__
		App_Heat_Cool_Ctrl();
#endif
		App_FlashUpdate();
		App_LogRecord();
		App_ProID_Deal();

#ifdef wdog_enable
		Feed_IWatchDog;
#endif

	}
}


static UINT8 AFE3520_WriteRegChecked(UINT8 reg, UINT8 val)
{
	return AFE3520_WriteRegMaskedChecked(reg, val, AFE3520_READBACK_MASK_ALL);
}

static UINT8 AFE3520_WriteRegMaskedChecked(UINT8 reg, UINT8 val, UINT8 mask)
{
	if (!sh36735_write_reg_u8(reg, val))
	{
		return 1;
	}

	return AFE3520_ReadbackRegChecked(reg, val, mask);
}

static UINT8 AFE3520_ReadbackRegChecked(UINT8 reg, UINT8 val, UINT8 mask)
{
	UINT8 readback = 0;

	if (!sh36735_read_regs(reg, &readback, 1))
	{
		return 1;
	}

	return ((readback & mask) == (val & mask)) ? 0 : 1;
}

static UINT8 AFE3520_NormalizeSeriesNum(UINT16 series)
{
	if ((series >= AFE3520_SERIES_MIN) && (series <= AFE3520_SERIES_MAX))
	{
		return (UINT8)series;
	}

	return (UINT8)SNum;
}

static void AFE3520_SyncSeriesNum(UINT16 series)
{
	SeriesNum = AFE3520_NormalizeSeriesNum(series);
	OtherElement.u16Sys_SeriesNum = SeriesNum;
}

static UINT8 InitAFE3520_Registers(UINT8 chgmos, UINT8 dsgmos)
{
	UINT8 result = 0;
	UINT8 protect_en = AFE3520_SCONF6_PROTECT_EN;
	UINT8 otc;
	UINT8 utc;
	UINT8 otd;
	UINT8 utd;
	UINT16 ov_thd = 4250 / 5;
	UINT16 uv_thd = 2500 / 5;
	float Rt;

	result |= AFE3520_WriteRegChecked(AFE_SCONF1, 0);

	Registers_AFE1.sonf2.all |= 0x80;
	Registers_AFE1.sonf2.bits.PD_EN = 0;
	Registers_AFE1.sonf2.bits.CHGMOS = chgmos;
	Registers_AFE1.sonf2.bits.DSGMOS = dsgmos;
	Registers_AFE1.sonf2.bits.PUMP_EN = 0;
	result |= AFE3520_WriteRegMaskedChecked(AFE_SCONF2, Registers_AFE1.sonf2.all, AFE3520_SCONF2_CHECK_MASK);

	AFE3520_SyncSeriesNum(SeriesNum);
	Registers_AFE1.sonf4 = SeriesNum;
	result |= AFE3520_WriteRegChecked(AFE_SCONF4, Registers_AFE1.sonf4);

	Registers_AFE1.sonf3.bits.CRLD_EN = 0;
	result |= AFE3520_WriteRegChecked(AFE_SCONF3, Registers_AFE1.sonf3.all);
	result |= AFE3520_WriteRegChecked(AFE_SCONF6, protect_en);

	result |= AFE3520_WriteRegChecked(AFE_OVT_OVH, 0x03);
	result |= AFE3520_WriteRegChecked(AFE_OVL, 0x50);
	result |= AFE3520_WriteRegChecked(AFE_OVT_OVH, (UINT8)((ov_thd >> 8) & 0x3));
	result |= AFE3520_WriteRegChecked(AFE_OVL, (UINT8)(ov_thd & 0xff));
	result |= AFE3520_WriteRegChecked(AFE_UVT_UVH, (UINT8)((uv_thd >> 8) & 0x3));
	result |= AFE3520_WriteRegChecked(AFE_UVL, (UINT8)(uv_thd & 0xff));
	result |= AFE3520_WriteRegChecked(AFE_OCD2V_OCD2T, 3);
	result |= AFE3520_WriteRegChecked(AFE_OCCV_OCCT, 7);

	Rt = 3.55f;
	otc = (UINT8)(Rt * 512 / (10 + Rt));
	Rt = 27.513f;
	utc = (UINT8)(Rt * 512 / (10 + Rt));
	Rt = 1.935f;
	otd = (UINT8)(Rt * 512 / (10 + Rt));
	Rt = 116.11f;
	utd = (UINT8)(Rt * 512 / (10 + Rt));
	result |= AFE3520_WriteRegChecked(AFE_REG_OTC, otc);
	result |= AFE3520_WriteRegChecked(AFE_REG_UTC, utc);
	result |= AFE3520_WriteRegChecked(AFE_REG_OTD, otd);
	result |= AFE3520_WriteRegChecked(AFE_REG_UTD, utd);


	if (result != 0)
	{
		System_ERROR_UserCallback(ERROR_SPI);
	}
	else
	{
		System_ERROR_UserCallback(ERROR_REMOVE_SPI);
	}

	return result;
}

void InitDevice(void)
{
	SystemInit(); // HSE默认倍频�?2MHz，如果没HSE切回HSI怎么处理目前还没了解

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
	InitE2PROM(); // 决定把这个放在前面，优先级提高，因为客户串口初始化，有可能要读其自己的数�?
				  // InitAFE1();
#ifdef __FUNC__CAN__
	InitCan();
#endif // __FUNC__CAN__
	// InitADC();
	InitSci();

#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif

	InitMosRelay_DOx();
	InitData_SOC(); // 必须放在读完eeprom数据后面

	bsp_InitSPIBus();
	sh36735_spi_sw_init();

#ifdef wdog_enable
	Init_IWDG();
#endif // !1
	InitTimer();
	log_w("init over");

}

void InitVar(void)
{
	// SystemMonitorResetData_EEPROM();							//这个函数的初始化默认需求功能修改了，要修改EEPROM的上电标志位
	InitSystemMonitorData_EEPROM();
	AFE3520_SyncSeriesNum(OtherElement.u16Sys_SeriesNum);
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemStatus.bits.b1StartUpBMS = 0; // 去掉开机时�?
	SystemStatus.bits.b1Status_ToSleep = 1;

	// SystemStatus.bits.b4Status_ProjectVer = 1;
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
