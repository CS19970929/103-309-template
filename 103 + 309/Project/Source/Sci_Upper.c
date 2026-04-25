#include "main.h"

struct RS485MSG g_stCurrentMsgPtr_SCI1;
UINT16 gu16_CommuErrCnt_SCI1 = 0; // SCI通信异常计数
UINT8 gu8_TxEnable_SCI1 = 0;
UINT8 gu8_TxFinishFlag_SCI1 = 0;

struct RS485MSG g_stCurrentMsgPtr_SCI2;
UINT16 gu16_CommuErrCnt_SCI2 = 0; // SCI通信异常计数
UINT8 gu8_TxEnable_SCI2 = 0;
UINT8 gu8_TxFinishFlag_SCI2 = 0;

struct RS485MSG g_stCurrentMsgPtr_SCI3;
UINT16 gu16_CommuErrCnt_SCI3 = 0; // SCI通信异常计数
UINT8 gu8_TxEnable_SCI3 = 0;
UINT8 gu8_TxFinishFlag_SCI3 = 0;

UINT8 g_u8SCITxBuff[SCI_TX_BUF_LEN];

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
static UINT8 Sci_ModbusProtocolFeed(void *pvProtocolCtx, UINT8 u8Data);
static void Sci_ModbusProcessFrame(void *pvProtocolCtx);
static UINT8 *Sci_ModbusGetTxBuffer(void *pvProtocolCtx);
static UINT16 Sci_ModbusGetTxLength(void *pvProtocolCtx);
static UINT8 Sci_ModbusIsBusy(void *pvProtocolCtx);
static void Sci_ModbusResetProtocol(void *pvProtocolCtx);
static void Sci_ModbusOnRxIdle(void *pvProtocolCtx);
static void Sci_ModbusOnTxComplete(void *pvProtocolCtx);

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
void Sci_WrRegs_0x10_HeatCoolElement(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_FlashConnect(struct RS485MSG *s);
void Sci_WrRegs_0x10_SN_Version(UINT16 startADDR, struct RS485MSG *s);

void Sci_WrReg_0x06_Reset_CalibCoef(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_ProtectRecord(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_ProtectElement(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_OtherCanAdd(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_HeatCool(struct RS485MSG *s);
void Sci_WrReg_0x06_SwitchON(struct RS485MSG *s);
void Sci_WrReg_0x06_SwitchOFF(struct RS485MSG *s);
void Sci_WrReg_0x06_BMS_FunctionON(struct RS485MSG *s);
void Sci_WrReg_0x06_BMS_FunctionOFF(struct RS485MSG *s);
void Sci_WrReg_0x06_SetSocOnce(struct RS485MSG *s);

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

	t_u16Temp = s->u16Buffer[3] + (s->u16Buffer[2] << 8);
	s->u16RdRegStartAddrActure = t_u16Temp;

	if (t_u16Temp >= RS485_ADDR_RO_START2)
	{ // 1个字
		t_u16Temp -= (RS485_ADDR_RO_START2 - 63 - 33);
	}

	else if (t_u16Temp >= RS485_ADDR_RO_START1)
	{ // 33个字
		t_u16Temp -= (RS485_ADDR_RO_START1 - 63);
	}

	else if (t_u16Temp >= RS485_ADDR_RO_START0)
	{ // 63个字
		t_u16Temp -= RS485_ADDR_RO_START0;
	}
	// 新加进来的
	else if (t_u16Temp >= RS485_ADDR_RO_LCD)
	{
		t_u16Temp -= RS485_ADDR_RO_LCD; // LCD，有一次顺序乱了，显示数据不对导致找不到原因
	}
	else if (t_u16Temp >= RS485_ADDR_RW_AFE_PARAMETER)
	{
		t_u16Temp -= RS485_ADDR_RW_AFE_PARAMETER; // AFE，耕耘代码添加，前面出问题是忘了这里要添加
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
	s->u16RdRegByteNum = (s->u16Buffer[5] + (s->u16Buffer[4] << 8)) << 1;
}

void Sci_Deal_WrReg_0x06(struct RS485MSG *s)
{
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

	case RS485_CMD_ADDR_RESET_HEAT_COOL:
		Sci_WrReg_0x06_Reset_HeatCool(s);
		break;

	case RS485_CMD_ADDR_SWITCH_ON:
		Sci_WrReg_0x06_SwitchON(s);
		break;

	case RS485_CMD_ADDR_SWITCH_OFF:
		Sci_WrReg_0x06_SwitchOFF(s);
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

	if (Sci_RangeOverlaps(offset, count, 24, 4))
	{
		InitData_SOC();
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
	UINT16 u16SciRegStartAddr;
	u16SciRegStartAddr = s->u16Buffer[3] + (s->u16Buffer[2] << 8);

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

	if ((u16SciRegStartAddr >= RS485_CMD_ADDR_HEAT_DSG_HIGH) &&
		(u16SciRegStartAddr < (UINT16)(RS485_CMD_ADDR_HEAT_DSG_HIGH + E2P_PARA_NUM_HEAT_COOL)))
	{
		Sci_WrRegs_0x10_HeatCoolElement(u16SciRegStartAddr, s);
		return;
	}

	switch (u16SciRegStartAddr)
	{
	case RS485_CMD_ADDR_VC1CALIB_K:
	case RS485_CMD_ADDR_VC2CALIB_K:
	case RS485_CMD_ADDR_VC3CALIB_K:
	case RS485_CMD_ADDR_VC4CALIB_K:
	case RS485_CMD_ADDR_VC5CALIB_K:
	case RS485_CMD_ADDR_VC6CALIB_K:
	case RS485_CMD_ADDR_VC7CALIB_K:
	case RS485_CMD_ADDR_VC8CALIB_K:
	case RS485_CMD_ADDR_VC9CALIB_K:
	case RS485_CMD_ADDR_VC10CALIB_K:
	case RS485_CMD_ADDR_VC11CALIB_K:
	case RS485_CMD_ADDR_VC12CALIB_K:
	case RS485_CMD_ADDR_VC13CALIB_K:
	case RS485_CMD_ADDR_VC14CALIB_K:
	case RS485_CMD_ADDR_VC15CALIB_K:
	case RS485_CMD_ADDR_VC16CALIB_K:
	case RS485_CMD_ADDR_VC17CALIB_K:
	case RS485_CMD_ADDR_VC18CALIB_K:
	case RS485_CMD_ADDR_VC19CALIB_K:
	case RS485_CMD_ADDR_VC20CALIB_K:
	case RS485_CMD_ADDR_VC21CALIB_K:
	case RS485_CMD_ADDR_VC22CALIB_K:
	case RS485_CMD_ADDR_VC23CALIB_K:
	case RS485_CMD_ADDR_VC24CALIB_K:
	case RS485_CMD_ADDR_VC25CALIB_K:
	case RS485_CMD_ADDR_VC26CALIB_K:
	case RS485_CMD_ADDR_VC27CALIB_K:
	case RS485_CMD_ADDR_VC28CALIB_K:
	case RS485_CMD_ADDR_VC29CALIB_K:
	case RS485_CMD_ADDR_VC30CALIB_K:
	case RS485_CMD_ADDR_VC31CALIB_K:
	case RS485_CMD_ADDR_VC32CALIB_K:
	case RS485_CMD_ADDR_AFE1CALIB_K:
	case RS485_CMD_ADDR_AFE2CALIB_K:
	case RS485_CMD_ADDR_VBUSCALIB_K:
	case RS485_CMD_ADDR_ICHGCALIB_K:
	case RS485_CMD_ADDR_IDISCHGCALIB_K:
	case RS485_CMD_ADDR_TEMP1_CALIB_K:
	case RS485_CMD_ADDR_TEMP2_CALIB_K:
	case RS485_CMD_ADDR_TEMP3_CALIB_K:
	case RS485_CMD_ADDR_TEMP4_CALIB_K:
	case RS485_CMD_ADDR_TEMP5_CALIB_K:
	case RS485_CMD_ADDR_TEMP6_CALIB_K:
	case RS485_CMD_ADDR_TEMP_ENV1_CALIB_K:
	case RS485_CMD_ADDR_TEMP_ENV2_CALIB_K:
	case RS485_CMD_ADDR_TEMP_ENV3_CALIB_K:
	case RS485_CMD_ADDR_TEMP_MOS_CALIB_K:
		Sci_WrRegs_0x10_CalibCoef(u16SciRegStartAddr, s);
		break;

	case RS485_CMD_ADDR_VCELL_OVP_FIRST:
	case RS485_CMD_ADDR_VCELL_UVP_FIRST:
	case RS485_CMD_ADDR_VBUS_OVP_FIRST:
	case RS485_CMD_ADDR_VBUS_UVP_FIRST:
	case RS485_CMD_ADDR_ICHG_OCP_FIRST:
	case RS485_CMD_ADDR_IDSG_OCP_FIRST:
	case RS485_CMD_ADDR_TCHG_OTP_FIRST:
	case RS485_CMD_ADDR_TCHG_UTP_FIRST:
	case RS485_CMD_ADDR_TDSG_OTP_FIRST:
	case RS485_CMD_ADDR_TDSG_UTP_FIRST:
	case RS485_CMD_ADDR_TMOS_OTP_FIRST:
	case RS485_CMD_ADDR_VDELTA_OP_FIRST:
	case RS485_CMD_ADDR_SOC_UP_FIRST:
		Sci_WrRegs_0x10_Protect(u16SciRegStartAddr, s);
		break;

	case RS485_CMD_ADDR_SOC_VOLTAGE1:
		Sci_WrRegs_0x10_SocTable(s);
		break;

	case RS485_CMD_ADDR_COPPERLOSS1:
		Sci_WrRegs_0x10_CopperLoss(s);
		break;

	case RS485_CMD_ADDR_RTC_TIME_YEAR:
		Sci_WrRegs_0x10_RTC(s);
		break;

	case RS485_CMD_ADDR_BALANCE_OV:
		Sci_WrRegs_0x10_Balance(s);
		break;

	case RS485_CMD_ADDR_CS_CUR_CHGMAX:
		Sci_WrRegs_0x10_SysOther(s);
		break;

	case RS485_CMD_ADDR_SLEEP_V_NORMAL:
		Sci_WrRegs_0x10_SleepElement(s);
		break;

	case RS485_CMD_ADDR_SOC_AH:
		Sci_WrRegs_0x10_SocElement(s);
		break;

	case RS485_CMD_ADDR_SYS_SERIES_NUM:
		Sci_WrRegs_0x10_SystemElement(s);
		break;

	case RS485_CMD_ADDR_HEAT_DSG_HIGH:
		Sci_WrRegs_0x10_HeatCoolElement(u16SciRegStartAddr, s);
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
}

void Sci_ACK_0x03_ReadRegs_LCD(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
	UINT16 u16SciTemp;
	UINT16 i, j;
	INT8 k, x;

	i = 0;
	switch (s->u16RdRegStartAddr)
	{
	case 0: // LCD
		u16SciTemp = 1;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

		u16SciTemp = (g_stCellInfoReport.u16VCellTotle + 50) / 100;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

		if (g_stCellInfoReport.u16Ichg > 0)
		{
			u16SciTemp = (g_stCellInfoReport.u16Ichg + 5005) / 10;
		}
		else
		{
			u16SciTemp = (5000 - g_stCellInfoReport.u16IDischg) / 10;
		}
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

		u16SciTemp = (g_stCellInfoReport.u16TempMax + 5) / 10;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

		u16SciTemp = g_stCellInfoReport.SocElement.u16Soc;
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
		break;

	case 1: // 上位机第三级保护，60+10=70个
		for (j = 0; j < Record_len; j++)
		{
			k = FaultPoint_Third - 1 - j;
			if (k < 0)
			{
				k = Record_len + k;
			}
			u16SciTemp = Fault_record_Third[k];
			t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
			t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

			for (x = 0; x < 6; ++x)
			{
				u16SciTemp = RTC_Fault_record_Third[k][x];
				t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
				t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
			}
		}
		break;

	case 2: // 序列号，硬件版本号，软件版本号
		for (j = 0; j < PRODUCT_ID_LENGTH_MAX; j++)
		{
			t_u8BuffTemp[i++] = ProductionInfor.BMS_SerialNumber[j];
		}
		for (j = 0; j < PRODUCT_ID_LENGTH_MAX; j++)
		{
			t_u8BuffTemp[i++] = ProductionInfor.BMS_HardWareVersion[j];
		}
		for (j = 0; j < PRODUCT_ID_LENGTH_MAX; j++)
		{
			t_u8BuffTemp[i++] = ProductionInfor.BMS_SoftWareVersion[j];
		}
		break;

	case 8:
		Sci_ACK_0x03_ReadRegs_EventRecord(t_u8BuffTemp);
		break;

	default:
		s->u16RdRegStartAddr = 0;
		break;
	}
	s->u16RdRegStartAddr = 0;
}

void Sci_ACK_0x03_ReadRegs_Data(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
	UINT16 u16SciTemp;
	UINT16 i = 0, j;
	INT8 k;
	UINT8 a[4];

	for (j = 0; j < 63; j++)
	{ // 0xD000_63
		u16SciTemp = *(&g_stCellInfoReport.u16VCell[0] + j);
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	// 0xD100_33
	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Month) | (RTC_time.RTC_Time_Year << 8);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Hour) | (RTC_time.RTC_Time_Day << 8);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Second) | (RTC_time.RTC_Time_Minute << 8);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	for (j = 0; j < 4; j++)
	{
		k = FaultPoint_First2 - 1 - j;
		if (k < 0)
		{
			k = Record_len + k;
		}
		a[j] = k;
	}
	u16SciTemp = (Fault_record_First2[a[0]] << 8) | Fault_record_First2[a[1]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	u16SciTemp = (Fault_record_First2[a[2]] << 8) | Fault_record_First2[a[3]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	for (j = 0; j < 4; j++)
	{
		k = FaultPoint_Second2 - 1 - j;
		if (k < 0)
		{
			k = Record_len + k;
		}
		a[j] = k;
	}
	u16SciTemp = (Fault_record_Second2[a[0]] << 8) | Fault_record_Second2[a[1]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	u16SciTemp = (Fault_record_Second2[a[2]] << 8) | Fault_record_Second2[a[3]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	for (j = 0; j < 4; j++)
	{
		k = FaultPoint_Third2 - 1 - j;
		if (k < 0)
		{
			k = Record_len + k;
		}
		a[j] = k;
	}
	u16SciTemp = (Fault_record_Third2[a[0]] << 8) | Fault_record_Third2[a[1]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	u16SciTemp = (Fault_record_Third2[a[2]] << 8) | Fault_record_Third2[a[3]];
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	for (j = 0; j < 12; j++)
	{ // 0xD002到这里。
		u16SciTemp = ((*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + 2 * j)) << 8) | (*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + 2 * j + 1));
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	switch (OPEN)
	{
	case 0:
		u16SciTemp = ((~((UINT16)(SystemStatus.all & 0x0000FFFF))) & 0x00FE) | (((UINT16)(SystemStatus.all & 0x0000FFFF)) & 0xFF01);
		break;
	case 1:
		u16SciTemp = (UINT16)(SystemStatus.all & 0x0000FFFF);
		break;
	default:
		u16SciTemp = (UINT16)(SystemStatus.all & 0x0000FFFF);
		break;
	}
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)(SystemStatus.all >> 16);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)(System_OnOFF_Func.all & 0x0000FFFF);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)(System_OnOFF_Func.all >> 16);
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)0;
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = (UINT16)0;
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = Heat_Cool_FaultFlag.all; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;

	// 0xD200_1
	u16SciTemp = 0; // 可以加多一个
	t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
	t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
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
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
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
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
		u16SciTemp = g_i16CalibCoefB[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}
}

void Sci_ACK_0x03_RW_Data_Other(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 86
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < SOC_Size_TableCanSet; j++)
	{ // 由于GetEndValue()函数的问题，只能混在一起
		switch (OtherElement.u16Soc_TableSelect)
		{
		case SOC_TABLE_TEST:
			u16SciTemp = SOC_Table_Set[j];
			break;
		case SOC_TABLE_LIFEPO:
			u16SciTemp = SOC_Table_LiFePO[j];
			break;
		case SOC_TABLE_TERNARYLI:
			u16SciTemp = SocTable_TernaryLi[j];
			break;
		case SOC_TABLE_LIFEPO2:
			u16SciTemp = SocTable_LiFePO2[j];
			break;
		default:
			u16SciTemp = SOC_Table_Set[j];
			break;
		}
		// u16SciTemp = SOC_Table_Set[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	for (j = 0; j < CompensateNUM; j++)
	{
		u16SciTemp = CopperLoss[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	for (j = 0; j < CompensateNUM; j++)
	{
		u16SciTemp = CopperLoss_Num[j];
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	for (j = 0; j < E2P_PARA_NUM_RTC; j++)
	{
		u16SciTemp = *(&RTC_time.RTC_Time_Year + j);
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}
}

void Sci_ACK_0x03_RW_Data_OtherCanAdd(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 32+24=56个
	UINT16 u16SciTemp;
	UINT16 i = 0, j;

	for (j = 0; j < E2P_PARA_NUM_OTHER_ELEMENT1; j++)
	{
		u16SciTemp = *(&OtherElement.u16Balance_OpenVoltage + j);
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}

	for (j = 0; j < E2P_PARA_NUM_HEAT_COOL; j++)
	{
		u16SciTemp = *(&Heat_Cool_Element.u16Heat_OpenTemp + j);
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}
}

void Sci_ACK_0x03(struct RS485MSG *s)
{
	UINT8 i;
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

		RTC_ExtComCnt++;
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
	return (UINT8)(Sci_PortIsBusy(&g_stSciPort1) ||
				   Sci_PortIsBusy(&g_stSciPort2) ||
				   Sci_PortIsBusy(&g_stSciPort3));
}

void Sci1_CommonUpper_IRQHandler(void)
{
	Sci_PortIRQHandler(&g_stSciPort1);
}

void Sci2_CommonUpper_IRQHandler(void)
{
	Sci_PortIRQHandler(&g_stSciPort2);
}

void Sci3_CommonUpper_IRQHandler(void)
{
	Sci_PortIRQHandler(&g_stSciPort3);
}

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
	Sci_InitCommonPort(&g_stSciPort2,
					   USART2_IRQn,
					   0U,
					   RCC_APB1Periph_USART2,
					   GPIOA,
					   GPIO_Pin_2,
					   GPIOA,
					   GPIO_Pin_3,
					   0U);
}

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

void Sci_WrRegs_0x10_CalibCoef(UINT16 u16Channel, struct RS485MSG *s)
{
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
}

void Sci_WrRegs_0x10_CopperLoss(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == E2P_PARA_NUM_COPPERLOSS * 2)
	{
		for (i = 0; i < E2P_PARA_NUM_COPPERLOSS; ++i)
		{
			CopperLoss[i] = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
			CopperLoss_Num[i] = (UINT16)(s->u16Buffer[2 * (i + 16) + 8] + (s->u16Buffer[2 * (i + 16) + 7] << 8));
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

void Sci_WrRegs_0x10_RTC(struct RS485MSG *s)
{
	UINT8 i;
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == E2P_PARA_NUM_RTC)
	{
		for (i = 0; i < E2P_PARA_NUM_RTC; ++i)
		{
			*(&RTC_time.RTC_Time_Year + i) = (UINT16)(s->u16Buffer[2 * i + 8] + (s->u16Buffer[2 * i + 7] << 8));
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
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

void Sci_WrRegs_0x10_HeatCoolElement(UINT16 u16Channel, struct RS485MSG *s)
{
	UINT16 offset;
	UINT16 u16WrRegNum;
	UINT16 snapshot[E2P_PARA_NUM_HEAT_COOL];
	const struct HEAT_COOL_ELEMENT heat_min = HeatCoolElement_Min;
	const struct HEAT_COOL_ELEMENT heat_max = HeatCoolElement_Max;
	UINT16 *base = &Heat_Cool_Element.u16Heat_OpenTemp;

	u16WrRegNum = Sci_GetWrRegNum(s);
	offset = (UINT16)(u16Channel - RS485_CMD_ADDR_HEAT_DSG_HIGH);
	if (!Sci_WrRegsByteCountValid(s, u16WrRegNum) ||
		!Sci_RangeFits(offset, u16WrRegNum, E2P_PARA_NUM_HEAT_COOL))
	{
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}

	if (!Sci_WrValuesInRange(s,
						   offset,
						   u16WrRegNum,
						   &heat_min.u16Heat_OpenTemp,
						   &heat_max.u16Heat_OpenTemp))
	{
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
		return;
	}

	Sci_CopyWords(snapshot, base, E2P_PARA_NUM_HEAT_COOL);
	Sci_WriteWordsFromRequest(s, base, offset, u16WrRegNum);

	if (!EEPROM_SaveRWParametersToFlash())
	{
		Sci_CopyWords(base, snapshot, E2P_PARA_NUM_HEAT_COOL);
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}
}

void Sci_WrRegs_0x10_FlashConnect(struct RS485MSG *s)
{
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 1)
	{
		if (FLASH_COMPLETE != FlashWriteOneHalfWord(FLASH_ADDR_UPDATE_FLAG, FLASH_TO_IAP_VALUE))
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
	UINT8 i;
	UINT16 u16WrSNlength;

	u16WrSNlength = (UINT16)((UINT16)s->u16Buffer[5] + ((UINT16)s->u16Buffer[4] << 8)) << 1;

	switch (startADDR - RS485_ADDR_SN_SERIAL_NUM)
	{
	case 0:
		for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
		{
			if (i < u16WrSNlength)
			{
				ProductionInfor.BMS_SerialNumber[i] = s->u16Buffer[7 + i];
			}
			else
			{
				ProductionInfor.BMS_SerialNumber[i] = '\0';
			}
		}
		ProductionInfor.BMS_SerialNumberLength = u16WrSNlength;
		break;

	case 1:
		for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
		{
			if (i < u16WrSNlength)
			{
				ProductionInfor.BMS_HardWareVersion[i] = s->u16Buffer[7 + i];
			}
			else
			{
				ProductionInfor.BMS_HardWareVersion[i] = '\0';
			}
		}
		ProductionInfor.BMS_HardWareVersionLength = u16WrSNlength;
		break;

	case 2:
		for (i = 0; i < PRODUCT_ID_LENGTH_MAX; ++i)
		{
			if (i < u16WrSNlength)
			{
				ProductionInfor.BMS_SoftWareVersion[i] = s->u16Buffer[7 + i];
			}
			else
			{
				ProductionInfor.BMS_SoftWareVersion[i] = '\0';
			}
		}
		ProductionInfor.BMS_SoftWareVersionLength = u16WrSNlength;
		break;

	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
		break;
	}
}

void Sci_WrReg_0x06_Reset_CalibCoef(struct RS485MSG *s)
{
	UINT8 i;
	switch (s->u16Buffer[5] + (s->u16Buffer[4] << 8))
	{
	case 0x55AA:
		for (i = 0; i < 32; i++)
		{
			g_u16CalibCoefK[i] = SYSKDEFAULT;
			g_i16CalibCoefB[i] = SYSBDEFAULT;
			WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (i << 1)), g_u16CalibCoefK[i]);
			WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (i << 1)), g_i16CalibCoefB[i]);
		}
		break;
	case 0x55AB:
		g_u16CalibCoefK[VOLT_AFE1] = SYSKDEFAULT;
		g_i16CalibCoefB[VOLT_AFE1] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (VOLT_AFE1 << 1)), g_u16CalibCoefK[VOLT_AFE1]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (VOLT_AFE1 << 1)), g_i16CalibCoefB[VOLT_AFE1]);
		break;
	case 0x55AC:
		g_u16CalibCoefK[VOLT_AFE2] = SYSKDEFAULT;
		g_i16CalibCoefB[VOLT_AFE2] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (VOLT_AFE2 << 1)), g_u16CalibCoefK[VOLT_AFE2]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (VOLT_AFE2 << 1)), g_i16CalibCoefB[VOLT_AFE2]);
		break;
	case 0x55AD:
		g_u16CalibCoefK[VOLT_VBUS] = SYSKDEFAULT;
		g_i16CalibCoefB[VOLT_VBUS] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (VOLT_VBUS << 1)), g_u16CalibCoefK[VOLT_VBUS]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (VOLT_VBUS << 1)), g_i16CalibCoefB[VOLT_VBUS]);
		break;
	case 0x55AE:
		for (i = 0; i < 10; i++)
		{
			g_u16CalibCoefK[MDL_TEMP1 + i] = SYSKDEFAULT;
			g_i16CalibCoefB[MDL_TEMP1 + i] = SYSBDEFAULT;
			WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + ((MDL_TEMP1 + i) << 1)), g_u16CalibCoefK[i]);
			WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + ((MDL_TEMP1 + i) << 1)), g_i16CalibCoefB[i]);
		}
		break;
	case 0x55AF:
		g_u16CalibCoefK[MDL_IDSG] = SYSKDEFAULT;
		g_i16CalibCoefB[MDL_IDSG] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (MDL_IDSG << 1)), g_u16CalibCoefK[MDL_IDSG]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (MDL_IDSG << 1)), g_i16CalibCoefB[MDL_IDSG]);
		break;
	case 0x55B0:
		g_u16CalibCoefK[MDL_ICHG] = SYSKDEFAULT;
		g_i16CalibCoefB[MDL_ICHG] = SYSBDEFAULT;
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_K + (MDL_ICHG << 1)), g_u16CalibCoefK[MDL_ICHG]);
		WriteEEPROM_Word_NoZone((E2P_ADDR_START_CALIB_B + (MDL_ICHG << 1)), g_i16CalibCoefB[MDL_ICHG]);
		break;
	default:
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
		break;
	}
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
			Fault_record_First2[i] = 0;
			Fault_record_Second2[i] = 0;
			Fault_record_Third2[i] = 0;
		}
		FaultPoint_First2 = 0;
		FaultPoint_Second2 = 0;
		FaultPoint_Third2 = 0;
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

