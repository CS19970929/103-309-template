#include "main.h"
#include "CurrentCalibration.h"
#include "FaultSnapshot.h"
#include "Flash.h"
#include "Sci_Protocol.h"

static UINT8 g_u8SCITxBuff[SCI_TX_BUF_LEN];

struct stCell_Info g_stCellInfoReport;
volatile UINT8 u8FlashUpdateFlag = 0;


static void Sci_SetWrError(struct RS485MSG *s, UINT8 error);
static UINT8 Sci_RangeFits(UINT16 offset, UINT16 count, UINT16 total);
enum SCI_READ_BLOCK { SCI_READ_REPORT, SCI_READ_LCD, SCI_READ_AFE,
    SCI_READ_OTHER1, SCI_READ_OTHER, SCI_READ_PROTECT, SCI_READ_CALIB };
struct SCI_READ_WINDOW {
    UINT16 base, words, buffer_offset;
    UINT8 block, selector;
};
/* C001/C002 select entire blocks, rather than overlapping linear addresses.
 * D100/D200 retain their aliases into the combined D000 buffer. */
static const struct SCI_READ_WINDOW s_read_windows[] = {
    {RS485_ADDR_RO_START2, RS485_RO_TOTAL_WORDS-96U, 96U, SCI_READ_REPORT, 0U},
    {RS485_ADDR_RO_START1, RS485_RO_TOTAL_WORDS-63U, 63U, SCI_READ_REPORT, 0U},
    {RS485_ADDR_RO_START0, RS485_RO_TOTAL_WORDS, 0U, SCI_READ_REPORT, 0U},
    {RS485_ADDR_AGING_STATUS, RS485_ADDR_AGING_STATUS_WORDS, 0x80U, SCI_READ_LCD, 0U},
    {RS485_ADDR_EVENT_RECORD, FLASH_STORAGE_LOG_RECORD_COUNT, 8U, SCI_READ_LCD, 0U},
    {RS485_ADDR_SN_READ, PRODUCT_ID_LENGTH_MAX*3U/2U, 2U, SCI_READ_LCD, 1U},
    {RS485_ADDR_BATTERY_NUMBER_READ, 11U, 4U, SCI_READ_LCD, 1U},
    {RS485_ADDR_RO_FA_RTC, Record_len*7U, 1U, SCI_READ_LCD, 1U},
    {RS485_ADDR_RO_LCD, 5U, 0U, SCI_READ_LCD, 1U},
    {RS485_ADDR_RW_AFE_PARAMETER, AFE_PARAMETES_TOTAL_LENGTH, 0U, SCI_READ_AFE, 0U},
    {RS485_ADDR_RW_OTHER_CANADD, E2P_PARA_NUM_OTHER_ELEMENT1, 0U, SCI_READ_OTHER1, 0U},
    {RS485_ADDR_RW_OTHER, SOC_TABLE_SIZE+32U+E2P_PARA_NUM_RTC, 0U, SCI_READ_OTHER, 0U},
    {RS485_ADDR_RW_PORTECT, E2P_PARA_NUM_PROTECT, 0U, SCI_READ_PROTECT, 0U},
    {RS485_ADDR_RW_CALIB, KB_NUM*2U, 0U, SCI_READ_CALIB, 0U}
};
static const struct SCI_READ_WINDOW *Sci_FindReadWindow(UINT16 address, UINT16 *offset);
static void Sci_LoadReadData(struct RS485MSG *s);

