/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "IAP.h"
#include "usart.h"
#include "flash.h"
#include "InitialApp.h"
#include "Uart2App.h"

BOOL bIAPFlg = 0;					// 系统处于IAP更新状态中

BOOL bIapIspFlg;					// 0：表示当前执行IAP操作；1：表示当前执行ISP操作
BOOL bHandsheakOkFlg;

U16 usReceCheckSum = 0;				// IAP数据帧校验
U8	ucUart2ErrCode = 0;				// IAP错误代码

U32	uiIapChksum;
U8 ucIapRestCommand = 0;			// 暂存中断接收的IAP命令
volatile U32 uiIapDataPtr = 0;		// 偏移地址
volatile U32 uiIapRecDataLen = 0;	// IAP文件大小
volatile U8 ucIndexBackup = 0;		// IAP接收到封包索引

IapCtrlMap IapCtrlClass;

U8 ucIapBuf[MCU_CODE_SECTOR_SIZE];

/*************************************************************************************************
* 函数名: IAPParaReset
* 参  数: 无
* 返回值: 无
* 描  述: IAP相关控制变量复位
*************************************************************************************************/
void IAPParaReset(void)
{
	uiIapRecDataLen = 0;
	ucIndexBackup = 0;
	uiIapDataPtr = 0;
}

/*************************************************************************************************
* 函数名: ISPProcess
* 参  数: 无
* 返回值: 无
* 描  述: IAP的接口函数
*************************************************************************************************/
void IAPProcess(void)
{
	if(bIAPFlg && (ucIapRestCommand == CMD_MCU_RESET) && bUart2SndAckFlg)
	{	
		bIAPFlg = 0;
		bUart2SndAckFlg = 0;
		ResetInit();
	}
}

/*************************************************************************************************
* 函数名: UartSendIAPAck
* 参  数: 无
* 返回值: 无
* 描  述: IAP的接口函数
*************************************************************************************************/
void UartSendIAPAck(void)
{
	U8 i, DataBak;
    U16 CheckSum = 0;
	
	ucUart2Buf[LENGTH] = 0;
	ucUart2Buf[COMMAND] = 0x0B;
	ucUart2Buf[INDEXES] = ucUart2ErrCode;
	
	DataBak = ucUart2Buf[SOURCE];                        // 交换源ID和目标ID
	ucUart2Buf[SOURCE] = ucUart2Buf[TARGET];
	ucUart2Buf[TARGET] = DataBak;
            
	for(i=2; i<(ucUart2Buf[LENGTH]+7); i++)
	{
		CheckSum += ucUart2Buf[i];
	}
            
	ucUart2Buf[7+ucUart2Buf[LENGTH]] = (U8)CheckSum;
	ucUart2Buf[8+ucUart2Buf[LENGTH]] = (U8)(CheckSum>>8);
	
	HAL_UART_Transmit_IT(&huart2, &ucUart2Buf[ucUart2BufPT], 1);
}

