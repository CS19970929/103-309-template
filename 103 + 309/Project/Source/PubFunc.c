#include "main.h"

/*=================================================================
 * FUNCTION: GetEndValue
 * PURPOSE : 查表
 * INPUT:    UINT16
 *
 * RETURN:   UINT16
 *
 * CALLS:    void
 *
 * CALLED BY:Adc_AnlogCal()
 *
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
 * INPUT:    *t_sPubOPChk：判断正负逻辑及当前判断值、比较值传入 *(t_sPubOPChk->i16ChkCnt)：计时累加器地址
 *           t_sPubOPChk->u16TimeCntB：大值时间判断值 t_sPubOPChk->u16TimeCntS：小值时间判断值
 * RETURN:   0：数值异常 1：处理完成
 *
 * CALLS:    void
 *
 * CALLED BY:
 *
 *=================================================================*/
UINT8 App_PubOPUPChk(SPUBOPUPCHK *t_sPubOPChk)
{
	if ((t_sPubOPChk->u8FlagLogic > 1) || (t_sPubOPChk->u16OPValB < t_sPubOPChk->u16OPValS))
	{
		return (0); // 数值异常返回0
	}

	if (t_sPubOPChk->u8FlagBit == (1 - t_sPubOPChk->u8FlagLogic))
	{
		if (t_sPubOPChk->u16ChkVal >= t_sPubOPChk->u16OPValB) // 当前判断值超过大值
		{
			if ((++(*(t_sPubOPChk->i16ChkCnt))) >= t_sPubOPChk->u16TimeCntB) // 当前判断值超过大值t_sPubOPChk->u16TimeCntB时间置标志位为u8FlagLogic
			{
				(*(t_sPubOPChk->i16ChkCnt)) = 0;
				t_sPubOPChk->u8FlagBit = t_sPubOPChk->u8FlagLogic;
			}
		}
		else // 低于大值则计时器递减
		{
			if ((*(t_sPubOPChk->i16ChkCnt)) > 0)
			{
				(*(t_sPubOPChk->i16ChkCnt))--;
			}
		}
	}
	else
	{
		if (t_sPubOPChk->u16ChkVal <= t_sPubOPChk->u16OPValS) // 当前判断值低于小值
		{
			if ((++(*(t_sPubOPChk->i16ChkCnt))) >= t_sPubOPChk->u16TimeCntS)
			{
				(*(t_sPubOPChk->i16ChkCnt)) = 0;
				t_sPubOPChk->u8FlagBit = 1 - t_sPubOPChk->u8FlagLogic; // 当前判断值低于小值t_sPubOPChk->u16TimeCntS时间置标志位为非u8FlagLogic
			}
		}
		else // 大于小值则计时器递减
		{
			if ((*(t_sPubOPChk->i16ChkCnt)) > 0)
			{
				(*(t_sPubOPChk->i16ChkCnt))--;
			}
		}
	}

	return (1); // 处理完成返回1
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

// 求绝对值
UINT32 ModulusSub(UINT32 Data1, UINT32 Data2)
{
	return (UINT32)(Data1 > Data2 ? Data1 - Data2 : Data2 - Data1);
}

// 以10us为时基，软件延时
// 1ms以内误差 10%以内（而且是偏小。10us大概9.4us）
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

// 软件延时函数
void Delay1ms(UINT8 delaycnt)
{
	UINT8 i, k;
	UINT16 j;

#if 0
	for(i=0; i<delaycnt; i++) {
		for(j=0; j<1670; j++) {
			//system clock = 24MHz
		}
	}
#endif

	for (i = 0; i < delaycnt; i++)
	{
		for (k = 0; k < 9; k++)
		{ // 9倍便是72MHz
			for (j = 0; j < 560; j++)
			{
				// system clock = 8MHz
			}
		}
	}
}

/*******************************************************************************
Function: MemoryCopy()
Description:
Input:	source--源Memory指针
		target---目的Memory指针
		length---需要拷贝的数据长度(Byres)
Output:
Others:
*******************************************************************************/
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

// 能不能写一个函数，顺便把原值改变呢？而不是单单传值
UINT16 *U16_SwapEndian_Adress(UINT16 *target)
{
#if 0
	UINT8* temp1,temp2;
	UINT8 value1;
	
	*temp1 = (UINT8*)target;
	*temp2 = ++(*temp1);
	value1 = **temp1;
	**temp1 = **temp2;
	**temp2 = value1;
#endif
	return target;
}

// 大小端转换函数
UINT16 U16_SwapEndian(UINT16 target)
{
	return (((uint16_t)target & 0xFF00) >> 8) | (((uint16_t)target & 0x00FF) << 8);
}

// 1:有温度断线，0：正常
// 有问题，接好温度线，重启BMS才能消除报错
// 添加自动复原功能，不需要重启
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
	case 0: // 刚开机，不能判断，因为查询AFE函数已经被分割，不能拿到数据，此时判断必为错
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
			*temp_AD = 110; // 定死在-29摄氏度。以防上位机显示NA以为没问题
			System_ERROR_UserCallback(ERROR_TEMP_BREAK);
			su8_Recover_Cnt = 0;
		}
		else
		{
			if (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK))
			{
				if (++su8_Recover_Cnt >= 50)
				{ // 判断50次自动复原，约为200*50=10s
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
#if 1
	/* 禁用 JTAG，PB3、PB4、PA15重定义为普通IO */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE); // 使能PA和PB端口时钟

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);	 // 配置复用时钟
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 启用SW，禁用JTAG，PA15、PB3、PB4可用

#if 0
	GPIO_ResetBits(GPIOB, GPIO_Pin_4);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_3; // 端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;	   // 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	   // IO口速度为50MHz
	GPIO_Init(GPIOB, &GPIO_InitStructure);				   // 根据设定参数初始化

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;		  // 端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // IO口速度为50MHz
	GPIO_Init(GPIOA, &GPIO_InitStructure);			  // 根据设定参数初始化
#endif

#endif
}
