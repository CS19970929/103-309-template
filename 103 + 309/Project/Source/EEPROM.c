#include "main.h"

UINT32 u32E2P_Pro_VolCur_WriteFlag = 0;
UINT32 u32E2P_Pro_Temp_WriteFlag = 0;
UINT32 u32E2P_Pro_Other_WriteFlag = 0;
UINT32 u32E2P_RTC_Element_WriteFlag = 0;
UINT32 u32E2P_OtherElement1_WriteFlag = 0;
UINT32 u32E2P_HeatCool_WriteFlag = 0;

UINT8 u8E2P_SocTable_WriteFlag = 0;
UINT8 u8E2P_CopperLoss_WriteFlag = 0;
UINT8 u8E2P_KB_WriteFlag = 0;
UINT8 u8E2P_KB_WritePos = 0;
UINT16 g_u16CurrentCaliOffsetValue = 0;

volatile UINT32 u32EepromDirtyMask = 0;

void EEPROM_MarkDirty(EEPROM_DIRTY_BLOCK_E block)
{
	u32EepromDirtyMask |= ((UINT32)1 << block);
}

void EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_E block)
{
	u32EepromDirtyMask &= ~((UINT32)1 << block);
}

UINT32 EEPROM_GetDirtyMask(void)
{
	return u32EepromDirtyMask;
}

void InitData_E2prom(void);

// 产生IIC起始信号
void IIC_Start_SEE(void)
{
	SDA_OUT_SEE(); // sda线输出
	IIC_SDA_SEE = 1;
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SDA_SEE = 0; // START:when CLK is high,DATA change form high to low
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 0; // 钳住I2C总线，准备发送或接收数据
}

// 产生IIC停止信号
void IIC_Stop_SEE(void)
{
	SDA_OUT_SEE(); // sda线输出
	IIC_SCL_SEE = 0;
	IIC_SDA_SEE = 0; // STOP:when CLK is high DATA change form low to high
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SDA_SEE = 1; // 发送I2C总线结束信号
	__delay_us(DELAY_US_IIC_EEPROM);
}

// 等待应答信号到来
// 返回值：1，接收应答失败
//         0，接收应答成功
UINT8 IIC_Wait_Ack_SEE(void)
{
	UINT8 ucErrTime = 0;
	SDA_IN_SEE(); // SDA设置为输入
	// IIC_SDA_SEE=1;__delay_us(4);
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	while (READ_SDA_SEE)
	{
		ucErrTime++;
		if (ucErrTime > 250)
		{
			IIC_Stop_SEE();
			return 1;
		}
	}
	IIC_SCL_SEE = 0; // 时钟输出0
	__delay_us(DELAY_US_IIC_EEPROM);
	return 0;
}

// 产生ACK应答
void IIC_Ack_SEE(void)
{
	IIC_SCL_SEE = 0;
	SDA_OUT_SEE();
	IIC_SDA_SEE = 0;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 0;
}

// 不产生ACK应答
void IIC_NAck_SEE(void)
{
	IIC_SCL_SEE = 0;
	SDA_OUT_SEE();
	IIC_SDA_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 1;
	__delay_us(DELAY_US_IIC_EEPROM);
	IIC_SCL_SEE = 0;
}

// IIC发送一个字节
// 返回从机有无应答
// 1，有应答
// 0，无应答
void IIC_Send_Byte_SEE(UINT8 txd)
{
	UINT8 t;
	SDA_OUT_SEE();
	IIC_SCL_SEE = 0; // 拉低时钟开始数据传输
	for (t = 0; t < 8; t++)
	{
		// IIC_SDA=(txd&0x80)>>7;
		if ((txd & 0x80) >> 7)
			IIC_SDA_SEE = 1;
		else
			IIC_SDA_SEE = 0;
		txd <<= 1;
		__delay_us(DELAY_US_IIC_EEPROM); // 对TEA5767这三个延时都是必须的
		IIC_SCL_SEE = 1;
		__delay_us(DELAY_US_IIC_EEPROM);
		IIC_SCL_SEE = 0;
		__delay_us(DELAY_US_IIC_EEPROM);
	}
}

