#include "main.h"

#define APP_UPGRADE_MAILBOX_ADDR    ((UINT32)0x20004FE0)
#define APP_UPGRADE_MAILBOX_MAGIC   ((UINT32)0x49415031)
#define APP_UPGRADE_MAILBOX_REQUEST ((UINT32)0x5AA55AA5)

typedef struct
{
	UINT32 magic;
	UINT32 magic_inv;
	UINT32 request;
	UINT32 request_inv;
	UINT32 crc;
	UINT32 reserved[3];
} APP_UPGRADE_MAILBOX;

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr, uint16_t Buffer)
{
	FLASH_Status result;
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	while (FLASH_ErasePage(StartAddr) != FLASH_COMPLETE)
		;
	result = FLASH_ProgramHalfWord(StartAddr, Buffer);
	FLASH_Lock();
	return result;
}

/*
	函数功能:	读取指定地址的半字(16位数据)
	输入参数:	faddr:读地址(此地址必须为2的倍数!!)
	返回值: 	对应数据
*/
UINT16 FlashReadOneHalfWord(UINT32 faddr)
{
	return *(vu16 *)faddr;
}

static volatile APP_UPGRADE_MAILBOX *AppUpgrade_Mailbox(void)
{
	return (volatile APP_UPGRADE_MAILBOX *)APP_UPGRADE_MAILBOX_ADDR;
}

static UINT32 AppUpgrade_MailboxCrc(UINT32 magic, UINT32 request)
{
	return magic ^ request ^ 0xA5A55A5A;
}

static UINT8 AppUpgrade_IsIapRequested(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox = AppUpgrade_Mailbox();

	if ((mailbox->magic != APP_UPGRADE_MAILBOX_MAGIC) ||
		(mailbox->magic_inv != (UINT32)~APP_UPGRADE_MAILBOX_MAGIC) ||
		(mailbox->request != APP_UPGRADE_MAILBOX_REQUEST) ||
		(mailbox->request_inv != (UINT32)~APP_UPGRADE_MAILBOX_REQUEST) ||
		(mailbox->crc != AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC, APP_UPGRADE_MAILBOX_REQUEST)))
	{
		return 0U;
	}

	return 1U;
}

UINT8 AppUpgrade_RequestIap(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox = AppUpgrade_Mailbox();

	mailbox->magic = APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->magic_inv = (UINT32)~APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->request = APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->request_inv = (UINT32)~APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->crc = AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC, APP_UPGRADE_MAILBOX_REQUEST);

	return AppUpgrade_IsIapRequested();
}

void FlashTest(void)
{
	/*
	if(FLASH_COMPLETE != FlashWriteOneHalfWord(FLASH_ADDR_UPDATE_FLAG - 1024, FLASH_TO_APP_VALUE -1)) {
		Flash_Faultcnt++;
	}

	g_stCellInfoReport.u16VCell[2] = FlashReadOneHalfWord(FLASH_ADDR_UPDATE_FLAG - 1024);
	*/
	g_stCellInfoReport.u16VCell[2] = FlashReadOneHalfWord(FLASH_ADDR_UPDATE_FLAG);
}

void App_FlashUpdate(void)
{
#ifdef _IAP
	if (1 == u8FlashUpdateFlag)
	{
		SH367309_DriverMos_Ctrl(GPIO_CHG, 0);
		SH367309_DriverMos_Ctrl(GPIO_DSG, 0);
		__delay_ms(10);
		u8FlashUpdateFlag = 0;
		if (AppUpgrade_RequestIap() == 0U)
		{
			return;
		}
		__disable_fault_irq();
		MCU_RESET(); // reset避免了一切中断初始化问题
	}
#endif
}

void APP_To_IAP_Jump(void)
{
	if (AppUpgrade_RequestIap() != 0U)
	{
		__disable_fault_irq();
		MCU_RESET();
	}
}

void InitAreaSelect(void)
{
	if (AppUpgrade_IsIapRequested() != 0U)
	{
		__disable_fault_irq();
		MCU_RESET();
	}

	if (FlashReadOneHalfWord(FLASH_ADDR_UPDATE_FLAG) == FLASH_TO_IAP_VALUE)
	{
		APP_To_IAP_Jump(); // 跳回去不能开各种中断或者初始化，也即下面的初始化不能放上来
	}
}
