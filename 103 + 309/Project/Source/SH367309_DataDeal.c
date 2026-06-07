#include "main.h"
#include "SH367309_DataDeal.h"
#include "string.h"
#include "DebugWatch.h"

int AFE_PARAM_WRITE_Flag = 1;

static const UINT16 s_sh_afe_ocd1v_occv[16] = {20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 160, 180, 200};
static const UINT16 s_sh_afe_ocd2v[16] = {30, 40, 50, 60, 70, 80, 90, 100, 120, 140, 160, 180, 200, 300, 400, 500};
const UINT16 g_u16ShAfeScvTable[16] = {50, 80, 110, 140, 170, 200, 230, 260, 290, 320, 350, 400, 500, 600, 800, 1000};
static const UINT16 s_sh_afe_ovt_uvt[16] = {100, 200, 300, 400, 600, 800, 1000, 2000, 3000, 4000, 6000, 8000, 10000, 20000, 30000, 40000};
const UINT16 g_u16ShAfeSctTable[16] = {0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960};
static const UINT16 s_sh_afe_ocd1t[16] = {50, 100, 200, 400, 600, 800, 1000, 2000, 4000, 6000, 8000, 10000, 15000, 20000, 30000, 40000};
static const UINT16 s_sh_afe_occt_ocd2t[16] = {10, 20, 40, 60, 80, 100, 200, 400, 600, 800, 1000, 2000, 4000, 8000, 10000, 20000};

AFE_ROM_PARAMETERS_TypeDef AFE_ROM_PARAMETERS_Struction = {0};
static AFE_Parameters_RS485_Typedef AFE_Parameters_RS485_Struction = AFE_PARAMETERS_RS485_STRUCTION_DEFAULT;

extern UINT8 ucMTPBuffer[26];
extern const UINT16 iSheldTemp_10K_NTC[141];

#if DEBUG_WATCH_ENABLED
void SH367309Data_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->afe.rom_params = &AFE_ROM_PARAMETERS_Struction;
	watch->afe.rs485_params = &AFE_Parameters_RS485_Struction;
	watch->afe.param_write_flag = &AFE_PARAM_WRITE_Flag;
	watch->tables.sh_afe_scv = g_u16ShAfeScvTable;
	watch->tables.sh_afe_sct = g_u16ShAfeSctTable;
	watch->tables.sh_afe_ocd1v_occv = s_sh_afe_ocd1v_occv;
	watch->tables.sh_afe_ocd2v = s_sh_afe_ocd2v;
	watch->tables.sh_afe_ovt_uvt = s_sh_afe_ovt_uvt;
	watch->tables.sh_afe_ocd1t = s_sh_afe_ocd1t;
	watch->tables.sh_afe_occt_ocd2t = s_sh_afe_occt_ocd2t;
}
#endif

#define DSG_CHG_OCP_DELAY_TIME (30 * 100)
#define OFF 0
#define ON 1

int Choose_Right_Value(UINT16 cur_Value, const UINT16 *AFE_list)
{
	int i = 0;
	for (i = 0; i < 15; i++)
	{
		if (cur_Value <= AFE_list[i])
		{
			break;
		}
	}
	return i;
}

