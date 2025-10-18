#ifndef FLASH_H
#define FLASH_H

//ϵͳ�洢����������ST����������ڲ�Ԥ����һ��BootLoader��Ҳ����ISP��������һ��ROM
//С������Ʒ���洢��1-32KB��	 ÿҳ1KB��ϵͳ�洢��2KB��
//��������Ʒ���洢��64-128KB��	 ÿҳ1KB��ϵͳ�洢��2KB��
//��������Ʒ���洢��256KB���ϣ�  ÿҳ2KB��ϵͳ�洢��2KB��
//�����Ͳ�Ʒ���洢��256KB���ϣ�  ÿҳ2KB��ϵͳ�洢��18KB��
#define FLASH_ADDR_IAP_START 			0x08000000
#define FLASH_ADDR_APP_START 			0x08004800			//256-18-2-2=234K������ط�����һ�����⣬�޸ĺ����6K�����淴ӳ���������Zi-dataҲ��flash�Ķ���

#define FLASH_ADDR_SH367309_VALUE 		0x0803E000			//���ߴ���ؼ�����(����ƫ��ֵ)��1K
#define FLASH_ADDR_SH367309_FLAG 		0x0803E800			//�����Ƿ�����־λ��1K
#define FLASH_ADDR_UPDATE_FLAG 			0x0801F800			//IAP=18K
#define FLASH_ADDR_SLEEP_FLAG           0x0801FC00			//���߹ؼ�ָ�2K

#define FLASH_309_RTC_RTC_VALUE			((UINT16)0x1222)	//RTC���ߣ�RTC���ѣ�ֻ��RTC���ߣ����п���RTC���ѡ�
#define FLASH_309_RTC_NORMAL_VALUE		((UINT16)0x2333)	//RTC���ߣ������ʽ����(ͨѶ����ť�ȵ�)
#define FLASH_309_NORMAL_NORMAL_VALUE	((UINT16)0xFFFF)	//��ͨ���ߣ������ʽ����(ͨѶ����ť�ȵ�)

#define FLASH_TO_IAP_VALUE				((UINT16)0x00AB)
#define FLASH_TO_APP_VALUE				((UINT16)0xFFFF)

#define FLASH_NORMAL_SLEEP_VALUE    	((UINT16)0x1234)
#define FLASH_DEEP_SLEEP_VALUE    		((UINT16)0x1235)
#define FLASH_HICCUP_SLEEP_VALUE    	((UINT16)0x1236)
#define FLASH_SLEEP_RESET_VALUE    		((UINT16)0xFFFF)


FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr,uint16_t Buffer);
UINT16 FlashReadOneHalfWord(UINT32 faddr);
void FlashTest(void);

void App_FlashUpdate(void);
void APP_To_IAP_Jump(void);

#endif	/* FLASH_H */

