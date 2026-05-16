#ifndef FLASH_H
#define FLASH_H

#include "Project_Template_Config.h"

//系统存储器：出厂由ST在这个区域内部预置了一段BootLoader，也就是ISP程序，这是一块ROM
//没有什么中低容量产品，只会用C8和R8，均为64K，	 每页1KB。系统存储器3KB。
#define FLASH_ADDR_IAP_START            PROJECT_CFG_FLASH_IAP_START
#define FLASH_ADDR_APP_START            PROJECT_CFG_FLASH_APP_START


// #define FLASH_ADDR_SOC_RTC_CNT          0x0800F000		//RTC次数统计，用于是否更新SOC

#define FLASH_ADDR_WAKE_TYPE            PROJECT_CFG_FLASH_FLAG_WAKE_TYPE
#define FLASH_ADDR_UPDATE_FLAG          PROJECT_CFG_FLASH_FLAG_UPDATE
#define FLASH_ADDR_SLEEP_FLAG           PROJECT_CFG_FLASH_FLAG_SLEEP

#define FLASH_TO_IAP_VALUE				((UINT16)0x00AB)
#define FLASH_TO_APP_VALUE				((UINT16)0xFFFF)

#define FLASH_VALUE_WAKE_RTC            ((UINT16)0x1234)
#define FLASH_VALUE_WAKE_OTHER          ((UINT16)0xFFFF)

#define FLASH_NORMAL_SLEEP_VALUE    	((UINT16)0x1234)
#define FLASH_DEEP_SLEEP_VALUE    		((UINT16)0x1235)
#define FLASH_HICCUP_SLEEP_VALUE    	((UINT16)0x1236)
#define FLASH_SLEEP_RESET_VALUE    		((UINT16)0xFFFF)


#define MCU_RESET()	NVIC_SystemReset()


FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr,uint16_t Buffer);
UINT16 FlashReadOneHalfWord(UINT32 faddr);
void App_FlashUpdateDet(void);
void Init_IAPAPP(void);

#endif	/* FLASH_H */
