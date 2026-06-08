#include "main.h"

volatile union Can_Status Can_Status_Flag;
volatile union CanTxType_Status CanTxType_Flag;
CanTxMsg TxMessage;
CanRxMsg RxMessage;

#define CAN_0X02_SEND_PERIOD_TICKS      DELAYB10MS_1S
#define CAN_TX_WAIT_COUNTER_MAX         ((UINT16)0x0FFF)
#define CAN_RX_FIFO_DRAIN_MAX           ((UINT8)8u)
#define CAN_TX_FLAG_TEST                ((UINT32)0x00000001u)
#define CAN_TX_FLAG_BY_ID(id)           ((UINT32)1u << ((UINT8)(id) + 1u))

#define CAN_APP_CMD_QUEUE_SIZE          ((UINT8)4u)
#define CAN_APP_CMD_ID                  ((UINT16)0x60u)
#define CAN_APP_ACK_ID                  ((UINT16)0x61u)
#define CAN_APP_CMD_GET_STATUS          ((UINT8)0x01u)
#define CAN_APP_CMD_ENTER_IAP           ((UINT8)0x02u)
#define CAN_APP_CMD_READ_REG            ((UINT8)0x03u)
#define CAN_APP_CMD_WRITE_PREP          ((UINT8)0x04u)
#define CAN_APP_CMD_WRITE_COMMIT        ((UINT8)0x05u)
#define CAN_APP_CMD_READ_BLOCK          ((UINT8)0x06u)
#define CAN_APP_CMD_READ_BLOCK_DATA     ((UINT8)0x86u)
#define CAN_APP_READ_BLOCK_MAX_WORDS    ((UINT8)120u)
#define CAN_APP_ACK_OK                  ((UINT8)0x00u)
#define CAN_APP_ACK_BAD_CMD             ((UINT8)0x01u)
#define CAN_APP_ACK_BAD_PARAM           ((UINT8)0x02u)
#define CAN_APP_ACK_FLASH_ERR           ((UINT8)0x05u)
#define CAN_APP_ACK_NO_PERMISSION       ((UINT8)0x07u)
#define CAN_APP_ACK_BMS_ERROR           ((UINT8)0x08u)
#define CAN_APP_ENTER_IAP_DELAY_TICKS   ((UINT8)20u)

static BOOL s_bCanLowPower = FALSE;
static BOOL s_bLastCanTxOk = FALSE;
static UINT16 s_u16CanTxSearchId = CANID_CHECK_0x00;

typedef struct
{
	volatile UINT8 cmd_head;
	volatile UINT8 cmd_tail;
	volatile UINT8 cmd_count;
	UINT8 cmd_queue[CAN_APP_CMD_QUEUE_SIZE][8];
	UINT8 write_pending;
	UINT16 write_addr;
	UINT8 write_value_hi;
	UINT8 enter_iap_delay_ticks;
	UINT16 read_block_words[CAN_APP_READ_BLOCK_MAX_WORDS];
	UINT8 read_block_count;
	UINT8 read_block_index;
	UINT8 read_block_active;
} CanAppRuntime;

static CanAppRuntime s_can_app;

UINT16 g_u16BusOff_InitTestCnt = 0; // CAN总线关闭计时
UINT16 g_u16BusOff_RecoverCnt = 0;	// 5s计时标志位

void CAN_TX_Test(void)
{
	UINT8 TXCounter = 0, TXStatus = 0;
	UINT8 u8MailBoxUsed;
	s_bLastCanTxOk = FALSE;
	TxMessage.StdId = CANID_TX_Test; // 标准标识符
	TxMessage.ExtId = 0;			 // 扩展标识符
	TxMessage.IDE = CAN_ID_STD;		 // 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;	 // 为数据帧
	TxMessage.DLC = 8;				 // 消息的数据长度为8个字节
	TxMessage.Data[0] = 1;			 // 数据
	TxMessage.Data[1] = 2;
	TxMessage.Data[2] = 3;
	TxMessage.Data[3] = 4;
	TxMessage.Data[4] = 5;
	TxMessage.Data[5] = 6;
	TxMessage.Data[6] = 7;
	TxMessage.Data[7] = 8;

	/*
	while((CAN_TxStatus_Ok!=CAN_TransmitStatus(CAN1,CAN_Transmit(CAN1,&TxMessage)))&&(++i<0xFFF));		//发送数据
	if(i >= 0xFF) {
		ERROR_UserCallback(CAN_ERROR);
	}
	*/
	u8MailBoxUsed = CAN_Transmit(CAN1, &TxMessage);
	// CAN_Transmit已集成Can发送流程代码，非常方便
	// 这个++i和硬件的BusOFF是不冲突的，这个可以是在等邮箱空的次数，CAN_TxStatus_Pending和BusOFF分开
	do
	{
		// 出现Can错误的原因就是这句话！连续执行导致连续mailbox0是满的！CAN_Transmit()地方都放错了！
		// TXStatus = CAN_TransmitStatus(CAN1,CAN_Transmit(CAN1,&TxMessage));
		TXStatus = CAN_TransmitStatus(CAN1, u8MailBoxUsed);
		TXCounter++;
	} while ((TXStatus != CAN_TxStatus_Ok) && (TXCounter < 0xFF)); // Fail和OK不用管

	s_bLastCanTxOk = (TXStatus == CAN_TxStatus_Ok) ? TRUE : FALSE;

	if (TXCounter >= 0xFF)
	{
		System_ERROR_UserCallback(ERROR_CAN); // 这里应该是一个Pending_Error但是Can模块不可能需要等这么久吧。
	}
}