// 读1个字节，ack=1时，发送ACK，ack=0，发送nACK
UINT8 IIC_Read_Byte_SEE(unsigned char ack)
{
	unsigned char i, receive = 0;
	SDA_IN_SEE(); // SDA设置为输入
	for (i = 0; i < 8; i++)
	{
		IIC_SCL_SEE = 0;
		__delay_us(DELAY_US_IIC_EEPROM);
		IIC_SCL_SEE = 1;
		receive <<= 1;
		if (READ_SDA_SEE)
			receive++;
		__delay_us(DELAY_US_IIC_EEPROM);
	}
	if (!ack)
		IIC_NAck_SEE(); // 发送nACK
	else
		IIC_Ack_SEE(); // 发送ACK
	return receive;
}

// 以下想法失败
UINT8 ReadEEPROM_Byte2(UINT16 addr, UINT8 *data)
{
	IIC_Start_SEE();
	IIC_Send_Byte_SEE(sEEAddress | I2C_RW_W); // 发送写命令
	if (1 == IIC_Wait_Ack_SEE())
	{
		return 1; // 但是返回值的问题，只能在这里加了
	}

#ifndef AT24C02
	IIC_Send_Byte_SEE(addr >> 8); // 发送高地址
	if (1 == IIC_Wait_Ack_SEE())
	{
		return 1;
	}
#endif

	IIC_Send_Byte_SEE(addr % 256); // 发送低地址
	if (1 == IIC_Wait_Ack_SEE())
	{
		return 1;
	}

	IIC_Start_SEE();
	IIC_Send_Byte_SEE(sEEAddress | I2C_RW_R); // 进入接收模式
	if (1 == IIC_Wait_Ack_SEE())
	{
		return 1;
	}

	*data = IIC_Read_Byte_SEE(0);
	IIC_Stop_SEE(); // 产生一个停止条件
	return 0;
}

// 后续维护人员禁止使用这个函数
UINT8 WriteEEPROM_Byte(UINT16 addr, UINT8 val)
{
	UINT8 result = 0;

	Feed_IWatchDog;
	MCUO_E2PR_WP = 0;

	IIC_Start_SEE();
	IIC_Send_Byte_SEE(sEEAddress | I2C_RW_W); // 发送写命令
	if (1 == IIC_Wait_Ack_SEE())
	{
		result = 1;
		goto __exit;
	}

#ifndef AT24C02
	IIC_Send_Byte_SEE(addr >> 8); // 发送高地址
	if (1 == IIC_Wait_Ack_SEE())
	{
		result = 1;
		goto __exit;
	}

#endif

	IIC_Send_Byte_SEE(addr % 256); // 发送低地址
	if (1 == IIC_Wait_Ack_SEE())
	{
		result = 1;
		goto __exit;
	}
	IIC_Send_Byte_SEE(val); // 发送字节
	if (1 == IIC_Wait_Ack_SEE())
	{
		result = 1;
		goto __exit;
	}
	IIC_Stop_SEE(); // 产生一个停止条件
	__delay_ms(5);	// EEPROM特性，需要5ms保证写完

__exit:
	MCUO_E2PR_WP = 1;
	Feed_IWatchDog;
	return result;
}