void Sci_WrRegs_0x10_CalibCoef(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_Protect(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_SocTable(struct RS485MSG *s);
void Sci_WrRegs_0x10_RTC(struct RS485MSG *s);
void Sci_WrRegs_0x10_OtherElement(UINT16 u16Channel, struct RS485MSG *s);
void Sci_WrRegs_0x10_FlashConnect(struct RS485MSG *s);

void Sci_WrReg_0x06_Reset_CalibCoef(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_ProtectRecord(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_ProtectElement(struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_OtherCanAdd(struct RS485MSG *s);
void Sci_WrReg_0x06_BMS_FunctionON(struct RS485MSG *s);
void Sci_WrReg_0x06_SetSocOnce(struct RS485MSG *s);

void CRC_verify(struct RS485MSG *s)
{
	UINT16 u16SciVerify;
	UINT16 t_u16FrameLenth;
	if ((s->ptr_no < 4U) || (s->ptr_no > RS485_MAX_BUFFER_SIZE))
	{
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
		return;
	}

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
    UINT16 offset, count;
    const struct SCI_READ_WINDOW *window;
    s->u16RdRegStartAddrActure = (UINT16)((s->u16Buffer[2] << 8) | s->u16Buffer[3]);
    count = (UINT16)((s->u16Buffer[4] << 8) | s->u16Buffer[5]);
    s->u16RdRegByteNum = 0U;
    if (count == 0U || count > (RS485_MAX_BUFFER_SIZE-5U)/2U)
    {
        Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
        return;
    }
    window = Sci_FindReadWindow(s->u16RdRegStartAddrActure, &offset);
    if (window == 0 || !Sci_RangeFits(offset, count, window->words))
    {
        Sci_SetWrError(s, RS485_ERROR_ADDR_INVALID);
        return;
    }
    s->u16RdRegStartAddr = (UINT16)(window->buffer_offset + offset);
    s->u16RdRegByteNum = (UINT8)(count*2U);
}
void Sci_Deal_WrReg_0x06(struct RS485MSG *s)
{
#if PROJECT_CFG_HOST_WRITE_ENABLE
	UINT16 u16SciRegAddr;
	u16SciRegAddr = s->u16Buffer[3] + (s->u16Buffer[2] << 8);
	switch (u16SciRegAddr)
	{
	case RS485_CMD_ADDR_SET_BAUD_MODE:
		{
			UINT16 mode=(UINT16)((s->u16Buffer[4]<<8) | s->u16Buffer[5]);
			if (mode<1U || mode>3U) Sci_SetWrError(s,RS485_ERROR_DATA_INVALID);
			else if (!Sci_RequestBaudMode(mode)) Sci_SetWrError(s,RS485_ERROR_CMD_INVALID);
		}
		break;
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
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
		break;

	case RS485_CMD_ADDR_SET_ONCE_SOC:
		Sci_WrReg_0x06_SetSocOnce(s);
		break;

	// �颖AFE参数�读可写新�
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

static void Sci_PutFactoryAgingStatusWords(UINT8 buff[], UINT16 *index)
{
	/* Aging is not implemented in this target; retain five reserved words. */
	Sci_PutZeroWordsBE(buff, index, RS485_ADDR_AGING_STATUS_WORDS);
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

static const struct SCI_READ_WINDOW *Sci_FindReadWindow(UINT16 address, UINT16 *offset)
{
    UINT16 i;
    for (i=0U; i<sizeof(s_read_windows)/sizeof(s_read_windows[0]); ++i)
    {
        const struct SCI_READ_WINDOW *window = &s_read_windows[i];
        if (address < window->base)
            continue;
        *offset = (UINT16)(address-window->base);
        if (*offset < window->words && (!window->selector || *offset == 0U))
            return window;
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
    if (Sci_RangeOverlaps(offset, count,
        RS485_CMD_ADDR_TMOS_OTP_FIRST - RS485_CMD_ADDR_VCELL_OVP_FIRST, 5U))
        Fault_ResetMosConfirmation();
}

static void Sci_ApplyOtherElementSideEffects(UINT16 offset, UINT16 count)
{

	if (Sci_RangeOverlaps(offset, count, 0, 8))
	{
		AFE_PARAM_WRITE_Flag = 1;
	}

	if (Sci_RangeOverlaps(offset, count, 8, 8) ||
		Sci_RangeOverlaps(offset, count, 28, 4))
	{
		AFE_PARAM_WRITE_Flag = 1;
	}

    if (Sci_RangeOverlaps(offset, count, 24, 2))
        SOC_ApplyParameters(Sci_RangeOverlaps(offset, count, 25, 1));
    if (Sci_RangeOverlaps(offset, count, 26, 2) ||
        Sci_RangeOverlaps(offset, count, 28, 1)) {
        SOC_ResetVoltageCalibration();
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
	case RS485_ADDR_BATTERY_NUMBER_WRITE:
		if (Sci_GetWrRegNum(s)!=6U || s->u16Buffer[6]!=12U || s->u16Buffer[18]!=0xFFU)
			Sci_SetWrError(s,RS485_ERROR_DATA_INVALID);
		else {
			UINT16 i;
			for (i=7U; i<18U; ++i)
				if (s->u16Buffer[i]<0x20U || s->u16Buffer[i]>0x7EU) break;
			if (i!=18U) Sci_SetWrError(s,RS485_ERROR_DATA_INVALID);
			else if (!StorageFlash_WriteBatteryNumber(&s->u16Buffer[7]))
				Sci_SetWrError(s,RS485_ERROR_CMD_INVALID);
		}
		break;
	case RS485_CMD_ADDR_SOC_VOLTAGE1:
		Sci_WrRegs_0x10_SocTable(s);
		break;

	case RS485_CMD_ADDR_RTC_TIME_YEAR:
		Sci_WrRegs_0x10_RTC(s);
		break;

	case RS485_ADDR_SN_SERIAL_NUM:
	case RS485_ADDR_SN_HAEDWARE_VER:
	case RS485_ADDR_SN_SOFTWARE_VER:
		/* Historical identity writes are acknowledged without changing the identity. */
		break;

	case RS485_CMD_ADDR_FLASH_CONNECT:
		Sci_WrRegs_0x10_FlashConnect(s);
		break; // 少了个BREAK导致OVER�

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
	else if ((s->u16RdRegStartAddr >= (UINT16)(RS485_ADDR_AGING_STATUS - RS485_ADDR_RO_LCD)) &&
		(s->u16RdRegStartAddr < (UINT16)(RS485_ADDR_AGING_STATUS - RS485_ADDR_RO_LCD + RS485_ADDR_AGING_STATUS_WORDS)))
	{
		u16SourceOffset = (UINT16)(s->u16RdRegStartAddr - (UINT16)(RS485_ADDR_AGING_STATUS - RS485_ADDR_RO_LCD));
		Sci_PutFactoryAgingStatusWords(t_u8BuffTemp, &i);
	}
	else
	{
		switch (s->u16RdRegStartAddr)
		{
		case 0: // LCD
			Sci_PutZeroWordsBE(t_u8BuffTemp, &i, 5U);
			break;
		case 1: // 上位机�三级保护�60+10=70�
			for (j = 0; j < Record_len; j++)
			{
				k = (INT8)Sci_RecordBackIndex(FaultPoint_Third, j);
				u16SciTemp = Fault_record_Third[k];
				Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
				Sci_PutZeroWordsBE(t_u8BuffTemp, &i, 6U);
			}
			break;

		case 2: // 序列号，�件版�号，�件版��
			// Sci_PutBytes(t_u8BuffTemp, &i, ProductionInfor.BMS_SerialNumber, PRODUCT_ID_LENGTH_MAX);
			// Sci_PutBytes(t_u8BuffTemp, &i, ProductionInfor.BMS_HardWareVersion, PRODUCT_ID_LENGTH_MAX);
			// Sci_PutBytes(t_u8BuffTemp, &i, ProductionInfor.BMS_SoftWareVersion, PRODUCT_ID_LENGTH_MAX);
			break;

		case 4: /* for-c034 requests 11 registers; its first 11 bytes are ASCII. */
			memset(t_u8BuffTemp,0xFF,22U);
			if (!StorageFlash_ReadBatteryNumber(t_u8BuffTemp))
				Sci_SetWrError(s,RS485_ERROR_CMD_INVALID);
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


    for (j=0U; j<32U; ++j)
    {
        u16SciTemp = g_stCellInfoReport.u16VCell[j];
        Sci_PutWordBE(t_u8BuffTemp,&i,u16SciTemp);
    }
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16VCellMax);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16VCellMin);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16VCellMaxPosition);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16VCellMinPosition);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16VCellDelta);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16VCellTotle);
    for (j=0U; j<TEMP_NUM; ++j)
        Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16Temperature[j]);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16TempMax);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16TempMin);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16Ichg);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16IDischg);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.SocElement.u16Soc);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.SocElement.u16Soh);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.SocElement.u16CapacityNow);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.SocElement.u16CapacityFull);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.SocElement.u16CapacityFactory);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.SocElement.u16Cycle_times);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.unMdlFault_First.all);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.unMdlFault_Second.all);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.unMdlFault_Third.all);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16BalanceFlag1);
    Sci_PutWordBE(t_u8BuffTemp,&i,g_stCellInfoReport.u16BalanceFlag2);

	// 0xD100_33
	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Month) | (RTC_time.RTC_Time_Year << 8);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Hour) | (RTC_time.RTC_Time_Day << 8);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	u16SciTemp = (UINT16)(RTC_time.RTC_Time_Second) | (RTC_time.RTC_Time_Minute << 8);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	Sci_PutZeroWordsBE(t_u8BuffTemp, &i, 4U);
	Sci_PutLatestFaultWords(t_u8BuffTemp, &i, Fault_record_Third2, FaultPoint_Third2);

	for (j = 0; j < 12; j++)
	{ // 0xD002到这里�
		u16SciTemp = ((*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + 2 * j)) << 8) | (*(&System_ErrFlag.u8ErrFlag_Com_AFE1 + 2 * j + 1));
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}

	status_snapshot = SystemRuntime_GetStatusSnapshot();
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

	Sci_PutZeroWordsBE(t_u8BuffTemp, &i, 2U);

	Sci_PutZeroWordsBE(t_u8BuffTemp, &i, 8U);

	// 0xD200_2: last Cortex fault reason and inverse snapshot.
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
	u16SciTemp = BKP_ReadBackupRegister(FAULT_BKP_REASON_REG);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	u16SciTemp = BKP_ReadBackupRegister(FAULT_BKP_REASON_INV_REG);
	Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);

	/* Reserved report padding (16 words) — retained for protocol compatibility */
	Sci_PutZeroWordsBE(t_u8BuffTemp, &i, 16U);
}

