/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "Protect.h"
#include "AFE.h"
#include "ChargerLoad.h"

BOOL bPorSelfTestFlg;			// 上电自检标志位

U16	usOVRDelayCnt = 0;			// 单节OV保护恢复延时计数
U16	usUVRDelayCnt = 0;			// 单节OV保护恢复延时计数
U16	usPACKOVDelayCnt = 0;		// 总压PACKOV保护延时计数
U16	usPACKOVRDelayCnt = 0;		// 总压PACKOV保护恢复延时计数
U16	usPACKUVDelayCnt = 0;		// 总压PACKUV保护延时计数
U16	usPACKUVRDelayCnt = 0;		// 总压PACKUV保护恢复延时计数
	
U16	usOTCRDelayCnt = 0;			// OTC保护恢复延时计数
U16	usUTCRDelayCnt = 0;			// UTC保护恢复延时计数
U16	usOTDRDelayCnt = 0;			// OTD保护恢复延时计数
U16	usUTDRDelayCnt = 0;			// UTD保护恢复延时计数

U16	usOTIDelayCnt = 0;			// 内部温度保护计数
U16	usOTIRDelayCnt = 0;			// 内部高温保护恢复延时计数

/*************************************************************************************************
* 函数名: ProtectOV
* 参  数: 无
* 返回值: 无
* 描  述: 单节电池过压保护检测，过压后置位bOV为1
*************************************************************************************************/
void ProtectOV(void)
{
	if(!Info.bOV)
	{
        usOVRDelayCnt = 0;
	}
	else
	{
        if(Info.ssVCellMax < parameter.E2usOVRVol)				// OV恢复电压判定
        {			
			if(++usOVRDelayCnt >= OVR_DELAY_CNT)  
            {
				if(SH_AFE_ClearProtectFlag(AFE_FLAG_OV))		// 清零AFE中的标志位
				{
					Info.bOV = 0; 
					usOVRDelayCnt = 0;
				}
			}
		}
		else
		{
			usOVRDelayCnt = 0;
		}
	}
}

/*************************************************************************************************
* 函数名: ProtectPACKOV
* 参  数: 无
* 返回值: 无
* 描  述: 总压电池过压保护检测，过压后置位bPACKOV为1
*************************************************************************************************/
void ProtectPACKOV(void)
{
	if(!Info.bPACKOV)
	{
		if(Info.uiVoltage2 > PACKOV_VOL)
		{		
			if(++usPACKOVDelayCnt >= PACKOV_DELAY_CNT)
			{
				Info.bPACKOV = 1;
				usPACKOVDelayCnt = 0;
				usPACKOVRDelayCnt = 0;
			}
		}
		else 
		{
			usPACKOVDelayCnt = 0;
		}
	}
	else
	{
		if(Info.uiVoltage2 < PACKOVR_VOL)
		{			
			if(++usPACKOVRDelayCnt >= PACKOVR_DELAY_CNT)
			{
				Info.bPACKOV = 0;
				usPACKOVRDelayCnt = 0;
				usPACKOVDelayCnt = 0;
			}
		}
		else
		{
			usPACKOVRDelayCnt = 0;
		}
	}    
}

/*************************************************************************************************
* 函数名: ProtectUV
* 参  数: 无
* 返回值: 无
* 描  述: 单节电池欠压保护检测，过压后置位bUV为1
*************************************************************************************************/
void ProtectUV(void)
{
	if(!Info.bUV)
	{
		usUVRDelayCnt = 0;
	}
	else
	{
		if(Info.ssVCellMin > parameter.E2usUVRVol)				// UV恢复电压判定
        {			
            if(++usUVRDelayCnt >= UVR_DELAY_CNT)				// UV恢复延时判定  
            {
                if(SH_AFE_ClearProtectFlag(AFE_FLAG_UV))		// 清零AFE中的标志位
                {
                    Info.bUV = 0; 
                    usUVRDelayCnt = 0;
                }
            }
        }
        else
        {
            usUVRDelayCnt = 0;
        }
	}  
}

