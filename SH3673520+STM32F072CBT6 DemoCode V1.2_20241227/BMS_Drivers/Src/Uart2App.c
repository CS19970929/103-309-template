/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2020/03/02
History:
	V0.0		2020/03/02		 Preliminary
********************************************************************************/
#include "Uart2App.h"
#include "usart.h"
#include "flash.h"
#include "IAP.h"
#include "Calibrate.h"

BOOL bUart2SndAckFlg = 0;				// UART已经发送ACK给主机
BOOL bUart2ReadFlg = 0;					// 上位机读指令
BOOL bUart2WriteFlg = 0;				// 上位机写指令
BOOL bUart2HandShakeFlg = 0;			// 有上位机通讯的标志位
U8 ucUart2Buf[256];
U8 ucRxData = 0;						// 接收中断缓冲
U8 ucUart2BufPT = 0;					// 接收缓冲计数
U8 ucResetFlg = 0;						// 软件复位标志位
U8 ucSubClassID = 0; 					// 子类命令号
U8 ucCmdAFEMode = 0;					// AFE进入不同模式，上位机指令

static const size_t ucPageOfSubClass[] = {
	offsetof(PARAMETER, E2usSubclass00),
	0,//	offsetof(PARAMETER, E2usSubclass01),
	0,//	offsetof(PARAMETER, E2usSubclass02),
	0,//	offsetof(PARAMETER, E2usSubclass03),
	0,//	offsetof(PARAMETER, E2usSubclass04),
	0,//	offsetof(PARAMETER, E2usSubclass05),
	0,//	offsetof(PARAMETER, E2usSubclass06),
	0,//	offsetof(PARAMETER, E2usSubclass07),
	0,//	offsetof(PARAMETER, E2usSubclass08),
	0,//	offsetof(PARAMETER, E2usSubclass09),
	offsetof(PARAMETER, E2usSubclass0A), 
	offsetof(PARAMETER, E2usSubclass0B),
//	offsetof(PARAMETER, E2usSubclass0C),
//	offsetof(PARAMETER, E2usSubclass0D),
};


/*************************************************************************************************
* 函数名: UartWriteInfo
* 参  数: ptr：数据需要写入的起始地址
* 返回值: 无
* 描  述: UART写数据
*************************************************************************************************/
void UartWriteInfo(U8 *ptr)
{
	U8 i;
	
	if(ucUart2Buf[3+ucUart2Buf[UART_LENGTH]] == CRC8Calcu((U8 *)&ucUart2Buf, ucUart2Buf[UART_LENGTH]+3))
	{
		for(i=0; i<ucUart2Buf[UART_LENGTH]; i++)
		{
			IWDG_Refresh();
			*ptr = ucUart2Buf[3+i];
			ptr++;
		}
        
		if(ucSubClassID == SUBCLASSID_AFE)
		{
			bAFEWrFlag = 1;				// 当更新AFE的配置参数时，更新AFE寄存器
		}
		
		bMcuFlashWrWaitFlg = 1;         // 更新参数后，需要写入FLASH
		ucMcuFlashWrWaitCnt = 0;
		
		Uart2SendAck();
	}
	else
	{
		Uart2SendNack();
	}
}

/*************************************************************************************************
* 函数名: UartReadInfo
* 参  数: ptr：数据需要读出的起始地址
* 返回值: 无
* 描  述: UART读数据
*************************************************************************************************/
void UartReadInfo(U8 *ptr)
{
	U8 i;

	if(ucUart2Buf[UART_LENGTH] > 140)
	{
		ucUart2Buf[UART_LENGTH] = 0;
	}
    
	for(i=0; i<ucUart2Buf[UART_LENGTH]; i++)
	{
		IWDG_Refresh();
		ucUart2Buf[3+i] =  *ptr;
		ptr++;
	}
	ucUart2Buf[3+ucUart2Buf[UART_LENGTH]] = CRC8Calcu((U8 *)&ucUart2Buf,ucUart2Buf[UART_LENGTH]+3);
	
	HAL_UART_Transmit_IT(&huart2, &ucUart2Buf[ucUart2BufPT], 1);		// 启动发送数据
}

/*************************************************************************************************
* 函数名: ReadSubClassID
* 参  数: 无
* 返回值: 无
* 描  述: UART解析子命令号
*************************************************************************************************/
void ReadSubClassID(void)
{
	if(ucUart2Buf[3+ucUart2Buf[UART_LENGTH]] == CRC8Calcu((U8 *)&ucUart2Buf, ucUart2Buf[UART_LENGTH]+3))
	{
		ucSubClassID=ucUart2Buf[3];
		Uart2SendAck();
	}
	else
	{
		Uart2SendNack();
	}
}