/*=================================================================
 * FUNCTION: Sci_Tx_RW_Fun
 * PURPOSE : 将需要发送的数据进�更�
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
{ // 65�
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
{ // 94�
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < KB_NUM; j++)
	{
		u16SciTemp = g_u16CalibCoefK[j];
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
		if ((j == (UINT16)MDL_ICHG) || (j == (UINT16)MDL_IDSG))
		{
			u16SciTemp = (UINT16)CurrentCalibration_EncodeReadbackB((int16_t)g_i16CalibCoefB[j]);
		}
		else
		{
			u16SciTemp = (UINT16)g_i16CalibCoefB[j];
		}
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}
}

/* Read-only legacy protocol data; not a SOC calibration input. */
static UINT16 Sci_GetSocTableWord(UINT16 index)
{
    static const UINT16 table[SOC_TABLE_SIZE] = {
#if PROJECT_CFG_BAT_CHEMISTRY == 1

	3336, 100, 3332, 90, 3330, 80, 3327, 75, 3316, 70, 3301, 65,
	3294, 60, 3291, 55, 3290, 50, 3288, 45, 3286, 40, 3279, 35,
	3266, 30, 3254, 25, 3236, 20, 3212, 15, 3198, 10, 3112, 5,
	2950, 0, 1000, 0, 1000, 0,

#else

	4160, 100, 4100, 95, 4050, 90, 3995, 85, 3935, 80, 3880, 75,
	3835, 70, 3795, 65, 3760, 60, 3725, 55, 3695, 50, 3670, 45,
	3645, 40, 3615, 35, 3585, 30, 3555, 25, 3525, 20, 3480, 15,
	3400, 10, 3250, 5, 3000, 0,

#endif
    };
    return table[index];
}

