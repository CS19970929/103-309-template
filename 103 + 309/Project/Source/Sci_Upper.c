#include "main.h"
#include "FaultSnapshot.h"

static struct RS485MSG g_stCurrentMsgPtr_SCI1;
static UINT16 gu16_CommuErrCnt_SCI1 = 0; // SCI通信异常计数
static UINT8 gu8_TxEnable_SCI1 = 0;
static UINT8 gu8_TxFinishFlag_SCI1 = 0;

#ifdef _COMMOM_UPPER_SCI2
static struct RS485MSG g_stCurrentMsgPtr_SCI2;
static UINT16 gu16_CommuErrCnt_SCI2 = 0; // SCI通信异常计数
static UINT8 gu8_TxEnable_SCI2 = 0;
static UINT8 gu8_TxFinishFlag_SCI2 = 0;
#endif

#ifdef _COMMOM_UPPER_SCI3
static struct RS485MSG g_stCurrentMsgPtr_SCI3;
static UINT16 gu16_CommuErrCnt_SCI3 = 0; // SCI通信异常计数
static UINT8 gu8_TxEnable_SCI3 = 0;
static UINT8 gu8_TxFinishFlag_SCI3 = 0;
#endif

static UINT8 g_u8SCITxBuff[SCI_TX_BUF_LEN];

struct stCell_Info g_stCellInfoReport;
UINT8 u8FlashUpdateFlag = 0;
UINT8 u8FlashUpdateE2PROM = 0;

#define SCI_USART_ERROR_FLAGS ((UINT16)(USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE))

typedef UINT8 (*SCI_PROTOCOL_RX_FEED_FN)(void *pvProtocolCtx, UINT8 u8Data);
typedef void (*SCI_PROTOCOL_PROCESS_FN)(void *pvProtocolCtx);
typedef UINT8 *(*SCI_PROTOCOL_TX_BUFFER_FN)(void *pvProtocolCtx);
typedef UINT16 (*SCI_PROTOCOL_TX_LENGTH_FN)(void *pvProtocolCtx);
typedef UINT8 (*SCI_PROTOCOL_IS_BUSY_FN)(void *pvProtocolCtx);
typedef void (*SCI_PROTOCOL_RESET_FN)(void *pvProtocolCtx);
typedef void (*SCI_PROTOCOL_RX_IDLE_FN)(void *pvProtocolCtx);
typedef void (*SCI_PROTOCOL_TX_COMPLETE_FN)(void *pvProtocolCtx);

struct SCI_PROTOCOL_OPS {
	SCI_PROTOCOL_RESET_FN pfReset;
	SCI_PROTOCOL_RX_FEED_FN pfRxFeed;
	SCI_PROTOCOL_PROCESS_FN pfProcessFrame;
	SCI_PROTOCOL_TX_BUFFER_FN pfGetTxBuffer;
	SCI_PROTOCOL_TX_LENGTH_FN pfGetTxLength;
	SCI_PROTOCOL_IS_BUSY_FN pfIsBusy;
	SCI_PROTOCOL_RX_IDLE_FN pfOnRxIdle;
	SCI_PROTOCOL_TX_COMPLETE_FN pfOnTxComplete;
};

struct SCI_PORT_RUNTIME {
	USART_TypeDef *pstUsart;
	void *pvProtocolCtx;
	const struct SCI_PROTOCOL_OPS *pstProtocolOps;
	volatile UINT16 *pu16ErrorCounter;
	volatile UINT8 *pu8TxEnableFlag;
	volatile UINT8 *pu8TxFinishFlag;
	UINT8 *pu8TxBuffer;
	UINT16 u16TxIndex;
	UINT16 u16TxLength;
	UINT8 u8FramePending;
};

static void Sci_ModbusResetMessage(struct RS485MSG *s);
static void Sci_SetWrError(struct RS485MSG *s, UINT8 error);
static UINT8 Sci_ModbusProtocolFeed(void *pvProtocolCtx, UINT8 u8Data);
static void Sci_ModbusProcessFrame(void *pvProtocolCtx);
static UINT8 *Sci_ModbusGetTxBuffer(void *pvProtocolCtx);
static UINT16 Sci_ModbusGetTxLength(void *pvProtocolCtx);
static UINT8 Sci_ModbusIsBusy(void *pvProtocolCtx);
static void Sci_ModbusResetProtocol(void *pvProtocolCtx);
static void Sci_ModbusOnRxIdle(void *pvProtocolCtx);
static void Sci_ModbusOnTxComplete(void *pvProtocolCtx);
static UINT8 Sci_RangeFits(UINT16 offset, UINT16 count, UINT16 total);
static UINT8 Sci_GetReadWindowWordCount(UINT16 actual_addr, UINT16 *word_count);

static void Sci_PortArmReceiver(struct SCI_PORT_RUNTIME *pstPort);
static void Sci_PortAbortTransfer(struct SCI_PORT_RUNTIME *pstPort);
static void Sci_PortStartTx(struct SCI_PORT_RUNTIME *pstPort);
static void Sci_PortFinishTx(struct SCI_PORT_RUNTIME *pstPort);
static void Sci_PortHandleError(struct SCI_PORT_RUNTIME *pstPort);
static void Sci_PortIRQHandler(struct SCI_PORT_RUNTIME *pstPort);
static void Sci_PortService(struct SCI_PORT_RUNTIME *pstPort);
static UINT8 Sci_PortIsBusy(const struct SCI_PORT_RUNTIME *pstPort);
static void Sci_InitCommonPort(struct SCI_PORT_RUNTIME *pstPort,
							   IRQn_Type eIrqChannel,
							   UINT8 u8UseApb2Bus,
							   UINT32 u32UsartClock,
							   GPIO_TypeDef *pstTxGpio,
							   UINT16 u16TxPin,
							   GPIO_TypeDef *pstRxGpio,
							   UINT16 u16RxPin,
							   UINT32 u32RemapConfig);

static const struct SCI_PROTOCOL_OPS g_stSciModbusProtocolOps = {
	Sci_ModbusResetProtocol,
	Sci_ModbusProtocolFeed,
	Sci_ModbusProcessFrame,
	Sci_ModbusGetTxBuffer,
	Sci_ModbusGetTxLength,
	Sci_ModbusIsBusy,
	Sci_ModbusOnRxIdle,
	Sci_ModbusOnTxComplete};

static struct SCI_PORT_RUNTIME g_stSciPort1 = {
	USART1,
	&g_stCurrentMsgPtr_SCI1,
	&g_stSciModbusProtocolOps,
	&gu16_CommuErrCnt_SCI1,
	&gu8_TxEnable_SCI1,
	&gu8_TxFinishFlag_SCI1,
	0,
	0,
	0,
	0};

#ifdef _COMMOM_UPPER_SCI2
static struct SCI_PORT_RUNTIME g_stSciPort2 = {
	USART2,
	&g_stCurrentMsgPtr_SCI2,
	&g_stSciModbusProtocolOps,
	&gu16_CommuErrCnt_SCI2,
	&gu8_TxEnable_SCI2,
	&gu8_TxFinishFlag_SCI2,
	0,
	0,
	0,
	0};
#endif


#ifdef _COMMOM_UPPER_SCI3
static struct SCI_PORT_RUNTIME g_stSciPort3 = {
	USART3,
	&g_stCurrentMsgPtr_SCI3,
	&g_stSciModbusProtocolOps,
	&gu16_CommuErrCnt_SCI3,
	&gu8_TxEnable_SCI3,
	&gu8_TxFinishFlag_SCI3,
	0,
	0,
	0,
	0};