UINT8 ReadEEPROM_Byte(UINT16 addr)
{
	UINT8 temp = 0;
	Feed_IWatchDog;
	IIC_Start_SEE();
	IIC_Send_Byte_SEE(sEEAddress | I2C_RW_W); // 发送写命令
	if (1 == IIC_Wait_Ack_SEE())
	{
		System_ERROR_UserCallback(ERROR_EEPROM_COM); // 本来打算在最后一层才调用这个函数，这个编程风格
		return 0;									 // 但是返回值的问题，只能在这里加了
	}

#ifndef AT24C02
	IIC_Send_Byte_SEE(addr >> 8); // 发送高地址
	if (1 == IIC_Wait_Ack_SEE())
	{
		System_ERROR_UserCallback(ERROR_EEPROM_COM);
		return 0;
	}

#endif

	IIC_Send_Byte_SEE(addr % 256); // 发送低地址
	if (1 == IIC_Wait_Ack_SEE())
	{
		System_ERROR_UserCallback(ERROR_EEPROM_COM);
		return 0;
	}

	IIC_Start_SEE();
	IIC_Send_Byte_SEE(sEEAddress | I2C_RW_R); // 进入接收模式
	if (1 == IIC_Wait_Ack_SEE())
	{
		System_ERROR_UserCallback(ERROR_EEPROM_COM);
		return 0;
	}

	temp = IIC_Read_Byte_SEE(0);
	IIC_Stop_SEE(); // 产生一个停止条件
	Feed_IWatchDog;
	return temp;
}

UINT16 ReadEEPROM_Word_NoZone(UINT16 addr)
{
	UINT16 tmp16a;
	UINT8 tmp8a, tmp8b;
	tmp8a = ReadEEPROM_Byte(addr);	   // 读取低位地址A对应的数据
	tmp8b = ReadEEPROM_Byte(addr + 1); // 读取高位地址A+1对应的数据
	tmp16a = tmp8b;
	tmp16a = (tmp16a << 8) | tmp8a; // 数据存储

	return tmp16a;
}

// 主要调这个，加了几句话
UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data)
{
	UINT8 tmp8a, tmp8b, WriteCounter = 0, result = 0;
	UINT16 tmp_addr, tmp16;
	;

	tmp_addr = addr; // 移植忘了这句话
	WriteCounter = 0;
	do
	{
		result += WriteEEPROM_Byte(tmp_addr, data & 0xff);	 // 数据的低8位写入EEPROM
		result += WriteEEPROM_Byte(tmp_addr + 1, data >> 8); // 数据的高8位写入EEPROM
		tmp8a = ReadEEPROM_Byte(tmp_addr);					 // 获取刚存入EEPROM的低8位数据
		tmp8b = ReadEEPROM_Byte(tmp_addr + 1);				 // 获取刚存入EEPROM的高8位数据
		tmp16 = (tmp8b << 8) | tmp8a;						 // 存储读到的数据于变量tmp16

		WriteCounter++;
		if (WriteCounter > 2 || result != 0)
		{			  /*判断tmp16 != data的计数*/
			++result; // 要跳出来执行写保护置位
			break;
		}
	} while (tmp16 != data);
	return result;
}

