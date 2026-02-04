#include "main.h"
#include "sh3673520.h"
#include "sh3673520_port_softspi_stm32f1.h"

UINT8 SeriesNum = 16;

const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 1串
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 2串
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

#define _DEBUG_CODE

int main(void)
{
	InitDevice(); // 初始化外设
	InitVar();	  // 初始化变量
	while (1)
	{
#if (defined _DEBUG_CODE)
		App_SysTime();
		App_SOC();
		App_AFEGet();
		App_WarnCtrl();
		App_Sci();
		App_E2promDeal();
		App_SleepDeal(); // 关闭这个功能的话，在InitVar()中System_OnOFF_Func相关置零，或者直接屏蔽
#else
		App_SysTime();
		App_AFEGet();

		App_Sci();
		App_AnlogCal();
		App_E2promDeal();
		App_CellBalance();
#ifdef __FUNC__CAN__
		App_Can();
#endif // __FUNC__CAN__
		App_SleepDeal(); // 关闭这个功能的话，在InitVar()中System_OnOFF_Func相关置零，或者直接屏蔽
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

#endif
	}
}

static void delay_us(uint32_t us)
{
	/* Replace with your SysTick/DWT delay if you have one */
	while (us--)
	{
		for (volatile int i = 0; i < 24; i++)
			__NOP();
	}
}
sh3673520_t afe;
sh3673520_softspi_t soft = {
	.cs_port = GPIO_CS_SPI,
	.cs_pin = PIN_CS_SPI,
	.sck_port = GPIO_SCLK_SPI,
	.sck_pin = PIN_SCLK_SPI,
	.miso_port = GPIO_MISO_SPI,
	.miso_pin = PIN_MISO_SPI,
	.mosi_port = GPIO_MOSI_SPI,
	.mosi_pin = PIN_MOSI_SPI,
	.half_period_nops = 40,
	.delay_us_cb = delay_us,
};
sh3673520_spi_t spi;
void spi_init(void)
{

	sh3673520_softspi_init(&soft);

	sh3673520_spi_init(&spi, sh3673520_softspi_make_port(&soft));

	sh3673520_init(&afe, spi);
}
void InitDevice(void)
{
	SystemInit(); // HSE默认倍频到72MHz，如果没HSE切回HSI怎么处理目前还没了解

#if (defined _DEBUG_CODE)
	InitDelay();
	IsSleepStartUp();
	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	// spi_init();
	InitSci();
	// init_afe();
	InitE2PROM(); // 决定把这个放在前面，优先级提高，因为客户串口初始化，有可能要读其自己的数据

	bsp_InitSPIBus();
	sh36735_spi_sw_init();

	sh36735_read_regs(0x6B, (uint8_t)&g_stCellInfoReport.u16VCell[1], 2);

	InitMosRelay_DOx();

	Registers_AFE1.sonf2.all |= 0x80;
	Registers_AFE1.sonf2.bits.CHGMOS = 1;
	Registers_AFE1.sonf2.bits.DSGMOS = 1;
	sh36735_write_reg_u8(AFE_SCONF2, Registers_AFE1.sonf2.all);
	Registers_AFE1.sonf4 = SNum;
	sh36735_write_reg_u8(AFE_SCONF4, Registers_AFE1.sonf2.all);

	InitTimer();
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
	InitE2PROM(); // 决定把这个放在前面，优先级提高，因为客户串口初始化，有可能要读其自己的数据
	InitAFE1();
#ifdef __FUNC__CAN__
	InitCan();
#endif // __FUNC__CAN__
	InitADC();
	InitSci();

#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif

	InitMosRelay_DOx();
	InitData_SOC(); // 必须放在读完eeprom数据后面

#ifdef wdog_enable
	Init_IWDG();
#endif // !1
	InitTimer();
	log_w("init over");

#endif
	sh36735_write_reg_u8(AFE_SCONF1, 0);
}

void InitVar(void)
{
	// SystemMonitorResetData_EEPROM();							//这个函数的初始化默认需求功能修改了，要修改EEPROM的上电标志位
	InitSystemMonitorData_EEPROM();
	SeriesNum = OtherElement.u16Sys_SeriesNum;
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemStatus.bits.b1StartUpBMS = 0; // 去掉开机时序
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
