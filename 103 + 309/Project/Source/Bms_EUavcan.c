#include "main.h"
#include "Bms_EUavcan.h"

#define BMS_EUAVCAN_PAYLOAD_LEN           ((UINT8)64)
#define BMS_EUAVCAN_PERIOD_10MS           ((UINT8)25)
#define BMS_EUAVCAN_FIRST_PAYLOAD_LEN     ((UINT8)5)
#define BMS_EUAVCAN_FRAME_PAYLOAD_MAX     ((UINT8)7)
#define BMS_EUAVCAN_FRAME_COUNT           ((UINT8)10)

#define BMS_EUAVCAN_TAIL_START            ((UINT8)0x80)
#define BMS_EUAVCAN_TAIL_END              ((UINT8)0x40)
#define BMS_EUAVCAN_TAIL_TOGGLE           ((UINT8)0x20)

typedef struct
{
	UINT8 payload[BMS_EUAVCAN_PAYLOAD_LEN];
	UINT8 period10ms;
	UINT8 frameIndex;
	UINT8 transferId;
	UINT8 txActive;
	UINT16 crc;
} BMS_EUAVCAN_TX_STATE;

static BMS_EUAVCAN_TX_STATE s_stBmsEUavcanTx;

static void BmsEUavcan_PutU16LE(UINT8 *buf, UINT8 offset, UINT16 value)
{
	buf[offset] = (UINT8)(value & 0xFFU);
	buf[offset + 1] = (UINT8)((value >> 8) & 0xFFU);
}

static void BmsEUavcan_PutI16LE(UINT8 *buf, UINT8 offset, INT16 value)
{
	BmsEUavcan_PutU16LE(buf, offset, (UINT16)value);
}

static void BmsEUavcan_PutU32LE(UINT8 *buf, UINT8 offset, UINT32 value)
{
	buf[offset] = (UINT8)(value & 0xFFU);
	buf[offset + 1] = (UINT8)((value >> 8) & 0xFFU);
	buf[offset + 2] = (UINT8)((value >> 16) & 0xFFU);
	buf[offset + 3] = (UINT8)((value >> 24) & 0xFFU);
}

static UINT16 BmsEUavcan_ClampU16(UINT32 value)
{
	return (value > 0xFFFFU) ? (UINT16)0xFFFFU : (UINT16)value;
}

static INT16 BmsEUavcan_ClampI16(INT32 value)
{
	if (value > 32767)
	{
		return (INT16)32767;
	}
	if (value < -32768)
	{
		return (INT16)-32768;
	}
	return (INT16)value;
}

static UINT16 BmsEUavcan_TotalVoltageMv(void)
{
	return BmsEUavcan_ClampU16((UINT32)g_stCellInfoReport.u16VCellTotle * 10U);
}

static INT16 BmsEUavcan_Current10mA(void)
{
	if (g_stCellInfoReport.u16IDischg > 0)
	{
		return BmsEUavcan_ClampI16(-((INT32)g_stCellInfoReport.u16IDischg));
	}
	return BmsEUavcan_ClampI16((INT32)g_stCellInfoReport.u16Ichg);
}

static INT16 BmsEUavcan_TemperatureC(void)
{
	UINT16 temp = g_stCellInfoReport.u16TempMax;
	if (temp == 0)
	{
		temp = g_stCellInfoReport.u16TempMin;
	}
	return (INT16)(((INT32)temp + 5) / 10 - 40);
}

static UINT16 BmsEUavcan_CapacityNowMah(void)
{
#if BMS_EUAVCAN_REPORT_CAPACITY_MAH
	return BmsEUavcan_ClampU16((UINT32)g_stCellInfoReport.SocElement.u16CapacityNow * 10U);
#else
	return (UINT16)0;
#endif
}

static UINT16 BmsEUavcan_CapacityDesignMah(void)
{
#if BMS_EUAVCAN_REPORT_CAPACITY_MAH
	return BmsEUavcan_ClampU16((UINT32)g_stCellInfoReport.SocElement.u16CapacityFactory * 10U);
#else
	return (UINT16)0;
#endif
}

