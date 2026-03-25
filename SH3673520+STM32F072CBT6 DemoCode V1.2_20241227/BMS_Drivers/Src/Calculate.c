/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "Calculate.h"

// AFE相关的变量
BOOL bVADCFlg = 0;				// VADC转换完成标志
BOOL bCADCFlg = 0;				// CADC转换完成标志

/*************************************************************************************************
* 函数名: CalcuTemp
* 参  数: getdata: AFE采集的码值
* 返回值: 返回对应NTC的阻值
* 描  述: 根据AFE ADC采集的码值，通过计算公式得到NTC对应的阻值，然后查表得到当前温度
*************************************************************************************************/
U16 CalcuTemp(U16 getdata)
{
	U8	low,high,mid;
	U16	Temperature;
    U32 tempcalcu;
   
	tempcalcu  = (U32)(getdata*1000)/(32768-getdata);	// 计算阻值单位为10Ω   
	if(tempcalcu >= NTC103AT[0])						// 查找温度
	{
		Temperature = 2731+(TEMP_LOWER_LIMIT*10);			
	}
	else if(tempcalcu <= NTC103AT[NTC103AT_ARRAY_LEN-1])
	{
		Temperature = 2731+(TEMP_UPPER_LIMIT*10);
	}
	else
	{
		low = 0;										// 二分法查找
		high = NTC103AT_ARRAY_LEN-1;
		while(low < high-1)
		{
			mid = (high+low)/2;
			if(tempcalcu > NTC103AT[mid])
			{
				high = mid;
			}
			else
			{
				low = mid;
			}
		}
		Temperature = (U16)(low-50)*10+(NTC103AT[low]-tempcalcu)*10/(NTC103AT[low]-NTC103AT[low+1])+2731;	  
	}
	return Temperature;
}