/*************************************************************************************************
* 函数名: IapHandShake
* 参  数: 无
* 返回值: 无
* 描  述: IAP升级，握手
*************************************************************************************************/
void IapHandShake(void)
{
	if(ucUart2Buf[DATA]=='I' && ucUart2Buf[DATA+1]=='A' && ucUart2Buf[DATA+2]=='P')
	{
		bIapIspFlg = IAP_MODE;
        bHandsheakOkFlg = 1;
	}
	else if(ucUart2Buf[DATA]=='I' && ucUart2Buf[DATA+1]=='S' && ucUart2Buf[DATA+2]=='P')
	{
		bIapIspFlg = ISP_MODE;
        bHandsheakOkFlg = 1;
	}
	else
	{
		ucUart2ErrCode |= IAPERROR_HANDSHAKE;		// 握手失败
        bHandsheakOkFlg = 0;
	}
}
/*************************************************************************************************
* 函数名: IapBeginAck
* 参  数: 无
* 返回值: 无
* 描  述: IAP升级开始
*************************************************************************************************/
void IapBeginAck(void)
{
	uiIapDataPtr = 0;
	uiIapChksum = 0;
	uiIapRecDataLen = 0;                          // 数据长度
	
	uiIapRecDataLen = ((U32)ucUart2Buf[DATA+3]<<24)
					| ((U32)ucUart2Buf[DATA+2]<<16)
					| ((U32)ucUart2Buf[DATA+1]<<8)
					| (U32)ucUart2Buf[DATA];
	
    if((uiIapRecDataLen > IAP_BK_CODE_SIZE) && (bIapIspFlg == IAP_MODE))
	{
		ucUart2ErrCode |= IAPERROR_SIZE;							// 如果数据长度不等于IAP和ISP长度，则默认为长度异常
	}
	else
	{
		FlashRead(FLASH_IAP_CTRL_ADDR, (U8 *)&IapCtrlClass, IAP_CTRLMAP_SIZE);	// 读取CtrlMap区域
		IapCtrlClass.usUpdateFlg = 0;
		
		__disable_irq();
		if(FlashErase(FLASH_IAP_CTRL_ADDR, IAP_CTRLMAP_SIZE))			// 擦除并写入CtrlMap区域
		{
			if(FlashWrite(FLASH_IAP_CTRL_ADDR, (U8 *)&IapCtrlClass, IAP_CTRLMAP_SIZE))
			{
				if(!FlashErase(FLASH_APP_BK_ADDR, IAP_BK_CODE_SIZE))	// 擦除临时程序区
				{
					ucUart2ErrCode |= IAPERROR_ERASE;
				}
				else
				{
					ucIapRestCommand = CMD_IAP_BEGIN;
					IAPParaReset();
					
					bIAPFlg = 1;
				}
			}
			else
			{
				ucUart2ErrCode |= IAPERROR_WR;					
			}
		}
		else
		{
			ucUart2ErrCode |= IAPERROR_ERASE;
		}
		__enable_irq();
	}	
}

/*************************************************************************************************
* 函数名: IapReceiveData
* 参  数: 无
* 返回值: 无
* 描  述: IAP升级，数据传输
*************************************************************************************************/
void IapReceiveData(void)
{
	U16 i;
	U8 j;
	
    bHandsheakOkFlg = 0;
    
    if( ((ucUart2Buf[INDEXES]<ucIndexBackup) || (ucUart2Buf[INDEXES]>(ucIndexBackup+4))) && (ucUart2Buf[INDEXES] != 0) )
	{
		ucUart2ErrCode |= IAPERROR_INDEX;						// 数据索引错误，首先判断是否连续，其次判断是否超出范围
	}
	else
	{
		ucIndexBackup = ucUart2Buf[INDEXES];
		
		if(ucUart2Buf[LENGTH] == 0)
		{
			uiIapDataPtr += 512;									// 如果传递的长度为0，表示当前512个字节为0，指针加，但不写
		}
		else
		{
			j = ucIndexBackup % 4;									// 目前暂定sector长度为512bytes，每次传输128bytes，所以定义为4
            for(i=0; i<ucUart2Buf[LENGTH]; i++)
			{
				ucIapBuf[i+(U16)j*ucUart2Buf[LENGTH]] = ucUart2Buf[DATA+i];
			}
			if(j == 3)								// 连续接收满1个sector，才进行写入操作
			{
				if(!FlashWrite(uiIapDataPtr+FLASH_APP_BK_ADDR, ucIapBuf, MCU_CODE_SECTOR_SIZE))
				{
					if(!FlashWrite(uiIapDataPtr+FLASH_APP_BK_ADDR, ucIapBuf, MCU_CODE_SECTOR_SIZE))
					{
						ucUart2ErrCode |= IAPERROR_WR;				// 如果连续写两次错误，则返回给上位机异常
					}
				}
				else
				{
					uiIapDataPtr += 512;
					
					ucIapRestCommand = CMD_IAP_TRANS;
					
					for(j=0; j<MCU_CODE_SECTOR_SIZE/4; j++)			// 计算checksum需要按照32bit，尽量兼容STM32
					{
						uiIapChksum += (((U32)ucIapBuf[j*4+3]<<24)
									  | ((U32)ucIapBuf[j*4+2]<<16)
									  | ((U32)ucIapBuf[j*4+1]<<8)
									  | ((U32)ucIapBuf[j*4+0]));
					}
				}
			}
		}
	}
}
/*************************************************************************************************
* 函数名: IapRDataVerify
* 参  数: 无
* 返回值: 无
* 描  述: IAP升级，数据校验
*************************************************************************************************/
void IapRDataVerify(void)
{
	U32 CheckSum = 0;
	
	CheckSum =	  ((U32)ucUart2Buf[DATA+3]<<24)				// 获取上位机下发的校验和
				| ((U32)ucUart2Buf[DATA+2]<<16)
				| ((U32)ucUart2Buf[DATA+1]<<8)
				| ((U32)ucUart2Buf[DATA+0]);
	
	if(uiIapChksum != CheckSum)								// 数据校验失败
	{
		ucUart2ErrCode |= IAPERROR_CRC;
	}
	else
	{
		ucIapRestCommand = CMD_IAP_VERIFY;
	}
}