/*
=================以下进入第二层应用阶段=================
*/
void ReadEEPROM_ByteData_StartUp(void)
{
	UINT16 i;
	UINT16 t_u16RdTemp;
	INT16 t_i16RdTemp;
	UINT16 t_u16TempMax, t_u16TempMin;

	const struct PRT_E2ROM_PARAS PrtE2paras_Min = E2P_PROTECT_MIN_PRT;
	const struct PRT_E2ROM_PARAS PrtE2paras_Max = E2P_PROTECT_MAX_PRT;
	const struct PRT_E2ROM_PARAS PrtE2paras_Pos = E2P_ADDR_E2POS_PROTECT;

	const struct OTHER_ELEMENT OtherElement_to_Max = OtherElement_max;
	const struct OTHER_ELEMENT OtherElement_to_Min = OtherElement_min;
	const struct OTHER_ELEMENT OtherElement_to_Pos = E2P_ADDR_E2POS_OTHER_ELEMENT1;

	const struct HEAT_COOL_ELEMENT HeatCoolEle_Max = HeatCoolElement_Max;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Min = HeatCoolElement_Min;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Pos = E2P_ADDR_E2POS_HEAT_COOL;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{ // 保护点
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VcellOvp_First + i));
		t_u16TempMax = (*(&PrtE2paras_Max.u16VcellOvp_First + i));
		t_u16TempMin = (*(&PrtE2paras_Min.u16VcellOvp_First + i));
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = t_u16RdTemp;
		if ((t_u16RdTemp >= t_u16TempMin) && (t_u16RdTemp <= t_u16TempMax))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{ // 这样其实不太好，最好的办法是把ReadEEPROM_Word_NoZone()这个函数改造以下
				// g_st_SysStatusFlag.bits.b1EepromErr = 1;		//重新改造了一下这个函数，最后失败告终，不改好过改
				System_ERROR_UserCallback(ERROR_EEPROM_STORE); // 只要确保通讯没问题，就是这个错误。
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_CALIB_K; ++i)
	{ // K值
		t_u16RdTemp = ReadEEPROM_Word_NoZone(E2P_ADDR_START_CALIB_K + (i << 1));
		g_u16CalibCoefK[i] = t_u16RdTemp;
		if ((t_u16RdTemp >= SYSKMIN) && (t_u16RdTemp <= SYSKMAX))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}

		t_i16RdTemp = ReadEEPROM_Word_NoZone(E2P_ADDR_START_CALIB_B + (i << 1));
		g_i16CalibCoefB[i] = t_i16RdTemp; // B值
		if ((t_i16RdTemp >= SYSBMIN) && (t_i16RdTemp <= SYSBMAX))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{ // Other_CanAdd
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&OtherElement_to_Pos.u16Balance_OpenVoltage + i));
		t_u16TempMax = (*(&OtherElement_to_Max.u16Balance_OpenVoltage + i));
		t_u16TempMin = (*(&OtherElement_to_Min.u16Balance_OpenVoltage + i));
		*(&OtherElement.u16Balance_OpenVoltage + i) = t_u16RdTemp;
		if ((t_u16RdTemp >= t_u16TempMin) && (t_u16RdTemp <= t_u16TempMax))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				// g_st_SysStatusFlag.bits.b1EepromErr = 1;
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
	{ // HeatCool_element
		t_u16RdTemp = ReadEEPROM_Word_NoZone((UINT16) * (&HeatCoolEle_Pos.u16Heat_OpenTemp + i));
		t_u16TempMax = (*(&HeatCoolEle_Max.u16Heat_OpenTemp + i));
		t_u16TempMin = (*(&HeatCoolEle_Min.u16Heat_OpenTemp + i));
		*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = t_u16RdTemp;
		if ((t_u16RdTemp >= t_u16TempMin) && (t_u16RdTemp <= t_u16TempMax))
		{
		}
		else
		{
			if (0 == System_ErrFlag.u8ErrFlag_Com_EEPROM)
			{
				// g_st_SysStatusFlag.bits.b1EepromErr = 1;
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			}
		}
	}

	ReadEEPROM_AFE_Parameters();
	ReadEEPROM_EventRecord_Parameters();
}

// Sci命令的数据
void EEPROM_ResetData_AllToDefault(void)
{
	const struct PRT_E2ROM_PARAS PrtE2PARAS_Default = E2P_PROTECT_DEFAULT_PRT;
	const struct OTHER_ELEMENT OtherElement_Default = OtherElement_default;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Default = HeatCoolElement_Default;
	UINT8 i;

	for (i = 0; i < KB_NUM; ++i)
	{
		g_u16CalibCoefK[i] = SYSKDEFAULT;
		g_i16CalibCoefB[i] = SYSBDEFAULT;
	}
	u8E2P_KB_WriteFlag = KB_NUM;
	EEPROM_MarkDirty(EEPROM_DIRTY_BLOCK_CALIB);
	u8E2P_KB_WritePos = 0;

	// Protect
	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = *(&PrtE2PARAS_Default.u16VcellOvp_First + i);
	}
	u32E2P_Pro_VolCur_WriteFlag = E2P_PARA_ALL_VOLCUR_PROTECT;
	EEPROM_MarkDirty(EEPROM_DIRTY_BLOCK_PROTECT);
	u32E2P_Pro_Temp_WriteFlag = E2P_PARA_ALL_TEM_PROTECT;
	EEPROM_MarkDirty(EEPROM_DIRTY_BLOCK_PROTECT);
	u32E2P_Pro_Other_WriteFlag = E2P_PARA_ALL_OTHER_PROTECT;
	EEPROM_MarkDirty(EEPROM_DIRTY_BLOCK_PROTECT);

	// Other_CanAdd_element
	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = *(&OtherElement_Default.u16Balance_OpenVoltage + i);
	}
	u32E2P_OtherElement1_WriteFlag = E2P_PARA_ALL_OTHER_ELEMENT1;
	EEPROM_MarkDirty(EEPROM_DIRTY_BLOCK_OTHER1);

	// HeatCool_element
	for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
	{
		*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = *(&HeatCoolEle_Default.u16Heat_OpenTemp + i);
	}
	u32E2P_HeatCool_WriteFlag = E2P_PARA_ALL_HEAT_COOL_ELE;
	EEPROM_MarkDirty(EEPROM_DIRTY_BLOCK_HEAT_COOL);
}