void Sci_ACK_0x03_RW_Data_Other(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 86
	UINT16 u16SciTemp;
	UINT16 i, j;
	i = 0;
	for (j = 0; j < SOC_TABLE_SIZE; j++)
	{ // 由于GetEndValue()函数的问题，�能混在一�
		u16SciTemp = Sci_GetSocTableWord(j);
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}

	/* CopperLoss + CopperLoss_Num slots (32 words) — retained for protocol compatibility */
	for (j = 0; j < 32U; j++)
	{
		Sci_PutWordBE(t_u8BuffTemp, &i, 0U);
	}

	for (j = 0; j < E2P_PARA_NUM_RTC; j++)
	{
		u16SciTemp = *(&RTC_time.RTC_Time_Year + j);
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}
}

void Sci_ACK_0x03_RW_Data_OtherCanAdd(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{ // 32�
	UINT16 u16SciTemp;
	UINT16 i = 0, j;

	for (j = 0; j < E2P_PARA_NUM_OTHER_ELEMENT1; j++)
	{
		u16SciTemp = *(&OtherElement.u16Balance_OpenVoltage + j);
		Sci_PutWordBE(t_u8BuffTemp, &i, u16SciTemp);
	}
}

static void Sci_LoadReadData(struct RS485MSG *s)
{
    UINT16 offset;
    const struct SCI_READ_WINDOW *window = Sci_FindReadWindow(s->u16RdRegStartAddrActure,&offset);
    if (window == 0)
        return; /* Caller has already checked the complete read range. */
    switch (window->block)
    {
    case SCI_READ_REPORT: Sci_ACK_0x03_ReadRegs_Data(s,g_u8SCITxBuff); break;
    case SCI_READ_LCD: Sci_ACK_0x03_ReadRegs_LCD(s,g_u8SCITxBuff); break;
    case SCI_READ_AFE: Sci_ACK_0x03_RW_AFE_Parameters(s,g_u8SCITxBuff); break;
    case SCI_READ_OTHER1: Sci_ACK_0x03_RW_Data_OtherCanAdd(s,g_u8SCITxBuff); break;
    case SCI_READ_OTHER: Sci_ACK_0x03_RW_Data_Other(s,g_u8SCITxBuff); break;
    case SCI_READ_PROTECT: Sci_ACK_0x03_RW_Data_Pro(s,g_u8SCITxBuff); break;
    case SCI_READ_CALIB: Sci_ACK_0x03_RW_Data_Cali(s,g_u8SCITxBuff); break;
    }
}

void Sci_ACK_0x03(struct RS485MSG *s)
{
	UINT8 i = 0U;
	UINT16 u16SciTemp;
	if (s->AckType == RS485_ACK_POS && s->u16RdRegStartAddrActure >= RS485_ADDR_RW_CALIB)
		Sci_LoadReadData(s);
	if (s->AckType == RS485_ACK_POS)
	{
		if (s->u16RdRegStartAddrActure >= RS485_ADDR_RW_CALIB)
		{
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
		(u16Count > (RS485_MAX_BUFFER_SIZE - 5U)/2U))
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

    Sci_LoadReadData(&stMsg);
    for (i=0U; i<u16Count; ++i)
    {
        UINT16 byte = (UINT16)((stMsg.u16RdRegStartAddr+i)*2U);
        pu16Words[i] = (UINT16)((g_u8SCITxBuff[byte]<<8) | g_u8SCITxBuff[byte+1U]);
    }

	return 0U;
}

UINT8 Sci_HostWriteWords(UINT16 u16StartAddr, const UINT16 *pu16Words, UINT16 u16Count)
{
	struct RS485MSG stMsg;
	UINT16 i;

	if ((pu16Words == 0) ||
		(u16Count == 0U) ||
		(u16Count > (RS485_MAX_BUFFER_SIZE - 9U)/2U))
	{
		return RS485_ERROR_DATA_INVALID;
	}

    if (u16StartAddr == RS485_CMD_ADDR_FLASH_CONNECT)
        return RS485_ERROR_NO_PERMISSION; /* CAN uses its dedicated IAP command. */

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

void Sci_ResetMessage(struct RS485MSG *s)
{
	s->ptr_no = 0;
	s->reset_after_tx = 0U;
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

UINT8 Sci_ModbusFeed(struct RS485MSG *s, UINT8 u8Data)
{
	UINT16 u16FrameEndIndex;

	if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
	{
		Sci_ResetMessage(s);
	}

	s->u16Buffer[s->ptr_no] = u8Data;

	if (s->ptr_no == 0)
	{
		if ((u8Data != RS485_SLAVE_ADDR) && (u8Data != RS485_BROADCAST_ADDR))
		{
			Sci_ResetMessage(s);
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
			Sci_ResetMessage(s);
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
					Sci_ResetMessage(s);
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
			Sci_ResetMessage(s);
			return 0;
		}
	}

	s->ptr_no++;
	if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
	{
		Sci_ResetMessage(s);
	}

	return 0;
}

UINT8 Sci_ModbusFrameValid(const struct RS485MSG *s)
{
    UINT16 count=s->ptr_no, length;
    const UINT8 *b=s->u16Buffer;
    if (count < 8U || count > RS485_MAX_BUFFER_SIZE ||
        (b[0] != RS485_SLAVE_ADDR && b[0] != RS485_BROADCAST_ADDR))
        return 0U;
    if (b[1] == RS485_CMD_READ_REGS || b[1] == RS485_CMD_WRITE_REG)
    {
        if (count != 8U || (b[0] == RS485_BROADCAST_ADDR && b[1] == RS485_CMD_READ_REGS))
            return 0U;
    }
    else if (b[1] == RS485_CMD_WRITE_REGS)
    {
        length=(UINT16)((b[4]<<8) | b[5]);
        if (count != (UINT16)(b[6]+9U) || !length ||
            length > (RS485_MAX_BUFFER_SIZE-9U)/2U || b[6] != length*2U)
            return 0U;
    }
    else return 0U;
    return (UINT8)(Sci_CRC16RTU((UINT8 *)b,(UINT16)(count-2U)) ==
        (UINT16)(b[count-2U] | (b[count-1U]<<8)));
}

void Sci_ModbusProcessFrame(struct RS485MSG *s)
{
	UINT8 broadcast = (UINT8)(s->u16Buffer[0] == RS485_BROADCAST_ADDR);

	s->AckType = RS485_ACK_POS;
	s->ErrorType = RS485_ERROR_NULL;

	CRC_verify(s);
	if ((s->AckType != RS485_ACK_POS) ||
		(broadcast && s->enRs485CmdType == RS485_CMD_READ_REGS))
	{
		Sci_ResetMessage(s);
		return;
	}
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

	if (broadcast)
	{
		Sci_ResetMessage(s);
		return;
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
		Sci_ResetMessage(s);
		break;
	}
}

void Sci_WrRegs_0x10_CalibCoef(UINT16 u16Channel, struct RS485MSG *s)
{
	UINT16 u16WrRegNum;
	UINT16 k;
	UINT16 b_raw;
	UINT16 calib_index;
	UINT16 old_k;
	INT16 b;
	INT16 old_b;

	/*
	 * Current K/B follows the historical pair-write protocol:
	 *   ICHGCALIB_K + ICHGCALIB_B
	 *   IDISCHGCALIB_K + IDISCHGCALIB_B
	 * Exactly two registers must be written atomically.
	 */
	if ((u16Channel != RS485_CMD_ADDR_ICHGCALIB_K) &&
		(u16Channel != RS485_CMD_ADDR_IDISCHGCALIB_K))
	{
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}

	u16WrRegNum = Sci_GetWrRegNum(s);
	if (!Sci_WrRegsByteCountValid(s, u16WrRegNum) || (u16WrRegNum != 2U))
	{
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}

	k = Sci_GetWrValue(s, 0U);
	b_raw = Sci_GetWrValue(s, 1U);
	b = (INT16)CurrentCalibration_DecodeWriteProtocolB((uint16_t)b_raw);

	if ((k < CURRENT_CAL_K_MIN) || (k > CURRENT_CAL_K_MAX) ||
		(b < CURRENT_CAL_B_MIN_Q10_A) || (b > CURRENT_CAL_B_MAX_Q10_A))
	{
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
		return;
	}

	calib_index = (u16Channel == RS485_CMD_ADDR_ICHGCALIB_K) ?
		(UINT16)MDL_ICHG : (UINT16)MDL_IDSG;
	old_k = g_u16CalibCoefK[calib_index];
	old_b = g_i16CalibCoefB[calib_index];

	g_u16CalibCoefK[calib_index] = k;
	g_i16CalibCoefB[calib_index] = b;

	if (!StorageFlash_SaveParameters(0U))
	{
		g_u16CalibCoefK[calib_index] = old_k;
		g_i16CalibCoefB[calib_index] = old_b;
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
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
    if (Sci_RangeOverlaps(offset, u16WrRegNum,
        RS485_CMD_ADDR_TMOS_OTP_FIRST - RS485_CMD_ADDR_VCELL_OVP_FIRST, 5U) &&
        !Fault_MosParametersValid(&PRT_E2ROMParas))
    {
        Sci_CopyWords(base, snapshot, E2P_PARA_NUM_PROTECT);
        Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
        return;
    }

	if (!StorageFlash_SaveParameters(0U))
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
	s->AckType = RS485_ACK_NEG;
	s->ErrorType = RS485_ERROR_CMD_INVALID;
}

void Sci_WrRegs_0x10_RTC(struct RS485MSG *s)
{
	Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
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

	if (!StorageFlash_SaveParameters(Sci_RangeOverlaps(offset, u16WrRegNum, 25U, 1U)))
	{
		Sci_CopyWords(base, snapshot, E2P_PARA_NUM_OTHER_ELEMENT1);
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
		return;
	}

	Sci_ApplyOtherElementSideEffects(offset, u16WrRegNum);
}

void Sci_WrRegs_0x10_FlashConnect(struct RS485MSG *s)
{
	UINT16 u16WrRegNum;
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16WrRegNum == 1U && s->u16Buffer[0] == RS485_SLAVE_ADDR)
	{
		if (AppUpgrade_RequestIap() == 0U)
		{
			// System_ERROR_UserCallback(ERROR_FLASH);
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_CMD_INVALID;
		}
		else
		{
			s->reset_after_tx = 1U;
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_CMD_INVALID;
	}
}

/* 把BMS序列号，�件版�号， �件版�号写� ohterInfor结构�
 * startADDR  如起始地�
 */

void Sci_WrReg_0x06_Reset_CalibCoef(struct RS485MSG *s)
{
	UINT16 command;
	UINT16 old_chg_k;
	UINT16 old_dsg_k;
	INT16 old_chg_b;
	INT16 old_dsg_b;
	UINT8 reset_chg = 0U;
	UINT8 reset_dsg = 0U;

	command = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	switch (command)
	{
	case 0x0001U: /* reset both current K/B pairs */
		reset_chg = 1U;
		reset_dsg = 1U;
		break;
	case 0x55AFU: /* historical: reset discharge current K/B */
		reset_dsg = 1U;
		break;
	case 0x55B0U: /* historical: reset charge current K/B */
		reset_chg = 1U;
		break;
	default:
		Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
		return;
	}

	old_chg_k = g_u16CalibCoefK[MDL_ICHG];
	old_chg_b = g_i16CalibCoefB[MDL_ICHG];
	old_dsg_k = g_u16CalibCoefK[MDL_IDSG];
	old_dsg_b = g_i16CalibCoefB[MDL_IDSG];

	if (reset_chg != 0U)
	{
		g_u16CalibCoefK[MDL_ICHG] = SYSKDEFAULT;
		g_i16CalibCoefB[MDL_ICHG] = SYSBDEFAULT;
	}
	if (reset_dsg != 0U)
	{
		g_u16CalibCoefK[MDL_IDSG] = SYSKDEFAULT;
		g_i16CalibCoefB[MDL_IDSG] = SYSBDEFAULT;
	}

	if (!StorageFlash_SaveParameters(0U))
	{
		g_u16CalibCoefK[MDL_ICHG] = old_chg_k;
		g_i16CalibCoefB[MDL_ICHG] = old_chg_b;
		g_u16CalibCoefK[MDL_IDSG] = old_dsg_k;
		g_i16CalibCoefB[MDL_IDSG] = old_dsg_b;
		Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
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
			Fault_record_Third2[i] = 0;
		}
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

		if (!StorageFlash_SaveParameters(0U))
		{
			Sci_CopyWords(base, snapshot, E2P_PARA_NUM_PROTECT);
			Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
			return;
		}
		Fault_ResetMosConfirmation();
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

		if (!StorageFlash_SaveParameters(1U))
		{
			Sci_CopyWords(base, snapshot, E2P_PARA_NUM_OTHER_ELEMENT1);
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

void Sci_WrReg_0x06_BMS_FunctionON(struct RS485MSG *s)
{
    UINT16 id = (UINT16)(s->u16Buffer[5] | (s->u16Buffer[4] << 8));
    switch (id)
    {
    case 10U:
        LowPower_Request(DEEP_MODE);
        break;
    default:
        Sci_SetWrError(s, RS485_ERROR_DATA_INVALID);
        break;
    }
}


void Sci_WrReg_0x06_SetSocOnce(struct RS485MSG *s)
{
	UINT16 u16SciRegData;
	u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (u16SciRegData <= 100)
	{
        if (!SOC_RequestSetOnce((UINT8)u16SciRegData))
            Sci_SetWrError(s, RS485_ERROR_CMD_INVALID);
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}
