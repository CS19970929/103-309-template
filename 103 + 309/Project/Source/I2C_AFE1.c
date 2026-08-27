#include "main.h"

AFEDATA Registers_AFE1;
struct SH367309_Read SH367309_Read_AFE1;

#define LENGTH_TBLTEMP_AFE_10K ((UINT16)56)
#define TWI_READ_MAX_LENGTH ((UINT8)42U)
static const UINT16 iSheldTemp_10K_AFE[LENGTH_TBLTEMP_AFE_10K] = {
	// AD(kΩ*100)		(Temp+40)*10
	11611,
	100, //-30
	8935,
	150, //-25
	6943,
	200, //-20
	5442,
	250, //-15
	4300,
	300, //-10
	3422,
	350, //-5
	2751,
	400, // 0
	2214,
	450, // 5
	1801,
	500, // 10
	1470,
	550, // 15
	1209,
	600, // 20
	1000,
	650, // 25
	831,
	700, // 30
	694,
	750, // 35
	583,
	800, // 40
	492,
	850, // 45
	416,
	900, // 50
	355,
	950, // 55
	303,
	1000, // 60
	260,
	1050, // 65
	224,
	1100, // 70
	193,
	1150, // 75
	167,
	1200, // 80
	146,
	1250, // 85
	127,
	1300, // 90
	111,
	1350, // 95
	98,
	1400, // 100
	86,
	1450, // 105
};

/* CRC-8, polynomial 0x07. A 16-entry nibble table is byte-for-byte
 * equivalent to the previous 256-entry table while saving 240 bytes of ROM. */
static const UINT8 s_u8Crc8NibbleTable[16] = {
	0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
	0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D};

static UINT8 CRC8Update(UINT8 crc8, UINT8 data)
{
	crc8 ^= data;
	crc8 = (UINT8)((UINT8)(crc8 << 4) ^ s_u8Crc8NibbleTable[crc8 >> 4]);
	crc8 = (UINT8)((UINT8)(crc8 << 4) ^ s_u8Crc8NibbleTable[crc8 >> 4]);
	return crc8;
}

/*******************************************************************************
Function: Delay4us()
Description: optimization-independent TWI timing delay
Input:
Output:
Others:
*******************************************************************************/
void Delay4us(void)
{
	/* SysTick is used only as a polling delay source in this project; TIM3 owns
	 * the scheduler tick. Runtime_Boot() and STOP wake recovery call InitDelay()
	 * before AFE transactions, so this delay does not depend on instruction
	 * count and remains valid under ARMCC5 -O2. */
	__delay_us(4U);
}

#ifdef _TWI_COM
/*******************************************************************************
Function: CRC8cal()
Description: calculate CRC-8 (poly 0x07) using two 4-bit table steps per byte
Input:       *p--data to calculate
             Length--data length
Output: crc8
********************************************************************************/
UINT8 CRC8cal(UINT8 *p, UINT8 Length)
{
	UINT8 crc8 = 0U;

	while (Length-- != 0U)
	{
		crc8 = CRC8Update(crc8, *p++);
	}

	return crc8;
}

void TwiStart(void)
{
	TWI_DAT_HIGH;
	TWI_CLK_HIGH;
	TWI_DAT_OUT;
	TWI_CLK_OUT;
	TWI_DAT_LOW;
	Delay4us();
	TWI_CLK_LOW;
}

void TwiReStart(void)
{
	TWI_DAT_HIGH;
	TWI_CLK_HIGH;
	Delay4us();
	TWI_DAT_LOW;
	Delay4us();
	TWI_CLK_LOW;
}

void TwiStop(void)
{
	TWI_DAT_OUT;
	TWI_DAT_LOW;
	Delay4us();
	TWI_CLK_HIGH;
	Delay4us();
	TWI_DAT_HIGH;
	Delay4us();
	TWI_DAT_IN;
	TWI_CLK_IN;
}

/*******************************************************************************
Function:TwiChkClkRelease()
Description: check clk release
Input:
Output:
********************************************************************************/
UINT8 TwiChkClkRelease(void)
{
	UINT16 TimeoutCnt = 1000; // If Clock is not released within 4ms, is considered overtime
	UINT8 result = 0;

	TWI_CLK_IN;

	while (TimeoutCnt--)
	{
		Feed_IWatchDog;
		Delay4us();
		if (TWI_RD_CLK)
		{
			result = 1;
			break;
		}
	}

	TWI_CLK_HIGH;
	TWI_CLK_OUT;

	return result;
}