static UINT32 BmsEUavcan_ErrorBits(void)
{
	UINT32 err = 0;

	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp ||
		g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp)
	{
		err |= (1UL << 0);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp ||
		g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp ||
		g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp)
	{
		err |= (1UL << 1);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp)
	{
		err |= (1UL << 2);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp)
	{
		err |= (1UL << 3);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp)
	{
		err |= (1UL << 4);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1BatOvp)
	{
		err |= (1UL << 5);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1VcellDeltaBig)
	{
		err |= (1UL << 6);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp)
	{
		err |= (1UL << 7);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp)
	{
		err |= (1UL << 8);
	}
	if (System_ErrFlag.u8ErrFlag_CBC_DSG)
	{
		err |= (1UL << 10);
	}
	if (g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow)
	{
		err |= (1UL << 11);
	}

	return err;
}

static UINT16 BmsEUavcan_Crc16Ccitt(const UINT8 *bytes, UINT8 len)
{
	UINT16 crc = 0xFFFFU;
	UINT8 i;
	UINT8 j;

	for (i = 0; i < len; i++)
	{
		crc ^= ((UINT16)bytes[i] << 8);
		for (j = 0; j < 8; j++)
		{
			if (crc & 0x8000U)
			{
				crc = (UINT16)((crc << 1) ^ 0x1021U);
			}
			else
			{
				crc = (UINT16)(crc << 1);
			}
		}
	}

	return crc;
}

static void BmsEUavcan_BuildPayload(void)
{
	UINT8 i;

	memset(s_stBmsEUavcanTx.payload, 0, sizeof(s_stBmsEUavcanTx.payload));

	BmsEUavcan_PutU16LE(s_stBmsEUavcanTx.payload, 0, BMS_EUAVCAN_MANUFACTURER_ID);
	BmsEUavcan_PutU16LE(s_stBmsEUavcanTx.payload, 2, BMS_EUAVCAN_BATTERY_MODEL_ID);
	BmsEUavcan_PutU16LE(s_stBmsEUavcanTx.payload, 4, BmsEUavcan_TotalVoltageMv());
	BmsEUavcan_PutI16LE(s_stBmsEUavcanTx.payload, 6, BmsEUavcan_Current10mA());
	BmsEUavcan_PutI16LE(s_stBmsEUavcanTx.payload, 8, BmsEUavcan_TemperatureC());
	BmsEUavcan_PutU16LE(s_stBmsEUavcanTx.payload, 10, g_stCellInfoReport.SocElement.u16Soc);
	BmsEUavcan_PutU16LE(s_stBmsEUavcanTx.payload, 12, g_stCellInfoReport.SocElement.u16Cycle_times);
	BmsEUavcan_PutI16LE(s_stBmsEUavcanTx.payload, 14, (INT16)g_stCellInfoReport.SocElement.u16Soh);

	for (i = 0; i < 12; i++)
	{
		BmsEUavcan_PutU16LE(s_stBmsEUavcanTx.payload, (UINT8)(16 + i * 2), g_stCellInfoReport.u16VCell[i]);
	}

	BmsEUavcan_PutU16LE(s_stBmsEUavcanTx.payload, 40, BmsEUavcan_CapacityDesignMah());
	BmsEUavcan_PutU16LE(s_stBmsEUavcanTx.payload, 42, BmsEUavcan_CapacityNowMah());
	BmsEUavcan_PutU32LE(s_stBmsEUavcanTx.payload, 44, BmsEUavcan_ErrorBits());

	for (i = 0; i < 16; i++)
	{
		s_stBmsEUavcanTx.payload[48 + i] = ProductionInfor.BMS_SerialNumber[i];
	}

	s_stBmsEUavcanTx.crc = BmsEUavcan_Crc16Ccitt(s_stBmsEUavcanTx.payload, BMS_EUAVCAN_PAYLOAD_LEN);
}

static UINT8 BmsEUavcan_TailByte(UINT8 frameIndex)
{
	UINT8 tail = (UINT8)(s_stBmsEUavcanTx.transferId & 0x1FU);

	if (frameIndex == 0)
	{
		tail |= BMS_EUAVCAN_TAIL_START;
	}
	if (frameIndex == (BMS_EUAVCAN_FRAME_COUNT - 1))
	{
		tail |= BMS_EUAVCAN_TAIL_END;
	}
	if (frameIndex & 0x01U)
	{
		tail |= BMS_EUAVCAN_TAIL_TOGGLE;
	}

	return tail;
}

