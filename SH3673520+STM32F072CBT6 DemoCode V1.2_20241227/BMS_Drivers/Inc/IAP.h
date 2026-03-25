#ifndef __IAP_H
#define __IAP_H

#include "Common.h"

typedef struct IapCtrlMap_ {
	U16 usUpdateFlg;
}IapCtrlMap;

#define IAP_MODE                0
#define ISP_MODE                1

//IAP相关定义
#define HEARD1                  0x00
#define HEARD2                  0x01
#define LENGTH                  0x02
#define SOURCE                  0x03
#define TARGET                  0x04
#define COMMAND                 0x05
#define INDEXES                 0x06
#define DATA                    0x07
#define IAP_BMSID				0x07		// 电池管理系统1
#define IAP_PCID				0x3D        // 上位机系统

//IAP Command
#define IAP_CMD_HANDSHAKE		0x06
#define IAP_CMD_BEGIN			0x07
#define IAP_CMD_TRANS			0x08
#define IAP_CMD_VERIFY			0x09
#define IAP_CMD_RESET			0x0A

//CMD_IAP_ACK error
#define IAPERROR_SIZE           0x01        // 固件大小超范围
#define IAPERROR_ERASE          0x02        // 擦除flash失败
#define IAPERROR_WR				0x03        // 写入flash失败
#define IAPERROR_UNLOCK         0x04        // 车辆未处于锁车状态，未处于可更新固件状态
#define IAPERROR_INDEX          0x05        // 数据索引错误
#define IAPERROR_BUSY           0x06        // IAP正忙
#define IAPERROR_FORM           0x07        // 数据格式错误（非8的整数倍）
#define IAPERROR_CRC            0x08        // 数据校验失败
#define IAPERROR_RESET          0x09		// 芯片复位失败
#define IAPERROR_HANDSHAKE		0x0A		// 握手失败

#define IAPERROR_CHECKSUM		0x80		// 数据帧校验错误
#define IAPERROR_ADDR			0x40		
#define IAPERROR_CMD			0x20		// 指令错误

// IAP当前状态
#define CMD_IAP_BEGIN			0x07
#define CMD_IAP_TRANS			0x08
#define CMD_IAP_VERIFY			0x09
#define CMD_MCU_RESET			0x0A

#define FLASH_BASE_ADDR   		(U32)0x08000000			// Flash的起始地址
#define FLASH_APP_FORMAL_ADDR   (U32)0x08002800 		// 正式运行的APP程序的起始地址
#define FLASH_APP_BK_ADDR		(U32)0x08011000			// IAP升级APP程序的暂存起始地址
#define FLASH_IAP_CTRL_ADDR		(U32)0x0801F800    		// IAP升级用控制表的存放地址

#define IAP_BK_CODE_SIZE		(U32)0xE800				// 正式APP程序（包括参数备份区）占用Flash空间最大值
#define MAX_PARA_SIZE      		(U32)0x800				// 单个参数备份区占用Flash空间最大值

#define MCU_CODE_SECTOR_SIZE	((U32)0x200)

#define IAP_UPGRADE_FLG			0x5AA5
#define IAP_CTRLMAP_SIZE		sizeof(IapCtrlMap)	// IAP控制表大小

extern BOOL bIAPFlg;
extern BOOL	bIapIspFlg;
extern BOOL bHandsheakOkFlg;

extern U16 usReceCheckSum;
extern U8 ucUart2ErrCode;

extern void IAPProcess(void);
extern void UartIAPCmdProcess(void);
#endif