/*******************************************************************************
Function: TwiSendData()
Description: i2c send data(one byte)
Input: 		Data: data to send
			ClkFlg: 1 means need to check clk release
Output:	RdData: the data received
********************************************************************************/
UINT8 TwiSendData(UINT8 Data, UINT8 ClkFlg)
{
	UINT8 i;
	UINT8 result = 0;

	// 1. After sending the Start signal, there is no need to detect Clock is released, And sending the first bit
	if (Data & 0x80)
	{
		TWI_DAT_HIGH;
	}
	else
	{
		TWI_DAT_LOW;
	}

	if (ClkFlg == 1)
	{
		Delay4us();
		if (TwiChkClkRelease())
		{
			TWI_CLK_LOW;
		}
		else
		{
			return result;
		}
	}
	else
	{
		Delay4us();
		TWI_CLK_HIGH;
		Delay4us();
		TWI_CLK_LOW;
	}

	// 2. Send the remaining seven bit
	Data = Data << 1;
	for (i = 0; i < 7; i++)
	{
		if (Data & 0x80)
		{
			TWI_DAT_HIGH;
		}
		else
		{
			TWI_DAT_LOW;
		}
		Data = Data << 1;
		Delay4us();
		TWI_CLK_HIGH;
		Delay4us();
		TWI_CLK_LOW;
	}
	TWI_DAT_IN;
	Delay4us();

	for (i = 0; i < 10; i++)
	{
		if (TWI_RD_DAT == 0)
		{
			result = 1;
			break;
		}
	}
	TWI_CLK_HIGH;

	Delay4us();
	TWI_DAT_LOW;
	TWI_DAT_OUT;
	TWI_CLK_LOW;
	Delay4us();

	return result;
}

/*******************************************************************************
Function: TwiGetData()
Description: i2c get data(one byte)
Input: 		AckFlg: 0 means no need to send ack
Output:	RdData: the data received
********************************************************************************/
UINT8 TwiGetData(UINT8 AckFlg)
{
	UINT8 i, RdData = 0;

	TWI_DAT_IN;
	Delay4us();

	for (i = 0; i < 8; i++)
	{
		TWI_CLK_HIGH;
		Delay4us();
		if (TWI_RD_DAT)
		{
			RdData |= (1 << (7 - i));
		}
		TWI_CLK_LOW;
		Delay4us();
	}

	TWI_DAT_OUT;
	if (AckFlg)
	{
		TWI_DAT_LOW;
	}
	else
	{
		TWI_DAT_HIGH;
	}
	TWI_CLK_HIGH;
	Delay4us();
	TWI_CLK_LOW;
	Delay4us();

	return RdData;
}

/*******************************************************************************
Function: TwiWrite()
Description:  write multi bytes
Input: SlaveID--Slave Address
		  WrAddr--register addr
		  Length--write data length
		  *WrBuf--data buffer
Output: result:1--OK
			   0--Error
********************************************************************************/
UINT8 TwiWrite(UINT8 SlaveID, UINT16 WrAddr, UINT8 Length, UINT8 *WrBuf)
{
	UINT8 i;
	UINT8 result = 0U;
	UINT8 write_crc;

	if ((Length == 0U) || (WrBuf == 0))
	{
		return 0U;
	}

	/* Preserve the legacy wire format: callers transact one MTP byte at a time,
	 * and the CRC covers slave ID, register address and the first data byte. */
	write_crc = 0U;
	write_crc = CRC8Update(write_crc, SlaveID);
	write_crc = CRC8Update(write_crc, (UINT8)WrAddr);
	write_crc = CRC8Update(write_crc, *WrBuf);

	TwiStart();

	if (!TwiSendData(SlaveID, 1))
	{ // Send Slave ID
		goto WrErr;
	}

	if (TwiSendData(WrAddr, 0))
	{ // Send Write Address(Low 8bit)
		result = 1U;
		for (i = 0; i < Length; i++)
		{
			if (TwiSendData(*WrBuf, 0))
			{ // Send Write Data
				WrBuf++;
			}
			else
			{
				result = 0U;
				break;
			}
		}
		if (!TwiSendData(write_crc, 0))
		{ // write CRC
			result = 0U;
		}
	}
WrErr:
	TwiStop();

	return result;
}