/*************************************************************************************************
* 函数名: Uart2RdCmdProcess
* 参  数: 无
* 返回值: 无
* 描  述: UART2读命令处理函数
*************************************************************************************************/
void Uart2RdCmdProcess(void)
{
    switch(ucUart2Buf[UART_CMD_NO])
    {
        U8 *rdaddr;
        
        case CELL1:
        case CELL2:
        case CELL3:
        case CELL4:
        case CELL5:
        case CELL6:
        case CELL7:
        case CELL8:
        case CELL9:
        case CELL10:
        case CELL11:
        case CELL12:
        case CELL13:
        case CELL14:
        case CELL15:
        case CELL16:
        case CELL17:
        case CELL18:
        case CELL19:
        case CELL20:
			UartReadInfo((U8 *)&Info.ssVCell[ucUart2Buf[UART_CMD_NO]-1]);
			break;   
		
		case CUL_TOTAL_VOLTAGE:
			UartReadInfo((U8 *)&Info.uiVoltage1);
			break;
		
		case TOTAL_VOLTAGE:
			UartReadInfo((U8 *)&Info.uiVoltage2);
			break;
        
		case CADC_CURRENT:
			UartReadInfo((U8 *)&Info.siCADCCurr);
			break;
		
		case VADC_CURRENT:
			UartReadInfo((U8 *)&Info.siVADCCurr);
			break;
 
		case EXT_TEMP1:
			UartReadInfo((U8 *)&Info.usTemp[0]);
			break;

		case EXT_TEMP2:
			UartReadInfo((U8 *)&Info.usTemp[1]);
			break;

		case EXT_TEMP3:
			UartReadInfo((U8 *)&Info.usTemp[2]);
			break;
		
		case EXT_TEMP4:
			UartReadInfo((U8 *)&Info.usTemp[3]);
			break;

		case IC_TEMP:
			UartReadInfo((U8 *)&Info.usTempI);
			break;

		case BAL_CHANNEL:
			UartReadInfo((U8 *)&Info.uiBALChannel);
			break;
		
		case CTO_CHANNEL:
			UartReadInfo((U8 *)&Info.uiCTOChannel);
			break;
		
		case PACK_STATUS:
			UartReadInfo((U8 *)&Info.usPackStatus);
			break;

		case BATTERY_STATUS:
			UartReadInfo((U8 *)&Info.usBatStatus);
			break;
		
		case PACK_CONFIG:
			UartReadInfo((U8 *)&Info.usPackConfig);
			break;
		
		case SOFT_VERSION:
			UartReadInfo((U8 *)&Info.usSoftVersion);
			break;
		
		case MANUFACTURE_COMMAND:
			UartReadInfo((U8 *)&Info.usManufactureAccess);
			break;
		
		case SUB_PAGE1:		// read extern EEPRom data
			rdaddr = (U8 *)&parameter + ucPageOfSubClass[ucSubClassID];
			UartReadInfo(rdaddr);
			break;
		
		case SUB_PAGE2:
			rdaddr = (U8 *)&parameter + ucPageOfSubClass[ucSubClassID] + 32;
			UartReadInfo(rdaddr);
			break;
		
        default:
			break;
    }                   
}

/*************************************************************************************************
* 函数名: WriteManufacture
* 参  数: 无
* 返回值: 无
* 描  述: 上位机通过UART写入自定义命令
			0x41：系统复位
			0x05：系统进入PD
*************************************************************************************************/
void WriteManufacture(void)
{ 
	if(ucUart2Buf[3+ucUart2Buf[UART_LENGTH]] == CRC8Calcu((U8 *)&ucUart2Buf, ucUart2Buf[UART_LENGTH]+3))
	{
		switch(ucUart2Buf[3])
		{
			case AFE_NORMAL:
				ucCmdAFEMode = 0x01;
				break;
			
			case AFE_IDLE:
				ucCmdAFEMode = 0x02;
				break;
			
			case AFE_SLEEP:
				ucCmdAFEMode = 0x03;
				break;
			
			case AFE_PD:
				ucCmdAFEMode = 0x04;
				break;
						
			case AFE_RESET:
				ucResetFlg = 0x12;
				break;
			default:
				break;
		}
		Uart2SendAck();
	}
	else
	{
		Uart2SendNack();
	}
}