#endif
void Sci_WrRegs_0x10_CalibCoef(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_Protect(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_SocTable(struct RS485MSG *s);
void Sci_WrRegs_0x10_CopperLoss(struct RS485MSG *s);
void Sci_WrRegs_0x10_RTC(struct RS485MSG *s);
void Sci_WrRegs_0x10_Balance(struct RS485MSG *s);
void Sci_WrRegs_0x10_SysOther(struct RS485MSG *s);
void Sci_WrRegs_0x10_SleepElement(struct RS485MSG *s);
void Sci_WrRegs_0x10_SocElement(struct RS485MSG *s);
void Sci_WrRegs_0x10_SystemElement(struct RS485MSG *s);
void Sci_WrRegs_0x10_OtherElement(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_FlashConnect(struct RS485MSG *s);
void Sci_WrRegs_0x10_SN_Version(UINT16 startADDR, struct RS485MSG *s);
void Sci_WrRegs_0x10_SocTestMode(struct RS485MSG *s);

void Sci_WrReg_0x06_Reset_CalibCoef(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_ProtectRecord(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_ProtectElement(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_OtherCanAdd(struct RS485MSG *s);
void Sci_WrReg_0x06_BMS_FunctionON(struct RS485MSG *s);
void Sci_WrReg_0x06_BMS_FunctionOFF(struct RS485MSG *s);
void Sci_WrReg_0x06_SetSocOnce(struct RS485MSG *s);

static UINT8 Sci_BmsFunctionIdIsSupported(UINT16 id)
{
	switch (id)
	{
	case 1U:
	case 2U:
	case 3U:
	case 5U:
	case 8U:
	case 9U:
	case 10U:
	case 11U:
		return 1U;
	default:
		return 0U;
	}
}
void Sci_DataInit(struct RS485MSG *s)
{
	UINT16 i;

	s->ptr_no = 0;
	s->csr = RS485_STA_IDLE;
	s->enRs485CmdType = RS485_CMD_READ_REGS;
	for (i = 0; i < RS485_MAX_BUFFER_SIZE; i++)
	{
		s->u16Buffer[i] = 0;
	}
	for (i = 0; i < SCI_TX_BUF_LEN; i++)
	{
		g_u8SCITxBuff[i] = 0;
	}
}

void CRC_verify(struct RS485MSG *s)
{
	UINT16 u16SciVerify;
	UINT16 t_u16FrameLenth;

	t_u16FrameLenth = s->ptr_no - 2;
	u16SciVerify = s->u16Buffer[t_u16FrameLenth] + (s->u16Buffer[t_u16FrameLenth + 1] << 8);
	if (u16SciVerify == Sci_CRC16RTU((UINT8 *)s->u16Buffer, t_u16FrameLenth))
	{
		s->AckType = RS485_ACK_POS;
	}
	else
	{
		s->u16RdRegByteNum = 0;
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CRC_ERROR;
	}
}

void Sci_Deal_ReadRegs_0x03(struct RS485MSG *s)
{
	UINT16 t_u16Temp;
	UINT16 u16ActualAddr;
	UINT16 u16RegCount;
	UINT16 u16WindowWords;
	UINT16 u16ReadByteNum;
	UINT16 u16ValidateOffset;

	t_u16Temp = s->u16Buffer[3] + (s->u16Buffer[2] << 8);
	u16ActualAddr = t_u16Temp;
	s->u16RdRegStartAddrActure = t_u16Temp;

	if (t_u16Temp >= RS485_ADDR_RO_SOC_TEST)
	{
		t_u16Temp -= (RS485_ADDR_RO_SOC_TEST - RS485_RO_SOC_TEST_OFFSET);
	}
	else if (t_u16Temp >= RS485_ADDR_RO_START2)
	{ // D200 offset maps to combined RO buffer
		t_u16Temp -= (RS485_ADDR_RO_START2 - 63 - 33);
	}

	else if (t_u16Temp >= RS485_ADDR_RO_START1)
	{ // D100 offset maps to combined RO buffer
		t_u16Temp -= (RS485_ADDR_RO_START1 - 63);
	}

	else if (t_u16Temp >= RS485_ADDR_RO_START0)
	{ // D000 base window
		t_u16Temp -= RS485_ADDR_RO_START0;
	}
	// Independent read-only sub-blocks
	else if (t_u16Temp >= RS485_ADDR_RO_LCD)
	{
		t_u16Temp -= RS485_ADDR_RO_LCD;
	}
	else if (t_u16Temp >= RS485_ADDR_RW_AFE_PARAMETER)
	{
		t_u16Temp -= RS485_ADDR_RW_AFE_PARAMETER;
	}
	else if (t_u16Temp >= RS485_ADDR_RW_OTHER_CANADD)
	{
		t_u16Temp -= RS485_ADDR_RW_OTHER_CANADD;
	}
	else if (t_u16Temp >= RS485_ADDR_RW_OTHER)
	{
		t_u16Temp -= RS485_ADDR_RW_OTHER;
	}
	else if (t_u16Temp >= RS485_ADDR_RW_PORTECT)
	{
		t_u16Temp -= RS485_ADDR_RW_PORTECT;
	}
	else if (t_u16Temp >= RS485_ADDR_RW_CALIB)
	{
		t_u16Temp -= RS485_ADDR_RW_CALIB;
	}

	s->u16RdRegStartAddr = t_u16Temp;
	u16RegCount = (UINT16)(s->u16Buffer[5] + (s->u16Buffer[4] << 8));
	u16ReadByteNum = (UINT16)(u16RegCount << 1);

	if ((u16RegCount == 0U) ||
		(u16ReadByteNum > (UINT16)(RS485_MAX_BUFFER_SIZE - 5U)))
	{
		s->u16RdRegByteNum = 0;
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
		return;
	}

	u16ValidateOffset = t_u16Temp;
	if ((u16ActualAddr >= RS485_ADDR_RO_LCD) && (u16ActualAddr < RS485_ADDR_RO_START0))
	{
		u16ValidateOffset = 0U;
		if ((u16ActualAddr >= RS485_ADDR_EVENT_RECORD) &&
			(u16ActualAddr < (UINT16)(RS485_ADDR_EVENT_RECORD + FLASH_STORAGE_LOG_RECORD_COUNT)))
		{
			u16ValidateOffset = (UINT16)(u16ActualAddr - RS485_ADDR_EVENT_RECORD);
		}
	}

	if ((!Sci_GetReadWindowWordCount(u16ActualAddr, &u16WindowWords)) ||
		(!Sci_RangeFits(u16ValidateOffset, u16RegCount, u16WindowWords)))
	{
		s->u16RdRegByteNum = 0;
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_ADDR_INVALID;
		return;
	}

	s->u16RdRegByteNum = (UINT8)u16ReadByteNum;
}
void Sci_Deal_WrReg_0x06(struct RS485MSG *s)
{
#if PROJECT_CFG_HOST_WRITE_ENABLE
	UINT16 u16SciRegAddr;
	u16SciRegAddr = s->u16Buffer[3] + (s->u16Buffer[2] << 8);
	switch (u16SciRegAddr)
	{
	case RS485_CMD_ADDR_RESET_CALIB_COEF:
		Sci_WrReg_0x06_Reset_CalibCoef(s);
		break;

	case RS485_CMD_ADDR_RESET_PROTECT_RECORD:
		Sci_WrReg_0x06_Reset_ProtectRecord(s);
		break;

	case RS485_CMD_ADDR_RESET_PROTECT_ELEMENT:
		Sci_WrReg_0x06_Reset_ProtectElement(s);
		break;

	case RS485_CMD_ADDR_RESET_OTHER_CANADD:
		Sci_WrReg_0x06_Reset_OtherCanAdd(s);
		break;


	case RS485_CMD_ADDR_SYSTEM_FUNCTION_ON:
		Sci_WrReg_0x06_BMS_FunctionON(s);
		break;

	case RS485_CMD_ADDR_SYSTEM_FUNCTION_OFF:
		Sci_WrReg_0x06_BMS_FunctionOFF(s);
		break;

	case RS485_CMD_ADDR_SET_ONCE_SOC:
		Sci_WrReg_0x06_SetSocOnce(s);
		break;

	// 中颖AFE参数可读可写新增
	case RS485_CMD_ADDR_RESET_AFE_PARAMETERS:
		Sci_WrReg_0x06_Reset_AFE_Parameters(s);
		break;

	case RS485_CMD_ADDR_RESET_EVENT_RECORD:
		Sci_WrReg_0x06_Reset_EventRecord(s);
		break;

	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_NO_PERMISSION;
		break;
	}
#else
	Sci_SetWrError(s, RS485_ERROR_NO_PERMISSION);
#endif
}

static void Sci_SetWrError(struct RS485MSG *s, UINT8 error)
{
	s->AckType = RS485_ACK_NEG;
	s->ErrorType = error;
}

static UINT16 Sci_GetWrRegNum(const struct RS485MSG *s)
{
	return (UINT16)(s->u16Buffer[5] + (s->u16Buffer[4] << 8));
}

static UINT16 Sci_GetWrValue(const struct RS485MSG *s, UINT16 index)
{
	return (UINT16)(s->u16Buffer[2 * index + 8] + (s->u16Buffer[2 * index + 7] << 8));
}

#if PROJECT_CFG_HOST_WRITE_ENABLE
static UINT8 Sci_IsCalibPairStart(UINT16 addr)
{
	if ((addr < RS485_CMD_ADDR_VC1CALIB_K) || (addr > RS485_CMD_ADDR_TEMP_MOS_CALIB_K))
	{
		return 0;
	}

	return (UINT8)(((addr - RS485_CMD_ADDR_VC1CALIB_K) & 1U) == 0U);
}
#endif

static void Sci_PutWordBE(UINT8 buff[], UINT16 *index, UINT16 value)
{
	buff[(*index)++] = (UINT8)(value >> 8);
	buff[(*index)++] = (UINT8)value;
}

static void Sci_PutZeroWordsBE(UINT8 buff[], UINT16 *index, UINT16 count)
{
	while (count != 0U)
	{
		buff[(*index)++] = 0U;
		buff[(*index)++] = 0U;
		--count;
	}
}

static UINT8 Sci_RecordBackIndex(UINT8 point, UINT16 back)
{
	INT16 index;

	index = (INT16)point - 1 - (INT16)back;
	if (index < 0)
	{
		index = (INT16)(index + Record_len);
	}

	return (UINT8)index;
}

static void Sci_PutLatestFaultWords(UINT8 buff[], UINT16 *index, const UINT16 record[], UINT8 point)
{
	UINT16 value;

	value = (UINT16)((record[Sci_RecordBackIndex(point, 0U)] << 8) |
					 record[Sci_RecordBackIndex(point, 1U)]);
	Sci_PutWordBE(buff, index, value);

	value = (UINT16)((record[Sci_RecordBackIndex(point, 2U)] << 8) |
					 record[Sci_RecordBackIndex(point, 3U)]);
	Sci_PutWordBE(buff, index, value);
}

static void Sci_CopyProductIdBytes(UINT8 dst[],
								   UINT16 *length,
								   const struct RS485MSG *s,
								   UINT16 byte_count)
{
	UINT8 i;

	for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
	{
		dst[i] = (i < byte_count) ? (UINT8)s->u16Buffer[7 + i] : 0U;
	}

	*length = byte_count;
}

static void Sci_PutBytes(UINT8 buff[], UINT16 *index, const UINT8 src[], UINT16 count)
{
	UINT16 i;

	for (i = 0; i < count; ++i)
	{
		buff[(*index)++] = src[i];
	}
}

static UINT8 Sci_WrRegsByteCountValid(const struct RS485MSG *s, UINT16 reg_count)
{
	return (UINT8)(s->u16Buffer[6] == (UINT8)(reg_count << 1));
}

static UINT8 Sci_RangeFits(UINT16 offset, UINT16 count, UINT16 total)
{
	if ((count == 0) || (offset >= total))
	{
		return 0;
	}
	return (UINT8)(count <= (UINT16)(total - offset));
}

static UINT8 Sci_RangeOverlaps(UINT16 start, UINT16 count, UINT16 block_start, UINT16 block_count)
{
	UINT16 end = (UINT16)(start + count);
	UINT16 block_end = (UINT16)(block_start + block_count);

	return (UINT8)((start < block_end) && (block_start < end));
}

static UINT8 Sci_GetReadWindowWordCount(UINT16 actual_addr, UINT16 *word_count)
{
	if (word_count == 0)
	{
		return 0;
	}

	if (actual_addr >= RS485_ADDR_RO_START0)
	{
		*word_count = RS485_RO_TOTAL_WORDS;
		return 1;
	}
	if (actual_addr >= RS485_ADDR_RO_LCD)
	{
		if ((actual_addr >= RS485_ADDR_EVENT_RECORD) &&
			(actual_addr < (UINT16)(RS485_ADDR_EVENT_RECORD + FLASH_STORAGE_LOG_RECORD_COUNT)))
		{
			*word_count = FLASH_STORAGE_LOG_RECORD_COUNT;
			return 1;
		}
		switch (actual_addr)
		{
		case RS485_ADDR_RO_LCD:
			*word_count = 5U;
			return 1;
		case RS485_ADDR_RO_FA_RTC:
			*word_count = (UINT16)(Record_len * 7U);
			return 1;
		case RS485_ADDR_SN_READ:
			*word_count = (UINT16)(((UINT16)PRODUCT_ID_LENGTH_MAX * 3U + 1U) / 2U);
			return 1;
		case RS485_ADDR_EVENT_RECORD:
			/* Each event log entry is two bytes, exactly one Modbus register. */
			*word_count = FLASH_STORAGE_LOG_RECORD_COUNT;
			return 1;
		default:
			return 0;
		}
	}
	if (actual_addr >= RS485_ADDR_RW_AFE_PARAMETER)
	{
		*word_count = AFE_PARAMETES_TOTAL_LENGTH;
		return 1;
	}
	if (actual_addr >= RS485_ADDR_RW_OTHER_CANADD)
	{
		*word_count = E2P_PARA_NUM_OTHER_ELEMENT1;
		return 1;
	}
	if (actual_addr >= RS485_ADDR_RW_OTHER)
	{
		*word_count = (UINT16)(SOC_Size_TableCanSet + CompensateNUM + CompensateNUM + E2P_PARA_NUM_RTC);
		return 1;
	}
	if (actual_addr >= RS485_ADDR_RW_PORTECT)
	{
		*word_count = E2P_PARA_NUM_PROTECT;
		return 1;
	}
	if (actual_addr >= RS485_ADDR_RW_CALIB)
	{
		*word_count = (UINT16)(KB_NUM * 2U);
		return 1;
	}

	return 0;
}

static void Sci_CopyWords(UINT16 *dst, const UINT16 *src, UINT16 count)
{
	UINT16 i;

	for (i = 0; i < count; ++i)
	{
		dst[i] = src[i];
	}
}

static UINT8 Sci_WrValuesInRange(const struct RS485MSG *s,
								 UINT16 offset,
								 UINT16 count,
								 const UINT16 *min_values,
								 const UINT16 *max_values)
{
	UINT16 i;
	UINT16 value;

	for (i = 0; i < count; ++i)
	{
		value = Sci_GetWrValue(s, i);
		if ((value < min_values[offset + i]) || (value > max_values[offset + i]))
		{
			return 0;
		}
	}

	return 1;
}

static void Sci_WriteWordsFromRequest(struct RS485MSG *s, UINT16 *dst, UINT16 offset, UINT16 count)
{
	UINT16 i;

	for (i = 0; i < count; ++i)
	{
		dst[offset + i] = Sci_GetWrValue(s, i);
	}
}

static void Sci_ApplyProtectSideEffects(UINT16 offset, UINT16 count)
{
	if (Sci_RangeOverlaps(offset,
						count,
						0,
						(UINT16)(RS485_CMD_ADDR_TCHG_OTP_FIRST - RS485_CMD_ADDR_VCELL_OVP_FIRST)))
	{
		InitData_SOC();
	}
}

static void Sci_ApplyOtherElementSideEffects(UINT16 offset, UINT16 count)
{
	UINT8 reload_soc = 0U;
	UINT8 reset_soc_capacity = 0U;

	if (Sci_RangeOverlaps(offset, count, 0, 8))
	{
#if AFE_TYPE == bq76xx_afe
#elif AFE_TYPE == sh36xx
		AFE_PARAM_WRITE_Flag = 1;
#else
#error "error!!!"
#endif
	}

	if (Sci_RangeOverlaps(offset, count, 8, 8) ||
		Sci_RangeOverlaps(offset, count, 28, 4))
	{
		AFE_PARAM_WRITE_Flag = 1;
	}

	if (Sci_RangeOverlaps(offset, count, 12, 1))
	{
		reload_soc = 1U;
	}

	if (Sci_RangeOverlaps(offset, count, 24, 4))
	{
		reload_soc = 1U;
		reset_soc_capacity = 1U;
	}

	if (reload_soc)
	{
		InitData_SOC();
	}

	if (reset_soc_capacity)
	{
		SOC_Enhance_Element.u16_RefreshData_Flag = 2;
	}

	if (Sci_RangeOverlaps(offset, count, 28, 4))
	{
		SeriesNum = (UINT8)OtherElement.u16Sys_SeriesNum;
		if (OtherElement.u16Sys_CS_Res != 0)
		{
			g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;
		}
	}
}

void Sci_Deal_WrRegs_0x10(struct RS485MSG *s)
{
#if PROJECT_CFG_HOST_WRITE_ENABLE
	UINT16 u16SciRegStartAddr;
	u16SciRegStartAddr = s->u16Buffer[3] + (s->u16Buffer[2] << 8);

	if (u16SciRegStartAddr == RS485_CMD_ADDR_SOC_TEST_SAMPLE)
	{
		Sci_WrRegs_0x10_SocTestMode(s);
		return;
	}

	if (Sci_WrRegs_0x10_AFE_Parameters(u16SciRegStartAddr, s))
	{
		return;
	}

	if ((u16SciRegStartAddr >= RS485_CMD_ADDR_VCELL_OVP_FIRST) &&
		(u16SciRegStartAddr < (UINT16)(RS485_CMD_ADDR_VCELL_OVP_FIRST + E2P_PARA_NUM_PROTECT)))
	{
		Sci_WrRegs_0x10_Protect(u16SciRegStartAddr, s);
		return;
	}

	if ((u16SciRegStartAddr >= RS485_CMD_ADDR_BALANCE_OV) &&
		(u16SciRegStartAddr < (UINT16)(RS485_CMD_ADDR_BALANCE_OV + E2P_PARA_NUM_OTHER_ELEMENT1)))
	{
		Sci_WrRegs_0x10_OtherElement(u16SciRegStartAddr, s);
		return;
	}

	if (Sci_IsCalibPairStart(u16SciRegStartAddr))
	{
		Sci_WrRegs_0x10_CalibCoef(u16SciRegStartAddr, s);
		return;
	}

	switch (u16SciRegStartAddr)
	{
	case RS485_CMD_ADDR_SOC_VOLTAGE1:
		Sci_WrRegs_0x10_SocTable(s);
		break;

	case RS485_CMD_ADDR_COPPERLOSS1:
		Sci_WrRegs_0x10_CopperLoss(s);
		break;

	case RS485_CMD_ADDR_RTC_TIME_YEAR:
		Sci_WrRegs_0x10_RTC(s);
		break;

	case RS485_ADDR_SN_SERIAL_NUM:
	case RS485_ADDR_SN_HAEDWARE_VER:
	case RS485_ADDR_SN_SOFTWARE_VER:
		Sci_WrRegs_0x10_SN_Version(u16SciRegStartAddr, s);
		break;

	case RS485_CMD_ADDR_FLASH_CONNECT:
		Sci_WrRegs_0x10_FlashConnect(s);
		break; // 少了个BREAK导致OVER。

	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
		break;
	}
#else
	Sci_SetWrError(s, RS485_ERROR_NO_PERMISSION);
#endif
}

void Sci_ACK_0x03_ReadRegs_LCD(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
	UINT16 u16SciTemp;
	UINT16 i, j;
	UINT16 u16SourceOffset = 0U;
	INT8 k;

	i = 0;
	if ((s->u16RdRegStartAddr >= (UINT16)(RS485_ADDR_EVENT_RECORD - RS485_ADDR_RO_LCD)) &&
		(s->u16RdRegStartAddr < (UINT16)(RS485_ADDR_EVENT_RECORD - RS485_ADDR_RO_LCD + FLASH_STORAGE_LOG_RECORD_COUNT)))
	{
		u16SourceOffset = (UINT16)(s->u16RdRegStartAddr - (UINT16)(RS485_ADDR_EVENT_RECORD - RS485_ADDR_RO_LCD));
		Sci_ACK_0x03_ReadRegs_EventRecord(t_u8BuffTemp);
	}
	else
	{
	switch (s->u16RdRegStartAddr)
	{
	case 0: // LCD
		break;
	case 1: // 上位机第三级保护，60+10=70个
		for (j = 0; j < Record_len; j++)
		{
			k = (INT8)Sci_RecordBackIndex(FaultPoint_Third, j);
			u16SciTemp = Fault_record_Third[k];
			Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
			Sci_PutZeroWordsBE(t_u8BuffTemp, &i, 6U);
		}
		break;

	case 2: // 序列号，硬件版本号，软件版本号
		Sci_PutBytes(t_u8BuffTemp, &i, ProductionInfor.BMS_SerialNumber, PRODUCT_ID_LENGTH_MAX);
		Sci_PutBytes(t_u8BuffTemp, &i, ProductionInfor.BMS_HardWareVersion, PRODUCT_ID_LENGTH_MAX);
		Sci_PutBytes(t_u8BuffTemp, &i, ProductionInfor.BMS_SoftWareVersion, PRODUCT_ID_LENGTH_MAX);
		break;

	case 8:
		Sci_ACK_0x03_ReadRegs_EventRecord(t_u8BuffTemp);
		break;

	default:
		s->u16RdRegStartAddr = 0;
		break;
	}
	}
	s->u16RdRegStartAddr = u16SourceOffset;
}

void Sci_ACK_0x03_ReadRegs_Data(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
	UINT16 u16SciTemp;
	UINT16 i = 0, j;
	UINT32 status_snapshot;
	UINT32 feature_mask;

	for (j = 0; j < 63; j++)
	{ // 0xD000_63
		u16SciTemp = *(&g_stCellInfoReport.u16VCell[0] + j);
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}

	// 0xD100_33
	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Month) | (RTC_time.RTC_Time_Year << 8);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Hour) | (RTC_time.RTC_Time_Day << 8);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Second) | (RTC_time.RTC_Time_Minute << 8);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	Sci_PutLatestFaultWords(t_u8BuffTemp, &i, Fault_record_Third, FaultPoint_Third);
	Sci_PutWordBE(t_u8BuffTemp, &i, 0U);
	Sci_PutWordBE(t_u8BuffTemp, &i, 0U);

	for (j = 0; j < 12; j++)
	{ // 0xD002到这里。
		u16SciTemp = ((*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + 2 * j)) << 8) | (*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + 2 * j + 1));
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}

	status_snapshot = SystemRuntime_GetStatusSnapshot();
	feature_mask = SystemFeature_GetMask();
	switch (OPEN)
	{
	case 0:
		u16SciTemp = ((~((UINT16)(status_snapshot & 0x0000FFFF))) & 0x00FE) | (((UINT16)(status_snapshot & 0x0000FFFF)) & 0xFF01);
		break;
	case 1:
		u16SciTemp = (UINT16)(status_snapshot & 0x0000FFFF);
		break;
	default:
		u16SciTemp = (UINT16)(status_snapshot & 0x0000FFFF);
		break;
	}
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	u16SciTemp = (UINT16)(status_snapshot >> 16);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	u16SciTemp = (UINT16)(feature_mask & 0x0000FFFF);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	u16SciTemp = (UINT16)(feature_mask >> 16);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	Sci_PutZeroWordsBE(t_u8BuffTemp, &i, 8U);

	// 0xD200_2: last Cortex fault reason and inverse snapshot.
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
	u16SciTemp = BKP_ReadBackupRegister(FAULT_BKP_REASON_REG);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	u16SciTemp = BKP_ReadBackupRegister(FAULT_BKP_REASON_INV_REG);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	{
		UINT16 status_words[RS485_RO_SOC_TEST_WORDS];
		SOC_TestMode_ReadStatus(status_words, RS485_RO_SOC_TEST_WORDS);
		for (j = 0; j < RS485_RO_SOC_TEST_WORDS; ++j)
		{
			u16SciTemp = status_words[j];
			Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
		}
	}
}

/*=================================================================
 * FUNCTION: Sci_Tx_RW_Fun
 * PURPOSE : 将需要发送的数据进行更新
 * INPUT:    void
 *
 * RETURN:   void
 *
 * CALLS:    void
 *
 * CALLED BY:Sci2_Updata()
 *
 *=================================================================*/
void Sci_ACK_0x03_RW_Data_Pro(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 65个
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < E2P_PARA_NUM_PROTECT; j++)
	{
		u16SciTemp = *(&PRT_E2ROMParas.u16VcellOvp_First + j);
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}
}