/*******************************************************************************
Function: TwiRead()
Description:  read multi bytes
Input: SlaveID--Slave Address
		  RdAddr--register addr
		  Length--read data length
		  *RdBuf--data buffer
Output: result:1--OK
			   0--Error
Others:
********************************************************************************/
UINT8 TwiRead(UINT8 SlaveID, UINT16 RdAddr, UINT8 Length, UINT8 *RdBuf)
{
	UINT8 i;
	UINT8 result = 0U;
	UINT8 rd_crc;
	UINT8 calc_crc;
	UINT8 rd_data;

	if ((Length == 0U) || (Length > TWI_READ_MAX_LENGTH) || (RdBuf == 0))
	{
		return 0U;
	}

	calc_crc = 0U;
	calc_crc = CRC8Update(calc_crc, SlaveID);
	calc_crc = CRC8Update(calc_crc, (UINT8)RdAddr);
	calc_crc = CRC8Update(calc_crc, Length);
	calc_crc = CRC8Update(calc_crc, (UINT8)(SlaveID | 0x01U));

	TwiStart();

	if (!TwiSendData(SlaveID, 1))
	{ // Send Slave ID
		goto RdErr;
	}

	if (!TwiSendData(RdAddr, 0))
	{ // Send Read Address(Low 8bit)
		goto RdErr;
	}

	if (!TwiSendData(Length, 0))
	{
		goto RdErr;
	}

	TwiReStart();

	if (TwiSendData(SlaveID | 0x1, 0))
	{ // Send Slave ID
		for (i = 0U; i < Length; ++i)
		{
			rd_data = TwiGetData(1U);
			RdBuf[i] = rd_data;
			calc_crc = CRC8Update(calc_crc, rd_data);
		}

		rd_crc = TwiGetData(0U);
		result = (rd_crc == calc_crc) ? 1U : 0U;
	}

RdErr:
	TwiStop();

	return result;
}

#endif

// RAM类型寄存器，用这个函数写，大于0x40的类型
// Output: result:1--OK，0--Error
// 这个返回值就不改了，和目前体系相反，别的函数返回来就行
UINT8 MTPWrite(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf)
{
	UINT8 result = 1U;
	UINT8 i;
	Feed_IWatchDog;

	if ((Length != 0U) && (WrBuf == 0))
	{
		return 0U;
	}

	for (i = 0; i < Length; i++)
	{
		result = TwiWrite(AFE_ID, WrAddr, 1, WrBuf);
		if (!result)
		{
			Delay1ms(1);
			result = TwiWrite(AFE_ID, WrAddr, 1, WrBuf);
			if (!result)
			{
				break;
			}
		}
		WrAddr++;
		WrBuf++;
		Delay1ms(1);
	}

	if (!result)
	{
		System_ERROR_UserCallback(ERROR_AFE1);
	}

	return result;
}

// 0x00----0x19，写EEPROM寄存器用这个函数
// Output: result:1--OK，0--Error
// 这个返回值就不改了，和目前体系相反，别的函数返回来就行
UINT8 MTPWriteROM(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf)
{
	UINT8 result = 1U;
	UINT8 i;

	if ((Length != 0U) && (WrBuf == 0))
	{
		return 0U;
	}

	for (i = 0; i < Length; i++)
	{
		Feed_IWatchDog;
		result = TwiWrite(AFE_ID, WrAddr, 1, WrBuf);
		if (!result)
		{
			Delay1ms(40);
			result = TwiWrite(AFE_ID, WrAddr, 1, WrBuf);
			if (!result)
			{
				break;
			}
		}
		WrAddr++;
		WrBuf++;
		Delay1ms(40);
	}

	if (!result)
	{
		System_ERROR_UserCallback(ERROR_AFE1);
	}

	return result;
}

// Output: result:1--OK，0--Error
// 这个返回值就不改了，和目前体系相反，别的函数返回来就行
UINT8 MTPRead(UINT8 RdAddr, UINT8 Length, UINT8 *RdBuf)
{
	UINT8 result = 1;
	Feed_IWatchDog;

	if ((Length != 0U) && (RdBuf == 0))
	{
		return 0U;
	}

	result = TwiRead(AFE_ID, RdAddr, Length, RdBuf);
	if (!result)
	{
		result = TwiRead(AFE_ID, RdAddr, Length, RdBuf);
	}

	if (!result)
	{
		System_ERROR_UserCallback(ERROR_AFE1);
	}
	return result;
}