/*************************************************************************************************
* 函数名: UartCaliVoltage
* 参  数: 无
* 返回值: 无
* 描  述: UART2通讯发送校准电压的数据
*************************************************************************************************/
void UartCaliVoltage(void)
{	
	if(ucUart2Buf[3+ucUart2Buf[UART_LENGTH]] == CRC8Calcu((U8 *)&ucUart2Buf, ucUart2Buf[UART_LENGTH]+3))
	{
		uiExtVPack = ((U32)ucUart2Buf[6]<<24)
                        | ((U32)ucUart2Buf[5]<<16)
                        | ((U32)ucUart2Buf[4]<<8)
                        | ((U32)ucUart2Buf[3]);
        
        if(!uiExtVPack)
        {
            Uart2SendNack();
        }
        else
        {
            bCaliFlg = 1;
            ucExtcaliSwitch1 |= 0x01;
            Uart2SendAck();
        }
	}
	else
	{
		Uart2SendNack();
	}
}

/*************************************************************************************************
* 函数名: UartCaliCurrent
* 参  数: 无
* 返回值: 无
* 描  述: UART2通讯发送校准电流增益的数据
*************************************************************************************************/
void UartCaliCurrent(void)
{
	if(ucUart2Buf[3+ucUart2Buf[UART_LENGTH]] == CRC8Calcu((U8 *)&ucUart2Buf, ucUart2Buf[UART_LENGTH]+3))
	{
		siExtCur = ((U32)ucUart2Buf[6]<<24)
						| ((U32)ucUart2Buf[5]<<16)
						| ((U32)ucUart2Buf[4]<<8)
						| ((U32)ucUart2Buf[3]);
        
        if((!AFE.ssCADCCurr) || (!siExtCur))
        {
            Uart2SendNack();
        }
        else
        {
            bCaliFlg = 1;
            ucExtcaliSwitch1 |= 0x04;       // 校准电流增益
            Uart2SendAck();
        }
	}
	else
	{
		Uart2SendNack();
	}
}

/*************************************************************************************************
* 函数名: UartCaliCurOffset
* 参  数: 无
* 返回值: 无
* 描  述: UART2通讯发送校准电流Offset的数据
*************************************************************************************************/
void UartCaliCurOffset(void)
{
	if(ucUart2Buf[3+ucUart2Buf[UART_LENGTH]] == CRC8Calcu((U8 *)&ucUart2Buf, ucUart2Buf[UART_LENGTH]+3))
	{
		bCaliFlg = 1;
		ucExtcaliSwitch1 |= 0x08;           // 校准电流offset

		Uart2SendAck();
	}
	else
	{
		Uart2SendNack();	
	}
}

/*************************************************************************************************
* 函数名: UartCaliTSn
* 参  数: TS_Num:需要校准的外部温度序号(0~3)
* 返回值: 无
* 描  述: UART通讯发送校准TSn(1~4)数据
*************************************************************************************************/
void UartCaliTSn(U8 TS_Num)
{
	if(ucUart2Buf[3+ucUart2Buf[UART_LENGTH]] == CRC8Calcu((U8 *)&ucUart2Buf, ucUart2Buf[UART_LENGTH]+3))
	{
		bCaliFlg = 1;
		uiExtTemp[TS_Num] = ((U16)ucUart2Buf[4]<<8)|ucUart2Buf[3];
		ucExtcaliSwitch1 |= 0x10<<TS_Num;			// 校准TS1~4
		
		Uart2SendAck();
	}
	else
	{
		Uart2SendNack();	
	}
}

/*************************************************************************************************
* 函数名: Uart2WrCmdProcess
* 参  数: 无
* 返回值: 无
* 描  述: UART2写命令处理函数
*************************************************************************************************/
void Uart2WrCmdProcess(void)
{
    U8 *rdaddr;
    
    switch(ucUart2Buf[UART_CMD_NO])
    {
        case MANUFACTURE_COMMAND:
            WriteManufacture();
        break;
        case DATA_FLASH_COMMAND:  
            ReadSubClassID();			// 接收的命令是DataFlash命令0x77
        break;
        case CALI_VOL_COMMAND:          // 校准电压命令
            UartCaliVoltage();
        break;
        case CALI_CUR_COMMAND:          // 校准电流Gain命令
            UartCaliCurrent();
        break;
        case CALI_ZERO_CUR_COMMAND:     // 校准电流offset命令
            UartCaliCurOffset();
        break;
        case CALI_TS1_COMMAND:          // 校准温度1~4
        case CALI_TS2_COMMAND:
        case CALI_TS3_COMMAND:
        case CALI_TS4_COMMAND:
            UartCaliTSn(ucUart2Buf[UART_CMD_NO]-CALI_TS1_COMMAND);
        break;
        case SUB_PAGE1:
            rdaddr = (U8 *)&parameter + ucPageOfSubClass[ucSubClassID];
            UartWriteInfo(rdaddr);
        break;
        case SUB_PAGE2:
            rdaddr = (U8 *)&parameter + ucPageOfSubClass[ucSubClassID] + 32;
            UartWriteInfo(rdaddr);
        break;
        
		default:
		break;
    }
}