BOOL CAN_Tx_Data(CanTxMsg *Msg)
{
	UINT16 TXCounter = 0;
	UINT8 TXStatus = CAN_TxStatus_Failed;
	UINT8 u8MailBoxUsed;

	s_bLastCanTxOk = FALSE;
	Msg->StdId += ((UINT32)CAN_ADRESS_STD_ID << 7); // 单机版地址默认为0
	u8MailBoxUsed = CAN_Transmit(CAN1, Msg);
	if (u8MailBoxUsed == CAN_TxStatus_NoMailBox)
	{
		return FALSE;
	}

	do
	{
		TXStatus = CAN_TransmitStatus(CAN1, u8MailBoxUsed);
		TXCounter++;
	} while ((TXStatus == CAN_TxStatus_Pending) && (TXCounter < CAN_TX_WAIT_COUNTER_MAX));

	if (TXStatus == CAN_TxStatus_Ok)
	{
		s_bLastCanTxOk = TRUE;
		return TRUE;
	}

	if (TXStatus == CAN_TxStatus_Pending)
	{
		CAN_CancelTransmit(CAN1, u8MailBoxUsed);
	}
	else if (TXCounter >= CAN_TX_WAIT_COUNTER_MAX)
	{
		System_ERROR_UserCallback(ERROR_CAN);
	}

	return FALSE;
}

