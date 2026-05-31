#include "main.h"

/* ═══════════════════════════════════════════════════════════════
 * PubFunc.c merged with ShortFunc.c → utils.c
 * ═══════════════════════════════════════════════════════════════ */

/*=================================================================
 * FUNCTION: GetEndValue
 * PURPOSE : 查表
 *=================================================================*/
UINT16 GetEndValue(const UINT16 *ptbl, UINT16 tblsize, UINT16 dat)
{
	UINT16 i, t_linenum;
	UINT32 x1 = 0, y1 = 0, x2 = 1, y2 = 1;
	const UINT16 *p;
	UINT16 t_tmp16a, t_tmp16b;
	INT32 t_tmp32a, t_tmp32b;
	UINT32 k, b;
	INT32 ret;
	p = ptbl;

	t_linenum = tblsize - 1;
	for (i = 0; i < tblsize - 2; i = i + 2)
	{
		t_tmp16a = p[i];
		t_tmp16b = p[i + 2];

		if (((dat >= t_tmp16a) && (dat <= t_tmp16b)) || ((dat <= t_tmp16a) && (dat >= t_tmp16b)))
		{
			x1 = t_tmp16a;
			x2 = t_tmp16b;
			y1 = p[i + 1];
			y2 = p[i + 3];
			break;
		}
	}

	if (i >= t_linenum - 1)
	{
		p = ptbl;
		t_tmp16a = p[0];
		t_tmp16b = p[tblsize - 2];

		if (t_tmp16a <= t_tmp16b)
		{
			if (dat >= t_tmp16b)
			{
				t_tmp16a = p[tblsize - 1];
			}
			else
			{
				t_tmp16a = p[1];
			}
		}
		else
		{
			if (dat >= t_tmp16a)
			{
				t_tmp16a = p[1];
			}
			else
			{
				t_tmp16a = p[tblsize - 1];
			}
		}
		return t_tmp16a;
	}
	else
	{
		if (x2 < x1)
		{
			ret = x2;
			x2 = x1;
			x1 = ret;
			ret = y2;
			y2 = y1;
			y1 = ret;
		}

		if (y2 >= y1)
		{
			t_tmp32a = y1 * x2;
			t_tmp32b = y2 * x1;
			ret = dat;
			k = y2 - y1;
			ret = ret * k;
			if (t_tmp32a >= t_tmp32b)
			{
				b = t_tmp32a - t_tmp32b;
				ret = ret + b;
			}
			else
			{
				b = t_tmp32b - t_tmp32a;
				ret = ret - b;
			}
			ret = ret / (x2 - x1);
		}
		else
		{
			t_tmp32a = y1 * x2;
			t_tmp32b = y2 * x1;
			ret = dat;
			k = y1 - y2;
			ret = ret * k;
			b = t_tmp32a - t_tmp32b;
			ret = b - ret;
			ret = ret / (x2 - x1);
		}
		return (ret & 0xffff);
	}
}

/*=================================================================
 * FUNCTION: App_PubOPUPChk
 * PURPOSE : 过欠判断处理
 *=================================================================*/
UINT8 App_PubOPUPChk(SPUBOPUPCHK *t_sPubOPChk)
{
	if ((t_sPubOPChk->u8FlagLogic > 1) || (t_sPubOPChk->u16OPValB < t_sPubOPChk->u16OPValS))
	{
		return (0);
	}

	if (t_sPubOPChk->u8FlagBit == (1 - t_sPubOPChk->u8FlagLogic))
	{
		if (t_sPubOPChk->u16ChkVal >= t_sPubOPChk->u16OPValB)
		{
			if ((++(*(t_sPubOPChk->i16ChkCnt))) >= t_sPubOPChk->u16TimeCntB)
			{
				(*(t_sPubOPChk->i16ChkCnt)) = 0;
				t_sPubOPChk->u8FlagBit = t_sPubOPChk->u8FlagLogic;
			}
		}
		else
		{
			if ((*(t_sPubOPChk->i16ChkCnt)) > 0)
			{
				(*(t_sPubOPChk->i16ChkCnt))--;
			}
		}
	}
	else
	{
		if (t_sPubOPChk->u16ChkVal <= t_sPubOPChk->u16OPValS)
		{
			if ((++(*(t_sPubOPChk->i16ChkCnt))) >= t_sPubOPChk->u16TimeCntS)
			{
				(*(t_sPubOPChk->i16ChkCnt)) = 0;
				t_sPubOPChk->u8FlagBit = 1 - t_sPubOPChk->u8FlagLogic;
			}
		}
		else
		{
			if ((*(t_sPubOPChk->i16ChkCnt)) > 0)
			{
				(*(t_sPubOPChk->i16ChkCnt))--;
			}
		}
	}

	return (1);
}

UINT16 Sci_CRC16RTU(UINT8 *pszBuf, UINT8 unLength)
{
	UINT16 CRCC = 0XFFFF;
	UINT32 CRC_count;

	for (CRC_count = 0; CRC_count < unLength; CRC_count++)
	{
		int i;

		CRCC = CRCC ^ *(pszBuf + CRC_count);

		for (i = 0; i < 8; i++)
		{
			if (CRCC & 1)
			{
				CRCC >>= 1;
				CRCC ^= 0xA001;
			}
			else
			{
				CRCC >>= 1;
			}
		}
	}

	return CRCC;
}

