/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "LowPower.h"
#include "Uart2App.h"
#include "AFE.h"
#include "Flash.h"

BOOL bPDFlg = 0;
BOOL bSleepFlg = 0;
U8 ucLowPowerDelayCnt = 0;

/*************************************************************************************************
* 函数名: LowPowerProcess
* 参  数: 无
* 返回值: 无
* 描  述: 上位机控制进入不同模式，MCU始终处于正常工作模式(AFE进入PD，MCU掉电)
*************************************************************************************************/
void LowPowerProcess(void)
{
	if(!bMcuFlashWrWaitFlg && (ucCmdAFEMode != 0) )
	{
		if(ucLowPowerDelayCnt++ >= LOWPOWER_DELAY_CNT)
		{
			switch(ucCmdAFEMode)
			{
				case 0x01:					// 上位机配置进入Normal模式，仅在Sleep和IDLE模式有效
					SH_AFE_WorkModeConfig(AFE_Mode_Normal);
					break;
				
				case 0x02:					// 上位机配置进入IDLE模式，仅在Normal模式有效
					SH_AFE_WorkModeConfig(AFE_Mode_Idle);
					break;
				
				case 0x03:					// 上位机配置进入Sleep模式，仅在Normal和IDLE模式有效
					SH_AFE_WorkModeConfig(AFE_Mode_Sleep);
					break;
				
				case 0x04:					// 上位机配置进入PD模式，仅在Normal、Sleep和IDLE模式有效
					SH_AFE_WorkModeConfig(AFE_Mode_PowerDown);			// AFE进入PD后，MCU掉电
					break;
				
				default:
					break;
			}
			ucCmdAFEMode = 0;
		}
	}
	else
	{
		ucLowPowerDelayCnt = 0;
	}
}

