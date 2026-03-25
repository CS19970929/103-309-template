/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "InitialApp.h"
#include "flash.h"
#include "IAP.h"
#include "DataFlash.h"
#include "AFE.h"
#include "main.h"
#include "usart.h"
#include "Uart2App.h"

BOOL bMcuFlashErr;

/*************************************************************************************************
* 函数名: InitVar
* 参  数: 无
* 返回值: 无
* 描  述: 初始化用户自定义变量
*************************************************************************************************/
void InitVar(void)
{
	if(Info.bHighSide)						// 高侧方案时，开启Charge Pump(PUMP_EN = 1)
	{
		parameter.E2ucAFESCONF2 = parameter.E2ucAFESCONF2|0x10;
		
		if(parameter.bPDSGMOS_EN)							// 开启预充电时，配置PDSGMOS=1，预放电功能由SH3673520控制。
		{
			parameter.E2ucAFESCONF2 = parameter.E2ucAFESCONF2|0x04;
		}
	}
	
	Info.siCADCCurr = 0; 									// After the program is reset, the current default is "0"
	Info.usPackConfig = parameter.E2usPackConfigMap;
	
	Info.uiCTOChannel = 0;
	Info.uiBALChannel = 0;
	Info.usSoftVersion = SOFT_SERVION;
	
	Info.ucCellNum = (parameter.E2ucAFESCONF4 & 0x1F);		// 根据配置初始化电芯串数和Offset
	if ((Info.ucCellNum >= 20) || (Info.ucCellNum < 4))
	{
		Info.ucCellNum = 20;
	}
}

/*************************************************************************************************
* 函数名: InitSysPara
* 参  数: 无
* 返回值: 无
* 描  述: 初始化系统参数，从Flash区读取对应参数
*************************************************************************************************/
void InitSysPara(void)
{
	PARAMETER *pE2DataBak1 = (PARAMETER *)FLASH_PARAA_ADDR;
	PARAMETER *pE2DataBak2 = (PARAMETER *)FLASH_PARAB_ADDR;
	if(pE2DataBak1->E2usCheckFlag == 0x5AA5)
	{
		memcpy((U8 *)&parameter, (U8 *)pE2DataBak1, DATAFLASH_BLOCK_SIZE);
		if(pE2DataBak2->E2usCheckFlag != 0x5AA5)
		{
			FlashProcess(FLASH_PARAB_ADDR, (U8 *)&parameter, DATAFLASH_BLOCK_SIZE);		// 更新备份区2
		}
	}
	else if(pE2DataBak2->E2usCheckFlag == 0x5AA5)
	{
		memcpy((U8 *)&parameter, (U8 *)pE2DataBak2, DATAFLASH_BLOCK_SIZE);

		FlashProcess(FLASH_PARAA_ADDR, (U8 *)&parameter, DATAFLASH_BLOCK_SIZE);			// 更新备份区1
	}
	else
	{
		bMcuFlashErr = 1;
		return;
	}
}

/*************************************************************************************************
* 函数名: UserInitial
* 参  数: 无
* 返回值: 无
* 描  述: 系统初始化
*************************************************************************************************/
void UserInitial(void)
{
	Info.bHighSide = 0;
	if(HAL_GPIO_ReadPin(HL_Side_GPIO_Port,HL_Side_Pin))		// PB14管脚悬空：高侧方案。PB14管脚下拉到GND：低侧方案。
	{
		Info.bHighSide = 1;
	}
	
	InitSysPara();					// 系统参数初始化
	
	InitVar();						// 初始化变量
	
	if(!SH_AFE_RegisterInit())		// 初始化AFE主控
	{
		Info.bAFE_ERR = 1;			// 第一次如果初始化失败则置位该标志，中途是连续判断5S置位该标志
	}
	SH_AFE_ClearAllProtectFlag();	// 清零AFE状态寄存器
}

/*******************************************************************************
* 函数名: ResetInit
* 参  数: 无
* 返回值: 无
* 描  述: MCU复位
*******************************************************************************/
void ResetInit(void)
{
	IWDG_Refresh();					// 喂狗
	__disable_irq();				// 关闭总中断
	SYS_EXCOM_ABORT();				// 系统对外通讯操作全部终止
	NVIC_SystemReset();				// MCU复位
}

/*************************************************************************************************
* 函数名: SystemResetProcess
* 参  数: 无
* 返回值: 无
* 描  述: 系统复位处理，当接收到上位机复位命令时复位MCU
*************************************************************************************************/
void SystemResetProcess(void)
{
	//收到上位机复位命令并回复完Ack
	if((ucResetFlg == 0x12) && bUart2SndAckFlg)
	{
		if(!bWriteFlashFlg)				// 等待Flash写入完成
		{
			ResetInit();				// Reset MCU
		}
	}
}