void CAN_TX_0x00(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x00; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCellTotle; // 总压，10mV
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	if (g_stCellInfoReport.u16IDischg > 0)
	{													 // 总电流，符号型，充电为正，放电为负
		u16_tmp16a = g_stCellInfoReport.u16IDischg * 10; // 10mA
		u16_tmp16a = (0x7FFF - u16_tmp16a + 1) | 0x8000;
	}
	else
	{
		u16_tmp16a = g_stCellInfoReport.u16Ichg * 10; // 电池总电流
	}
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.SocElement.u16CapacityNow; // 剩余容量10mAh
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x01(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x01; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.SocElement.u16CapacityFull; // 满电容量10mAh
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.SocElement.u16Cycle_times; // 循环次数
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.SocElement.u16Soc;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

BOOL CAN_TX_0x02(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x02; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16BalanceFlag1;
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16BalanceFlag2;
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.unMdlFault_Third.all;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	// u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	u16_tmp16a = g_stCellInfoReport.unMdlFault_Second.all;
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	return CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x03(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x03; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	switch (OPEN)
	{
	case 0:
		u16_tmp16a = ((~((UINT16)(SystemStatus.all & 0x000003FF))) & 0x00FE) | (((UINT16)(SystemStatus.all & 0x000003FF)) & 0xFF01);
		break;
	case 1:
		u16_tmp16a = (UINT16)(SystemStatus.all & 0x000003FF);
		break;
	default:
		u16_tmp16a = (UINT16)(SystemStatus.all & 0x000003FF);
		break;
	}
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0x1234; // 生产日期
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0x1234; // 软件版本
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x04(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x04; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	TxMessage.Data[0] = SeriesNum; // 电池串数
	TxMessage.Data[1] = System_ErrFlag.u8ErrFlag_CBC_DSG > 0 ? 1 : 0;

	u16_tmp16a = g_stCellInfoReport.u16TempMax;
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16TempMin;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x05(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x05; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.SocElement.u16Soh;
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x06(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x06; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = 0;
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x07(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x07; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[0];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[1];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[2];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x08(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x08; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[3];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[4];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[5];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x09(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x09; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[6];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[7];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[8];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0A(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0A; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[9];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[10];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[11];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0B(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0B; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[12];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[13];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[14];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0C(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0C; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[15];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[16];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[17];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0D(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0D; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[18];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[19];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[20];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0E(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0E; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[21];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[22];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[23];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x0F(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x0F; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[24];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[25];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[26];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x10(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x10; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[27];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[28];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[29];
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void CAN_TX_0x11(void)
{
	UINT16 u16_tmp16a;

	TxMessage.StdId = CANID_CHECK_0x11; // 标准标识符
	TxMessage.ExtId = 0;				// 扩展标识符
	TxMessage.IDE = CAN_ID_STD;			// 使用标准标识符
	TxMessage.RTR = CAN_RTR_DATA;		// 为数据帧
	TxMessage.DLC = 8;					// 消息的数据长度为8个字节

	u16_tmp16a = g_stCellInfoReport.u16VCell[30];
	TxMessage.Data[0] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[1] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = g_stCellInfoReport.u16VCell[31];
	TxMessage.Data[2] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[3] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = 0;
	TxMessage.Data[4] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[5] = (UINT8)(u16_tmp16a & 0xFF);

	u16_tmp16a = Sci_CRC16RTU(TxMessage.Data, 6);
	TxMessage.Data[6] = (UINT8)((u16_tmp16a >> 8) & 0xFF);
	TxMessage.Data[7] = (UINT8)(u16_tmp16a & 0xFF);

	CAN_Tx_Data(&TxMessage);
}

void InitCan_GPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA, ENABLE); // 复用功能和GPIOB端口时钟使能

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;	  // Configure CAN pin: RX    // PD0
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;		// Configure CAN pin: TX    // PD1
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// GPIO_PinRemapConfig(GPIO_Remap2_CAN1, ENABLE);	 	//14:13位  //0：映射到PA11，PA12(默认)//1: 不用。
}

void InitCan_NVIC(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/*不用设置了，systemInit()已经做了相应操作
	#ifdef  VECT_TAB_RAM
	  NVIC_SetVectorTable(NVIC_VectTab_RAM, 0x0);
	#else
	  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0);
	#endif
	*/
	/* enabling interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void InitCan_Filter(void)
{
	UINT16 u16CAN_FilterIdHigh;
	UINT16 u16CAN_FilterIdLow;
	UINT16 u16CAN_FilterMaskIdHigh;
	UINT16 u16CAN_FilterMaskIdLow;
	CAN_FilterInitTypeDef CAN_FilterInitStructure;

	// 这两句怎么来的，详细看截图——过滤器配置表
	// CAN_ID_STD之类的不需要移位，官方已经定好
	// CAN_FilterMode_IdList，列表模式FBMx = 1
	// CAN_FilterScale_16bit，过滤器组的位宽，FSCx = 0
	// 结合这两个，高位的屏蔽位也作为标识符列表，可以搞4个CANID
	// 别的情况是高位作为低位的屏蔽位。

	// stm32 can的屏蔽位模式：
	// 一个是标识符寄存器(过滤器Filter)，一个是屏蔽位寄存器(Mask)。
	// 凡是屏蔽位寄存器里为1的位所对应的标识符寄存器的位，这些位是必须匹配的
	// 也就是说，你接受到的Message里面的标识符（ID）里面对应的位必须跟标识符寄存器里对应的位相同，才能被接受。

	// 远程帧过滤器
	u16CAN_FilterIdHigh = (CANID_RX_COMMON_MSG_MASK << 5) | CAN_ID_STD | CAN_RTR_DATA;
	u16CAN_FilterIdLow = (CANID_RX_COMMON_MSG_FILTER << 5) | CAN_ID_STD | CAN_RTR_DATA;

	u16CAN_FilterMaskIdHigh = (CANID_RX_COMMON_MSG_MASK << 5) | CAN_ID_STD | CAN_RTR_DATA; // 设置成一样
	u16CAN_FilterMaskIdLow = (CANID_RX_COMMON_MSG_FILTER << 5) | CAN_ID_STD | CAN_RTR_DATA;

	CAN_FilterInitStructure.CAN_FilterNumber = 0;							// 指定过滤器为0，如果想接收多几个，范围为0——13
	CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;			// 指定过滤器为屏蔽模式
	CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_16bit;		// 过滤器位宽为16位，也即2个带屏蔽位的标准帧
	CAN_FilterInitStructure.CAN_FilterIdHigh = u16CAN_FilterIdHigh;			// 过滤器标识符的高16位值
	CAN_FilterInitStructure.CAN_FilterIdLow = u16CAN_FilterIdLow;			// 过滤器标识符的低16位值
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = u16CAN_FilterMaskIdHigh; // 过滤器屏蔽标识符的高16位值
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = u16CAN_FilterMaskIdLow;	// 过滤器屏蔽标识符的低16位值
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;	// 设定了指向过滤器的FIFO为0
	CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;					// 使能过滤器
	CAN_FilterInit(&CAN_FilterInitStructure);								// 按上面的参数初始化过滤器
}

void InitCan_CAN1(void)
{
	CAN_InitTypeDef CAN_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE); // CAN1 模块时钟使能
														 // APB1时钟最高36MHz

	CAN_DeInit(CAN1);					// 将外设CAN的全部寄存器重设为缺省值
	CAN_StructInit(&CAN_InitStructure); // 把CAN_InitStruct中的每一个参数按缺省值填入

	CAN_InitStructure.CAN_TTCM = DISABLE;		  // 没有使能时间触发模式
	// CAN_InitStructure.CAN_ABOM = DISABLE;		  // 没有使能自动离线管理，BusOFF自动离线取消，需要手动处理
	CAN_InitStructure.CAN_ABOM = ENABLE;		  // 使能自动离线管理，BusOFF由硬件尝试恢复
	CAN_InitStructure.CAN_AWUM = DISABLE;		  // 没有使能自动唤醒模式
	CAN_InitStructure.CAN_NART = ENABLE;          // 单次发送，避免无ACK时自动重发拉高功耗
	CAN_InitStructure.CAN_RFLM = DISABLE;		  // 没有使能接收FIFO锁定模式
	CAN_InitStructure.CAN_TXFP = DISABLE;		  // 没有使能发送FIFO优先级
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal; // CAN设置为正常模式
	// CAN_InitStructure.CAN_Mode = CAN_Mode_LoopBack;

	// 关于以下的设置，sample=(1+CAN_BS1)/(1+CAN_BS1+CAN_BS2)，采样点设置在80%到87.5%之间比较好。
	// 如果can采样点选取合适，can总线就能容纳更多的can节点。因此极其重要。
	// 如果这个不行，就改为那个PDF里面的常用参考参数
	CAN_InitStructure.CAN_SJW = CAN_SJW_1tq; // 重新同步跳跃宽度1个时间单位
	CAN_InitStructure.CAN_BS1 = CAN_BS1_2tq; // 时间段1为3个时间单位
	CAN_InitStructure.CAN_BS2 = CAN_BS2_1tq; // 时间段2为2个时间单位
	CAN_InitStructure.CAN_Prescaler = 4;	 // 时间单位长度为60
	CAN_Init(CAN1, &CAN_InitStructure);		 // 波特率为：72M/2/6/(1+8+3)=0.5 即500K，非PDF范例
											 // 波特率为：72M/2/12/(1+3+2)=0.5 即500K，为DPF的范例
											 // 波特率为：72M/2/24/(1+3+2)=0.25 即250K，为DPF的范例

	CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE); // RX由App_Can轮询处理
}

void Can_BusOFF_FaultTimeCtrl(void)
{
	if (TRUE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		g_u16BusOff_InitTestCnt++; // BUSOFF计时
	}
	if ((FALSE == (CAN1->ESR & CAN_ESR_BOFF)) && (FALSE == Can_Status_Flag.bits.b1Can_BusOFF))
	{
		g_u16BusOff_RecoverCnt++; // BUSOFF清除计时
	}
}

void Can_BusOFF_FaultChk(void)
{
	static UINT8 s_u8FlagBusOff = 0;
	if (FALSE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		if (CAN1->ESR & CAN_ESR_BOFF)
		{											  // 检测到BusOff，总线进入离线状态，找不到相关函数，自己写
			Can_Status_Flag.bits.b1Can_BusOFF = TRUE; // 找到了 --> CAN_GetFlagStatus(CAN1, CAN_FLAG_BOF)==SET
			CAN1->MCR |= CAN_MCR_INRQ;				  // 置位，从正常模式转为初始化模式(一旦当前的CAN活动(发送或接收)结束，CAN就进入初始化模式)
			s_u8FlagBusOff = 1;
			g_u16BusOff_RecoverCnt = 0; // 时序计算初始化
			g_u16BusOff_InitTestCnt = 0;
			System_ERROR_UserCallback(ERROR_CAN);
		}
	}

	if (1 == s_u8FlagBusOff)
	{ // 下一轮过来置回环模式
		s_u8FlagBusOff = 0;
		// CAN1->BTR =	(UINT32)CAN_Mode_LoopBack<<30;    	//请求环回模式，不需要静默回环模式，收到需要发ACK到总线上。
		// 真的需要改为回环模式吗，打个问号？软件INRQ位处理可在初始化模式和正常模式切换
	}
}

void Can_BusOFF_Recover(void)
{
	UINT16 u16BusOFF_InitCycleT;
	static UINT8 s_u8BusOFF_InitCnt = 0;
	if (TRUE == Can_Status_Flag.bits.b1Can_BusOFF)
	{
		if (s_u8BusOFF_InitCnt < 10)
		{											 // 快恢复计时10次
			u16BusOFF_InitCycleT = DELAYB10MS_100MS; // 快恢复计时10次100ms
		}
		else
		{
			s_u8BusOFF_InitCnt = 10;
			u16BusOFF_InitCycleT = DELAYB10MS_1S; // 1s周期
		}

		if (g_u16BusOff_InitTestCnt >= u16BusOFF_InitCycleT)
		{ // 周期初始化CAN，前10次为100ms，后面为1s
			s_u8BusOFF_InitCnt++;
			g_u16BusOff_InitTestCnt = 0;
			Can_Status_Flag.bits.b1Can_BusOFF_TestSd = 1; // 时间到尝试发送Test报文
			Can_Status_Flag.bits.b1Can_BusOFF = FALSE;
			// CAN1->BTR =	(UINT32)CAN_Mode_Normal<<30;		//请求正常模式
			CAN1->MCR &= ~CAN_MCR_INRQ; // 复位，从初始化模式转为正常模式(当CAN在接收引脚检测到连续的11个隐性位后，CAN就达到同步)
		}
	}

	if (g_u16BusOff_RecoverCnt > DELAYB10MS_500MS)
	{ // 5S内未检测到BusOFF标志，则表示与外部通信恢复正常
		g_u16BusOff_RecoverCnt = DELAYB10MS_500MS;
		s_u8BusOFF_InitCnt = 0;
	}
}

void Can_BusOFF_Monitor(void)
{
	Can_BusOFF_FaultChk();
	Can_BusOFF_FaultTimeCtrl();
	Can_BusOFF_Recover();
}


static UINT8 Can_AppCrcOk(const UINT8 data[8])
{
	UINT16 expect_crc = (UINT16)(((UINT16)data[6] << 8) | data[7]);
	UINT16 actual_crc = Sci_CRC16RTU((UINT8 *)data, 6U);

	return (expect_crc == actual_crc) ? 1U : 0U;
}

static void Can_AppFillCrc(UINT8 data[8])
{
	UINT16 crc = Sci_CRC16RTU(data, 6U);
	data[6] = (UINT8)(crc >> 8);
	data[7] = (UINT8)crc;
}

static void Can_AppSendFrame(UINT8 cmd, UINT8 status_or_seq, UINT8 value0, UINT8 value1)
{
	CanTxMsg tx_msg;

	memset(&tx_msg, 0, sizeof(tx_msg));
	tx_msg.StdId = CAN_APP_ACK_ID;
	tx_msg.ExtId = 0;
	tx_msg.IDE = CAN_ID_STD;
	tx_msg.RTR = CAN_RTR_DATA;
	tx_msg.DLC = 8;
	tx_msg.Data[0] = 0x5A;
	tx_msg.Data[1] = 0xA5;
	tx_msg.Data[2] = cmd;
	tx_msg.Data[3] = status_or_seq;
	tx_msg.Data[4] = value0;
	tx_msg.Data[5] = value1;
	Can_AppFillCrc(tx_msg.Data);
	(void)CAN_Tx_Data(&tx_msg);
}

static UINT8 Can_AppStatusFromHostError(UINT8 error)
{
	switch (error)
	{
	case 0U:
		return CAN_APP_ACK_OK;
	case RS485_ERROR_NO_PERMISSION:
		return CAN_APP_ACK_NO_PERMISSION;
	case RS485_ERROR_ADDR_INVALID:
	case RS485_ERROR_DATA_INVALID:
	case RS485_ERROR_RONLY_NO_W:
	case RS485_ERROR_WONLY_NO_R:
		return CAN_APP_ACK_BAD_PARAM;
	default:
		return CAN_APP_ACK_BMS_ERROR;
	}
}

static void Can_AppClearCmdQueue(void)
{
	__disable_irq();
	s_can_app.cmd_head = 0;
	s_can_app.cmd_tail = 0;
	s_can_app.cmd_count = 0;
	__enable_irq();
}

static void Can_AppStopReadBlock(void)
{
	s_can_app.read_block_active = 0;
	s_can_app.read_block_count = 0;
	s_can_app.read_block_index = 0;
}

static void Can_AppQueueCmd(const UINT8 data[8])
{
	__disable_irq();
	if (s_can_app.cmd_count < CAN_APP_CMD_QUEUE_SIZE)
	{
		memcpy(s_can_app.cmd_queue[s_can_app.cmd_tail], data, 8U);
		s_can_app.cmd_tail++;
		if (s_can_app.cmd_tail >= CAN_APP_CMD_QUEUE_SIZE)
		{
			s_can_app.cmd_tail = 0;
		}
		s_can_app.cmd_count++;
	}
	__enable_irq();
}

static UINT8 Can_AppTakeCmd(UINT8 data[8])
{
	UINT8 has_cmd = 0;

	__disable_irq();
	if (s_can_app.cmd_count != 0)
	{
		memcpy(data, s_can_app.cmd_queue[s_can_app.cmd_head], 8U);
		s_can_app.cmd_head++;
		if (s_can_app.cmd_head >= CAN_APP_CMD_QUEUE_SIZE)
		{
			s_can_app.cmd_head = 0;
		}
		s_can_app.cmd_count--;
		has_cmd = 1;
	}
	__enable_irq();

	return has_cmd;
}

static void Can_AppStartReadBlock(UINT8 count)
{
	s_can_app.read_block_count = count;
	s_can_app.read_block_index = 0;
	s_can_app.read_block_active = 1;
}

static void Can_AppServiceReadBlock(void)
{
	if (s_can_app.read_block_active == 0)
	{
		return;
	}
	if (s_can_app.read_block_index >= s_can_app.read_block_count)
	{
		Can_AppStopReadBlock();
		return;
	}

	Can_AppSendFrame(CAN_APP_CMD_READ_BLOCK_DATA,
					 s_can_app.read_block_index,
					 (UINT8)(s_can_app.read_block_words[s_can_app.read_block_index] >> 8),
					 (UINT8)s_can_app.read_block_words[s_can_app.read_block_index]);
	s_can_app.read_block_index++;
	if (s_can_app.read_block_index >= s_can_app.read_block_count)
	{
		Can_AppStopReadBlock();
	}
}

static void Can_AppServiceEnterIapDelay(void)
{
	if (s_can_app.enter_iap_delay_ticks == 0)
	{
		return;
	}

	s_can_app.enter_iap_delay_ticks--;
	if (s_can_app.enter_iap_delay_ticks == 0)
	{
		u8FlashUpdateFlag = 1;
	}
}

static void Can_AppHandleCmdData(const UINT8 data[8])
{
	UINT8 status = CAN_APP_ACK_OK;
	UINT8 value0 = 0;
	UINT8 value1 = 0;
	UINT8 cmd;
	UINT16 reg_addr;
	UINT16 reg_value;
	UINT8 reg_count;
	UINT8 host_error;

	if ((data[0] != 0xA5) ||
		(data[1] != 0x5A) ||
		(Can_AppCrcOk(data) == 0))
	{
		return;
	}

	cmd = data[2];
	Can_AppStopReadBlock();

	switch (cmd)
	{
	case CAN_APP_CMD_GET_STATUS:
		reg_value = g_stCellInfoReport.SocElement.u16Soc;
		value0 = (UINT8)((reg_value > 100U) ? 100U : reg_value);
		reg_value = g_stCellInfoReport.SocElement.u16Soh;
		value1 = (UINT8)((reg_value > 100U) ? 100U : reg_value);
		break;

	case CAN_APP_CMD_ENTER_IAP:
		if ((data[3] != 0xC3) ||
			(data[4] != 0x3C) ||
			(data[5] != (UINT8)CAN_ADRESS_STD_ID))
		{
			status = CAN_APP_ACK_BAD_PARAM;
			break;
		}
		if (AppUpgrade_RequestIap() == 0U)
		{
			status = CAN_APP_ACK_FLASH_ERR;
			break;
		}
		value0 = 0x08;
		value1 = 0x48;
		s_can_app.enter_iap_delay_ticks = CAN_APP_ENTER_IAP_DELAY_TICKS;
		break;

	case CAN_APP_CMD_READ_REG:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		host_error = Sci_HostReadWords(reg_addr, 1U, &reg_value);
		status = Can_AppStatusFromHostError(host_error);
		if (status == CAN_APP_ACK_OK)
		{
			value0 = (UINT8)(reg_value >> 8);
			value1 = (UINT8)reg_value;
		}
		break;

	case CAN_APP_CMD_READ_BLOCK:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		reg_count = data[5];
		if ((reg_count == 0) ||
			(reg_count > CAN_APP_READ_BLOCK_MAX_WORDS) ||
			(((UINT32)reg_addr + (UINT32)reg_count - 1U) > 0xFFFFU))
		{
			status = CAN_APP_ACK_BAD_PARAM;
			break;
		}
		host_error = Sci_HostReadWords(reg_addr, reg_count, s_can_app.read_block_words);
		status = Can_AppStatusFromHostError(host_error);
		if (status == CAN_APP_ACK_OK)
		{
			value0 = reg_count;
			value1 = 0;
			Can_AppStartReadBlock(reg_count);
		}
		break;

	case CAN_APP_CMD_WRITE_PREP:
		s_can_app.write_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		s_can_app.write_value_hi = data[5];
		s_can_app.write_pending = 1;
		value0 = data[3];
		value1 = data[4];
		break;

	case CAN_APP_CMD_WRITE_COMMIT:
		reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
		if ((s_can_app.write_pending == 0) || (reg_addr != s_can_app.write_addr))
		{
			s_can_app.write_pending = 0;
			status = CAN_APP_ACK_BAD_PARAM;
			break;
		}
		reg_value = (UINT16)(((UINT16)s_can_app.write_value_hi << 8) | data[5]);
		s_can_app.write_pending = 0;
		host_error = Sci_HostWriteWords(reg_addr, &reg_value, 1U);
		status = Can_AppStatusFromHostError(host_error);
		break;

	default:
		status = CAN_APP_ACK_BAD_CMD;
		break;
	}

	Can_AppSendFrame(cmd, status, value0, value1);
}

static void Can_AppService(void)
{
	UINT8 data[8];

	if (Can_AppTakeCmd(data) != 0)
	{
		Can_AppHandleCmdData(data);
	}
	Can_AppServiceReadBlock();
	Can_AppServiceEnterIapDelay();
}

static UINT32 Can_TxFlagFromRequestId(UINT16 request_id)
{
	if (request_id <= CANID_CHECK_0x11)
	{
		return CAN_TX_FLAG_BY_ID(request_id);
	}

	return 0u;
}

static void Can_QueueTxByRequestId(UINT16 request_id)
{
	UINT32 tx_flag = Can_TxFlagFromRequestId(request_id);

	if (tx_flag != 0u)
	{
		CanTxType_Flag.all |= tx_flag;
	}
}

static void Can_HandleRxMessage(const CanRxMsg *msg)
{
	UINT16 app_cmd_id = (UINT16)(((UINT16)CAN_ADRESS_STD_ID << 7) | CAN_APP_CMD_ID);

	if ((msg->IDE != CAN_ID_STD) || ((msg->StdId >> 7) != CAN_ADRESS_STD_ID))
	{
		return;
	}

	if (((UINT16)msg->StdId == app_cmd_id) && (msg->DLC == 8))
	{
		Can_AppQueueCmd(msg->Data);
		return;
	}

	Can_QueueTxByRequestId((UINT16)(msg->StdId & 0x007F));
}

void Can_ReceiveDeal(void)
{
	if (!Can_Status_Flag.bits.b1Can_Received)
	{
		return;
	}

	Can_Status_Flag.bits.b1Can_Received = 0;
	Can_HandleRxMessage(&RxMessage);
}

static BOOL Can_DrainRxFifo(UINT8 max_count)
{
	UINT8 u8RxCount = 0;
	BOOL bReceived = FALSE;

	while ((CAN_MessagePending(CAN1, CAN_FIFO0) != 0) && (u8RxCount < max_count))
	{
		u8RxCount++;
		memset(&RxMessage, 0, sizeof(RxMessage));
		CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);
		Can_Status_Flag.bits.b1Can_Received = 1;
		sys_time.can_rcv_cnt++;
		Can_ReceiveDeal();
		bReceived = TRUE;
	}

	return bReceived;
}

static BOOL Can_PollReceive(void)
{
	CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE);
	return Can_DrainRxFifo(CAN_RX_FIFO_DRAIN_MAX);
}

static UINT32 Can_GetNextTxFlag(void)
{
	UINT16 search_count;
	UINT16 request_id = s_u16CanTxSearchId;

	for (search_count = 0; search_count <= CANID_CHECK_0x11; search_count++)
	{
		UINT32 tx_flag = CAN_TX_FLAG_BY_ID(request_id);

		if ((CanTxType_Flag.all & tx_flag) != 0u)
		{
			return tx_flag;
		}

		request_id++;
		if (request_id > CANID_CHECK_0x11)
		{
			request_id = CANID_CHECK_0x00;
		}
	}

	if (CanTxType_Flag.bits.b1CanTx_Test)
	{
		return CAN_TX_FLAG_TEST;
	}

	return 0u;
}

static void Can_AdvanceTxSearchId(UINT32 tx_flag)
{
	UINT16 request_id;

	for (request_id = CANID_CHECK_0x00; request_id <= CANID_CHECK_0x11; request_id++)
	{
		if (tx_flag == CAN_TX_FLAG_BY_ID(request_id))
		{
			s_u16CanTxSearchId = request_id + 1u;
			if (s_u16CanTxSearchId > CANID_CHECK_0x11)
			{
				s_u16CanTxSearchId = CANID_CHECK_0x00;
			}
			return;
		}
	}
}

static void Can_ClearTxFlag(UINT32 tx_flag)
{
	CanTxType_Flag.all &= ~tx_flag;
}

static void Can_SendByTxFlag(UINT32 tx_flag)
{
	s_bLastCanTxOk = FALSE;

	switch (tx_flag)
	{
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x00):
		CAN_TX_0x00();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x01):
		CAN_TX_0x01();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x02):
		CAN_TX_0x02();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x03):
		CAN_TX_0x03();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x04):
		CAN_TX_0x04();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x05):
		CAN_TX_0x05();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x06):
		CAN_TX_0x06();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x07):
		CAN_TX_0x07();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x08):
		CAN_TX_0x08();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x09):
		CAN_TX_0x09();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x0A):
		CAN_TX_0x0A();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x0B):
		CAN_TX_0x0B();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x0C):
		CAN_TX_0x0C();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x0D):
		CAN_TX_0x0D();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x0E):
		CAN_TX_0x0E();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x0F):
		CAN_TX_0x0F();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x10):
		CAN_TX_0x10();
		break;
	case CAN_TX_FLAG_BY_ID(CANID_CHECK_0x11):
		CAN_TX_0x11();
		break;
	case CAN_TX_FLAG_TEST:
		CAN_TX_Test();
		break;
	default:
		break;
	}
}

static BOOL Can_TransmitDeal(void)
{
	UINT32 tx_flag = Can_GetNextTxFlag();

	if (tx_flag == 0u)
	{
		return FALSE;
	}

	RTC_ExtComCnt++;
	Can_SendByTxFlag(tx_flag);
	Can_AdvanceTxSearchId(tx_flag);

	if (s_bLastCanTxOk)
	{
		Can_ClearTxFlag(tx_flag);
		return TRUE;
	}

	return FALSE;
}

static void Can_EnsureNormalMode(void)
{
	sys_time.canPow_enable = true;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
	if ((FALSE != s_bCanLowPower) || ((CAN1->MSR & (CAN_MSR_INAK | CAN_MSR_SLAK)) != 0u))
	{
		CAN_WakeUp(CAN1);
		CAN_OperatingModeRequest(CAN1, CAN_OperatingMode_Normal);
		s_bCanLowPower = FALSE;
	}

	CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE);
}

void InitCan(void)
{
#if 0
	Can_Status_Flag.all = 0;
	CanTxType_Flag.all = 0;
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_Filter();
	InitCan_CAN1();			//目前是回环模式，要改回普通模式,Test
#endif
	Can_Status_Flag.all = 0;
	CanTxType_Flag.all = 0;
	InitCan_GPIO();
	InitCan_NVIC();
	InitCan_CAN1();	  // 目前是回环模式，要改回普通模式,Test
	InitCan_Filter(); // 这个调到后面，RX也可以了
	s_bCanLowPower = FALSE;
	memset(&s_can_app, 0, sizeof(s_can_app));
	Can_AppClearCmdQueue();
	Can_AppStopReadBlock();
	Can_EnsureNormalMode();
}

// 这个函数不能用Switch架构来解决，因为这个都是并行任务，不是串行。
void App_Can(void)
{
	static UINT16 cnt_send_0x02 = 0;
	BOOL bTxOk = FALSE;

	if (STARTUP_CONT == System_FUNC_StartUp(SYSTEM_FUNC_STARTUP_CAN))
	{
		// return;
	}

	if (0 == g_st_SysTimeFlag.bits.b1Sys10msFlag2)
	{
		return;
	}

	Can_EnsureNormalMode();
	// Can_BusOFF_Monitor();
	(void)Can_PollReceive();
	Can_AppService();
	(void)Can_TransmitDeal();

	if (++cnt_send_0x02 >= CAN_0X02_SEND_PERIOD_TICKS)
	{
		cnt_send_0x02 = 0;
		bTxOk = CAN_TX_0x02();
		sys_time.can_enable = (bTxOk == TRUE) ? true : false;
	}
}

void App_CanTest(void)
{
	if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag1)
	{
		return;
	}
	Can_EnsureNormalMode();
	CAN_TX_Test();
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	(void)Can_DrainRxFifo(CAN_RX_FIFO_DRAIN_MAX);
}
