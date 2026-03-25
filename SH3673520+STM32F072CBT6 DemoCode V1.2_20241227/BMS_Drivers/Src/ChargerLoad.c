/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "ChargerLoad.h"
#include "AFE.h"

BOOL bLoadCheckFlg;			// 开启负载状态检测标志位
BOOL bChgerCheckFlg;		// 开启负载状态检测标志位
BOOL bLoadAlterFlg;			// 轮循时，负载检测标志位
BOOL bChgerAlterFlg;		// 轮循时，充电器检测标志位
BOOL bLoadRStatusFlg;		// 负载未连接状态标志位
BOOL bChgerRStatusFlg;		// 充电器移除标志位


/*************************************************************************************************
* 函数名: AlternateCheck
* 参  数: 无
* 返回值: 无
* 描  述: 当负载状态检测与充电器状态检测均需要开启时，两种检测开始定时轮循
*************************************************************************************************/
void AlternateCheck(void)
{
	static U16 usAlternateCnt = 0;			// 充电器、负载检测轮循计数
	
	if(Info.bOCD1 || Info.bOCD2 || Info.bSC)
	{
		bLoadCheckFlg = 1;
	}
	else
	{
		bLoadCheckFlg = 0;
	}
	
	if(Info.bOCC)
	{
		bChgerCheckFlg = 1;
	}
	else
	{
		bChgerCheckFlg = 0;
	}
    
	if(bLoadCheckFlg && bChgerCheckFlg)
	{
		if(++usAlternateCnt <= ALTER_DELAY_CNT)
		{
			usAlternateCnt = 0;
			if(bLoadAlterFlg)
			{
				bLoadAlterFlg = 0;
				bChgerAlterFlg = 1;
			}
			else
			{
				bLoadAlterFlg = 1;
				bChgerAlterFlg = 0;
			}
		}
	}
	else
	{
		bLoadAlterFlg = 1; 
		bChgerAlterFlg = 1;
	}
}

/*************************************************************************************************
* 函数名: LoadChgerChkProcess
* 参  数: 无
* 返回值: 无
* 描  述: 负载和充电器状态检测
*************************************************************************************************/
void LoadChgerChkProcess(void)
{
	static BOOL bLoadChgerEnableFlg = 0;
	static U8 ucLoadRDelayCnt = 0;
	static U8 ucChgerRDelayCnt = 0;
	
	AlternateCheck();
    
	// 负载状态检测
	if(bLoadCheckFlg && bLoadAlterFlg)
	{
		bLoadChgerEnableFlg = 1;
		SH_AFE_LoadCheckConfig(AFE_RLD_60uA, SH_ENABLE); // 开启负载状态检测
		
		if((REG.AFEBSTATUS2&0x01) && (!(REG.AFEBSTATUS2&0x02)))      // 判断负载未连接
        {
			if(++ucLoadRDelayCnt >= LOADCHG_DELAY_CNT)
			{
				ucLoadRDelayCnt = LOADCHG_DELAY_CNT;
				bLoadRStatusFlg = 1;
			}
        }
		else
		{
			bLoadRStatusFlg = 0;
			ucLoadRDelayCnt = 0;
		}
	}
	// 充电器状态检测
	else if(bChgerCheckFlg && bChgerAlterFlg)
	{
		bLoadChgerEnableFlg = 1;
		SH_AFE_ChargerCheckConfig(SH_ENABLE); 	// 开启CHGD检测通道
		
		if(Info.uiVCHGD <= CHGR_CHK_VOL)
		{
			if(++ucChgerRDelayCnt >= LOADCHG_DELAY_CNT)
			{
				ucChgerRDelayCnt = LOADCHG_DELAY_CNT;
				bChgerRStatusFlg = 1;
			}
		}
		else
		{
			bChgerRStatusFlg = 0;
			ucChgerRDelayCnt = 0;
		}
	}
    else
    {
		if(bLoadChgerEnableFlg)
		{
			bLoadChgerEnableFlg = 0;
			ucLoadRDelayCnt = 0;
			ucChgerRDelayCnt = 0;
			bLoadRStatusFlg = 0;
			bChgerRStatusFlg = 0;
			SH_AFE_ChargerCheckConfig(SH_DISABLE);      // 关闭负载及C+电压状态检测
		}
    }
}