/*************************************************************************************************
* 函数名: ProtectPACKUV
* 参  数: 无
* 返回值: 无
* 描  述: 总压电池欠压保护检测，过压后置位bPACKUV为1
*************************************************************************************************/
void ProtectPACKUV(void)
{
	if(!Info.bPACKUV)
	{
		if(Info.uiVoltage2 < PACKUV_VOL)
		{		
			if(++usPACKUVDelayCnt >= PACKUV_DELAY_CNT)
			{
				Info.bPACKUV = 1;
				usPACKUVDelayCnt = 0;
				usPACKUVRDelayCnt = 0;
			}
		}
		else
		{
			usPACKUVDelayCnt = 0;
		}
	}
	else
	{
		if(Info.uiVoltage2 > PACKUVR_VOL)
		{			
			if(++usPACKUVRDelayCnt >= PACKUVR_DELAY_CNT)
			{
				Info.bPACKUV = 0;
				usPACKUVDelayCnt = 0;
				usPACKUVRDelayCnt = 0;
			}
		}
		else
		{
			usPACKUVRDelayCnt = 0;
		}
	}     
}

/*************************************************************************************************
* 函数名: ProtectOTC
* 参  数: 无
* 返回值: 无
* 描  述: 充电高温保护检测，保护后置位bOTC为1
*************************************************************************************************/
void ProtectOTC(void)
{
	if(!Info.bOTC)
	{
        usOTCRDelayCnt = 0;
	}
	else
	{
		if(Info.usTempMax < parameter.E2usOTCRTemp)
		{
			if(++usOTCRDelayCnt >= TEMPER_DELAY_CNT)
			{
                if(SH_AFE_ClearProtectFlag(AFE_FLAG_OTC))      // 清零AFE中的标志位
                {
                    Info.bOTC = 0;
                    usOTCRDelayCnt = 0;
                }
			}
		}
		else 
		{
			usOTCRDelayCnt = 0;
		}
	}
}

/*************************************************************************************************
* 函数名: ProtectUTC
* 参  数: 无
* 返回值: 无
* 描  述: 充电低温保护检测，保护后置位bUTC为1
*************************************************************************************************/
void ProtectUTC(void)
{
	if(!Info.bUTC)
	{
		usUTCRDelayCnt = 0;
	}
	else
	{
		if(Info.usTempMin > parameter.E2usUTCRTemp)
		{
			if(++usUTCRDelayCnt >= TEMPER_DELAY_CNT)
			{
                if(SH_AFE_ClearProtectFlag(AFE_FLAG_UTC))		// 清零AFE中的标志位
                {
                    Info.bUTC = 0;
                    usUTCRDelayCnt = 0;
                }
			}
		}
		else 
		{
			usUTCRDelayCnt = 0;
		}
	}
}

/*************************************************************************************************
* 函数名: ProtectOTD
* 参  数: 无
* 返回值: 无
* 描  述: 放电高温保护检测，保护后置位bOTD为1
*************************************************************************************************/
void ProtectOTD(void)
{
	if(!Info.bOTD)
	{
        usOTDRDelayCnt = 0;
	}
	else
	{
		if(Info.usTempMax < parameter.E2usOTDRTemp)
		{
			if(++usOTDRDelayCnt >= TEMPER_DELAY_CNT)
			{
                if(SH_AFE_ClearProtectFlag(AFE_FLAG_OTD))      // 清零AFE中的标志位
                {
                    Info.bOTD = 0;
                    usOTDRDelayCnt = 0;
                }
			}
		}
		else 
		{
			usOTDRDelayCnt = 0;
		}
	}
}

/*************************************************************************************************
* 函数名: ProtectUTD
* 参  数: 无
* 返回值: 无
* 描  述: 放电低温保护检测，保护后置位bOTD为1
*************************************************************************************************/
void ProtectUTD(void)
{
	if(!Info.bUTD)
	{
        usUTDRDelayCnt = 0;
	}
	else
	{
		if(Info.usTempMin > parameter.E2usUTDRTemp)
		{
			if(++usUTDRDelayCnt >= TEMPER_DELAY_CNT)
			{
                if(SH_AFE_ClearProtectFlag(AFE_FLAG_UTD))      // 清零AFE中的标志位
                {
                    Info.bUTD = 0;
                    usUTDRDelayCnt = 0;
                }
			}
		}
		else 
		{
			usUTDRDelayCnt = 0;
		}
	}
}