/*************************************************************************************************
* 函数名: SH_AFE_GetOtherStatus
* 参  数: 无
* 返回值: 无
* 描  述: 读取AFE当前保护状态
*************************************************************************************************/
void SH_AFE_GetProtectStatus(void)
{
	if((REG.AFEFLAG1&0x01) != 0)		// 获取AFE的保护状态
	{
		Info.bOV = 1;
	}
	if((REG.AFEFLAG1&0x02) != 0)    
	{
		Info.bUV = 1;
	} 
	if((REG.AFEFLAG1&0x04) != 0)
	{
		Info.bOCD1 = 1;
	}    
	if((REG.AFEFLAG1&0x08) != 0)    
	{
		Info.bOCD2 = 1;
	}
	if((REG.AFEFLAG1&0x10) != 0)    
	{
		Info.bSC = 1;
	}
	if((REG.AFEFLAG1&0x20) != 0)    
	{
		Info.bOCC = 1;
	}
	if((REG.AFEFLAG2&0x10) != 0)    
	{
		Info.bUTC = 1;
	}
	if((REG.AFEFLAG2&0x20) != 0)    
	{
		Info.bOTC = 1;
	}
	if ((REG.AFEFLAG2&0x40) != 0)    
	{
		Info.bUTD = 1;
	}    
	if ((REG.AFEFLAG2&0x80) != 0)    
	{
		Info.bOTD = 1;
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_GetOtherStatus
* 参  数: HighSide：高低方案：1：高边方案;0：低边方案
* 返回值: 无
* 描  述: 读取AFE当前MOS状态
*************************************************************************************************/
void SH_AFE_GetMOSStatus(BOOL HighSide)
{
	if(HighSide)						// 获取MOS管开关状态
	{
		if((REG.AFEBSTATUS1&0x10) != 0)
		{
			Info.bCHGMOS = 1;
		}
		else
		{
			Info.bCHGMOS = 0;
		}
		if((REG.AFEBSTATUS1&0x20) != 0)
		{
			Info.bDSGMOS = 1;
		}
		else
		{
			Info.bDSGMOS = 0;
		}
		if((REG.AFEBSTATUS1&0x04) != 0)
		{
			Info.bPDSGMOS = 1;
		}
		else
		{
			Info.bPDSGMOS = 0;
		}
	}
	else
	{
		if((REG.AFEBSTATUS1&0x01) != 0)
		{
			Info.bCHGMOS = 1;
		}
		else
		{
			Info.bCHGMOS = 0;
		}
		if((REG.AFEBSTATUS1&0x02) != 0)
		{
			Info.bDSGMOS = 1;
		}
		else
		{
			Info.bDSGMOS = 0;
		}
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_GetOtherStatus
* 参  数: 无
* 返回值: 无
* 描  述: 读取AFE当前工作模式状态
*************************************************************************************************/
void SH_AFE_GetWorkModeStatus(void)
{
	if((REG.AFEBSTATUS2&0x10) != 0)		// 获取AFE工作模式
	{
		Info.bIDLE = 1;
	}
	else
	{
		Info.bIDLE = 0;
	}
	if((REG.AFEBSTATUS2&0x20) != 0)
	{
		Info.bSLEEP = 1;
	}
	else
	{
		Info.bSLEEP = 0;
	}
}
/*************************************************************************************************
* 函数名: SH_AFE_GetOtherStatus
* 参  数: 无
* 返回值: 无
* 描  述: 读取AFE当前充放电状态
*************************************************************************************************/
void SH_AFE_GetChargeStatus(void)
{
	if((REG.AFEBSTATUS2&0x40) != 0)		// 获取充放电状态
	{
		Info.bDSGING = 1;
	}
	else
	{
		Info.bDSGING = 0;
	}
	if((REG.AFEBSTATUS2&0x80) != 0)
	{
		Info.bCHGING = 1;
	}
	else
	{
		Info.bCHGING = 0;
	}
}
/*************************************************************************************************
* 函数名: SH_AFE_GetOtherStatus
* 参  数: 无
* 返回值: 无
* 描  述: 读取AFE其他状态
*************************************************************************************************/
void SH_AFE_GetOtherStatus(void)
{
	if((REG.AFEFLAG2&0x01) != 0)		// CADC 电流转换完成
	{
		bCADCFlg = 1;
	}
	if((REG.AFEFLAG2&0x02) != 0)		// VADC 电流转换完成
	{
		bVADCFlg = 1;
	}
	if((REG.AFEBSTATUS2&0x08) != 0)		// 平衡状态
	{
		Info.bBAL = 1;
	}
	else
	{
		Info.bBAL = 0;
	}
}
/*************************************************************************************************
* 函数名: SH_AFE_GetStatus
* 参  数: HighSide：高低方案：1：高边方案;0：低边方案
* 返回值: 无
* 描  述: 获取AFE当前状态
*************************************************************************************************/
void SH_AFE_GetStatus(BOOL HighSide)
{
	SH_AFE_GetProtectStatus();
	SH_AFE_GetMOSStatus(HighSide);
	SH_AFE_GetWorkModeStatus();
	SH_AFE_GetChargeStatus();
	SH_AFE_GetOtherStatus();
}

/*************************************************************************************************
* 函数名: SH_AFE_GetCellVol
* 参  数: Num：需要获取的电芯电压数量;
          Temp：指向存放电芯电压数据数组的指针；
          Voltage：指向存放电池电压（电芯电压之和）数据的指针；
* 返回值: 返回是否成功获取的状态
* 描  述: 获取电芯电压及电芯电压之和
*************************************************************************************************/
void SH_AFE_GetCellVol(U8 Num, S16 *VCell, U32 *Voltage)
{
	U8 i;
	S16 ssTempCellVol=0;
	S32 siTempPackVol=0;
	
	for(i=0; i<Num; i++)
	{
		ssTempCellVol = (S32)AFE.ssCell[i]*CALIPACKVOL1/parameter.E2usVPackGain1;		// 计算电芯Cell电压
		VCell[i] = ssTempCellVol;
		
		siTempPackVol += AFE.ssCell[i];
	}
	*Voltage = (U32)siTempPackVol*CALIPACKVOL1/parameter.E2usVPackGain1;			// CELL电压之和（计算总压）
	
	for(i=Num; i<20; i++)				// 未配置串数电压显示0
	{
		VCell[i] = 0;
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_GetTotalVol
* 参  数: Voltage：指向存放电池电压数据的指针；
* 返回值: 返回是否成功获取的状态
* 描  述: 获取电池总压（B+）
*************************************************************************************************/
void SH_AFE_GetTotalVol(U32 *Voltage)
{
	*Voltage = (U32)AFE.usVTOP*CALIPACKVOL2/parameter.E2usVPackGain2;
}

/*************************************************************************************************
* 函数名: SH_AFE_GetChgVol
* 参  数: Voltage：指向存放充电器电压数据的指针；
* 返回值: 返回是否成功获取的状态
* 描  述: 获取充电器电压（CHGD）
*************************************************************************************************/
void SH_AFE_GetChgVol(U32 *Voltage)
{
	*Voltage  = (U32)AFE.usVCHGR*CALIPACKVOL2/parameter.E2usVPackGain2;
}

/*************************************************************************************************
* 函数名: SH_AFE_GetTempe
* 参  数: Num：需要获取的温度数量;
          Temp：指向存放外部温度数据数组的指针；
* 返回值: 返回是否成功获取的状态
* 描  述: 获取外部温度
*************************************************************************************************/
void SH_AFE_GetTempe(U8 Num, U16 *Temp)
{
	U8 i;
	
	for(i=0; i<Num; i++)
	{
		Temp[i] = CalcuTemp(AFE.usTS[i])+parameter.E2ssTSnOffset[i];			// 监测电芯温度n
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_GetTempI
* 参  数: Temp：指向存放内部温度数据的指针；
* 返回值: 返回是否成功获取的状态
* 描  述: 获取内部温度
*************************************************************************************************/
void SH_AFE_GetTempI(U16 *Temp)
{
	*Temp = ((S32)AFE.usTI*5625/16384-5625)*612/1090+410+2731;		// 计算IC内部温度，K氏，放大10倍
}

/*************************************************************************************************
* 函数名: SH_AFE_GetCadcCurr
* 参  数: Curr：指向存放CADC电流数据的指针；
* 返回值: 返回是否成功获取的状态
* 描  述: 获取CADC采集电流
*************************************************************************************************/
void SH_AFE_GetCadcCurr(S32 *Curr)
{
	U8 i;
	S32 Tempdata = 0;
	static U8 ucCAdcCnt = 0;
	static S32 siCAdcCurBuf[4] = {0},siCAdcCurAverage = 0;
	
	siCAdcCurBuf[ucCAdcCnt] = (S32)CALICUR*(AFE.ssCADCCurr-parameter.E2ssCurrOffset2)/parameter.E2ssCurrGain2;
	if(++ucCAdcCnt >= 4)		// 对连续采集的4次电流取平均值，作为当前电流值
	{
		ucCAdcCnt = 0;
	}
	for(i=0; i<4; i++)
	{
		Tempdata += siCAdcCurBuf[i];
	}
	siCAdcCurAverage = Tempdata/4;
		
	if(siCAdcCurAverage < (-parameter.E2ssDfilterCur) || (siCAdcCurAverage > parameter.E2ssDfilterCur))
	{
		*Curr = siCAdcCurAverage;
	}
	else
	{
		*Curr = 0;
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_GetVadcCurr
* 参  数: Curr：指向存放VADC电流数据的指针；
* 返回值: 返回是否成功获取的状态
* 描  述: 获取VADC采集电流
*************************************************************************************************/
void SH_AFE_GetVadcCurr(S32 *Curr)
{
	U8 i;
	S32 Tempdata = 0;
	static U8 ucVAdcCnt = 0;
	static S32 siVAdcCurBuf[4] = {0},siVAdcCurAverage = 0;
	
	siVAdcCurBuf[ucVAdcCnt] = (S32)CALICUR*(AFE.ssVADCCurr-parameter.E2ssCurrOffset1)/parameter.E2ssCurrGain1;
	if(++ucVAdcCnt >= 4)		// 对连续采集的4次电流取平均值，作为当前电流值
	{
		ucVAdcCnt = 0;
	}
	for(i=0; i<4; i++)
	{
		Tempdata += siVAdcCurBuf[i];
	}
	siVAdcCurAverage = Tempdata/4;
		
	if(siVAdcCurAverage < (-parameter.E2ssDfilterCur) || (siVAdcCurAverage > parameter.E2ssDfilterCur))
	{
		*Curr = siVAdcCurAverage;
	}
	else
	{
		*Curr = 0;
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_GetMaxMin
* 参  数: Num：需要用于比较的数据数量;
          Temp：指向存放数据的数组的指针；
          Min：指向存放最小数据的指针；
          Max：指向存放最大数据的指针。
* 返回值: 无
* 描  述: 获取最大/小值
*************************************************************************************************/
void SH_AFE_GetMaxMin(U8 Num, S16 *Array, S16 *Min, S16 *Max)
{
	U8 i;
	
	*Min = Array[0];
	*Max = Array[0];
	for(i=1; i<Num; i++)
	{
		if(Array[i] < *Min)
		{
			*Min = Array[i];
		}
		else if(Array[i] > *Max)
		{
			*Max = Array[i];
		}
	}
}


/*************************************************************************************************
* 函数名: AFEInfoProcess
* 参  数: CellNum: 电池串数
          TsNum：外部温度个数
          HighSide：高低方案：1：高边方案;0：低边方案
* 返回值: 无
* 描  述: 获取电池的电压、温度、电流和状态
*************************************************************************************************/
void SH_AFE_GetBattInfo(U8 CellNum, U8 TsNum, BOOL HighSide)
{
	SH_AFE_GetStatus(HighSide);										// 获取AFE的状态

	SH_AFE_GetCellVol(CellNum, Info.ssVCell, &Info.uiVoltage1);		// 获取电芯电压及电芯之和
	SH_AFE_GetTotalVol(&Info.uiVoltage2);							// 获取电芯总电压(B+)
	SH_AFE_GetChgVol(&Info.uiVCHGD);								// 获取充电器电压(CHGD)
	SH_AFE_GetTempe(TsNum, Info.usTemp);							// 获取外部温度
	SH_AFE_GetTempI(&Info.usTempI);									// 获取内部温度
	SH_AFE_GetVadcCurr(&Info.siVADCCurr);							// 获取VADC电流
	
	// 获取电芯电压最大/最小值
	SH_AFE_GetMaxMin(CellNum, Info.ssVCell, &Info.ssVCellMin, &Info.ssVCellMax);
	// 获取外部温度最大/最小值
	SH_AFE_GetMaxMin(TsNum, (S16 *)Info.usTemp, (S16 *)&Info.usTempMin, (S16 *)&Info.usTempMax);
	
	if(bCADCFlg)
	{
		bCADCFlg = 0;
		SH_AFE_GetCadcCurr(&Info.siCADCCurr);						// 获取CADC电流
	}
}


