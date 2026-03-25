/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "AFE.h"
#include "SPIApp.h"


/*************************************************************************************************
* 函数名: SH_AFE_ClearProtectFlag
* 参  数: AFE_Protect:需要清除的保护
* 返回值: 返回是否成功获取的状态
* 描  述: 清除某一个保护标志，退出当前AFE保护状态
*************************************************************************************************/
BOOL SH_AFE_ClearProtectFlag(AFE_ProtectType AFE_Protect)
{
	BOOL Result = false;
	U8 Temp;
	
	REG.AFESCONF2 |= 0x80;							// 配置LTCLR=1
	SH_AFE_Write(AFE_SCONF2, &REG.AFESCONF2);
	
	if(AFE_Protect&0xFF00)
	{
		Temp = (REG.AFEFLAG2)&(U8)(~(AFE_Protect|0xFE));	// 将需要恢复的保护标志位清零
		Result = SH_AFE_Write(AFE_FLAG2, &Temp);
	}
	else
	{
		Temp = (REG.AFEFLAG1)&(U8)(~AFE_Protect);
		Result = SH_AFE_Write(AFE_FLAG1, &Temp);
	}
	
	return Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_ClearAllProtectFlag
* 参  数: 无
* 返回值: 无
* 描  述: 清零AFE的所有保护标志，退出AFE保护状态
*************************************************************************************************/
void SH_AFE_ClearAllProtectFlag(void)
{
	U8 Temp;
	
	REG.AFESCONF2 |= 0x80;							// 配置LTCLR=1
	SH_AFE_Write(AFE_SCONF2, &REG.AFESCONF2);
	
	Temp = (REG.AFEFLAG1)&(0xC0);
	SH_AFE_Write(AFE_FLAG1, &Temp);
	
	REG.AFESCONF2 |= 0x80;							// 配置LTCLR=1
	SH_AFE_Write(AFE_SCONF2, &REG.AFESCONF2);
	
	Temp = (REG.AFEFLAG2)&(0x0F);
	SH_AFE_Write(AFE_FLAG2, &Temp);
}



/*************************************************************************************************
* 函数名: SH_AFE_EnterWorkMode
* 参  数: AFEMode：AFE的工作模式
* 返回值: 无
* 描  述: 控制AFE进入不同工作模式：Normal、Idle、Sleep和PowerDown模式
*************************************************************************************************/
void SH_AFE_WorkModeConfig(AFE_ModeType AFE_Mode)
{
	if(AFE_Mode == AFE_Mode_PowerDown)			// 进PowerDown模式，需配置位PD_CTRL = 1
	{
		REG.AFESCONF2 |= 0x20;
		SH_AFE_Write(AFE_SCONF2, &REG.AFESCONF2);
	}
	
	REG.AFESCONF1 = AFE_Mode;
	SH_AFE_Write(AFE_SCONF1, &REG.AFESCONF1);
}

/*************************************************************************************************
* 函数名: SH_AFE_SetChgMos
* 参  数: state：使能/失能
* 返回值: 无
* 描  述: 控制CHG、HCHG的开启和关闭
*************************************************************************************************/
void SH_AFE_SetChgMos(AFE_FunctionalState state)
{
	if(state == SH_ENABLE)
	{
		REG.AFESCONF2 |= 0x01;
		SH_AFE_Write(AFE_SCONF2, &REG.AFESCONF2);
	}
	else
	{
		REG.AFESCONF2 &= ~0x01;
		SH_AFE_Write(AFE_SCONF2, &REG.AFESCONF2);
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_SetChgMos
* 参  数: state：使能/失能
* 返回值: 无
* 描  述: 控制DSG、HDSG的开启和关闭
*************************************************************************************************/
void SH_AFE_SetDsgMos(AFE_FunctionalState state)
{
	if(state == SH_ENABLE)
	{
		REG.AFESCONF2 |= 0x02;
		SH_AFE_Write(AFE_SCONF2, &REG.AFESCONF2);
	}
	else
	{
		REG.AFESCONF2 &= ~0x02;
		SH_AFE_Write(AFE_SCONF2, &REG.AFESCONF2);
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_LoadCheckConfig
* 参  数: AFE_RLD：负载上拉电流大小选择；
          state：使能/失能
* 返回值: 无
* 描  述: Normal/IDLE模式下，使能负载检测，关闭通道检测
*************************************************************************************************/
void SH_AFE_LoadCheckConfig(AFE_LoadSourceType AFE_RLD, AFE_FunctionalState state)
{
	if(state == SH_ENABLE)
	{
		if(AFE_RLD == AFE_RLD_60uA)
		{
			REG.AFESCONF7 &= ~0x40;					// SCONF7中RLD置位0，负载检测上拉电流选择，60uA 
		}
		else
		{
			REG.AFESCONF7 |= 0x40;					// SCONF7中RLD置位1，负载检测上拉电流选择，500uA
		}
		SH_AFE_Write(AFE_SCONF7, &REG.AFESCONF7);	// 写入AFE对应的负载上拉电流

		REG.AFESCONF3 &= ~0x04;
		REG.AFESCONF3 |= 0x08;
		SH_AFE_Write(AFE_SCONF3, &REG.AFESCONF3);	// 开启AFE负载状态检测
	}
	else
	{
		REG.AFESCONF3 &= ~0x0C;
		SH_AFE_Write(AFE_SCONF3, &REG.AFESCONF3);	// 关闭AFE负载状态检测
	}
}
/*************************************************************************************************
* 函数名: SH_AFE_ChargerCheckConfig
* 参  数: state：使能/失能
* 返回值: 无
* 描  述: Normal/IDLE模式下，使能C+电压通道检测，关闭通道检测
*************************************************************************************************/
void SH_AFE_ChargerCheckConfig(AFE_FunctionalState state)
{
	if(state == SH_ENABLE)
	{
		REG.AFESCONF3 |= 0x04;
		REG.AFESCONF3 &= ~0x08;
		SH_AFE_Write(AFE_SCONF3, &REG.AFESCONF3);	// 开启AFE的C+电压通道检测
	}
	else
	{
		REG.AFESCONF3 &= ~0x0C;
		SH_AFE_Write(AFE_SCONF3, &REG.AFESCONF3);	// 关闭充电器状态检测
	}
}
/*************************************************************************************************
* 函数名: SH_AFE_LoadWakeUpConfig
* 参  数: AFE_LDWK：负载唤醒检测方式选择；
          AFE_RLD:负载上拉电流大小选择；
          state：使能/失能
* 返回值: 无
* 描  述: SLEEP模式下，使能/关闭负载唤醒检测
*************************************************************************************************/
void SH_AFE_LoadWakeUpConfig(AFE_LoadWakeUpType AFE_LDWK, AFE_LoadSourceType AFE_RLD, AFE_FunctionalState state)
{
	if(state == SH_ENABLE)
	{
		if(AFE_RLD == AFE_RLD_60uA)
		{
			REG.AFESCONF7 &= ~0x40;					// SCONF7中RLD置位0，负载检测上拉电流选择，60uA 
		}
		else
		{
			REG.AFESCONF7 |= 0x40;					// SCONF7中RLD置位1，负载检测上拉电流选择，500uA
		}
		SH_AFE_Write(AFE_SCONF7, &REG.AFESCONF7);	// 写入AFE对应的负载上拉电流
		
		if(AFE_LDWK == AFE_Load_On)                  
		{											// SCONF3中LD_WK置位01，开启负载连接唤醒检测 
			REG.AFESCONF3 |= 0x10;
			REG.AFESCONF3 &= ~0x20;
		}
		else
		{											// SCONF3中LD_WK置位10，开启负载未连接唤醒检测
			REG.AFESCONF3 &= ~0x10;
			REG.AFESCONF3 |= 0x20;                  
		}    
		SH_AFE_Write(AFE_SCONF3, &REG.AFESCONF3);	// 开启AFE负载状态检测
	}
	else
	{
		REG.AFESCONF3 &= ~0x30;
		SH_AFE_Write(AFE_SCONF3, &REG.AFESCONF3);
	}
}
/*************************************************************************************************
* 函数名: SH_AFE_ChargerWakeUpConfig
* 参  数: state：使能/失能
* 返回值: 无
* 描  述: AFE在SLEEP模式下，使能/关闭充电器唤醒检测（AFE在PD模式下，自动使能该模块）
*************************************************************************************************/
void SH_AFE_ChargerWakeUpConfig(AFE_FunctionalState state)
{
	if(state == SH_ENABLE)
	{
		REG.AFESCONF3 |= 0x40;                    
		SH_AFE_Write(AFE_SCONF3, &REG.AFESCONF3);     // SCONF3中CGR_WK置位1，开启充电器唤醒检测 
	}
	else
	{
		REG.AFESCONF3 &= ~0x40;
		SH_AFE_Write(AFE_SCONF3, &REG.AFESCONF3);     // SCONF3中CGR_WK置位0，关闭充电器唤醒检测 
	}
}


/*************************************************************************************************
* 函数名: SH_AFE_BalanceConfig
* 参  数: BALChannel：配置均衡执行的通道
* 返回值: 无
* 描  述: 配置电压均衡通道
*************************************************************************************************/
void SH_AFE_BalanceConfig(U32 BALChannel)
{
	REG.AFEBALANCEH = BALChannel>>16;
	REG.AFEBALANCEM = BALChannel>>8;
	REG.AFEBALANCEL = BALChannel;
	
	SH_AFE_Write(AFE_BALANCEH,&REG.AFEBALANCEH);
	SH_AFE_Write(AFE_BALANCEM,&REG.AFEBALANCEM);
	SH_AFE_Write(AFE_BALANCEL,&REG.AFEBALANCEL);
}

/*************************************************************************************************
* 函数名: SH_AFE_GetOWNValue
* 参  数: 无
* 返回值: 断线的OWN寄存器值
* 描  述: 获取断线的OWN寄存器值
*************************************************************************************************/
U32 SH_AFE_GetOWNValue(void)
{
	U32 DataTemp =0 ;
	
	SH_AFE_Read(AFE_OWDH, &REG.AFEOWDH, 3);
	DataTemp = REG.AFEOWDH<<16|REG.AFEOWDM<<8|REG.AFEOWDL;
	return DataTemp;
}

/*************************************************************************************************
* 函数名: SH_AFE_TrigerOWNCheck
* 参  数: state：使能/失能
* 返回值: 无
* 描  述: 触发一次断线检测
*************************************************************************************************/
void SH_AFE_TrigerOWNCheck(AFE_FunctionalState state)
{
	if(state == SH_ENABLE)
	{
		REG.AFESCONF3 |= 0x03;
		SH_AFE_Write(AFE_SCONF3,&REG.AFESCONF3);			// OWN_EN = 1，OWN_TRG = 1,触发一次断线检测
	}
	else
	{
		REG.AFESCONF3 &= ~0x03;
		SH_AFE_Write(AFE_SCONF3,&REG.AFESCONF3);			// OWN_EN = 0
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_RegisterInit
* 参  数: 无
* 返回值: 返回是否成功获取的状态
* 描  述: 初始化AFE
*************************************************************************************************/
BOOL SH_AFE_RegisterInit(void)
{
	U8 i=0;
	BOOL Result = true;
	U16 ResBuf;
	
	for(i=0; i<21; i++)
	{
		// 更新AFE寄存器数据
		if(i < 17)
		{
			*(&REG.AFESCONF1+i) = *(&parameter.E2ucAFESCONF1+i);
		}
		else						// 温度保护阈值，需要把配置的温度阈值(2731+10*x℃)，转化成AFE的配置到寄存器的码值
		{
			ResBuf = NTC103AT[(*(&parameter.E2usAFEOTCTemp+i-17)-2231)/10];
			
			if(i < 19 )
			{
				*(&REG.AFEOTC+i-17) = ResBuf*512/(1000+ResBuf);
			}
			else
			{
				*(&REG.AFEOTC+i-17) = ResBuf*512/(1000+ResBuf)-256;
			}
		}
		
		if(!SH_AFE_Write(AFE_SCONF1+i, &REG.AFESCONF1+i))
		{
        	Result = false;
    		break;
		}
	}
	return Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_SPICheck
* 参  数: 无
* 返回值: 无
* 描  述: 连续5S检测到AFE的SPI通讯错误，则置位bAFE_ERR=1	
*************************************************************************************************/
void SH_AFE_SPICheck(void)
{
	static U8 ucSPIRWErrDelayCnt = 0;		// SPI通讯失败计时
	static U8 ucSPIRWErrRDelayCnt = 0;		// SPI通讯失败恢复计时
	
	if(bAfeSPIRWErrFlg)
	{
		if(!Info.bAFE_ERR)
		{
			if(++ucSPIRWErrDelayCnt >= TIME_1S_5S)
			{
				Info.bAFE_ERR = 1;
				ucSPIRWErrDelayCnt = 0;
			}
		}
		ucSPIRWErrRDelayCnt = 0;
	}
	else
	{
		if(Info.bAFE_ERR)
		{
			if(++ucSPIRWErrRDelayCnt >= TIME_1S_5S)
			{
				Info.bAFE_ERR = 0;
				ucSPIRWErrRDelayCnt = 0;
			}
		}			
		ucSPIRWErrDelayCnt = 0;
	}
}

/*************************************************************************************************
* 函数名: SH_AFE_RegisterCheck
* 参  数: 无
* 返回值: 返回是否成功初始化的状态
* 描  述: 用于定时检测AFE的寄存器是否被改写
*************************************************************************************************/
BOOL SH_AFE_RegisterCheck(void)
{
	BOOL Result = true;
	U8 RdBuf[CHKRAM_NUM];
	U8 i;

	if(SH_AFE_Read(AFE_SCONF2, RdBuf, CHKRAM_NUM))					// 不对SCONF1做判断
	{
		for(i=0; i<CHKRAM_NUM; i++)
		{
			if(i == 0)
			{
				if((RdBuf[i]&(~0x80)) != (REG.AFESCONF2&(~0x80)))	// LTCLR位不做判断
				{
					Result = false;
					break;
				}
			}
			else if(i == 1)
			{
				if((RdBuf[i]&(~0x01)) != (REG.AFESCONF3&(~0x01)))	// OWD_TRG位不做判断
				{
					Result = false;
					break;
				}
			}
			else if((RdBuf[i]!=*(&REG.AFESCONF2+i)))
			{
				Result = false;
				break;
			}
		}
	}
	return Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_GetRegCellInfo
* 参  数: CellNum：读取电芯电压的节数;
          TsNum：读取外部温度的个数；
* 返回值: 寄存器读取的结果状态
* 描  述: 读取存储电池状态信息的寄存器
*************************************************************************************************/
U16 SH_AFE_ReadRegister(U8 CellNum, U8 TsNum)
{
	U16 Result = 0;
	
	if(!SH_AFE_Read(AFE_FLAG1, &REG.AFEFLAG1, 2))					// 读取FLAG1和FLAG2寄存器码值
	{Result |= AFE_RDERR_FLAG;}
	if(!SH_AFE_Read(AFE_BSTATUS1, &REG.AFEBSTATUS1, 2))				// 读取BSTATUS1和BSTATUS2寄存器码值
	{Result |= AFE_RDERR_STATUS;}
	
	if(!SH_AFE_Read(AFE_CELL1H, (U8 *)&AFE.ssCell[0], 2*CellNum))	// 读取VADC采集的电芯电压码值
	{Result |= AFE_RDERR_CELL;}
	if(!SH_AFE_Read(AFE_VTOPH, (U8 *)&AFE.usVTOP, 2))				// 读取VADC采集的B+电压码值
	{Result |= AFE_RDERR_VTOP;}
	if(!SH_AFE_Read(AFE_VCHGRH, (U8 *)&AFE.usVCHGR, 2))				// 读取VADC采集的CHGD电压(充电器)码值
	{Result |= AFE_RDERR_VCHG;}
	if(!SH_AFE_Read(AFE_TEMP1H, (U8 *)&AFE.usTS[0], 2*TsNum))		// 读取VADC采集的外部温度码值
	{Result |= AFE_RDERR_TEMP;}
	if(!SH_AFE_Read(AFE_TEMPIH, (U8 *)&AFE.usTI, 2))				// 读取VADC采集的内部温度码值
	{Result |= AFE_RDERR_TEMPI;}
	if(!SH_AFE_Read(AFE_CADCDH, (U8 *)&AFE.ssCADCCurr,2))			// 读取VADC采集的电流的值
	{Result |= AFE_RDERR_CADC;}
	if(!SH_AFE_Read(AFE_CURH, (U8 *)&AFE.ssVADCCurr,2))				// 读取CADC采集的电流码值
	{Result |= AFE_RDERR_CUR;}
	
	return Result;
}