void Refresh_Parameters(void)
{
	int i = 0;
	int temp = 0;
	UINT8 TR = 0;
	UINT16 AFE_TEMPERATURE[8] = {0};

	if (MTPRead(0x19, 1, &TR))
	{
		SH367309_Reg_Store.TR_ResRef = 680 + 5 * (TR & 0x7F);
		ucMTPBuffer[25] = TR & 0x7F;
		memcpy((UINT8 *)&AFE_ROM_PARAMETERS_Struction, ucMTPBuffer, 26);
	}

	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	AFE_ROM_PARAMETERS_Struction.m00H_01H.CN = SeriesNum;
	AFE_ROM_PARAMETERS_Struction.m00H_01H.CTLC = 3;
	AFE_ROM_PARAMETERS_Struction.m00H_01H.BAL = 1;
	// temp = (OtherElement.u16Balance_OpenVoltage + 10) / 20;
	temp = (4160 + 10) / 20;
	if (temp > 0xFF)
	{
		temp = 0xFF;
	}
	AFE_ROM_PARAMETERS_Struction.m08H_09H.BALV = (UINT8)temp;

	AFE_ROM_PARAMETERS_Struction.m02H_03H.OVH = ((AFE_Parameters_RS485_Struction.u16VcellOvp.curValue / 5) >> 8) & 0x3;
	AFE_ROM_PARAMETERS_Struction.m02H_03H.OVL = (AFE_Parameters_RS485_Struction.u16VcellOvp.curValue / 5) & 0x00FF;
	temp = AFE_Parameters_RS485_Struction.u16VcellOvp_Filter.curValue * 10;
	AFE_ROM_PARAMETERS_Struction.m02H_03H.OVT = Choose_Right_Value(temp, s_sh_afe_ovt_uvt);
	AFE_ROM_PARAMETERS_Struction.m04H_05H.OVRH = ((AFE_Parameters_RS485_Struction.u16VcellOvp_Rcv.curValue / 5) >> 8) & 0x3;
	AFE_ROM_PARAMETERS_Struction.m04H_05H.OVRL = (AFE_Parameters_RS485_Struction.u16VcellOvp_Rcv.curValue / 5) & 0x00FF;

	temp = AFE_Parameters_RS485_Struction.u16VcellUvp_Filter.curValue * 10;
	AFE_ROM_PARAMETERS_Struction.m04H_05H.UVT = Choose_Right_Value(temp, s_sh_afe_ovt_uvt);
	AFE_ROM_PARAMETERS_Struction.m06H_07H.UV = (AFE_Parameters_RS485_Struction.u16VcellUvp.curValue / 20) & 0x00FF;
	AFE_ROM_PARAMETERS_Struction.m06H_07H.UVR = (AFE_Parameters_RS485_Struction.u16VcellUvp_Rcv.curValue / 20) & 0x00FF;

	temp = AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue * 100 / g_u32CS_Res_AFE;
	AFE_ROM_PARAMETERS_Struction.m0CH_0DH.OCD1V = Choose_Right_Value(temp, s_sh_afe_ocd1v_occv);
	temp = AFE_Parameters_RS485_Struction.u16IdsgOcp_Filter_First.curValue * 10;
	AFE_ROM_PARAMETERS_Struction.m0CH_0DH.OCD1T = Choose_Right_Value(temp, s_sh_afe_ocd1t);

	temp = AFE_Parameters_RS485_Struction.u16IdsgOcp_Second.curValue * 100 / g_u32CS_Res_AFE;
	AFE_ROM_PARAMETERS_Struction.m0CH_0DH.OCD2V = Choose_Right_Value(temp, s_sh_afe_ocd2v);
	temp = AFE_Parameters_RS485_Struction.u16IdsgOcp_Filter_Second.curValue * 10;
	AFE_ROM_PARAMETERS_Struction.m0CH_0DH.OCD2T = Choose_Right_Value(temp, s_sh_afe_occt_ocd2t);

	temp = AFE_Parameters_RS485_Struction.u16IchgOcp_Second.curValue * 100 / g_u32CS_Res_AFE;
	AFE_ROM_PARAMETERS_Struction.m0EH_0FH.OCCV = Choose_Right_Value(temp, s_sh_afe_ocd1v_occv);
	temp = AFE_Parameters_RS485_Struction.u16IchgOcp_Filter_Second.curValue * 10;
	AFE_ROM_PARAMETERS_Struction.m0EH_0FH.OCCT = Choose_Right_Value(temp, s_sh_afe_occt_ocd2t);

	// InitShortCur();
	temp = AFE_Parameters_RS485_Struction.u16CBC_DelayT.curValue;
	AFE_ROM_PARAMETERS_Struction.m0EH_0FH.SCT = Choose_Right_Value(temp, g_u16ShAfeSctTable);
	temp = AFE_Parameters_RS485_Struction.u16CBC_Cur_DSG.curValue * 1000 / g_u32CS_Res_AFE; // 当前对应多少mv
	AFE_ROM_PARAMETERS_Struction.m0EH_0FH.SCV = Choose_Right_Value(temp, g_u16ShAfeScvTable);

	AFE_TEMPERATURE[0] = AFE_Parameters_RS485_Struction.u16TChgOTp.curValue / 10;
	AFE_TEMPERATURE[1] = AFE_Parameters_RS485_Struction.u16TChgOTp_Rcv.curValue / 10;
	AFE_TEMPERATURE[2] = AFE_Parameters_RS485_Struction.u16TchgUTp.curValue / 10;
	AFE_TEMPERATURE[3] = AFE_Parameters_RS485_Struction.u16TchgUTp_Rcv.curValue / 10;
	AFE_TEMPERATURE[4] = AFE_Parameters_RS485_Struction.u16TdischgOTp.curValue / 10;
	AFE_TEMPERATURE[5] = AFE_Parameters_RS485_Struction.u16TdischgOTp_Rcv.curValue / 10;
	AFE_TEMPERATURE[6] = AFE_Parameters_RS485_Struction.u16TdischgUTp.curValue / 10;
	AFE_TEMPERATURE[7] = AFE_Parameters_RS485_Struction.u16TdischgUTp_Rcv.curValue / 10;

	for (i = 0; i < 8; i++)
	{
		temp = iSheldTemp_10K_NTC[AFE_TEMPERATURE[i]];
		*(((UINT8 *)&AFE_ROM_PARAMETERS_Struction.m11H_19H) + i) = (UINT8)(((UINT32)temp << 9) / ((UINT32)SH367309_Reg_Store.TR_ResRef + temp));
	}
}

