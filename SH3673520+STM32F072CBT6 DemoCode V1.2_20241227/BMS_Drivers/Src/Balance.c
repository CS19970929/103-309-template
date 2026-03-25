/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "Balance.h"
#include "AFE.h"
#include "InitialApp.h"

/*************************************************************************************************
* 函数名: BalProcess
* 参  数: 无
* 返回值: 无
* 描  述: 
*************************************************************************************************/
void BalProcess(void)
{
	U8 i;
	U32 balancebk = 0;
	static U32 uiBalanceChannel = 0;
	static U8 ucBalUpdateTimeCnt = 0;
	static U8 ucBalanceTimeCnt[20] = {0};
	
	if(parameter.bBAL_EN)
	{
		// 满足平衡退出条件时立即退出平衡
		if(Info.bDSGING	|| Info.bUTC || Info.bOTC || Info.bUTD || Info.bOTD || Info.bICOT
			|| ((Info.ssVCellMax-Info.ssVCellMin) < parameter.E2usBalanceVolDiff) 
			|| (Info.siCADCCurr < parameter.E2ssBalanceCur))	
		{
			for(i=0; i<Info.ucCellNum; i++)
			{
				ucBalanceTimeCnt[i] = 0;
			}
			balancebk = 0;
		}
		// 电压电流满足平衡进入条件
		else
		{
			for(i=0; i<Info.ucCellNum; i++)
			{
				if((Info.ssVCell[i] >= parameter.E2usBalanceVol) && ((Info.ssVCell[i]-Info.ssVCellMin) >= parameter.E2usBalanceVolDiff))	// 电芯电压大于平衡电压，且压差大于阈值
				{
					if(++ucBalanceTimeCnt[i] >= BALANCE_DELAY_CNT)						// 该电芯满足平衡条件且超过延时阈值
					{
						ucBalanceTimeCnt[i] = BALANCE_DELAY_CNT;
						balancebk |= (1<<i);
					}
				}
				else						 // 电芯不满足平衡条件时立即停止该电芯的平衡
				{
					ucBalanceTimeCnt[i] = 0;
					balancebk &= ~(1<<i);
				}
			}
		}
			
		if ((uiBalanceChannel != balancebk)				// 平衡状态改变时配置平衡，或平衡过程中定时写平衡寄存器
			|| ((uiBalanceChannel != 0) && (++ucBalUpdateTimeCnt >= BalUpdate_DELAY_CNT)))
		{
			ucBalUpdateTimeCnt = 0;
			uiBalanceChannel = balancebk;
			Info.uiBALChannel = uiBalanceChannel;
			
			SH_AFE_BalanceConfig(Info.uiBALChannel);
		}	
	}
	else
	{
		for(i=0; i<Info.ucCellNum; i++)
		{
			ucBalanceTimeCnt[i] = 0;
		}
		balancebk = 0;
		Info.uiBALChannel = 0;
	}
}

/*************************************************************************************************
* 函数名: CellOpenProcess
* 参  数: 无
* 返回值: 无
* 描  述: 每1S开启一次断线检测
*************************************************************************************************/
void CellOpenProcess(void)
{
	static BOOL bOWNEnableFlg = 0;
	static U8 ucCTOTimeCnt = 0;
	static U8 ucCTORTimeCnt = 0;
	
	if(parameter.bCTO_EN)
	{
		bOWNEnableFlg = 1;
		
		SH_AFE_TrigerOWNCheck(SH_ENABLE);				// 触发一次断线检测
		Info.uiCTOChannel = SH_AFE_GetOWNValue();		// 获取断线的OWN寄存器值
		
		if(Info.uiCTOChannel)
		{
			ucCTORTimeCnt = 0;
			if(++ucCTOTimeCnt >= CTO_DELAY_CNT)
			{
				ucCTOTimeCnt = 0;
				Info.bCTO = 1;
			}
		}
		else
		{
			ucCTOTimeCnt = 0;
			if(++ucCTORTimeCnt >= CTO_DELAY_CNT)
			{
				ucCTORTimeCnt = 0;
				Info.bCTO = 0;
			}
		}
	}
	else
	{
		if(bOWNEnableFlg)
		{
			bOWNEnableFlg = 0;
			SH_AFE_TrigerOWNCheck(SH_DISABLE);			// 关闭断线检测
			
			Info.bCTO = 0;
			Info.uiCTOChannel = 0;
		}
	}
}


