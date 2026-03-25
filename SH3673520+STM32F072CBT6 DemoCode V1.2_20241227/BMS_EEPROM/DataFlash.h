#ifndef __DATAFLASH_H
#define __DATAFLASH_H
#include "Common.h"

#if defined(__CC_ARM)
#define DATAFLASH_ADDRESS_1 	(U32)0x08010000
#define DATAFLASH_ADDRESS_2 	(U32)0x08010800

#define FLASH_PARAA_ADDR   		(U32)0x08010000 		// 正式APP程序中参数备份A区的起始地址
#define FLASH_PARAB_ADDR   		(U32)0x08010800 		// 正式APP程序中参数备份B区的起始地址
#elif defined(__GNUC__)
#define DATAFLASH_ADDRESS_1 	"0x08010000"
#define DATAFLASH_ADDRESS_2 	"0x08010800"

#define FLASH_PARAA_ADDR   		(U32)0x08010000 		// 正式APP程序中参数备份A区的起始地址
#define FLASH_PARAB_ADDR   		(U32)0x08010800 		// 正式APP程序中参数备份B区的起始地址
#endif

#define _RAM_CHECK_DATA         0x5A

#endif