unsigned char CRC8(unsigned char *ptr, unsigned char len, unsigned char key)
{
	unsigned char i;
	unsigned char crc = 0;
	while (len-- != 0)
	{
		for (i = 0x80; i != 0; i /= 2)
		{
			if ((crc & 0x80) != 0)
			{
				crc *= 2;
				crc ^= key;
			}
			else
				crc *= 2;

			if ((*ptr & i) != 0)
				crc ^= key;
		}
		ptr++;
	}
	return (crc);
}

UINT32 ModulusSub(UINT32 Data1, UINT32 Data2)
{
	return (UINT32)(Data1 > Data2 ? Data1 - Data2 : Data2 - Data1);
}

void Delay_Base10us(int n)
{
	unsigned char a, b;
	while (n--)
	{
		for (b = 3; b > 0; b--)
			for (a = 22; a > 0; a--)
				;
	}
}

void Delay1ms(UINT8 delaycnt)
{
	UINT8 i, k;
	UINT16 j;

	for (i = 0; i < delaycnt; i++)
	{
		for (k = 0; k < 9; k++)
		{
			for (j = 0; j < 560; j++)
			{
			}
		}
	}
}

void MemoryCopy(UINT8 *source, UINT8 *target, UINT8 length)
{
	UINT8 i;
	for (i = 0; i < length; i++)
	{
		*target = *source;
		target++;
		source++;
	}
}

UINT16 *U16_SwapEndian_Adress(UINT16 *target)
{
	return target;
}

UINT16 U16_SwapEndian(UINT16 target)
{
	return (((uint16_t)target & 0xFF00) >> 8) | (((uint16_t)target & 0x00FF) << 8);
}

UINT8 Monitor_TempBreak(UINT16 *temp_AD)
{
	static UINT8 su8_Recover_Cnt = 0;
	static UINT8 su8_StartUp_Flag = 0;
	static UINT8 su8_Delay_Cnt = 0;
	UINT8 result = 0;

	if (sys_time.wakeup_rtc)
	{
		sys_time.wakeup_rtc = false;

		su8_StartUp_Flag = 0;
		su8_Delay_Cnt = 0;
	}

	switch (su8_StartUp_Flag)
	{
	case 0:
		if (++su8_Delay_Cnt >= 20)
		{
			su8_Delay_Cnt = 0;
			su8_StartUp_Flag = 1;
		}
		break;

	case 1:
		if (*temp_AD < 110)
		{
			++result;
			*temp_AD = 110;
			System_ERROR_UserCallback(ERROR_TEMP_BREAK);
			su8_Recover_Cnt = 0;
		}
		else
		{
			if (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK))
			{
				if (++su8_Recover_Cnt >= 50)
				{
					su8_Recover_Cnt = 0;
					System_ERROR_UserCallback(ERROR_REMOVE_TEMP_BREAK);
				}
			}
		}
		break;

	default:
		su8_StartUp_Flag = 0;
		break;
	}

	return result;
}

void jtag_disableAndConfIO(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
}

/* ═══════════════════════════════════════════════════════════════
 * ShortFunc.c — short circuit parameter init (SH367309 path)
 * ═══════════════════════════════════════════════════════════════ */

#if (AFE_TYPE == sh36xx)

extern const UINT16 g_u16ShAfeScvTable[16];
extern const UINT16 g_u16ShAfeSctTable[16];

void InitShortCur(void)
{
	UINT16 temp = 0;
	extern AFE_ROM_PARAMETERS_TypeDef AFE_ROM_PARAMETERS_Struction;

	OtherElement.u16CS_Cur_CHGmax = 2000 * OtherElement.u16Sys_CS_Res_Num / OtherElement.u16Sys_CS_Res;
	OtherElement.u16CS_Cur_DSGmax = 2000 * OtherElement.u16Sys_CS_Res_Num / OtherElement.u16Sys_CS_Res;

	/* 短路延时 */
	temp = Choose_Right_Value(OtherElement.u16CBC_DelayT / 10, g_u16ShAfeSctTable);
	AFE_ROM_PARAMETERS_Struction.m0EH_0FH.SCT = temp;
	OtherElement.u16CBC_DelayT = g_u16ShAfeSctTable[temp] * 10;

	/* 短路电压 */
	temp = OtherElement.u16CBC_Cur_DSG / 10;
	temp = temp * 1000 / g_u32CS_Res_AFE;
	AFE_ROM_PARAMETERS_Struction.m0EH_0FH.SCV = Choose_Right_Value(temp, g_u16ShAfeScvTable);

	OtherElement.u16CBC_Cur_DSG = g_u16ShAfeScvTable[AFE_ROM_PARAMETERS_Struction.m0EH_0FH.SCV] * g_u32CS_Res_AFE / 1000;
	OtherElement.u16CBC_Cur_DSG *= 10;
}

#else
#error "Only sh36xx AFE type is supported"
#endif