/*************************************************************************************************
* 函数名: IapCmdReset
* 参  数: 无
* 返回值: 无
* 描  述: IAP升级，系统复位
*************************************************************************************************/
void IapCmdReset(void)
{
	ucUart2Buf[INDEXES] = 0;
	
	FlashRead(FLASH_IAP_CTRL_ADDR, (U8 *)&IapCtrlClass, IAP_CTRLMAP_SIZE);			// 读取CtrlMap
	IapCtrlClass.usUpdateFlg = IAP_UPGRADE_FLG;
			
	__disable_irq();
	if(FlashErase(FLASH_IAP_CTRL_ADDR, IAP_CTRLMAP_SIZE))
	{
		if(FlashWrite(FLASH_IAP_CTRL_ADDR, (U8 *)&IapCtrlClass, IAP_CTRLMAP_SIZE))	// 写入CtrlMap
		{
			IAPParaReset(); 
			ucIapRestCommand = CMD_MCU_RESET;
		}
		else
		{
			ucUart2ErrCode |= IAPERROR_WR;
		}
	}
	else
	{
		ucUart2ErrCode |= IAPERROR_ERASE;
	}
	__enable_irq();
	
}

/*************************************************************************************************
* 函数名: UartIAPCmdProcess
* 参  数: 无
* 返回值: 无
* 描  述: IAP升级处理程序
*************************************************************************************************/
void UartIAPCmdProcess(void)
{
	if(ucUart2BufPT < (ucUart2Buf[LENGTH]+9))
	{
		if(ucUart2BufPT <= (ucUart2Buf[LENGTH]+7))
		{
			usReceCheckSum += ucUart2Buf[ucUart2BufPT-1];
		}
		
		if(ucUart2BufPT == (TARGET+1))						// 检查ID
		{
			if(ucUart2Buf[TARGET] != IAP_BMSID)
			{
				ucUart2BufPT = 0;
			}
		}
		else if(ucUart2BufPT == (COMMAND+1))				// 检测COMMAND
		{
			if((ucUart2Buf[COMMAND] != IAP_CMD_HANDSHAKE)
				&& (ucUart2Buf[COMMAND] != IAP_CMD_BEGIN)
				&& (ucUart2Buf[COMMAND] != IAP_CMD_TRANS)
				&& (ucUart2Buf[COMMAND] != IAP_CMD_VERIFY)
				&& (ucUart2Buf[COMMAND] != IAP_CMD_RESET))
			{
				ucUart2ErrCode |= IAPERROR_CMD;
			}
		}
	}
	else
	{
		ucUart2BufPT = 0;
		if(usReceCheckSum != ((ucUart2Buf[ucUart2Buf[LENGTH]+8]<<8) + ucUart2Buf[ucUart2Buf[LENGTH]+7]))
		{
			ucUart2ErrCode |= IAPERROR_CHECKSUM;
		}
				
		switch(ucUart2Buf[COMMAND])
		{
			case IAP_CMD_HANDSHAKE:			// 握手(IAP Handshake)
				IapHandShake();
				break;
			
			case IAP_CMD_BEGIN:				// 开始(IAP Begin)
				IapBeginAck();
				break;
			
			case IAP_CMD_TRANS:				// 数据传输(IAP TRANS)
				IapReceiveData();
				break;
				
			case IAP_CMD_VERIFY:			// 数据校验(IAP VERIFY)
				IapRDataVerify();
				break;
			
			case IAP_CMD_RESET:				// 系统复位(IAP RESET)
				IapCmdReset();
				break;
			default:
				break;
		}
		UartSendIAPAck();
	}
}





