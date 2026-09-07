#include "main.h"
#include "BmsParameters.h"
#include "AfeParamAccess.h"
#include "afe3520/Afe3520.h"
#include "afe3520/BmsProtection3520.h"
#include <string.h>

/* Persistent software protection parameters; hardware profile is independent. */
BMS_PARAMETERS g_bmsParameters = BMS_PARAMETERS_DEFAULT;

static uint8_t Bms3520_ParamImageValid(const BMS_PARAMETERS *p)
{
    uint16_t i;

    if (p == 0) return 0U;
    for (i = 0U; i < BMS_PARAMETER_COUNT; ++i)
    {
        const BMS_PARAMETER_VALUE *v = (const BMS_PARAMETER_VALUE *)((const UINT8 *)p +
                                    (uint32_t)i * sizeof(BMS_PARAMETER_VALUE));
        if ((v->curValue < v->minValue) || (v->curValue > v->maxValue)) return 0U;
    }

    if (p->u16VcellOvp_Rcv.curValue >= p->u16VcellOvp.curValue) return 0U;
    if (p->u16VcellUvp_Rcv.curValue <= p->u16VcellUvp.curValue) return 0U;
    if (p->u16TChgOTp_Rcv.curValue >= p->u16TChgOTp.curValue) return 0U;
    if (p->u16TchgUTp_Rcv.curValue <= p->u16TchgUTp.curValue) return 0U;
    if (p->u16TdischgOTp_Rcv.curValue >= p->u16TdischgOTp.curValue) return 0U;
    if (p->u16TdischgUTp_Rcv.curValue <= p->u16TdischgUTp.curValue) return 0U;

    if (p->u16VcellUvp_Rcv.curValue >= p->u16VcellOvp_Rcv.curValue ||
        p->u16TchgUTp_Rcv.curValue >= p->u16TChgOTp_Rcv.curValue ||
        p->u16TdischgUTp_Rcv.curValue >= p->u16TdischgOTp_Rcv.curValue) return 0U;
    return 1U;
}

static void Bms3520_CopyRuntimeValues(BMS_PARAMETERS *dst,
                                      const BMS_PARAMETERS *src)
{
    UINT16 i;
    for (i = 0U; i < BMS_PARAMETER_COUNT; ++i)
    {
        BMS_PARAMETER_VALUE *d = (BMS_PARAMETER_VALUE *)((UINT8 *)dst + (uint32_t)i * sizeof(BMS_PARAMETER_VALUE));
        const BMS_PARAMETER_VALUE *s = (const BMS_PARAMETER_VALUE *)((const UINT8 *)src + (uint32_t)i * sizeof(BMS_PARAMETER_VALUE));
        d->curValue = s->curValue;
    }
    Bms3520_RestartSoftwareTimers();
}

UINT8 Sci_WrRegs_0x10_AFE_Parameters(UINT16 u16Channel, struct RS485MSG *s)
{
    BMS_PARAMETERS candidate;
    UINT16 start;
    UINT16 count;
    UINT16 offset;
    UINT16 i;
    UINT16 value;
    (void)u16Channel;

    if (s == 0) return 0U;
    start = (UINT16)(((UINT16)s->u16Buffer[2] << 8) | s->u16Buffer[3]);
    if ((start < RS485_CMD_ADDR_BMS_PARAMETERS_START) ||
        (start > RS485_CMD_ADDR_BMS_PARAMETERS_END)) return 0U;

    count = (UINT16)(((UINT16)s->u16Buffer[4] << 8) | s->u16Buffer[5]);
    offset = (UINT16)(start - RS485_CMD_ADDR_BMS_PARAMETERS_START);
    if ((count == 0U) || (offset >= BMS_PARAMETER_COUNT) ||
        (count > (UINT16)(BMS_PARAMETER_COUNT - offset)) ||
        (s->u16Buffer[6] != (UINT8)(count << 1)))
    {
        s->AckType = RS485_ACK_NEG;
        s->ErrorType = RS485_ERROR_DATA_INVALID;
        return 1U;
    }

    candidate = g_bmsParameters;
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
        ((BMS_PARAMETER_VALUE *)((UINT8 *)&candidate +
          (uint32_t)(offset + i) * sizeof(BMS_PARAMETER_VALUE)))->curValue = value;
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
        value = ((BMS_PARAMETER_VALUE *)((UINT8 *)&candidate +
                 (uint32_t)(offset + i) * sizeof(BMS_PARAMETER_VALUE)))->curValue;
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

    Bms3520_CopyRuntimeValues(&g_bmsParameters, &candidate);
    return 1U;
}

void Sci_ACK_0x03_RW_AFE_Parameters(struct RS485MSG *s, UINT8 t_u8BuffTemp[])
{
    UINT16 i;
    UINT16 value;
    (void)s;
    for (i = 0U; i < BMS_PARAMETER_COUNT; ++i)
    {
        value = AfeParam_AtConst(i)->curValue;
        t_u8BuffTemp[i * 2U] = (UINT8)(value >> 8);
        t_u8BuffTemp[i * 2U + 1U] = (UINT8)value;
    }
}

UINT8 EEPROM_ResetData_AFE_ParametersToDefault(void)
{
    BMS_PARAMETERS defaults = g_bmsParameters;
    UINT16 i;

    /* Reuse immutable defaultValue members instead of a second ROM image. */
    for (i = 0U; i < BMS_PARAMETER_COUNT; ++i)
    {
        BMS_PARAMETER_VALUE *v = (BMS_PARAMETER_VALUE *)((UINT8 *)&defaults +
            (uint32_t)i * sizeof(BMS_PARAMETER_VALUE));
        v->curValue = v->defaultValue;
    }
    if (!Bms3520_ParamImageValid(&defaults))
    {
        System_ERROR_UserCallback(ERROR_EEPROM_STORE);
        return 0U;
    }

    EEPROM_ConfigEditBegin();
    for (i = 0U; i < BMS_PARAMETER_COUNT; ++i)
    {
        if (!EEPROM_ConfigEditSetAfeWord(i,
            ((BMS_PARAMETER_VALUE *)((UINT8 *)&defaults + (uint32_t)i * sizeof(BMS_PARAMETER_VALUE)))->curValue))
            return 0U;
    }
    if (!EEPROM_ConfigEditCommit()) return 0U;

    Bms3520_CopyRuntimeValues(&g_bmsParameters, &defaults);
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
}
