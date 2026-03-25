#ifndef __FLASH_H
#define __FLASH_H
#include "Common.h"

#define MCU_FLASH_WATI_DELAY		2
#define MAX_FPAGE_NUM				64			// оƬROM�ռ�Ϊ128k

extern BOOL bAFEWrFlag;
extern BOOL	bWriteFlashFlg;
extern BOOL	bMcuFlashWrWaitFlg;
extern U8 ucMcuFlashWrWaitCnt;

extern void CheckWrFlashProcess(void);
extern BOOL FlashProcess(U32 Address, U8 *pData, U32 Length);
extern BOOL RamCheckProcess(void);
extern void FlashRead(U32 Address, U8* Readbuff, U32 Length);
extern BOOL FlashErase(U32 Address, U32 Length);
extern BOOL FlashWrite(U32 Address, U8 *pData, U32 Length);
#endif
