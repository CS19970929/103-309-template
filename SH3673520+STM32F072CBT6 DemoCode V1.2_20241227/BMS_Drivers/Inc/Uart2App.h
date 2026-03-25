#ifndef __UART1APP_H
#define __UART1APP_H

#include "Common.h"

#define UART_SLAVE_ADDR			0x00
#define UART_CMD_NO				0x01
#define UART_LENGTH				0x02
#define UART_DATA				0x03

#define SUBCLASSID_MCU			0x00
#define SUBCLASSID_AFE			0x0A
#define SUBCLASSID_CALI			0x0B

// 主信息扫描命令字
#define CELL1					0X01
#define CELL2					0X02
#define CELL3					0X03
#define CELL4					0X04
#define CELL5					0X05
#define CELL6					0X06
#define CELL7					0X07
#define CELL8					0X08
#define CELL9					0X09
#define CELL10					0X0A
#define CELL11				    0X0B
#define CELL12				    0X0C
#define CELL13				    0X0D
#define CELL14				    0X0E
#define CELL15				    0X0F
#define CELL16				    0X10
#define CELL17				    0X11
#define CELL18				    0X12
#define CELL19				    0X13
#define CELL20				    0X14
#define CUL_TOTAL_VOLTAGE		0X15
#define TOTAL_VOLTAGE		    0X16
#define	CADC_CURRENT		    0X17
#define	VADC_CURRENT		    0X18
#define EXT_TEMP1			    0X19
#define EXT_TEMP2			    0X1A
#define EXT_TEMP3			    0X1B
#define EXT_TEMP4				0x1C
#define IC_TEMP					0x1D
#define	BAL_CHANNEL				0x1E
#define	CTO_CHANNEL				0x1F
#define PACK_STATUS			    0X20
#define BATTERY_STATUS		    0X21
#define PACK_CONFIG			    0X22
#define	SOFT_VERSION			0x23
#define MANUFACTURE_COMMAND	    0X40

// 校准命令字
#define CALI_VOL_COMMAND		0xA0
#define CALI_CUR_COMMAND		0xA1
#define CALI_ZERO_CUR_COMMAND	0xA2
#define CALI_TS1_COMMAND		0xA3
#define CALI_TS2_COMMAND		0xA4
#define CALI_TS3_COMMAND		0xA5
#define CALI_TS4_COMMAND    	0xA6
#define CALI_RTC_COMMAND		0xAF


#define CMD_VALID_ACK			0x5A
#define CMD_INVALID_ACK			0xFF

#define Uart2SendAck()			{ucUart2Buf[ucUart2BufPT] = CMD_VALID_ACK; HAL_UART_Transmit_IT(&huart2, &ucUart2Buf[ucUart2BufPT], 1);}
#define Uart2SendNack()			{ucUart2Buf[ucUart2BufPT] = CMD_INVALID_ACK; HAL_UART_Transmit_IT(&huart2, &ucUart2Buf[ucUart2BufPT], 1);}

// MCU参数区子命令号
#define	DATA_FLASH_COMMAND		0x77
#define SUB_PAGE1				0x78
#define SUB_PAGE2				0x79
#define SUB_PAGE3				0x7A
#define SUB_PAGE4				0x7B
#define SUB_PAGE5				0x7C
#define SUB_PAGE6				0x7D
#define SUB_PAGE7				0x7E
#define SUB_PAGE8				0x7F
#define RTC_SUBID				0x0C

// UART相关变量
#define SADDR                   0x0A

// MANUFACTURE_COMMAND指令
#define AFE_NORMAL				0x10
#define AFE_IDLE				0x15
#define AFE_SLEEP				0x1A
#define	AFE_PD					0x13
#define	AFE_RESET				0x41

extern BOOL bUart2HandShakeFlg;
extern BOOL bUart2SndAckFlg;
extern BOOL bUart2ReadFlg;
extern BOOL bUart2WriteFlg;

extern U8 ucUart2Buf[256];
extern U8 ucRxData;
extern U8 ucUart2BufPT;
extern U8 ucResetFlg;
extern U8 ucCmdAFEMode;

extern void UartReceive(void);
extern void UartTransmit(void);

#endif
