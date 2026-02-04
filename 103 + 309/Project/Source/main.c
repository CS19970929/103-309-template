#include "main.h"
#include "sh3673520.h"
#include "sh3673520_port_softspi_stm32f1.h"

UINT8 SeriesNum = 16;

#if 0
void init_afe(void)
{
	// sh36735_spi_sw_init();
	// 1) MCU 时钟初始化略…

    // 2) 初始化 SPI：模式3，建议 <= 1MHz（更稳）
    sh36735_spi_hw_init(72000000, 500000);

    // 3) 等待 AFE WarmUp（建议 20~50ms）
    sh_delay_us(50000);

    // 4) 先尝试软件复位，确认 ACK=0xA5（不通就先别写配置）
    if (!sh36735_sw_reset()) {
        // TODO: 打印错误，检查硬件连线 / SPI mode / CS
        // while (1) {}
    }
    sh_delay_us(50000);

    // 5) 读一段寄存器验证 SPI 通了（读 0x40 起 9 字节）
    // uint8_t r[9];
    // if (sh36735_read_regs(0x40, r, sizeof(r))) {
    //     // log_hex("REG40..", r, sizeof(r));
    // } else {
    //     while (1) {}
    // }

    // 6) 写配置（先跑通：不开温度保护，开 OV/UV/OCD/SC + pump + chg/dsg mos）
    sh36735_cfg_t cfg;
    sh36735_cfg_default_ternary_20s(&cfg);
    if (!sh36735_apply_cfg(&cfg)) {
        // while (1) {}
    }
}
#endif

#if 0
int main_3520_test(void)
{
    // 7) 主循环：周期性读取状态/电压（你按你的寄存器表补齐地址）
    while (1) {
        // 示例：读 BSTATUS1/BSTATUS2/FLAG1…（地址请按你的 PDF 校对）
        uint8_t st[3];
        if (sh36735_read_regs(0x5A, st, 3)) {
            log_hex("STATUS", st, 3);
        }
        sh_delay_us(100000);
    }
}
#endif


// 不同串数维护的表格
// 中颖
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
    while (us--) {
        for (volatile int i = 0; i < 24; i++) __NOP();
    }
}
sh3673520_t afe;
 sh3673520_softspi_t soft = {
        .cs_port = GPIO_CS_SPI, 	.cs_pin = PIN_CS_SPI,
        .sck_port = GPIO_SCLK_SPI,  .sck_pin = PIN_SCLK_SPI,
        .miso_port = GPIO_MISO_SPI, .miso_pin = PIN_MISO_SPI,
        .mosi_port = GPIO_MOSI_SPI, .mosi_pin = PIN_MOSI_SPI,
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
	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	// spi_init();
	InitSci();
	// init_afe();
	InitE2PROM(); // 决定把这个放在前面，优先级提高，因为客户串口初始化，有可能要读其自己的数据
	
	bsp_InitSPIBus();
	sh36735_spi_sw_init();
	// sh36735_write_reg_u8(0x43, 19);
	sh36735_read_regs(0x6B, (uint8_t)&g_stCellInfoReport.u16VCell[1], 2);

	// sh36735_write_reg_u8(0x41, 0x03);
	// sh36735_write_reg_u8(0x43, 20);
	// sh36735_write_reg_u8(0x45, 0);
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
