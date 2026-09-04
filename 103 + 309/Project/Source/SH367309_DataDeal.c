#include "main.h"
#include "SH367309_DataDeal.h"
#include "AfeParamAccess.h"
#include "afe3520/Afe3520.h"
#include "afe3520/BmsProtection3520.h"
#include <string.h>

/* Historical compile-slot name; this is now the SH3673520 parameter service. */
int AFE_PARAM_WRITE_Flag = 1;
AFE_ROM_PARAMETERS_TypeDef AFE_ROM_PARAMETERS_Struction = {0};
AFE_Parameters_RS485_Typedef AFE_Parameters_RS485_Struction = AFE_PARAMETERS_RS485_STRUCTION_DEFAULT;

static uint8_t Bms3520_ParamImageValid(const AFE_Parameters_RS485_Typedef *p)
{
    uint32_t dsg1SenseMv;
    uint32_t dsg2SenseMv;
    uint32_t chg2SenseUv;
    uint16_t i;

    if (p == 0) return 0U;
    for (i = 0U; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
    {
        const AFE_Value_Typedef *v = (const AFE_Value_Typedef *)((const UINT8 *)p +
                                    (uint32_t)i * sizeof(AFE_Value_Typedef));
        if ((v->curValue < v->minValue) || (v->curValue > v->maxValue)) return 0U;
    }

    if (p->u16VcellOvp_Rcv.curValue >= p->u16VcellOvp.curValue) return 0U;
    if (p->u16VcellUvp_Rcv.curValue <= p->u16VcellUvp.curValue) return 0U;
    if (p->u16IchgOcp_First.curValue > p->u16IchgOcp_Second.curValue) return 0U;
    if (p->u16IdsgOcp_First.curValue > p->u16IdsgOcp_Second.curValue) return 0U;
    if (p->u16CBC_Cur_DSG.curValue < p->u16IdsgOcp_Second.curValue) return 0U;

    if (p->u16TChgOTp_Rcv.curValue >= p->u16TChgOTp.curValue) return 0U;
    if (p->u16TchgUTp_Rcv.curValue <= p->u16TchgUTp.curValue) return 0U;
    if (p->u16TdischgOTp_Rcv.curValue >= p->u16TdischgOTp.curValue) return 0U;
    if (p->u16TdischgUTp_Rcv.curValue <= p->u16TdischgUTp.curValue) return 0U;

    if ((p->u16VcellOvp.curValue > 5115U) || (p->u16VcellUvp.curValue > 5115U)) return 0U;
    if (p->u16CBC_DelayT.curValue > 576U) return 0U;

    dsg1SenseMv = ((uint32_t)p->u16IdsgOcp_First.curValue * CS_Res +
                   (5UL * CS_Res_Num)) / (10UL * CS_Res_Num);
    dsg2SenseMv = ((uint32_t)p->u16IdsgOcp_Second.curValue * CS_Res +
                   (5UL * CS_Res_Num)) / (10UL * CS_Res_Num);
    chg2SenseUv = ((uint32_t)p->u16IchgOcp_Second.curValue * 100UL * CS_Res +
                   (CS_Res_Num / 2U)) / CS_Res_Num;

    /* SH3673520 hardware ranges: OCD1<=80mV, OCD2<=160mV, OCC<=44mV approx. */
    if (dsg1SenseMv > 80UL) return 0U;
    if (dsg2SenseMv > 160UL) return 0U;
    if (chg2SenseUv > 44000UL) return 0U;
    return 1U;
}

static void Bms3520_CopyRuntimeValues(AFE_Parameters_RS485_Typedef *dst,
                                      const AFE_Parameters_RS485_Typedef *src)
{
    UINT16 i;
    for (i = 0U; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
    {
        AFE_Value_Typedef *d = (AFE_Value_Typedef *)((UINT8 *)dst + (uint32_t)i * sizeof(AFE_Value_Typedef));
        const AFE_Value_Typedef *s = (const AFE_Value_Typedef *)((const UINT8 *)src + (uint32_t)i * sizeof(AFE_Value_Typedef));
        d->curValue = s->curValue;
    }
}

void App_SH367309_Supplement(void)
{
    /* Hardware configuration is derived from this parameter image and repaired
     * by Bms3520_ProtectionService after any AFE reset/config mismatch. */
    if (AFE_PARAM_WRITE_Flag)
    {
        if (Bms3520_ApplyAndVerifyAfeConfig()) AFE_PARAM_WRITE_Flag = 0;
    }
}

UINT8 Sci_WrRegs_0x10_AFE_Parameters(UINT16 u16Channel, struct RS485MSG *s)
{
    AFE_Parameters_RS485_Typedef candidate;
    UINT16 start;
    UINT16 count;
    UINT16 offset;
    UINT16 i;
    UINT16 value;
    (void)u16Channel;

    if (s == 0) return 0U;
    start = (UINT16)(((UINT16)s->u16Buffer[2] << 8) | s->u16Buffer[3]);
    if ((start < RS485_CMD_ADDR_AFE_ROM_PARAMETERS_START) ||
        (start > RS485_CMD_ADDR_AFE_ROM_PARAMETERS_END)) return 0U;

    count = (UINT16)(((UINT16)s->u16Buffer[4] << 8) | s->u16Buffer[5]);
    offset = (UINT16)(start - RS485_CMD_ADDR_AFE_ROM_PARAMETERS_START);
    if ((count == 0U) || (offset >= AFE_PARAMETES_TOTAL_LENGTH) ||
        (count > (UINT16)(AFE_PARAMETES_TOTAL_LENGTH - offset)) ||
        (s->u16Buffer[6] != (UINT8)(count << 1)))
    {
        s->AckType = RS485_ACK_NEG;
        s->ErrorType = RS485_ERROR_DATA_INVALID;
        return 1U;
    }

    candidate = AFE_Parameters_RS485_Struction;
    for (i = 0U; i < count; ++i)
    {
        value = (UINT16)(((UINT16)s->u16Buffer[7U + i * 2U] << 8) |
                         s->u16Buffer[8U + i * 2U]);
        if (!AfeParam_ValueIsValid((UINT16)(offset + i), value))
        {
            s->AckType = RS485_ACK_NEG;
            s->ErrorType = RS485_ERROR_DATA_INVALID;
            return 1U;
        }
        ((AFE_Value_Typedef *)((UINT8 *)&candidate +
          (uint32_t)(offset + i) * sizeof(AFE_Value_Typedef)))->curValue = value;
    }

    if (!Bms3520_ParamImageValid(&candidate))
    {
        s->AckType = RS485_ACK_NEG;
        s->ErrorType = RS485_ERROR_DATA_INVALID;
        return 1U;
    }

    /* Persist first. Runtime/AFE are changed only after the dual-slot CONFIG commit succeeds. */
    EEPROM_ConfigEditBegin();
    for (i = 0U; i < count; ++i)
    {
        value = ((AFE_Value_Typedef *)((UINT8 *)&candidate +
                 (uint32_t)(offset + i) * sizeof(AFE_Value_Typedef)))->curValue;
        if (!EEPROM_ConfigEditSetAfeWord((UINT16)(offset + i), value))
        {
            s->AckType = RS485_ACK_NEG;
            s->ErrorType = RS485_ERROR_CMD_INVALID;
            return 1U;
        }
    }
    if (!EEPROM_ConfigEditCommit())
    {
        s->AckType = RS485_ACK_NEG;
        s->ErrorType = RS485_ERROR_CMD_INVALID;
        return 1U;
    }

    Bms3520_CopyRuntimeValues(&AFE_Parameters_RS485_Struction, &candidate);
    AFE_PARAM_WRITE_Flag = 1;
    Afe3520_MarkConfigDirty();
    return 1U;
}

void Sci_ACK_0x03_RW_AFE_Parameters(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
    UINT16 i;
    UINT16 value;
    (void)s;
    for (i = 0U; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
    {
        value = AfeParam_AtConst(i)->curValue;
        t_u8BuffTemp[i * 2U] = (UINT8)(value >> 8);
        t_u8BuffTemp[i * 2U + 1U] = (UINT8)value;
    }
}

UINT8 EEPROM_ResetData_AFE_ParametersToDefault(void)
{
    AFE_Parameters_RS485_Typedef defaults = AFE_PARAMETERS_RS485_STRUCTION_DEFAULT;
    UINT16 i;

    if (!Bms3520_ParamImageValid(&defaults))
    {
        System_ERROR_UserCallback(ERROR_EEPROM_STORE);
        return 0U;
    }

    EEPROM_ConfigEditBegin();
    for (i = 0U; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
    {
        if (!EEPROM_ConfigEditSetAfeWord(i,
            ((AFE_Value_Typedef *)((UINT8 *)&defaults + (uint32_t)i * sizeof(AFE_Value_Typedef)))->curValue))
            return 0U;
    }
    if (!EEPROM_ConfigEditCommit()) return 0U;

    Bms3520_CopyRuntimeValues(&AFE_Parameters_RS485_Struction, &defaults);
    AFE_PARAM_WRITE_Flag = 1;
    Afe3520_MarkConfigDirty();
    return 1U;
}

void Sci_WrReg_0x06_Reset_AFE_Parameters(struct RS485MSG *s)
{
    UINT16 value;
    if (s == 0) return;
    value = (UINT16)(((UINT16)s->u16Buffer[4] << 8) | s->u16Buffer[5]);
    if (value != 1U)
    {
        s->AckType = RS485_ACK_NEG;
        s->ErrorType = RS485_ERROR_DATA_INVALID;
        return;
    }
    if (!EEPROM_ResetData_AFE_ParametersToDefault())
    {
        s->AckType = RS485_ACK_NEG;
        s->ErrorType = RS485_ERROR_CMD_INVALID;
    }
}

void ReadEEPROM_AFE_Parameters(void)
{
    /* Unified CONFIG loading in EEPROM.c already populates curValue fields. */
    AFE_PARAM_WRITE_Flag = 1;
    Afe3520_MarkConfigDirty();
}