/*************************************************************************************************
* 函数名: UartReceive
* 参  数: 无
* 返回值: 无
* 描  述: Uart中断回调函数（接收数据）
*************************************************************************************************/
void UartReceive(void)
{
    ucUart2Buf[ucUart2BufPT++] = ucRxData;						// 接收数据转存
    
    if(ucUart2BufPT == 1)
	{
		if((ucUart2Buf[UART_SLAVE_ADDR]&0xFE) == SADDR)			// 接收的第一个字节是否和UART地址匹配
		{
			bUart2HandShakeFlg = 1;
			if((ucUart2Buf[UART_SLAVE_ADDR]&0x01)==0)			// bit7是R/W标志；0--R, 1--W
			{
				bUart2ReadFlg = 1;
                bUart2WriteFlg = 0;
			}
			else
			{
				bUart2WriteFlg = 1;
                bUart2ReadFlg = 0;
			}
		}
	}
	
	if(bUart2ReadFlg)
	{
		if(ucUart2BufPT == 3)
		{
			bUart2ReadFlg = 0;
            Uart2RdCmdProcess();								// 读操作处理
		}
	}
	else if(bUart2WriteFlg)
	{
		if(ucUart2BufPT > (ucUart2Buf[UART_LENGTH]+3))			// 如果是写操作，且所有数据已被接收
		{
			Uart2WrCmdProcess();								// 写操作处理
			bUart2WriteFlg = 0;
			ucUart2BufPT = 0;
		}
	}
	else														// 非读非写操作时，即为ISP或者IAP的握手命令
	{
		if(ucUart2BufPT >= 140)									// 该指针不会超过140
		{
			ucUart2BufPT = 0;
		}
		
		if(ucUart2BufPT == 1)
		{
			if(ucUart2Buf[HEARD1] != 0x5A)						// 检查帧头是否为0x5AA5
			{
				ucUart2BufPT = 0;
			}
		}
		else if(ucUart2BufPT == 2)
		{
			if(ucUart2Buf[HEARD2] != 0xA5)
			{
				ucUart2BufPT = 0;
			}
			else
			{
				usReceCheckSum = 0;								// 帧头判断正确
				ucUart2ErrCode = 0;
			}
		}
		else
		{
			UartIAPCmdProcess();
		}
	}
}

/*************************************************************************************************
* 函数名: UartTransmit
* 参  数: 无
* 返回值: 无
* 描  述: Uart中断回调函数（发送数据）
*************************************************************************************************/
void UartTransmit(void)
{
	if((ucUart2Buf[HEARD1] == 0x5A) && (ucUart2Buf[HEARD2] == 0xA5))
    {
        if(ucUart2BufPT >= (ucUart2Buf[LENGTH]+8))
        {
            ucUart2BufPT = 0;
            
            ucUart2Buf[0] = 0;
            ucUart2Buf[1] = 0;
            ucUart2Buf[2] = 0;
            ucUart2Buf[3] = 0;
            bUart2SndAckFlg = 1;
        }
        else
        {
            ucUart2BufPT++;
			HAL_UART_Transmit_IT(&huart2, &ucUart2Buf[ucUart2BufPT], 1);
        }
    }
    else if((ucUart2BufPT == 0) || (ucUart2BufPT >= ucUart2Buf[UART_LENGTH]+3)) 
	{
		ucUart2BufPT = 0;
		
		ucUart2Buf[0] = 0;
		ucUart2Buf[1] = 0;
		ucUart2Buf[2] = 0;
		ucUart2Buf[3] = 0;
        bUart2SndAckFlg = 1;									// UART已发送过数据，主要用于需要等待发送ACK才能进一步操作的功能
	}
	else
	{
		ucUart2BufPT++;
		HAL_UART_Transmit_IT(&huart2, &ucUart2Buf[ucUart2BufPT], 1);
	}	
}