static void AFE_CopyCurValues(UINT16 *values)
{
	UINT16 i;
	AFE_Value_Typedef *param = &AFE_Parameters_RS485_Struction.u16VcellOvp;

	for (i = 0; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
	{
		values[i] = (param + i)->curValue;
	}
}

static void AFE_RestoreCurValues(const UINT16 *values)
{
	UINT16 i;
	AFE_Value_Typedef *param = &AFE_Parameters_RS485_Struction.u16VcellOvp;

	for (i = 0; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
	{
		(param + i)->curValue = values[i];
	}
}

static UINT8 AFE_SaveCurValuesToFlash(void)
{
	UINT16 values[AFE_PARAMETES_TOTAL_LENGTH];

	AFE_CopyCurValues(values);
	return StorageFlash_SaveAfeData(values, AFE_PARAMETES_TOTAL_LENGTH);
}

bool Write_Parameters(void)
{
	int i = 0;
	UINT8 temp[26] = {0};
	UINT8 verify[26] = {0};
	UINT8 *P = (UINT8 *)&AFE_ROM_PARAMETERS_Struction;

	if (!MTPRead(0x00, 25, temp))
	{
		return false;
	}

	for (i = 0; i < 25; i++)
	{
		if ((temp[i] != P[i]) && !MTPWriteROM((UINT8)i, 1, P + i))
		{
			return false;
		}
	}

	if (!MTPRead(0x00, 25, verify))
	{
		return false;
	}

	for (i = 0; i < 25; i++)
	{
		if (verify[i] != P[i])
		{
			System_ERROR_UserCallback(ERROR_AFE1);
			return false;
		}
	}

	return true;
}

bool SH367309_UpdataAfeConfig(void)
{
	bool ret = false;
	UINT8 isdiff = 0;
	UINT8 can_compare = 0;

	if (AFE_PARAM_WRITE_Flag)
	{
		AFE_PARAM_WRITE_Flag = 0;
		Refresh_Parameters();
		{
			int i = 0;
			UINT8 temp[26] = {0};
			UINT8 *P = (UINT8 *)&AFE_ROM_PARAMETERS_Struction;

			if (MTPRead(0x00, 25, temp))
			{
				can_compare = 1;
				for (i = 0; i < 25; i++)
				{
					if (temp[i] != P[i])
					{
						isdiff = 1;
						break;
					}
				}
			}
		}

		if (!can_compare)
		{
			AFE_PARAM_WRITE_Flag = 1;
			return false;
		}

		if (isdiff)
		{
			MCUO_AFE_VPRO = 1;
			Delay1ms(20);
			Feed_IWatchDog;

			ret = Write_Parameters();

			Feed_IWatchDog;
			MCUO_AFE_VPRO = 0;
			Delay1ms(1);

			if (!System_ERROR_UserCallback(ERROR_STATUS_AFE1))
			{
				AFE_Reset();
				Delay1ms(5);
				AFE_IsReady();
			}
			SH367309_Enable_AFE_Wdt_Cadc_Drivers();
			if (!ret)
			{
				AFE_PARAM_WRITE_Flag = 1;
			}
		}
		else
		{
			ret = true;
		}
	}

	return ret;
}

UINT8 Sci_WrRegs_0x10_AFE_Parameters(UINT16 u16Channel, struct RS485MSG *s)
{
	UINT16 u16WrRegNum;
	UINT16 u16SciRegStartAddr;
	int i = 0;
	UINT16 offset = 0;
	UINT16 *P = (UINT16 *)&AFE_Parameters_RS485_Struction;
	UINT16 snapshot[AFE_PARAMETES_TOTAL_LENGTH];
	(void)u16Channel;

	u16SciRegStartAddr = s->u16Buffer[3] + (s->u16Buffer[2] << 8);
	u16WrRegNum = s->u16Buffer[5] + (s->u16Buffer[4] << 8);

	if ((u16SciRegStartAddr >= RS485_CMD_ADDR_AFE_ROM_PARAMETERS_START) && (u16SciRegStartAddr <= RS485_CMD_ADDR_AFE_ROM_PARAMETERS_END) && (u16SciRegStartAddr + u16WrRegNum - 1 <= RS485_CMD_ADDR_AFE_ROM_PARAMETERS_END))
	{
		offset = u16SciRegStartAddr - RS485_CMD_ADDR_AFE_ROM_PARAMETERS_START;
		AFE_CopyCurValues(snapshot);

		Feed_IWatchDog;
		for (i = 0; i < u16WrRegNum; i++)
		{
			*(P + (i + offset) * 4) = s->u16Buffer[8 + i * 2] + (s->u16Buffer[7 + i * 2] << 8);
		}
		Feed_IWatchDog;

		if (!AFE_SaveCurValuesToFlash())
		{
			AFE_RestoreCurValues(snapshot);
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_CMD_INVALID;
			return 1;
		}

		AFE_PARAM_WRITE_Flag = 1;
		return 1;
	}

	return 0;
}

void Sci_WrReg_0x06_Reset_AFE_Parameters(struct RS485MSG *s)
{
	UINT16 u16SciRegData = s->u16Buffer[5] + (s->u16Buffer[4] << 8);
	if (0x0001 == u16SciRegData)
	{
		if (!EEPROM_ResetData_AFE_ParametersToDefault())
		{
			s->AckType = RS485_ACK_NEG;
			s->ErrorType = RS485_ERROR_CMD_INVALID;
		}
	}
	else
	{
		s->AckType = RS485_ACK_NEG;
		s->ErrorType = RS485_ERROR_DATA_INVALID;
	}
}

void Sci_ACK_0x03_RW_AFE_Parameters(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
	UINT16 u16SciTemp;
	UINT16 i = 0;
	UINT16 j;
	UINT16 *P = (UINT16 *)&AFE_Parameters_RS485_Struction;
	(void)s;

	for (j = 0; j < AFE_PARAMETES_TOTAL_LENGTH; j++)
	{
		u16SciTemp = *(P + j * 4);
		t_u8BuffTemp[i++] = (u16SciTemp >> 8) & 0x00FF;
		t_u8BuffTemp[i++] = u16SciTemp & 0x00FF;
	}
}

UINT8 EEPROM_ResetData_AFE_ParametersToDefault(void)
{
	UINT8 i;
	UINT16 *P = (UINT16 *)&AFE_Parameters_RS485_Struction.u16VcellOvp.defaultValue;

	Feed_IWatchDog;
	for (i = 0; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
	{
		*(P + i * 4 - 1) = *(P + i * 4);
	}
	Feed_IWatchDog;

	if (!AFE_SaveCurValuesToFlash())
	{
		return 0;
	}

	AFE_PARAM_WRITE_Flag = 1;
	return 1;
}

void ReadEEPROM_AFE_Parameters(void)
{
	UINT16 i;
	AFE_Value_Typedef *P = &AFE_Parameters_RS485_Struction.u16VcellOvp;
	UINT16 values[AFE_PARAMETES_TOTAL_LENGTH];
	UINT8 use_default = 0;

	if (!StorageFlash_LoadAfeData(values, AFE_PARAMETES_TOTAL_LENGTH))
	{
		use_default = 1;
	}

	for (i = 0; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
	{
		if (!use_default)
		{
			(P + i)->curValue = values[i];
			if (((P + i)->curValue < (P + i)->minValue) || ((P + i)->curValue > (P + i)->maxValue))
			{
				use_default = 1;
			}
		}
	}

	if (use_default)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		EEPROM_ResetData_AFE_ParametersToDefault();
	}
}