// 历史保护记录reset
void EEPROM_ResetData_OtherToDefault(void)
{
	EEPROM_ResetData_AFE_ParametersToDefault();
	EEPROM_ResetData_EventRecord_ToDefault();

	SystemMonitorResetData_EEPROM(); // 系统功能选取标志位存储
}

// Sci命令表中，因为STM8的缘故，决定全部从通讯中移出来写
void WriteEEPROM_ByteData_Circle(void)
{
	UINT8 i = 0;
	UINT8 u8temp;
	const struct PRT_E2ROM_PARAS PrtE2paras_Pos = E2P_ADDR_E2POS_PROTECT;
	const struct OTHER_ELEMENT OtherCanAdd_Pos = E2P_ADDR_E2POS_OTHER_ELEMENT1;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Pos = E2P_ADDR_E2POS_HEAT_COOL;

	if (u32EepromDirtyMask & ((UINT32)1 << EEPROM_DIRTY_BLOCK_SYS_FLAG))
	{
		if ((0 == WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT, (UINT16)(System_OnOFF_Func.all & 0x0000FFFF))) &&
			(0 == WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2, (UINT16)(System_OnOFF_Func.all >> 16))))
		{
			EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_SYS_FLAG);
		}
	}
	else if (u32EepromDirtyMask & ((UINT32)1 << EEPROM_DIRTY_BLOCK_OFFSET))
	{
		if (0 == WriteEEPROM_Word_NoZone(FLASH_ADDR_SH367309_VALUE, g_u16CurrentCaliOffsetValue))
		{
			EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_OFFSET);
		}
	}
	if (u8E2P_KB_WriteFlag)
	{ // 完整KB值写入可以逐对分段完成
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (u8E2P_KB_WritePos << 1)), g_u16CalibCoefK[u8E2P_KB_WritePos]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (u8E2P_KB_WritePos << 1)), g_i16CalibCoefB[u8E2P_KB_WritePos]);
		++u8E2P_KB_WritePos;
		--u8E2P_KB_WriteFlag;
		if (0 == u8E2P_KB_WriteFlag)
		{
			EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_CALIB);
		}
	}
	else if (u32E2P_Pro_VolCur_WriteFlag & E2P_PARA_ALL_VOLCUR_PROTECT)
	{
		while (i < E2P_PARA_ALL_VOLCUR_PROTECT)
		{
			if ((u32E2P_Pro_VolCur_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VcellOvp_First + i),
										  *(&PRT_E2ROMParas.u16VcellOvp_First + i));
				u32E2P_Pro_VolCur_WriteFlag -= ((long)1 << i); // 按位操作，有一个减一个。
				if ((0 == u32E2P_Pro_VolCur_WriteFlag) && (0 == u32E2P_Pro_Temp_WriteFlag) && (0 == u32E2P_Pro_Other_WriteFlag))
				{
					EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_PROTECT);
				}
				break;
			}
			i++;
		}
	}
	else if (u32E2P_Pro_Temp_WriteFlag & E2P_PARA_ALL_TEM_PROTECT)
	{
		while (i < E2P_PARA_ALL_TEM_PROTECT)
		{
			if ((u32E2P_Pro_Temp_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16TChgOTp_First + i),
										  *(&PRT_E2ROMParas.u16TChgOTp_First + i));
				u32E2P_Pro_Temp_WriteFlag -= ((long)1 << i);
				if ((0 == u32E2P_Pro_VolCur_WriteFlag) && (0 == u32E2P_Pro_Temp_WriteFlag) && (0 == u32E2P_Pro_Other_WriteFlag))
				{
					EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_PROTECT);
				}
				break;
			}
			i++;
		}
	}
	else if (u32E2P_Pro_Other_WriteFlag & E2P_PARA_ALL_OTHER_PROTECT)
	{
		while (i < E2P_PARA_ALL_OTHER_PROTECT)
		{
			if ((u32E2P_Pro_Other_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&PrtE2paras_Pos.u16VdeltaOvp_First + i),
										  *(&PRT_E2ROMParas.u16VdeltaOvp_First + i));
				u32E2P_Pro_Other_WriteFlag -= ((long)1 << i);
				if ((0 == u32E2P_Pro_VolCur_WriteFlag) && (0 == u32E2P_Pro_Temp_WriteFlag) && (0 == u32E2P_Pro_Other_WriteFlag))
				{
					EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_PROTECT);
				}
				break;
			}
			i++;
		}
	}
	else if (u32E2P_OtherElement1_WriteFlag & E2P_PARA_ALL_OTHER_ELEMENT1)
	{
		while (i < E2P_PARA_ALL_OTHER_ELEMENT1)
		{
			if ((u32E2P_OtherElement1_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&OtherCanAdd_Pos.u16Balance_OpenVoltage + i),
										  *(&OtherElement.u16Balance_OpenVoltage + i));
				u32E2P_OtherElement1_WriteFlag -= ((long)1 << i);
				if (0 == u32E2P_OtherElement1_WriteFlag)
				{
					EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_OTHER1);
				}
				break;
			}
			i++;
		}
	}
	else if (u32E2P_RTC_Element_WriteFlag)
{
	while (i < E2P_PARA_ALL_RTC_ELEMENT)
	{
		if ((u32E2P_RTC_Element_WriteFlag >> i) & 1)
		{
			WriteEEPROM_Word_NoZone((UINT16) * (&RTC_time.RTC_Time_Year + i), *(&RTC_time.RTC_Time_Year + i));
			u32E2P_RTC_Element_WriteFlag -= ((long)1 << i);
			if (0 == u32E2P_RTC_Element_WriteFlag)
			{
				EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_RTC);
			}
			break;
		}
		i++;
	}
}
else if (u8E2P_SocTable_WriteFlag)
{
	u8temp = E2P_PARA_NUM_SOC_TABLE - u8E2P_SocTable_WriteFlag;
	WriteEEPROM_Word_NoZone(E2P_ADDR_START_SOC_TABLE + (u8temp << 1), SOC_Table_Set[u8temp]);
	u8E2P_SocTable_WriteFlag--;
	if (0 == u8E2P_SocTable_WriteFlag)
	{
		EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_SOC_TABLE);
	}
}
else if (u8E2P_CopperLoss_WriteFlag)
{
	u8temp = E2P_PARA_NUM_COPPERLOSS - u8E2P_CopperLoss_WriteFlag;
	WriteEEPROM_Word_NoZone(E2P_ADDR_START_COPPERLOSS + (u8temp << 1), CopperLoss[u8temp]);
	WriteEEPROM_Word_NoZone(E2P_ADDR_START_COPPERLOSS_NUM + (u8temp << 1), CopperLoss_Num[u8temp]);
	u8E2P_CopperLoss_WriteFlag--;
	if (0 == u8E2P_CopperLoss_WriteFlag)
	{
		EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_COPPERLOSS);
	}
}
	// else if (u8E2P_SocTable_WriteFlag)
	// {
	// 	u8temp = E2P_PARA_NUM_SOC_TABLE - u8E2P_SocTable_WriteFlag;
	// 	WriteEEPROM_Word_NoZone(E2P_ADDR_START_SOC_TABLE + (u8temp << 1), SOC_Table_Set[u8temp]);
	// 	u8E2P_SocTable_WriteFlag--;
	// }
	// else if (u8E2P_CopperLoss_WriteFlag)
	// {
	// 	u8temp = E2P_PARA_NUM_COPPERLOSS - u8E2P_CopperLoss_WriteFlag;
	// 	WriteEEPROM_Word_NoZone(E2P_ADDR_START_COPPERLOSS + (u8temp << 1), CopperLoss[u8temp]);
	// 	WriteEEPROM_Word_NoZone(E2P_ADDR_START_COPPERLOSS_NUM + (u8temp << 1), CopperLoss_Num[u8temp]);
	// 	u8E2P_CopperLoss_WriteFlag--;
	// }
	else if (u32E2P_HeatCool_WriteFlag)
	{
		for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
		{
			if ((u32E2P_HeatCool_WriteFlag >> i) & 1)
			{
				WriteEEPROM_Word_NoZone((UINT16) * (&HeatCoolEle_Pos.u16Heat_OpenTemp + i), *(&Heat_Cool_Element.u16Heat_OpenTemp + i));
				u32E2P_HeatCool_WriteFlag -= ((long)1 << i);
				if (0 == u32E2P_HeatCool_WriteFlag)
				{
					EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_HEAT_COOL);
				}
				break;
			}
		}
	}
	else if (gu8_Reset_EventRecord)
	{
		u8temp = 100 - gu8_Reset_EventRecord;
		WriteEEPROM_Word_NoZone(E2P_ADDR_START_EVENT_RECORD + (u8temp << 1), 0);
		gu8_Reset_EventRecord--;
		if (0 == gu8_Reset_EventRecord)
		{
			EEPROM_ClearDirty(EEPROM_DIRTY_BLOCK_EVENT_RECORD);
		}
		if (gu8_Reset_EventRecord == 1)
		{
			WriteEEPROM_Word_NoZone(E2P_ADDR_E2POS_EVENT_POINT, 0);
		}
	}
}

