/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "Calibrate.h"
#include "Calculate.h"
#include "InitialApp.h"
#include "Flash.h"

BOOL bCaliFlg = 0;              // 上位机发送校准命令后置位该标志
U32	uiExtVPack;					// 总压校准输入电压值
S32	siExtCur;					// 电流校准输入电流值
U32	uiExtTemp[4];				// 温度校准输入温度值
U8	ucExtcaliSwitch1 = 0;

/*************************************************************************************************
* 函数名: CaliVoltage
* 参  数: 无
* 返回值: 无
* 描  述: 校准总电压，更新E2usVPackGain增益
*************************************************************************************************/
void CaliVoltage(void)
{
	U8 i;
	U32 VPackTemp = 0;
	
	for(i=0; i<Info.ucCellNum; i++)
	{
		VPackTemp += AFE.ssCell[i];
	}
	parameter.E2usVPackGain1 = (U32)VPackTemp*CALIPACKVOL1/uiExtVPack;		// 计算总压
	
	parameter.E2usVPackGain2 = (U32)AFE.usVTOP*CALIPACKVOL2/uiExtVPack;		// B+总压
}

/*************************************************************************************************
* 函数名: CaliCurrentGain
* 参  数: 无
* 返回值: 无
* 描  述: 校准电流增益E2siCadcGain，连续两次采集的电流求平均后再校准更新
*************************************************************************************************/
void CaliCurrentGain(void)
{
	S16 Tempe;

	Tempe = (S32)CALICUR*(AFE.ssVADCCurr-parameter.E2ssCurrOffset1)/siExtCur;		    
	if(Tempe != 0)
	{
		parameter.E2ssCurrGain1 = Tempe;
	}
	Tempe = (S32)CALICUR*(AFE.ssCADCCurr-parameter.E2ssCurrOffset2)/siExtCur;		    
	if(Tempe != 0)
	{
		parameter.E2ssCurrGain2 = Tempe;
	}
	
}

/*************************************************************************************************
* 函数名: CaliCurZero
* 参  数: 无
* 返回值: 无
* 描  述: 校准零电流
*************************************************************************************************/
void CaliCurZero(void)
{
	parameter.E2ssCurrOffset1 = AFE.ssVADCCurr;
	parameter.E2ssCurrOffset2 = AFE.ssCADCCurr;
}

/*************************************************************************************************
* 函数名: CaliTSn(1~4)
* 参  数: TS_Num:需要校准的外部温度序号(0~3)
* 返回值: 无
* 描  述: 校准温度，温度偏差小于15℃才允许校准
*************************************************************************************************/
void CaliTSn(U8 TS_Num)
{
	S16 Tempe;

	Tempe = uiExtTemp[TS_Num] - CalcuTemp(AFE.usTS[TS_Num]);
	if(((Tempe-parameter.E2ssTSnOffset[TS_Num])<150) && ((Tempe-parameter.E2ssTSnOffset[TS_Num])>-150))
	{
		parameter.E2ssTSnOffset[TS_Num] = Tempe;
	}
}


/*************************************************************************************************
* 函数名: CaliProcess
* 参  数: 无
* 返回值: 无
* 描  述: 处理校准，通讯中断会置位当前需要校准模块对应的标志。
          校准结束后，将校准参数写入Flash
*************************************************************************************************/
void CaliProcess(void)
{
	if(bCaliFlg)
	{
		bCaliFlg = 0;
	
		if((ucExtcaliSwitch1 & 0x01) != 0)          // 校准电压
		{
			CaliVoltage();
		}
		
		if((ucExtcaliSwitch1 & 0x04) != 0)		    // 校准电流增益
		{
			CaliCurrentGain();
		}
		
		if((ucExtcaliSwitch1 & 0x08) != 0)		    // 校准零电流
		{
			CaliCurZero();
		}
				
		if((ucExtcaliSwitch1 & 0x10) != 0)		    // 校准TS1					
		{
			CaliTSn(0);
		}
		
		if((ucExtcaliSwitch1 & 0x20) != 0)		    // 校准TS2
		{
			CaliTSn(1);
		}

		if((ucExtcaliSwitch1 & 0x40) != 0)		    // 校准TS3
		{
			CaliTSn(2);
		}
        
		if((ucExtcaliSwitch1 & 0x80) != 0)		    // 校准TS4
		{
			CaliTSn(3);
		}      
		
		ucExtcaliSwitch1 = 0;
		
		bMcuFlashWrWaitFlg = 1;						// 更新Flash参数
		ucMcuFlashWrWaitCnt = MCU_FLASH_WATI_DELAY;	// 立即写
	}
}

