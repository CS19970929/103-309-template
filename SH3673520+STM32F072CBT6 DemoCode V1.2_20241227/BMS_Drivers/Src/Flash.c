/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "flash.h"
#include "DataFlash.h"
#include "IAP.h"
#include "AFE.h"
#include "InitialApp.h"

BOOL bAFEWrFlag = 0;
BOOL bWriteFlashFlg;
BOOL bMcuFlashWrWaitFlg;
U8  ucMcuFlashWrWaitCnt;
U8  ucWrFlashDelayCnt;      //FLASH检测写入延时计数

/*************************************************************************************************
* 函数名: FlashPagesCalculate
* 参  数: Length: 字节长度
* 返回值: Page数量
* 描  述: 根据字节数量计算对应的Page数量
*************************************************************************************************/
U32 FlashPagesCalculate(U32 Length)
{
    U32 FlashPageNum;

    if((Length % FLASH_PAGE_SIZE) != 0)
    {
        FlashPageNum = Length/FLASH_PAGE_SIZE + 1;
    }
    else
    {
        FlashPageNum = Length/FLASH_PAGE_SIZE;
    }

    if(FlashPageNum > MAX_FPAGE_NUM)
    {
        FlashPageNum = 0;
    }

	return FlashPageNum;
}

/*************************************************************************************************
* 函数名: FlashErase
* 参  数: Address: 擦除的起始地址（字节）；Length：擦除的长度（最终折算成整数Page数）
* 返回值: 1：成功；0：失败
* 描  述: 根据字节数量计算对应的Page数量，擦除对应的Page
*************************************************************************************************/
BOOL FlashErase(U32 Address, U32 Length)
{
	BOOL Result = false;
    U32 PageError = 0;
    static FLASH_EraseInitTypeDef FlashErasePara;

    HAL_FLASH_Unlock();		//Flash开锁

    FlashErasePara.TypeErase = FLASH_TYPEERASE_PAGES;
    FlashErasePara.PageAddress = Address;
    FlashErasePara.NbPages = FlashPagesCalculate(Length);

    if(HAL_FLASHEx_Erase(&FlashErasePara, &PageError) == HAL_OK)
    {
        Result = true;
    }
	
    HAL_FLASH_Lock();		//Flash上锁

    return Result;
}

/*************************************************************************************************
* 函数名: FlashRead
* 参  数: Address: 读取地址；Readbuff：数据存放Buf；Length：读取长度
* 返回值: 无
* 描  述: 读取Flash数据
*************************************************************************************************/
void FlashRead(U32 Address, U8* Readbuff, U32 Length)
{
    U32 i = 0;

    for(i=0; i<Length; i++)
    {
		Readbuff[i] = ((uint8_t *)Address)[i];
    }
}

/*************************************************************************************************
* 函数名: FlashWrite
* 参  数: Address: 写地址；pData：数据存放Buf；Length：写数据长度
* 返回值: 无
* 描  述: 写Flash数据
*************************************************************************************************/
BOOL FlashWrite(U32 Address, U8 *pData, U32 Length)
{
	BOOL Result = true;
    U32 i;
	
    HAL_FLASH_Unlock();
	
    for(i=0; i<Length; i+=2)		// 每次写入输出为2Bytes
    {
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, Address, *(U16 *)pData) != HAL_OK)
        {
            Result = false;
			break;
        }
		Address += 2;
		pData += 2;
    }

    HAL_FLASH_Lock();
	
	return Result;
}

/*************************************************************************************************
* 函数名: FlashProcess
* 参  数: Address: 写地址；pData：数据存放Buf；Length：写数据长度
* 返回值: 无
* 描  述: 更新Flash参数
*************************************************************************************************/
BOOL FlashProcess(U32 Address, U8 *pData, U32 Length)
{

	BOOL result = 0;
    
    __disable_irq();

    // 将参数写入至备份区
    if (parameter.E2usCheckFlag == 0x5AA5)
    {
        if(FlashErase(Address, Length))       // 擦除
        {
            if(FlashWrite(Address, pData, Length))
            {
                result = 1;
            }
        }
    }	

    __enable_irq();
	
    return result;

}


/*************************************************************************************************
* 函数名: RamCheckProcess
* 参  数: 无
* 返回值: 1：成功；0：失败
* 描  述: 定时检测RAM区的参数是否正确，如果不正确，重新初始化
*************************************************************************************************/
BOOL RamCheckProcess(void)
{
	if((parameter.E2ucRamCheckFlg0  != _RAM_CHECK_DATA)
//	|| (parameter.E2ucRamCheckFlg1  != _RAM_CHECK_DATA)
//	|| (parameter.E2ucRamCheckFlg2  != _RAM_CHECK_DATA)
//	|| (parameter.E2ucRamCheckFlg3  != _RAM_CHECK_DATA)
//	|| (parameter.E2ucRamCheckFlg4  != _RAM_CHECK_DATA)
//	|| (parameter.E2ucRamCheckFlg8  != _RAM_CHECK_DATA)
//	|| (parameter.E2ucRamCheckFlg9  != _RAM_CHECK_DATA)
	|| (parameter.E2ucRamCheckFlgA  != _RAM_CHECK_DATA)
	|| (parameter.E2ucRamCheckFlgB	!= _RAM_CHECK_DATA)
	|| (parameter.E2usCheckFlag != 0x5AA5))
	{
		return false;
	}
	else
	{
		return true;
	}
}    

/*************************************************************************************************
* 函数名: CheckWrFlashProcess
* 参  数: 无
* 返回值: 无
* 描  述: 如果需要写Flash，最长2S会保存
*************************************************************************************************/
void CheckWrFlashProcess(void)
{
	if(bMcuFlashWrWaitFlg)
	{
	 	if(++ucWrFlashDelayCnt >= MCU_FLASH_WATI_DELAY)          // 延时2s保存
		{
		 	ucWrFlashDelayCnt = 0;
			bMcuFlashWrWaitFlg = 0;
			
			FlashProcess(FLASH_PARAA_ADDR, (U8 *)&parameter, DATAFLASH_BLOCK_SIZE);		// 更新备份区1
			FlashProcess(FLASH_PARAB_ADDR, (U8 *)&parameter, DATAFLASH_BLOCK_SIZE);		// 更新备份区2
			
			if(bAFEWrFlag)				// 更新AFE
			{
				bAFEWrFlag = 0;
				
				if(Info.bHighSide)						// 高侧方案时，开启Charge Pump(PUMP_EN = 1)
				{
					parameter.E2ucAFESCONF2 = parameter.E2ucAFESCONF2|0x10;
					
					if(parameter.bPDSGMOS_EN)			// 开启预充电时，配置PDSGMOS=1，预放电功能由SH3673520控制。
					{
						parameter.E2ucAFESCONF2 = parameter.E2ucAFESCONF2|0x04;
					}
				}
				
				SH_AFE_RegisterInit();
				
				Info.ucCellNum = (parameter.E2ucAFESCONF4 & 0x1F);		// 根据配置初始化电芯串数和Offset
				if ((Info.ucCellNum >= 20) || (Info.ucCellNum < 4))
				{
					Info.ucCellNum = 20;
				}  
			}
		}
	}
	else
	{
		ucWrFlashDelayCnt = 0;
	}
}