void InitE2PROM_i2c(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	// PB3_I2C_SCL_eeprom，PB5_I2C_SDA_eeprom
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	// GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);	//PB3为JTAG口

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11); // 输出高

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_13); // 输出高
}

void InitE2PROM(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	// PB3_I2C_SCL_eeprom，PB5_I2C_SDA_eeprom
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	// GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);	//PB3为JTAG口

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11); // 输出高

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_13); // 输出高

	__delay_ms(100);

	InitData_E2prom();
}

void DataLoad_CurrentCali_startup(void)
{
	static UINT8 su8_StartUp_CaliCnt = 0;
	static UINT16 su16_OffsetValue = 0;

	static UINT32 su32_OffsetValue_CHG = 0;
	static UINT32 su32_OffsetValue_DSG = 0;

	log_i("virtual current cali\n");
	SH367309_DriverMos_Ctrl(GPIO_CHG, 0);
	SH367309_DriverMos_Ctrl(GPIO_DSG, 0);

	__delay_ms(1000);
	// step 2
	while (su8_StartUp_CaliCnt < 16)
	{ // 先2s为20*2次，3s为20*3次
		UpdateVoltageFromBqMaximo();

		if ((SH367309_Read_AFE1.u16Current & 0x8000) == 0)
		{
			su32_OffsetValue_CHG += SH367309_Read_AFE1.u16Current;
		}
		else
		{
			su32_OffsetValue_DSG += 0xFFFF - SH367309_Read_AFE1.u16Current + 1;
		}

		if (su32_OffsetValue_CHG >= su32_OffsetValue_DSG)
		{
			su16_OffsetValue = (UINT16)((su32_OffsetValue_CHG - su32_OffsetValue_DSG) >> 4);
		}
		else
		{
			su16_OffsetValue = (UINT16)(0xFFFF - ((su32_OffsetValue_DSG - su32_OffsetValue_CHG) >> 4) + 1);
		}

		++su8_StartUp_CaliCnt;
		__delay_ms(500);
	}
	// step 2
	{
		// normal 休眠/唤醒场景也需要把最新的偏移量写回 EEPROM
		curr_offset = su16_OffsetValue;
		g_u16CurrentCaliOffsetValue = su16_OffsetValue;
		EEPROM_MarkDirty(EEPROM_DIRTY_BLOCK_OFFSET);
		while (u32EepromDirtyMask & ((UINT32)1 << EEPROM_DIRTY_BLOCK_OFFSET))
		{
			WriteEEPROM_ByteData_Circle();
		}
	}
}

