/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "Interrupt.h"

BOOL bTimer5msFlg = 0;
BOOL bTimer70msFlg = 0;
BOOL bTimer1sFlg = 0;

U8	ucTimer70ms = 0;
U8	ucTimer1s = 0;

/*************************************************************************************************
* 函数名: InterruptTimer3
* 参  数: 无
* 返回值: 无
* 描  述: 5ms定时中断
*************************************************************************************************/
void InterruptTimer3(void)
{
	bTimer5msFlg = 1;						// 5ms标志为预留标志
	
	if(++ucTimer70ms >= TIME_5MS_70MS)
	{
		ucTimer70ms = 0;
		bTimer70msFlg = 1;
//		HAL_GPIO_TogglePin(GPIOB,GPIO_PIN_15);		// Debug
	}
	
	if(++ucTimer1s >= TIME_5MS_1S)
	{
		ucTimer1s = 0;
		bTimer1sFlg = 1;
	}
}