static BOOL BmsEUavcan_SendCanFrame(const UINT8 *data, UINT8 dlc)
{
	CanTxMsg msg;
	UINT8 i;
	UINT8 mailbox;

	memset(&msg, 0, sizeof(msg));
	msg.ExtId = (UINT32)BMS_EUAVCAN_CAN_ID;
	msg.IDE = CAN_ID_EXT;
	msg.RTR = CAN_RTR_DATA;
	msg.DLC = dlc;

	for (i = 0; i < dlc; i++)
	{
		msg.Data[i] = data[i];
	}

	mailbox = CAN_Transmit(CAN1, &msg);
	return (mailbox == CAN_TxStatus_NoMailBox) ? FALSE : TRUE;
}

static void BmsEUavcan_SendCurrentFrame(void)
{
	UINT8 frame[8];
	UINT8 i;
	UINT8 payloadOffset;
	UINT8 payloadLen;

	memset(frame, 0, sizeof(frame));

	if (s_stBmsEUavcanTx.frameIndex == 0)
	{
		frame[0] = (UINT8)(s_stBmsEUavcanTx.crc & 0xFFU);
		frame[1] = (UINT8)((s_stBmsEUavcanTx.crc >> 8) & 0xFFU);
		for (i = 0; i < BMS_EUAVCAN_FIRST_PAYLOAD_LEN; i++)
		{
			frame[2 + i] = s_stBmsEUavcanTx.payload[i];
		}
		frame[7] = BmsEUavcan_TailByte(s_stBmsEUavcanTx.frameIndex);
		payloadLen = 8;
	}
	else
	{
		payloadOffset = (UINT8)(BMS_EUAVCAN_FIRST_PAYLOAD_LEN +
								(s_stBmsEUavcanTx.frameIndex - 1) * BMS_EUAVCAN_FRAME_PAYLOAD_MAX);
		payloadLen = (UINT8)(BMS_EUAVCAN_PAYLOAD_LEN - payloadOffset);
		if (payloadLen > BMS_EUAVCAN_FRAME_PAYLOAD_MAX)
		{
			payloadLen = BMS_EUAVCAN_FRAME_PAYLOAD_MAX;
		}

		for (i = 0; i < payloadLen; i++)
		{
			frame[i] = s_stBmsEUavcanTx.payload[payloadOffset + i];
		}
		frame[payloadLen] = BmsEUavcan_TailByte(s_stBmsEUavcanTx.frameIndex);
		payloadLen = (UINT8)(payloadLen + 1);
	}

	if (BmsEUavcan_SendCanFrame(frame, payloadLen))
	{
		s_stBmsEUavcanTx.frameIndex++;
		if (s_stBmsEUavcanTx.frameIndex >= BMS_EUAVCAN_FRAME_COUNT)
		{
			s_stBmsEUavcanTx.frameIndex = 0;
			s_stBmsEUavcanTx.txActive = 0;
			s_stBmsEUavcanTx.transferId = (UINT8)((s_stBmsEUavcanTx.transferId + 1) & 0x1FU);
		}
	}
}

void InitBmsEUavcan(void)
{
	memset(&s_stBmsEUavcanTx, 0, sizeof(s_stBmsEUavcanTx));
	s_stBmsEUavcanTx.period10ms = (UINT8)(BMS_EUAVCAN_PERIOD_10MS - 1);
}

void App_BmsEUavcan(void)
{
	if (0 == g_st_SysTimeFlag.bits.b1Sys10msFlag1)
	{
		return;
	}

	if (++s_stBmsEUavcanTx.period10ms >= BMS_EUAVCAN_PERIOD_10MS)
	{
		s_stBmsEUavcanTx.period10ms = 0;
		if (0 == s_stBmsEUavcanTx.txActive)
		{
			BmsEUavcan_BuildPayload();
			s_stBmsEUavcanTx.frameIndex = 0;
			s_stBmsEUavcanTx.txActive = 1;
		}
	}

	if (s_stBmsEUavcanTx.txActive)
	{
		BmsEUavcan_SendCurrentFrame();
	}
}
