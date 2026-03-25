/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "SPIApp.h"
#include "spi.h"

BOOL bAfeSPIRWErrFlg = 0;		// SPI通讯失败标志位

/*************************************************************************************************
* 函数名: SH_AFE_SPI_TransmitReceive
* 参  数: TxData指向传输数据缓冲区的指针；
          RxData指向接收数据缓冲区的指针；
		  Size：要发送和接收的数据量
* 返回值: 无
* 描  述: 以阻塞模式，发送和接收一定量的数据
*************************************************************************************************/
BOOL SH_AFE_SPI_TransmitReceive(U8 *TxData, U8 *RxData, U16 Size)
{
	BOOL Result= false;
	
	GPIO_SPI_CSEN();		// SPI_CS脚拉低，开始SPI通讯
	Delay5us(1);
	if(!HAL_SPI_TransmitReceive(&hspi1, TxData, RxData, Size, 0xFFFFFFFF))
	{
		Result= true;
	}
	Delay5us(1);
	GPIO_SPI_CSDis();		// SPI_CS脚拉高，结束SPI通讯
	
	return Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_SPI_Read
* 参  数: RegAddr：寄存器地址；
          RdLen：读取数据长度; 
          RdBuf：读取数据
* 返回值: 无
* 描  述: SPI读AFE寄存器（读时序）
          读操作发送Len+5个Bytes（1BytesCMD码+1Bytes寄存器地址+1Bytes数据长度+RdLen个Bytes数据+1ByteCRC）
*************************************************************************************************/
BOOL SH_AFE_SPI_Read(U8 RegAddr, U8 *RdBuf, U8 RdLen)
{
	BOOL Result= false;
	U8 i;
	U8 AFETxBuf[RdLen+5];	// 定义读操作时的发送数组
	U8 AFERxBuf[RdLen+5];	// 定义读操作时的接收数组
    
	AFETxBuf[0] = AFE_READ_CMD;
	AFETxBuf[1] = RegAddr;
	AFETxBuf[2] = RdLen;
	for(i = 3; i <= RdLen+4; i++ )
	{
		AFETxBuf[i] = 0x00;
	}
    
	if(SH_AFE_SPI_TransmitReceive(AFETxBuf,AFERxBuf,RdLen+5))
	{
		// 判定MISO端口接收的数据是否正确，读操作是否成功
		if((AFERxBuf[0] == 0xFF) && (AFERxBuf[1] == AFE_READ_CMD) && (AFERxBuf[2] == RegAddr) && (AFERxBuf[3] == RdLen) 
			&& (AFERxBuf[RdLen+4] == CRC8Calcu(AFERxBuf, RdLen+4)))
		{
			if(RdLen >= 1)
			{
				for(i=0; i<RdLen; i++)
				{
					#ifdef _USE_32BIT_MCU_
					if((RegAddr >= 0x5D) && (RegAddr <= 0x96))			// 32位机，小端对其
					{
						*(RdBuf+i+1) = AFERxBuf[4+i];
						*(RdBuf+i) = AFERxBuf[4+i+1];
						i++;
					}
					else
					#endif
					{
						*(RdBuf+i) = AFERxBuf[4+i];
					}
				}
			}
			Result = true;
		}
	}
	return Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_SPI_Write 
* 参  数: RegAddr：寄存器地址；
          WrBuf/RdBuf：写Buf数据
* 返回值: 无
* 描  述: SPI写AFE寄存器（写时序）
          写操作固定5Bytes（1BytesCMD码+1Bytes寄存器地址+1Bytes数据+1ByteCRC+0x00）
*************************************************************************************************/
BOOL SH_AFE_SPI_Write(U8 RegAddr, U8 *WrData)
{
	BOOL Result= false;
	U8 AFETxBuf[5];		// 定义写操作时的发送数组
	U8 AFERxBuf[5];		// 定义写操作时的接收数组
    
	AFETxBuf[0] = AFE_WRITE_CMD;
	AFETxBuf[1] = RegAddr;
	AFETxBuf[2] = *WrData;
	AFETxBuf[3] = CRC8Calcu(AFETxBuf, 3);
	AFETxBuf[4] = 0x00;
    
	if(SH_AFE_SPI_TransmitReceive(AFETxBuf,AFERxBuf,5))
	{
		// 判定MISO端口接收的数据是否正确，写操作是否成功
		if((AFERxBuf[0] == 0xFF) && (AFERxBuf[1] == AFE_WRITE_CMD) && (AFERxBuf[2] == RegAddr) && (AFERxBuf[3] == *WrData) 
			&& (AFERxBuf[4] == 0xA5))
		{
			Result = true;
		}
	}
	return Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_SPI_SoftReset
* 参  数: 无
* 返回值: 无
* 描  述: SPI写读AFE寄存器（软件复位时序）
          写操作固定5Bytes（1BytesCMD码+0xBB+0xCC+1ByteCRC+0x00）
*************************************************************************************************/
BOOL SH_AFE_SPI_SoftReset(void)
{
	BOOL Result= false;
	U8 AFETxBuf[5];
	U8 AFERxBuf[5];
	
	AFETxBuf[0] = AFE_RESET_CMD;
	AFETxBuf[1] = 0xBB;
	AFETxBuf[2] = 0xCC;
	AFETxBuf[3] = CRC8Calcu(AFETxBuf, 3);
	AFETxBuf[4] = 0x00;
    
	if(SH_AFE_SPI_TransmitReceive(AFETxBuf,AFERxBuf,5))
	{
		// 判定MISO端口接收的数据是否正确，软件复位操作是否成功
		if((AFERxBuf[0] == 0xFF) && (AFERxBuf[1] == AFE_RESET_CMD) && (AFERxBuf[2] == 0xBB) && (AFERxBuf[3] == 0xCC) 
			&& (AFERxBuf[4] == 0xA5))
		{
			Result = true;
		}
	}
	return Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_Write
* 参  数: RegAddr：寄存器地址
		  WrData：写的数据：1Byte数据
* 返回值: 返回是否成功获取的状态
* 描  述: 写AFE寄存器，每次写一个寄存器
*************************************************************************************************/
BOOL SH_AFE_Write(U8 RegAddr, U8 *WrData)
{
    BOOL Result = false;
    U8 Times = 0;

    //循环写寄存器5次
    while(Times++ < TRY_TIMES)
    {
		Result = SH_AFE_SPI_Write(RegAddr, WrData);
		if(Result)
		{
			break;
		}
		else
		{
			Delay1ms(1);
		}
    }
    bAfeSPIRWErrFlg = !Result;
    return  Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_SoftReset
* 参  数: 无
* 返回值: 返回是否成功获取的状态
* 描  述: 软件复位前端AFE
*************************************************************************************************/
BOOL SH_AFE_SoftReset(void)
{
    BOOL Result = false;
    U8 Times = 0;

    // 循环写寄存器5次
    while(Times++ < TRY_TIMES)
    {
		Result = SH_AFE_SPI_SoftReset();
		if(Result)
		{
			break;
		}
		else
		{
			Delay1ms(1);
		}
		Delay1ms(10);
    }
    
    return  Result;
}

/*************************************************************************************************
* 函数名: SH_AFE_Read
* 参  数: RegAddr：寄存器地址
		  RdBuf：读的数据
		  RdLen：读的长度
* 返回值: 返回是否成功获取的状态
* 描  述: 读AFE寄存器，根据读长度返回读取的寄存器
*************************************************************************************************/
BOOL SH_AFE_Read(U8 RegAddr, U8 *RdBuf,U8 RdLen)
{
    BOOL Result = false;
    U8 Times = 0;
    
    while(Times++ < TRY_TIMES)					//如果读数出错，则Try 5次
    {
		Result = SH_AFE_SPI_Read(RegAddr, RdBuf, RdLen);	
		if(Result)								//如果读数成功且CRC校验正确，则退出循环返回成功标标志
		{
			break;
		}
		else									//如果SPI读数或CRC校验失败，则延时1mS之后再读取
		{
			Delay1ms(1);
		}
    }
    
	bAfeSPIRWErrFlg = !Result;
    return  Result;
}