/*************************************************************************************************
* 函数名: ProtectICOT
* 参  数: 无
* 返回值: 无
* 描  述: 芯片内部温度过温阈值为100℃，过温恢复阈值为90℃
*************************************************************************************************/
void ProtectICOT(void)
{
	if(!Info.bICOT)
	{
		if(Info.usTempI > TEMP_OTI)
		{
			if(++usOTIDelayCnt >= TEMPER_DELAY_CNT)
			{
				Info.bICOT = 1;
				usOTIDelayCnt = 0;
			}
		}
		else
		{
			usOTIDelayCnt = 0;
		}
		usOTIRDelayCnt = 0;
	}
	else
	{
		if(Info.usTempI < TEMP_OTIR)
		{
			if(++usOTIRDelayCnt >= TEMPER_DELAY_CNT)
			{
				Info.bICOT = 0;
				usOTIRDelayCnt = 0;
			}
		}
		else
		{
			usOTIRDelayCnt = 0;
		}
		usOTIDelayCnt = 0;
	}
}


/*************************************************************************************************
* 函数名: ProtectOCC
* 参  数: 无
* 返回值: 无
* 描  述: 充电过流保护检测
*************************************************************************************************/
void ProtectOCC(void)
{
	if(Info.bOCC)
	{
		if(bChgerRStatusFlg)     // 判断充电器是否存在
		{
			if(SH_AFE_ClearProtectFlag(AFE_FLAG_OCC))      // 清零AFE中的标志位
			{
				Info.bOCC = 0;
			}
		}
	}
}

/*************************************************************************************************
* 函数名: ProtectOCD1
* 参  数: 无
* 返回值: 无
* 描  述: 放电过流1保护检测
*************************************************************************************************/
void ProtectOCD1(void)
{
	if(Info.bOCD1)
	{
		if(bLoadRStatusFlg)      // 判断负载未连接
        {
			if(SH_AFE_ClearProtectFlag(AFE_FLAG_OCD1))      // 清零AFE中的标志位
			{
				Info.bOCD1 = 0;
			}
        }
	}
}

/*************************************************************************************************
* 函数名: ProtectOCD2
* 参  数: 无
* 返回值: 无
* 描  述: 放电过流2保护检测
*************************************************************************************************/
void ProtectOCD2(void)
{
	if(Info.bOCD2)
	{
        if(bLoadRStatusFlg)      // 判断负载未连接
        {
            if(SH_AFE_ClearProtectFlag(AFE_FLAG_OCD2))      // 清零AFE中的标志位
            {
                Info.bOCD2 = 0;
            }
        }
	}
}

/*************************************************************************************************
* 函数名: ProtectSC
* 参  数: 无
* 返回值: 无
* 描  述: 短路保护检测
*************************************************************************************************/
void ProtectSC(void)
{
	if(Info.bSC)
	{
        if(bLoadRStatusFlg)      // 判断负载未连接
        {
            if(SH_AFE_ClearProtectFlag(AFE_FLAG_SC))		// 清零AFE中的标志位
            {
                Info.bSC = 0;
            }
        }
	}
}



/*************************************************************************************************
* 函数名: ProtectProcess
* 参  数: 无
* 返回值: 无
* 描  述: 各种保护检测
*************************************************************************************************/
void ProtectProcess(void)
{
	ProtectOV();			// 检测过压保护恢复
	ProtectPACKOV();		// 检测总压过压保护及其恢复
	ProtectUV();			// 检测欠压保护及其恢复
	ProtectPACKUV();		// 检测总压欠压保护恢复

	ProtectOCC();			// 充电过流保护恢复
	ProtectOCD1();			// 放电过流1保护恢复
	ProtectOCD2();			// 放电过流2保护恢复
	ProtectSC();			// 短路保护恢复
	
	ProtectOTC();			// 检测充电高温保护恢复
	ProtectUTC();			// 检测充电低温保护恢复
	ProtectOTD();			// 检测放电高温保护恢复
	ProtectUTD();			// 检测放电低温保护恢复
	ProtectICOT();			// 内部高温保护恢复
}