void Sci_ACK_0x03_RW_Data_Cali(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 94个
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < KB_NUM; j++)
	{
		u16SciTemp = g_u16CalibCoefK[j];
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
		u16SciTemp = g_i16CalibCoefB[j];
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}
}

static UINT16 Sci_GetSocTableWord(UINT16 index)
{
#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
	switch (OtherElement.u16Soc_TableSelect)
	{
	case SOC_TABLE_TEST:
		return SOC_Table_Set[index];
	case SOC_TABLE_LIFEPO:
		return SOC_Table_LiFePO[index];
	case SOC_TABLE_TERNARYLI:
		return SocTable_TernaryLi[index];
	case SOC_TABLE_LIFEPO2:
		return SocTable_LiFePO2[index];
	default:
		return SOC_Table_Set[index];
	}
#else
	(void)OtherElement.u16Soc_TableSelect;
#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
	return SOC_Table_LiFePO[index];
#else
	return SocTable_TernaryLi[index];
#endif
#endif
}

void Sci_ACK_0x03_RW_Data_Other(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 86
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < SOC_Size_TableCanSet; j++)
	{ // 由于GetEndValue()函数的问题，只能混在一起
		u16SciTemp = Sci_GetSocTableWord(j);
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}

	for (j = 0; j < CompensateNUM; j++)
	{
		u16SciTemp = CopperLoss[j];
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}

	for (j = 0; j < CompensateNUM; j++)
	{
		u16SciTemp = CopperLoss_Num[j];
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}

	for (j = 0; j < E2P_PARA_NUM_RTC; j++)
	{
		u16SciTemp = *(&RTC_time.RTC_Time_Year + j);
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}
}