uint16_t curr_offset;
UINT16 OffsetValue_CHG = 0;
UINT16 OffsetValue_DSG = 0;
void InitData_E2prom(void)
{
#if 1
	if (EEPROM_VALUE_BEGIN_FLAG == ReadEEPROM_Word_NoZone(EEPROM_ADDR_PASS))
	{ // 第二次上电就会执行这个
		ReadEEPROM_ByteData_StartUp();
		{
			g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;
			curr_offset = ReadEEPROM_Word_NoZone(FLASH_ADDR_SH367309_VALUE);
			if ((curr_offset & 0x8000) == 0)
			{
				OffsetValue_CHG = (UINT32)curr_offset * 200 * g_u32CS_Res_AFE / (21470);
			}
			else
			{
				OffsetValue_DSG = (UINT32)((UINT16)(0xFFFF - curr_offset + 1)) * 200 * g_u32CS_Res_AFE / (21470); // mA
			}
		}
	}
	else
	{ // 第一次上电，用于量产
		EEPROM_ResetData_AllToDefault();
		while (u32EepromDirtyMask || u8E2P_KB_WriteFlag || u32E2P_Pro_VolCur_WriteFlag || u32E2P_Pro_Temp_WriteFlag || u32E2P_Pro_Other_WriteFlag || u8E2P_SocTable_WriteFlag || u8E2P_CopperLoss_WriteFlag || u32E2P_RTC_Element_WriteFlag || u32E2P_OtherElement1_WriteFlag || u32E2P_HeatCool_WriteFlag)
		{ // 0x2000,0x2100,0x2200,0x2300
			WriteEEPROM_ByteData_Circle();
		}
		EEPROM_ResetData_OtherToDefault(); // 把E2P_BEGIN_FLAG写进头地址，
										   // 如果有别的添加，可以往这个函数写，目前加了保护记录初始化
		WriteProID_Default();
		{
			bool ret = false;
			do
			{
				initAFE1_IIC();
				AFE_IsReady();
				AFE_PARAM_WRITE_Flag = 1;
				// ret = fac_sh367309_param_init_first_powerup();
				ret = SH367309_UpdataAfeConfig();
			} while (ret == false);
			DataLoad_CurrentCali_startup();
		}
		soc_factory_param_init_first();

		WriteEEPROM_Word_NoZone(EEPROM_ADDR_PASS, EEPROM_VALUE_BEGIN_FLAG); // 第一次上电初始化完成

		MCU_RESET();
	}
#endif
}

void App_E2promDeal(void)
{
	if (u32EepromDirtyMask || u8E2P_KB_WriteFlag || u32E2P_Pro_VolCur_WriteFlag || u32E2P_Pro_Temp_WriteFlag || u32E2P_Pro_Other_WriteFlag || u8E2P_SocTable_WriteFlag || u8E2P_CopperLoss_WriteFlag || u32E2P_RTC_Element_WriteFlag || u32E2P_OtherElement1_WriteFlag || u32E2P_HeatCool_WriteFlag || gu8_Reset_EventRecord)
	{ // 0x2000,0x2100,0x2200,0x2300
		WriteEEPROM_ByteData_Circle();
	}
}