void Sci_WrReg_0x06_Reset_HeatCool(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	UINT8 i;
	UINT16 snapshot[E2P_PARA_NUM_HEAT_COOL];
	UINT16 *base = &Heat_Cool_Element.u16Heat_OpenTemp;
	const struct HEAT_COOL_ELEMENT HeatCoolEle_Default = HeatCoolElement_Default;

	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		Sci_CopyWords(snapshot, base, E2P_PARA_NUM_HEAT_COOL);
		for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
		{
			*(base + i) = *(&HeatCoolEle_Default.u16Heat_OpenTemp + i);
		}

		if (!EEPROM_SaveRWParametersToFlash())
		{
			Sci_CopyWords(base, snapshot, E2P_PARA_NUM_HEAT_COOL);
			Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
			return;
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_SwitchON(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16SciRegData >= 1 && u16SciRegData <= 32)
	{
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_WrReg_0x06_SwitchOFF(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16SciRegData >= 1 && u16SciRegData <= 32)
	{
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
	if (u16SciRegData >= 1 && u16SciRegData <= 32)
	{
		switch (u16SciRegData)
		{		// 如果是以下功能被打开，则需要初始化验证，别的功能直接关就好
		case 1: // 均衡
			if (!System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Balance)
			{
				System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Balance = 1;
				System_Func_StartUp.bits.b1StartUpFlag_Balance = 1;
			}
			break;

		case 3: // MOS或者接触器功能
			if (!System_OnOFF_Func_StartUpRec.bits.b1OnOFF_MOS_Relay)
			{
				System_OnOFF_Func_StartUpRec.bits.b1OnOFF_MOS_Relay = 1;
				System_Func_StartUp.bits.b1StartUpFlag_MOS = 1;
				System_Func_StartUp.bits.b1StartUpFlag_Relay = 1;
			}
			break;

		case 6: // 加热功能
			if (!System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Heat)
			{
				System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Heat = 1;
				System_Func_StartUp.bits.b1StartUpFlag_Heat = 1;
			}
			break;

		case 7: // 冷凝功能
			if (!System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Cool)
			{
				System_OnOFF_Func_StartUpRec.bits.b1OnOFF_Cool = 1;
				System_Func_StartUp.bits.b1StartUpFlag_Cool = 1;
			}
			break;

		case 8: // 激活模拟前端AFE1
			break;

		case 0x0A: // 立刻进入休眠
			entersleep(DEEP_MODE);
			break;

		default:
			break;
		}

		System_OnOFF_Func.all |= ((UINT32)1 << (u16SciRegData - 1));
		if (u16SciRegData == 0x0B)
		{
			// System_OnOFF_Func.bits.b1OnOFF_SOC_Zero
			// 默认为0，不需要保存
		}
		else
		{
			WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT, (UINT16)(System_OnOFF_Func.all & 0x0000FFFF));
			WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2, (UINT16)(System_OnOFF_Func.all >> 16));
		}

		if (System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
		{
			SOC_Enhance_Element.u16_RefreshData_Flag = 1;
		}
		if (System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
		{
			SOC_Enhance_Element.u16_RefreshData_Flag = 2;
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
	if (u16SciRegData >= 1 && u16SciRegData <= 32)
	{
		//*(&System_OnOFF_Func.bits.b1OnOFF_Balance+(u16SciRegData-1)) = 0;
		System_OnOFF_Func.all &= ~((UINT32)1 << (u16SciRegData - 1)); // 功能途中关闭不需要初始化验证

		if (u16SciRegData == 0x0B)
		{
			// System_OnOFF_Func.bits.b1OnOFF_SOC_Zero
			// 默认为0，不需要保存
		}
		else
		{
			WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT, (UINT16)(System_OnOFF_Func.all & 0x0000FFFF));
			WriteEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2, (UINT16)(System_OnOFF_Func.all >> 16));
		}
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

int fputc(int ch, FILE *f)
{
#if 0 /* 将需要printf的字符通过串口中断FIFO发送出去，printf函数会立即返回 */
	comSendChar(COM1, ch);

	return ch;
#else /* 采用阻塞方式发送每个字符,等待数据发送完毕 */
	/* 写一个字节到USART1 */
	USART_SendData(debug_uart, (uint8_t)ch);

	/* 等待发送结束 */
	while (USART_GetFlagStatus(debug_uart, USART_FLAG_TC) == RESET)
	{
	}

	return ch;
#endif
}