// 0为sleep模式，1为idle模式
void InitAFE1_Sleep(UINT8 mode)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_8 | GPIO_Pin_9); // 输出高

	AFE_IsReady();
	// 因为已经初始化过了，休眠的时候调用，不需要再次初始化参数
	// 这玩意会复位模拟前端，导致MOS开关打开关闭反复
	// SH367309_UpdataAfeConfig();

	if (mode)
	{
		SH367309_Enable_AFE_Wdt_Cadc_Drivers();
	}
}

void initAFE1_IIC(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_8 | GPIO_Pin_9); // 输出高
}

/*******************************************************************************
Function:InitAFE()
Description:  check SH367309 is ready, and initialization MTP Buffer
Input:	NULL
Output: NULL
Others:
*******************************************************************************/
void InitAFE1(void)
{
	UINT8 do_startup_zero;

	do_startup_zero = (AfeCurrent_GetSeq() == 0U) ? 1U : 0U;

	initAFE1_IIC();
	close_ctlc();

	AFE_Reset();
	AFE_IsReady();
	SH367309_UpdataAfeConfig();
	MosStartup_ApplyInitialState();
	if (do_startup_zero != 0U)
	{
		AfeCurrent_StartupZeroCal();
	}
	open_ctlc();
	MosStartup_ApplyInitialState();
}

/*调试心得
1，I2C没有上拉，导致AFE发送数据波形异常，上不到3.3V
2，波形正常后，发现读回来的数据有问题
3，第一时间应该是观察存储数据的数组的实际值和自带上位机读取的值作对比，而不是通过分析波形，再逻辑分析仪解析波形，这样太慢了。
4，果然还是030小端存储导致的数据相反错误
*/

/*
1，&Registers_AFE1--为结构体的首地址，不过该地址类型为结构体类型(实际上也是结构体第一个字节的地址)
2，(UINT8*)&Registers_AFE1--把该结构体类型的地址转化为UINT8类型的地址(实际上数值没变，也是结构体第一个字节的地址，类型变了而已)
3，(UINT8)&Registers_AFE1--把该地址值(32位数据)强制转化为8位数据，例如0x2000 0A17转为0x17
*/
UINT8 UpdateVoltageFromBqMaximo(void)
{
	UINT8 i, result = 0;
	UINT32 u32temp = 0;

	// if(sys_time.enbale_afe_err_test)
	// 	return 1;

	if (MTPRead(MTP_TEMP1, sizeof(Registers_AFE1), (UINT8 *)&Registers_AFE1))
	{ // demo代码返回1为OK，
		for (i = 0; i < SeriesNum; i++)
		{
			SH367309_Read_AFE1.u16VCell[i] = ((UINT32)U16_SwapEndian(Registers_AFE1.Cell[i]) * 5 >> 5); ////Vcell*5/32
		}

		u32temp = ((UINT32)SH367309_Reg_Store.TR_ResRef * U16_SwapEndian(Registers_AFE1.Temp1)) / (32769 - U16_SwapEndian(Registers_AFE1.Temp1));
		UPDNLMT16(u32temp, 65535, 0);
		SH367309_Read_AFE1.u16TempBat[0] = GetEndValue(iSheldTemp_10K_AFE, (UINT16)LENGTH_TBLTEMP_AFE_10K, u32temp);
		u32temp = ((UINT32)SH367309_Reg_Store.TR_ResRef * U16_SwapEndian(Registers_AFE1.Temp2)) / (32769 - U16_SwapEndian(Registers_AFE1.Temp2));
		UPDNLMT16(u32temp, 65535, 0);
		SH367309_Read_AFE1.u16TempBat[1] = GetEndValue(iSheldTemp_10K_AFE, (UINT16)LENGTH_TBLTEMP_AFE_10K, u32temp);
		u32temp = ((UINT32)SH367309_Reg_Store.TR_ResRef * U16_SwapEndian(Registers_AFE1.Temp3)) / (32769 - U16_SwapEndian(Registers_AFE1.Temp3));
		UPDNLMT16(u32temp, 65535, 0);
		SH367309_Read_AFE1.u16TempBat[2] = GetEndValue(iSheldTemp_10K_AFE, (UINT16)LENGTH_TBLTEMP_AFE_10K, u32temp);

		// 电流要不要加滤波1s除以4，demo是这样的，现在先观察一下
		// SH367309_Read_AFE1.i16Current = (UINT16)((UINT32)U16_SwapEndian(Registers_AFE1.Cadc)*200/(21470*RSENSE));		//TODO
		SH367309_Read_AFE1.u16Current = U16_SwapEndian(Registers_AFE1.Cadc);
	}
	else
	{
		result = 1;
	}
	return result;
}