void Sci_ACK_0x03_RW_Data_OtherCanAdd(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 32个
	UINT16 u16SciTemp;
	UINT16 i = 0, j;

	for (j = 0; j < E2P_PARA_NUM_OTHER_ELEMENT1; j++)
	{
		u16SciTemp = *(&OtherElement.u16Balance_OpenVoltage + j);
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}

}

void Sci_WrRegs_0x10_SocTestMode(struct RS485MSG *s)
{
#if 0
	UINT16 reg_count = Sci_GetWrRegNum(s);
	UINT8 enable;
	UINT16 vcell_max;
	UINT16 vcell_min;
	UINT16 ichg;
	UINT16 idsg;
	UINT16 ticks;

	if ((reg_count != 6U) || (!Sci_WrRegsByteCountValid(s, reg_count)))
	{
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
		return;
	}

	enable = (Sci_GetWrValue(s, 0) != 0U) ? 1U : 0U;
	vcell_max = Sci_GetWrValue(s, 1);
	vcell_min = Sci_GetWrValue(s, 2);
	ichg = Sci_GetWrValue(s, 3);
	idsg = Sci_GetWrValue(s, 4);
	ticks = Sci_GetWrValue(s, 5);
	if (!SOC_TestMode_RunSample(enable, vcell_max, vcell_min, ichg, idsg, ticks))
	{
#if PROJECT_CFG_SOC_TEST_MODE_ENABLE
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
#else
		Sci_SetWrError(s, RS485_ERROR_NO_PERMISSION);
#endif
	}
#endif
}
void Sci_ACK_0x03(struct RS485MSG *s)
{
	UINT8 i = 0U;
	UINT16 u16SciTemp;
	if (s->AckType == RS485_ACK_POS)
	{
		if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_CALIB)
		{
			if (s->u16RdRegStartAddrActure >= RS485_ADDR_RO_START0)
			{
				Sci_ACK_0x03_ReadRegs_Data(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RO_LCD)
			{
				Sci_ACK_0x03_ReadRegs_LCD(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_AFE_PARAMETER)
			{
				Sci_ACK_0x03_RW_AFE_Parameters(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_OTHER_CANADD)
			{
				Sci_ACK_0x03_RW_Data_OtherCanAdd(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_OTHER)
			{
				Sci_ACK_0x03_RW_Data_Other(s, g_u8SCITxBuff);
			}
			else if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_PORTECT)
			{
				Sci_ACK_0x03_RW_Data_Pro(s, g_u8SCITxBuff);
			}
			else
			{
				Sci_ACK_0x03_RW_Data_Cali(s, g_u8SCITxBuff);
			}
			// 头码，前三个字节保持不变
			s->u16Buffer[0] = (s->u16Buffer[0] != 0) ? RS485_SLAVE_ADDR : s->u16Buffer[0];
			s->u16Buffer[1] = s->enRs485CmdType;
			s->u16Buffer[2] = s->u16RdRegByteNum;
			// 数据
			for (i = 0; i < (s->u16RdRegByteNum); i++)
			{
				s->u16Buffer[i + 3] = g_u8SCITxBuff[i + ((s->u16RdRegStartAddr) << 1)];
			}
			i = s->u16RdRegByteNum + 3;
		}
	}
	else
	{
		i = 1;
		s->u16Buffer[i++] = s->enRs485CmdType | 0x80;
		s->u16Buffer[i++] = s->ErrorType;
	}
	u16SciTemp = Sci_CRC16RTU((UINT8 *)s->u16Buffer, i);
	s->u16Buffer[i++] = u16SciTemp & 0x00FF;
	s->u16Buffer[i++] = u16SciTemp >> 8;
	s->AckLenth = i;

	s->ptr_no = 0;
	s->csr = RS485_STA_TX_COMPLETE;
}

void Sci_ACK_0x06_0x10(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16SciTemp;

	if (s->AckType == RS485_ACK_POS)
	{
		i = 6;
	}
	else
	{
		i = 1;
		s->u16Buffer[i++] = s->enRs485CmdType | 0x80;
		s->u16Buffer[i++] = s->ErrorType;
	}

	u16SciTemp = Sci_CRC16RTU((UINT8 *)s->u16Buffer, i);
	s->u16Buffer[i++] = u16SciTemp & 0x00FF;
	s->u16Buffer[i++] = u16SciTemp >> 8;
	s->AckLenth = i;

	s->ptr_no = 0;
	s->csr = RS485_STA_TX_COMPLETE;
}

UINT8 Sci_HostReadWords(UINT16 u16StartAddr, UINT16 u16Count, UINT16 *pu16Words)
{
	struct RS485MSG stMsg;
	UINT16 i;

	if ((pu16Words == 0) ||
		(u16Count == 0U) ||
		((u16Count << 1) > (UINT16)(RS485_MAX_BUFFER_SIZE - 5U)))
	{
		return RS485_ERROR_DATA_INVALID;
	}

	memset(&stMsg, 0, sizeof(stMsg));
	stMsg.AckType = RS485_ACK_POS;
	stMsg.ErrorType = RS485_ERROR_NULL;
	stMsg.enRs485CmdType = RS485_CMD_READ_REGS;
	stMsg.u16Buffer[0] = RS485_SLAVE_ADDR;
	stMsg.u16Buffer[1] = RS485_CMD_READ_REGS;
	stMsg.u16Buffer[2] = (UINT8)(u16StartAddr >> 8);
	stMsg.u16Buffer[3] = (UINT8)u16StartAddr;
	stMsg.u16Buffer[4] = (UINT8)(u16Count >> 8);
	stMsg.u16Buffer[5] = (UINT8)u16Count;

	Sci_Deal_ReadRegs_0x03(&stMsg);
	if (stMsg.AckType != RS485_ACK_POS)
	{
		return stMsg.ErrorType;
	}

	Sci_ACK_0x03(&stMsg);
	if ((stMsg.AckType != RS485_ACK_POS) ||
		(stMsg.AckLenth < (UINT8)(5U + (u16Count << 1))) ||
		(stMsg.u16Buffer[1] != RS485_CMD_READ_REGS) ||
		(stMsg.u16Buffer[2] != (UINT8)(u16Count << 1)))
	{
		return (stMsg.ErrorType != RS485_ERROR_NULL) ? stMsg.ErrorType : RS485_ERROR_DATA_INVALID;
	}

	for (i = 0; i < u16Count; ++i)
	{
		pu16Words[i] = (UINT16)(((UINT16)stMsg.u16Buffer[3U + (i << 1)] << 8) |
								stMsg.u16Buffer[4U + (i << 1)]);
	}

	return 0U;
}

UINT8 Sci_HostWriteWords(UINT16 u16StartAddr, const UINT16 *pu16Words, UINT16 u16Count)
{
	struct RS485MSG stMsg;
	UINT16 i;

	if ((pu16Words == 0) ||
		(u16Count == 0U) ||
		((u16Count << 1) > (UINT16)(RS485_MAX_BUFFER_SIZE - 9U)))
	{
		return RS485_ERROR_DATA_INVALID;
	}

	memset(&stMsg, 0, sizeof(stMsg));
	stMsg.AckType = RS485_ACK_POS;
	stMsg.ErrorType = RS485_ERROR_NULL;
	stMsg.u16Buffer[0] = RS485_SLAVE_ADDR;

	if ((u16Count == 1U) && (u16StartAddr < RS485_ADDR_RW_CALIB))
	{
		stMsg.enRs485CmdType = RS485_CMD_WRITE_REG;
		stMsg.u16Buffer[1] = RS485_CMD_WRITE_REG;
		stMsg.u16Buffer[2] = (UINT8)(u16StartAddr >> 8);
		stMsg.u16Buffer[3] = (UINT8)u16StartAddr;
		stMsg.u16Buffer[4] = (UINT8)(pu16Words[0] >> 8);
		stMsg.u16Buffer[5] = (UINT8)pu16Words[0];
		Sci_Deal_WrReg_0x06(&stMsg);
	}
	else
	{
		stMsg.enRs485CmdType = RS485_CMD_WRITE_REGS;
		stMsg.u16Buffer[1] = RS485_CMD_WRITE_REGS;
		stMsg.u16Buffer[2] = (UINT8)(u16StartAddr >> 8);
		stMsg.u16Buffer[3] = (UINT8)u16StartAddr;
		stMsg.u16Buffer[4] = (UINT8)(u16Count >> 8);
		stMsg.u16Buffer[5] = (UINT8)u16Count;
		stMsg.u16Buffer[6] = (UINT8)(u16Count << 1);
		for (i = 0; i < u16Count; ++i)
		{
			stMsg.u16Buffer[7U + (i << 1)] = (UINT8)(pu16Words[i] >> 8);
			stMsg.u16Buffer[8U + (i << 1)] = (UINT8)pu16Words[i];
		}
		Sci_Deal_WrRegs_0x10(&stMsg);
	}

	if (stMsg.AckType != RS485_ACK_POS)
	{
		return stMsg.ErrorType;
	}

	return 0U;
}

static void Sci_ModbusResetMessage(struct RS485MSG *s)
{
	s->ptr_no = 0;
	s->csr = RS485_STA_IDLE;
	s->u16RdRegStartAddr = 0;
	s->u16RdRegStartAddrActure = 0;
	s->u16RdRegByteNum = 0;
	s->AckLenth = 0;
	s->AckType = RS485_ACK_POS;
	s->ErrorType = RS485_ERROR_NULL;
	s->enRs485CmdType = RS485_CMD_READ_REGS;
	s->u16Buffer[0] = 0;
	s->u16Buffer[1] = 0;
	s->u16Buffer[2] = 0;
	s->u16Buffer[3] = 0;
}

static void Sci_ModbusResetProtocol(void *pvProtocolCtx)
{
	Sci_ModbusResetMessage((struct RS485MSG *)pvProtocolCtx);
}

static UINT8 Sci_ModbusProtocolFeed(void *pvProtocolCtx, UINT8 u8Data)
{
	struct RS485MSG *s = (struct RS485MSG *)pvProtocolCtx;
	UINT16 u16FrameEndIndex;

	if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
	{
		Sci_ModbusResetMessage(s);
	}

	s->u16Buffer[s->ptr_no] = u8Data;

	if (s->ptr_no == 0)
	{
		if ((u8Data != RS485_SLAVE_ADDR) && (u8Data != RS485_BROADCAST_ADDR))
		{
			Sci_ModbusResetMessage(s);
			return 0;
		}
	}
	else if (s->ptr_no == 1)
	{
		switch (u8Data)
		{
		case RS485_CMD_READ_REGS:
			s->enRs485CmdType = RS485_CMD_READ_REGS;
			break;
		case RS485_CMD_WRITE_REG:
			s->enRs485CmdType = RS485_CMD_WRITE_REG;
			break;
		case RS485_CMD_WRITE_REGS:
			s->enRs485CmdType = RS485_CMD_WRITE_REGS;
			break;
		default:
			Sci_ModbusResetMessage(s);
			return 0;
		}
	}
	else
	{
		switch (s->enRs485CmdType)
		{
		case RS485_CMD_READ_REGS:
		case RS485_CMD_WRITE_REG:
			if (s->ptr_no == 7)
			{
				s->csr = RS485_STA_RX_COMPLETE;
				s->ptr_no++;
				return 1;
			}
			break;
		case RS485_CMD_WRITE_REGS:
			if (s->ptr_no == 6)
			{
				u16FrameEndIndex = (UINT16)s->u16Buffer[6] + 8U;
				if (u16FrameEndIndex >= RS485_MAX_BUFFER_SIZE)
				{
					Sci_ModbusResetMessage(s);
					return 0;
				}
			}
			if (s->ptr_no >= 7)
			{
				u16FrameEndIndex = (UINT16)s->u16Buffer[6] + 8U;
				if (s->ptr_no == u16FrameEndIndex)
				{
					s->csr = RS485_STA_RX_COMPLETE;
					s->ptr_no++;
					return 1;
				}
			}
			break;
		default:
			Sci_ModbusResetMessage(s);
			return 0;
		}
	}

	s->ptr_no++;
	if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
	{
		Sci_ModbusResetMessage(s);
	}

	return 0;
}

static void Sci_ModbusProcessFrame(void *pvProtocolCtx)
{
	struct RS485MSG *s = (struct RS485MSG *)pvProtocolCtx;

	s->AckType = RS485_ACK_POS;
	s->ErrorType = RS485_ERROR_NULL;

	CRC_verify(s);
	if (s->AckType == RS485_ACK_POS)
	{
		switch (s->enRs485CmdType)
		{
		case RS485_CMD_READ_REGS:
			Sci_Deal_ReadRegs_0x03(s);
			break;
		case RS485_CMD_WRITE_REG:
			Sci_Deal_WrReg_0x06(s);
			break;
		case RS485_CMD_WRITE_REGS:
			Sci_Deal_WrRegs_0x10(s);
			break;
		default:
			s->u16RdRegByteNum = 0;
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_CMD_INVALID;
			break;
		}
	}

	switch (s->enRs485CmdType)
	{
	case RS485_CMD_READ_REGS:
		Sci_ACK_0x03(s);
		break;
	case RS485_CMD_WRITE_REG:
	case RS485_CMD_WRITE_REGS:
		Sci_ACK_0x06_0x10(s);
		break;
	default:
		Sci_ModbusResetMessage(s);
		break;
	}
}

static UINT8 *Sci_ModbusGetTxBuffer(void *pvProtocolCtx)
{
	return ((struct RS485MSG *)pvProtocolCtx)->u16Buffer;
}

static UINT16 Sci_ModbusGetTxLength(void *pvProtocolCtx)
{
	return ((struct RS485MSG *)pvProtocolCtx)->AckLenth;
}

static UINT8 Sci_ModbusIsBusy(void *pvProtocolCtx)
{
	struct RS485MSG *s = (struct RS485MSG *)pvProtocolCtx;

	return (UINT8)((s->ptr_no != 0U) || (s->csr != RS485_STA_IDLE));
}

static void Sci_ModbusOnRxIdle(void *pvProtocolCtx)
{
	struct RS485MSG *s = (struct RS485MSG *)pvProtocolCtx;

	if ((s->ptr_no != 0U) && (s->csr == RS485_STA_IDLE))
	{
		Sci_ModbusResetMessage(s);
	}
}

static void Sci_ModbusOnTxComplete(void *pvProtocolCtx)
{
	Sci_ModbusResetMessage((struct RS485MSG *)pvProtocolCtx);
}

static void Sci_PortArmReceiver(struct SCI_PORT_RUNTIME *pstPort)
{
	volatile UINT16 u16Dummy;

	pstPort->u8FramePending = 0;
	pstPort->u16TxIndex = 0;
	pstPort->u16TxLength = 0;
	pstPort->pu8TxBuffer = 0;
	u16Dummy = pstPort->pstUsart->SR;
	u16Dummy = pstPort->pstUsart->DR;
	(void)u16Dummy;
	pstPort->pstUsart->CR1 |= (USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_IDLEIE);
	pstPort->pstUsart->CR1 &= (UINT16)~(USART_CR1_TXEIE | USART_CR1_TCIE);
}

static void Sci_PortAbortTransfer(struct SCI_PORT_RUNTIME *pstPort)
{
	pstPort->u8FramePending = 0;
	pstPort->u16TxIndex = 0;
	pstPort->u16TxLength = 0;
	pstPort->pu8TxBuffer = 0;

	if (pstPort->pu8TxEnableFlag != 0)
	{
		*pstPort->pu8TxEnableFlag = 0;
	}
	if (pstPort->pu8TxFinishFlag != 0)
	{
		*pstPort->pu8TxFinishFlag = 0;
	}
	if ((pstPort->pstProtocolOps != 0) && (pstPort->pstProtocolOps->pfReset != 0))
	{
		pstPort->pstProtocolOps->pfReset(pstPort->pvProtocolCtx);
	}
	Sci_PortArmReceiver(pstPort);
}

static void Sci_PortStartTx(struct SCI_PORT_RUNTIME *pstPort)
{
	pstPort->u16TxIndex = 0;

	if (pstPort->pu8TxEnableFlag != 0)
	{
		*pstPort->pu8TxEnableFlag = 1;
	}
	if (pstPort->pu8TxFinishFlag != 0)
	{
		*pstPort->pu8TxFinishFlag = 0;
	}

	pstPort->pstUsart->CR1 &= (UINT16)~(USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_IDLEIE | USART_CR1_TCIE);
	pstPort->pstUsart->CR1 |= (USART_CR1_TE | USART_CR1_TXEIE);
}

static void Sci_PortFinishTx(struct SCI_PORT_RUNTIME *pstPort)
{
	USART_ClearFlag(pstPort->pstUsart, USART_FLAG_TC);
	pstPort->pstUsart->CR1 &= (UINT16)~USART_CR1_TCIE;

	if (pstPort->pu8TxEnableFlag != 0)
	{
		*pstPort->pu8TxEnableFlag = 0;
	}
	if (pstPort->pu8TxFinishFlag != 0)
	{
		*pstPort->pu8TxFinishFlag = 1;
	}

	if ((pstPort->pstProtocolOps != 0) && (pstPort->pstProtocolOps->pfOnTxComplete != 0))
	{
		pstPort->pstProtocolOps->pfOnTxComplete(pstPort->pvProtocolCtx);
	}

	if (u8FlashUpdateE2PROM != 0U)
	{
		u8FlashUpdateE2PROM = 0;
		u8FlashUpdateFlag = 1;
	}

	Sci_PortArmReceiver(pstPort);
}

static void Sci_PortHandleError(struct SCI_PORT_RUNTIME *pstPort)
{
	volatile UINT16 u16Dummy;

	u16Dummy = pstPort->pstUsart->DR;
	(void)u16Dummy;

	if (pstPort->pu16ErrorCounter != 0)
	{
		(*pstPort->pu16ErrorCounter)++;
	}

	Sci_PortAbortTransfer(pstPort);
}

static void Sci_PortIRQHandler(struct SCI_PORT_RUNTIME *pstPort)
{
	UINT16 u16Status;

	u16Status = pstPort->pstUsart->SR;

	if ((u16Status & SCI_USART_ERROR_FLAGS) != 0U)
	{
		Sci_PortHandleError(pstPort);
		return;
	}

	if (((u16Status & USART_SR_RXNE) != 0U) &&
		((pstPort->pstUsart->CR1 & USART_CR1_RXNEIE) != 0U))
	{
		UINT8 u8RxData;

		LP_NotifyExternalComm();
		u8RxData = (UINT8)pstPort->pstUsart->DR;

		if ((pstPort->pstProtocolOps != 0) &&
			(pstPort->pstProtocolOps->pfRxFeed != 0) &&
			(pstPort->pstProtocolOps->pfRxFeed(pstPort->pvProtocolCtx, u8RxData) != 0U))
		{
			pstPort->u8FramePending = 1;
			pstPort->pstUsart->CR1 &= (UINT16)~(USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_IDLEIE);
		}
	}

	u16Status = pstPort->pstUsart->SR;
	if (((u16Status & USART_SR_IDLE) != 0U) &&
		((pstPort->pstUsart->CR1 & USART_CR1_IDLEIE) != 0U))
	{
		volatile UINT16 u16Dummy;

		u16Dummy = pstPort->pstUsart->DR;
		(void)u16Dummy;

		if ((pstPort->u8FramePending == 0U) &&
			(pstPort->u16TxLength == 0U) &&
			(pstPort->pstProtocolOps != 0) &&
			(pstPort->pstProtocolOps->pfOnRxIdle != 0))
		{
			pstPort->pstProtocolOps->pfOnRxIdle(pstPort->pvProtocolCtx);
		}
	}

	if (((pstPort->pstUsart->SR & USART_SR_TXE) != 0U) &&
		((pstPort->pstUsart->CR1 & USART_CR1_TXEIE) != 0U))
	{
		if ((pstPort->pu8TxBuffer == 0) || (pstPort->u16TxIndex >= pstPort->u16TxLength))
		{
			pstPort->pstUsart->CR1 &= (UINT16)~USART_CR1_TXEIE;
			pstPort->pstUsart->CR1 |= USART_CR1_TCIE;
		}
		else
		{
			pstPort->pstUsart->DR = pstPort->pu8TxBuffer[pstPort->u16TxIndex++];
			if (pstPort->u16TxIndex >= pstPort->u16TxLength)
			{
				pstPort->pstUsart->CR1 &= (UINT16)~USART_CR1_TXEIE;
				pstPort->pstUsart->CR1 |= USART_CR1_TCIE;
			}
		}
	}

	if (((pstPort->pstUsart->SR & USART_SR_TC) != 0U) &&
		((pstPort->pstUsart->CR1 & USART_CR1_TCIE) != 0U))
	{
		Sci_PortFinishTx(pstPort);
	}
}

static void Sci_PortService(struct SCI_PORT_RUNTIME *pstPort)
{
	if ((pstPort == 0) || (pstPort->u8FramePending == 0U))
	{
		return;
	}

	pstPort->u8FramePending = 0;

	if ((pstPort->pstProtocolOps != 0) && (pstPort->pstProtocolOps->pfProcessFrame != 0))
	{
		pstPort->pstProtocolOps->pfProcessFrame(pstPort->pvProtocolCtx);
	}

	if ((pstPort->pstProtocolOps != 0) && (pstPort->pstProtocolOps->pfGetTxBuffer != 0))
	{
		pstPort->pu8TxBuffer = pstPort->pstProtocolOps->pfGetTxBuffer(pstPort->pvProtocolCtx);
	}
	else
	{
		pstPort->pu8TxBuffer = 0;
	}

	if ((pstPort->pstProtocolOps != 0) && (pstPort->pstProtocolOps->pfGetTxLength != 0))
	{
		pstPort->u16TxLength = pstPort->pstProtocolOps->pfGetTxLength(pstPort->pvProtocolCtx);
	}
	else
	{
		pstPort->u16TxLength = 0;
	}

	if ((pstPort->pu8TxBuffer != 0) && (pstPort->u16TxLength != 0U))
	{
		Sci_PortStartTx(pstPort);
	}
	else
	{
		Sci_PortAbortTransfer(pstPort);
	}
}

static UINT8 Sci_PortIsBusy(const struct SCI_PORT_RUNTIME *pstPort)
{
	if (pstPort == 0)
	{
		return 0;
	}

	if ((pstPort->u8FramePending != 0U) || (pstPort->u16TxLength != 0U))
	{
		return 1;
	}

	if ((pstPort->pstProtocolOps != 0) && (pstPort->pstProtocolOps->pfIsBusy != 0))
	{
		return pstPort->pstProtocolOps->pfIsBusy(pstPort->pvProtocolCtx);
	}

	return 0;
}

static void Sci_InitCommonPort(struct SCI_PORT_RUNTIME *pstPort,
							   IRQn_Type eIrqChannel,
							   UINT8 u8UseApb2Bus,
							   UINT32 u32UsartClock,
							   GPIO_TypeDef *pstTxGpio,
							   UINT16 u16TxPin,
							   GPIO_TypeDef *pstRxGpio,
							   UINT16 u16RxPin,
							   UINT32 u32RemapConfig)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	if (u8UseApb2Bus != 0U)
	{
		RCC_APB2PeriphClockCmd(u32UsartClock, ENABLE);
	}
	else
	{
		RCC_APB1PeriphClockCmd(u32UsartClock, ENABLE);
	}

	if (u32RemapConfig != 0U)
	{
		GPIO_PinRemapConfig(u32RemapConfig, ENABLE);
	}

	NVIC_InitStructure.NVIC_IRQChannel = eIrqChannel;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	GPIO_InitStructure.GPIO_Pin = u16TxPin;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(pstTxGpio, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = u16RxPin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(pstRxGpio, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = 19200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(pstPort->pstUsart, &USART_InitStructure);

	pstPort->pstUsart->CR3 |= USART_CR3_EIE;
	USART_ITConfig(pstPort->pstUsart, USART_IT_RXNE, ENABLE);
	USART_ITConfig(pstPort->pstUsart, USART_IT_IDLE, ENABLE);
	USART_ITConfig(pstPort->pstUsart, USART_IT_TXE, DISABLE);
	USART_ITConfig(pstPort->pstUsart, USART_IT_TC, DISABLE);
	USART_Cmd(pstPort->pstUsart, ENABLE);

	if (pstPort->pvProtocolCtx != 0)
	{
		Sci_DataInit((struct RS485MSG *)pstPort->pvProtocolCtx);
	}
	if ((pstPort->pstProtocolOps != 0) && (pstPort->pstProtocolOps->pfReset != 0))
	{
		pstPort->pstProtocolOps->pfReset(pstPort->pvProtocolCtx);
	}
	if (pstPort->pu16ErrorCounter != 0)
	{
		*pstPort->pu16ErrorCounter = 0;
	}
	if (pstPort->pu8TxEnableFlag != 0)
	{
		*pstPort->pu8TxEnableFlag = 0;
	}
	if (pstPort->pu8TxFinishFlag != 0)
	{
		*pstPort->pu8TxFinishFlag = 0;
	}

	Sci_PortArmReceiver(pstPort);
}

UINT8 Sci_IsAnyPortBusy(void)
{
	UINT8 busy;

	busy = (UINT8)Sci_PortIsBusy(&g_stSciPort1);
#ifdef _COMMOM_UPPER_SCI2
	busy = (UINT8)(busy || Sci_PortIsBusy(&g_stSciPort2));
#endif
#ifdef _COMMOM_UPPER_SCI3
	busy = (UINT8)(busy || Sci_PortIsBusy(&g_stSciPort3));
#endif
	return busy;
}

void Sci1_CommonUpper_IRQHandler(void)
{
	Sci_PortIRQHandler(&g_stSciPort1);
}

void Sci2_CommonUpper_IRQHandler(void)
{
#ifdef _COMMOM_UPPER_SCI2
	Sci_PortIRQHandler(&g_stSciPort2);
#endif
}

#ifdef _COMMOM_UPPER_SCI3
void Sci3_CommonUpper_IRQHandler(void)
{
	Sci_PortIRQHandler(&g_stSciPort3);
}
#endif

void InitSCI1_CommonUpper(void)
{
	Sci_InitCommonPort(&g_stSciPort1,
					   USART1_IRQn,
					   1U,
					   RCC_APB2Periph_USART1,
					   GPIOB,
					   GPIO_Pin_6,
					   GPIOB,
					   GPIO_Pin_7,
					   GPIO_Remap_USART1);
}

void InitSCI2_CommonUpper(void)
{
#ifdef _COMMOM_UPPER_SCI2
	Sci_InitCommonPort(&g_stSciPort2,
					   USART2_IRQn,
					   0U,
					   RCC_APB1Periph_USART2,
					   GPIOA,
					   GPIO_Pin_2,
					   GPIOA,
					   GPIO_Pin_3,
					   0U);
#endif
}

#ifdef _COMMOM_UPPER_SCI3
void InitSCI3_CommonUpper(void)
{
	Sci_InitCommonPort(&g_stSciPort3,
					   USART3_IRQn,
					   0U,
					   RCC_APB1Periph_USART3,
					   GPIOD,
					   GPIO_Pin_8,
					   GPIOD,
					   GPIO_Pin_9,
					   GPIO_FullRemap_USART3);
}
#endif

void Sci_WrRegs_0x10_CalibCoef(UINT16 u16Channel, struct RS485MSG *s)
{
#if 0
	UINT16 t_u16K, t_u16B, t_u16Temp;
	INT16 t_i16B;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);

	if (u16WrRegNum == 2)
	{
		t_u16K = s->u16Buffer[8] + (s->u16Buffer[7] << 8);
		t_u16B = s->u16Buffer[10] + (s->u16Buffer[9] << 8);

		t_u16Temp = t_u16B & 0x8000;
		if (t_u16Temp == 0)
		{
			t_i16B = t_u16B & 0x7FFF;
		}
		else
		{
			t_i16B = -(t_u16B & 0x7FFF);
		}

		if ((t_u16K < SYSKMIN) || (t_u16K > SYSKMAX))
		{
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_DATA_INVALID;
			return;
		}

		if ((t_i16B < SYSBMIN) || (t_i16B > SYSBMAX))
		{
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_DATA_INVALID;
			return;
		}

		t_u16Temp = (u16Channel - RS485_CMD_ADDR_VC1CALIB_K) >> 1;
		g_u16CalibCoefK[t_u16Temp] = t_u16K;
		g_i16CalibCoefB[t_u16Temp] = t_i16B;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
#endif
}

// 节省了很多代码量吧？
void Sci_WrRegs_0x10_Protect(UINT16 u16Channel, struct RS485MSG *s)
{
	UINT16 offset;
	UINT16 u16WrRegNum;
	UINT16 snapshot[E2P_PARA_NUM_PROTECT];
	const struct PRT_E2ROM_PARAS protect_min = E2P_PROTECT_MIN_PRT;
	const struct PRT_E2ROM_PARAS protect_max = E2P_PROTECT_MAX_PRT;
	UINT16 *base = &PRT_E2ROMParas.u16VcellOvp_First;

	u16WrRegNum = Sci_GetWrRegNum(s);
	offset = (UINT16)(u16Channel - RS485_CMD_ADDR_VCELL_OVP_FIRST);
	if (!Sci_WrRegsByteCountValid(s, u16WrRegNum) ||
		!Sci_RangeFits(offset, u16WrRegNum, E2P_PARA_NUM_PROTECT))
	{
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}

	if (!Sci_WrValuesInRange(s,
						   offset,
						   u16WrRegNum,
						   &protect_min.u16VcellOvp_First,
						   &protect_max.u16VcellOvp_First))
	{
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
		return;
	}

	Sci_CopyWords(snapshot, base, E2P_PARA_NUM_PROTECT);
	Sci_WriteWordsFromRequest(s, base, offset, u16WrRegNum);

	if (!EEPROM_SaveRWParametersToFlash())
	{
		Sci_CopyWords(base, snapshot, E2P_PARA_NUM_PROTECT);
		Sci_ApplyProtectSideEffects(offset, u16WrRegNum);
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}

	Sci_ApplyProtectSideEffects(offset, u16WrRegNum);
}

void Sci_WrRegs_0x10_SocTable(struct RS485MSG *s)
{
#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == E2P_PARA_NUM_SOC_TABLE)
	{
		for (i = 0; i < E2P_PARA_NUM_SOC_TABLE; ++i)
		{
			SOC_Table_Set[i] = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}

		InitData_SOC();
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
#else
	s->AckType = RS485_ACK_NEG;
	s->ErrorType = RS485_ERROR_CMD_INVALID;
#endif
}

void Sci_WrRegs_0x10_CopperLoss(struct RS485MSG *s)
{
}

void Sci_WrRegs_0x10_RTC(struct RS485MSG *s)
{
	
}

void Sci_WrRegs_0x10_OtherElement(UINT16 u16Channel, struct RS485MSG *s)
{
	UINT16 offset;
	UINT16 u16WrRegNum;
	UINT16 snapshot[E2P_PARA_NUM_OTHER_ELEMENT1];
	const struct OTHER_ELEMENT other_min = OtherElement_min;
	const struct OTHER_ELEMENT other_max = OtherElement_max;
	UINT16 *base = &OtherElement.u16Balance_OpenVoltage;

	u16WrRegNum = Sci_GetWrRegNum(s);
	offset = (UINT16)(u16Channel - RS485_CMD_ADDR_BALANCE_OV);
	if (!Sci_WrRegsByteCountValid(s, u16WrRegNum) ||
		!Sci_RangeFits(offset, u16WrRegNum, E2P_PARA_NUM_OTHER_ELEMENT1))
	{
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}

	if (!Sci_WrValuesInRange(s,
						   offset,
						   u16WrRegNum,
						   &other_min.u16Balance_OpenVoltage,
						   &other_max.u16Balance_OpenVoltage))
	{
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
		return;
	}

	Sci_CopyWords(snapshot, base, E2P_PARA_NUM_OTHER_ELEMENT1);
	Sci_WriteWordsFromRequest(s, base, offset, u16WrRegNum);

	if (!EEPROM_SaveRWParametersToFlash())
	{
		Sci_CopyWords(base, snapshot, E2P_PARA_NUM_OTHER_ELEMENT1);
		Sci_ApplyOtherElementSideEffects(0, E2P_PARA_NUM_OTHER_ELEMENT1);
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}

	Sci_ApplyOtherElementSideEffects(offset, u16WrRegNum);
}

void Sci_WrRegs_0x10_Balance(struct RS485MSG *s)
{
	Sci_WrRegs_0x10_OtherElement(RS485_CMD_ADDR_BALANCE_OV, s);
}

void Sci_WrRegs_0x10_SysOther(struct RS485MSG *s)
{
	Sci_WrRegs_0x10_OtherElement(RS485_CMD_ADDR_CS_CUR_CHGMAX, s);
}

void Sci_WrRegs_0x10_SleepElement(struct RS485MSG *s)
{
	Sci_WrRegs_0x10_OtherElement(RS485_CMD_ADDR_SLEEP_V_NORMAL, s);
}

void Sci_WrRegs_0x10_SocElement(struct RS485MSG *s)
{
	Sci_WrRegs_0x10_OtherElement(RS485_CMD_ADDR_SOC_AH, s);
}

void Sci_WrRegs_0x10_SystemElement(struct RS485MSG *s)
{
	Sci_WrRegs_0x10_OtherElement(RS485_CMD_ADDR_SYS_SERIES_NUM, s);
}

void Sci_WrRegs_0x10_FlashConnect(struct RS485MSG *s)
{
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 1)
	{
		if (AppUpgrade_RequestIap() == 0U)
		{
			// System_ERROR_UserCallback(ERROR_FLASH);
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_CMD_INVALID;
		}
		else
		{
			u8FlashUpdateE2PROM = 1;
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

/* 把BMS序列号，硬件版本号， 软件版本号写入 ohterInfor结构体
 * startADDR  如起始地址
 */
void Sci_WrRegs_0x10_SN_Version(UINT16 startADDR, struct RS485MSG *s)
{
	UINT16 u16WrSNlength;

	u16WrSNlength = (UINT16)((UINT16)s->u16Buffer[5] + ((UINT16)s->u16Buffer[4] << 8)) << 1;

	switch (startADDR - RS485_ADDR_SN_SERIAL_NUM)
	{
	case 0:
		Sci_CopyProductIdBytes(ProductionInfor.BMS_SerialNumber,
							   &ProductionInfor.BMS_SerialNumberLength,
							   s,
							   u16WrSNlength);
		break;

	case 1:
		Sci_CopyProductIdBytes(ProductionInfor.BMS_HardWareVersion,
							   &ProductionInfor.BMS_HardWareVersionLength,
							   s,
							   u16WrSNlength);
		break;

	case 2:
		Sci_CopyProductIdBytes(ProductionInfor.BMS_SoftWareVersion,
							   &ProductionInfor.BMS_SoftWareVersionLength,
							   s,
							   u16WrSNlength);
		break;

	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
		break;
	}
}

void Sci_WrReg_0x06_Reset_CalibCoef(struct RS485MSG *s)
{
#if 0
	UINT16 u16SciRegData;
	UINT8 i;
	UINT8 first_index;
	UINT8 store_index;
	UINT8 count;

	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16SciRegData == 0x55AA)
	{
		first_index = VOLT_C1;
		store_index = VOLT_C1;
		count = 32U;
	}
	else if ((u16SciRegData >= 0x55AB) && (u16SciRegData <= 0x55AD))
	{
		first_index = (UINT8)(VOLT_AFE1 + (u16SciRegData - 0x55AB));
		store_index = first_index;
		count = 1U;
	}
	else if (u16SciRegData == 0x55AE)
	{
		first_index = MDL_TEMP1;
		store_index = VOLT_C1;
		count = 10U;
	}
	else if (u16SciRegData == 0x55AF)
	{
		first_index = MDL_IDSG;
		store_index = MDL_IDSG;
		count = 1U;
	}
	else if (u16SciRegData == 0x55B0)
	{
		first_index = MDL_ICHG;
		store_index = MDL_ICHG;
		count = 1U;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
		return;
	}

	for (i = 0; i < count; ++i)
	{
		Sci_ResetCalibCoefIndex((UINT8)(first_index + i), (UINT8)(store_index + i));
	}
#endif
}

void Sci_WrReg_0x06_Reset_ProtectRecord(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	UINT8 i;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		for (i = 0; i < Record_len; ++i)
		{
			Fault_record_Third[i] = 0;
		}
		FaultPoint_Third = 0;
		Fault_Flag_Fisrt.all = 0;
		Fault_Flag_Second.all = 0;
		Fault_Flag_Third.all = 0;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_Reset_ProtectElement(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	UINT8 i;
	UINT16 snapshot[E2P_PARA_NUM_PROTECT];
	UINT16 *base = &PRT_E2ROMParas.u16VcellOvp_First;
	const struct PRT_E2ROM_PARAS PrtE2PARAS_Default = E2P_PROTECT_DEFAULT_PRT;

	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		Sci_CopyWords(snapshot, base, E2P_PARA_NUM_PROTECT);
		for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
		{
			*(base + i) = *(&PrtE2PARAS_Default.u16VcellOvp_First + i);
		}

		if (!EEPROM_SaveRWParametersToFlash())
		{
			Sci_CopyWords(base, snapshot, E2P_PARA_NUM_PROTECT);
			Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
			return;
		}
		InitData_SOC();
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_Reset_OtherCanAdd(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	UINT8 i;
	UINT16 snapshot[E2P_PARA_NUM_OTHER_ELEMENT1];
	UINT16 *base = &OtherElement.u16Balance_OpenVoltage;
	const struct OTHER_ELEMENT OtherElement_Default = OtherElement_default;

	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		Sci_CopyWords(snapshot, base, E2P_PARA_NUM_OTHER_ELEMENT1);
		for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
		{
			*(base + i) = *(&OtherElement_Default.u16Balance_OpenVoltage + i);
		}

		if (!EEPROM_SaveRWParametersToFlash())
		{
			Sci_CopyWords(base, snapshot, E2P_PARA_NUM_OTHER_ELEMENT1);
			Sci_ApplyOtherElementSideEffects(0, E2P_PARA_NUM_OTHER_ELEMENT1);
			Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
			return;
		}
		Sci_ApplyOtherElementSideEffects(0, E2P_PARA_NUM_OTHER_ELEMENT1);
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

// 关于这个函数
// A:第一次打开这个功能，以前从来没打开过，则因为各种标志位变量都没变过(switch结构里面的)，所以会进行初始化验证
// B:其中关闭了，又打开，则已经初始化过一次，这次打开就继续按照上一次的进度继续下去
void Sci_WrReg_0x06_BMS_FunctionON(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (Sci_BmsFunctionIdIsSupported(u16SciRegData))
	{
		switch (u16SciRegData)
		{		// 如果是以下功能被打开，则需要初始化验证，别的功能直接关就好
		case 1: // 均衡
			break;
		case 3: // MOS或者接触器功能
			break;
		case 8: // 激活模拟前端AFE1
			break;
		case 0x0A: // 立刻进入休眠
			LP_RequestSleep(DEEP_MODE);
			break;
		default:
			break;
		}

		SystemFeature_SetById(u16SciRegData, 1U);
		if (u16SciRegData == 0x0B)
		{
			// SOC zero overlay defaults to off and is not persisted.
			// 默认为0，不需要保存
		}
		else
		{
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_BMS_FunctionOFF(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (Sci_BmsFunctionIdIsSupported(u16SciRegData))
	{
		SystemFeature_SetById(u16SciRegData, 0U);
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_SetSocOnce(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16SciRegData <= 100)
	{
		SOC_Enhance_Element.u16_RefreshData_Flag = 3;
		SOC_Enhance_Element.u8_SetSocOnce = u16SciRegData;
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void InitUSART_CommonUpper(void)
{
#ifdef _COMMOM_UPPER_SCI1
	InitSCI1_CommonUpper();
#endif

#ifdef _COMMOM_UPPER_SCI2
	InitSCI2_CommonUpper();
#endif

#ifdef _COMMOM_UPPER_SCI3
	InitSCI3_CommonUpper();
#endif
}

void App_CommonUpper(void)
{
#ifdef _COMMOM_UPPER_SCI1
	Sci_PortService(&g_stSciPort1);
#endif

#ifdef _COMMOM_UPPER_SCI2
	Sci_PortService(&g_stSciPort2);
#endif

#ifdef _COMMOM_UPPER_SCI3
	Sci_PortService(&g_stSciPort3);
#endif
}

#if 1
#define debug_uart USART1
#else
#define debug_uart USART2
#endif
#define SCI_DEBUG_UART_TX_WAIT_LOOP ((UINT32)100000U)

int fputc(int ch, FILE *f)
{
#if 0 /* 将需要printf的字符通过串口中断FIFO发送出去，printf函数会立即返回 */
	comSendChar(COM1, ch);

	return ch;
#else /* 采用阻塞方式发送每个字符,等待数据发送完毕 */
	UINT32 wait_loop = SCI_DEBUG_UART_TX_WAIT_LOOP;

	/* 写一个字节到USART1 */
	USART_SendData(debug_uart, (uint8_t)ch);

	/* 等待发送结束 */
	while ((USART_GetFlagStatus(debug_uart, USART_FLAG_TC) == RESET) && (wait_loop > 0U))
	{
		Feed_IWatchDog;
		wait_loop--;
	}

	return ch;
#endif
}
