#include "main.h"
#include "LxPowerProtocolPort.h"

#include <string.h>

#define LX_PORT_EVENT_RECORD_BYTES (FLASH_STORAGE_LOG_RECORD_COUNT * 2U)

typedef struct
{
    uint8_t active;
    uint8_t charge_disable;
    uint8_t discharge_disable;
} LX_PORT_SOFTWARE_MOS_STATE;

static LX_PORT_SOFTWARE_MOS_STATE s_lx_mos;

static uint16_t LxPort_ClampU16(INT32 value, uint16_t min_value, uint16_t max_value)
{
    if (value < (INT32)min_value)
    {
        return min_value;
    }
    if (value > (INT32)max_value)
    {
        return max_value;
    }
    return (uint16_t)value;
}

static uint8_t LxPort_ClampU8(INT32 value, uint8_t min_value, uint8_t max_value)
{
    if (value < (INT32)min_value)
    {
        return min_value;
    }
    if (value > (INT32)max_value)
    {
        return max_value;
    }
    return (uint8_t)value;
}

static uint8_t LxPort_AnyCellOvp(void)
{
    return (uint8_t)(g_stCellInfoReport.unMdlFault_First.bits.b1CellOvp ||
                     g_stCellInfoReport.unMdlFault_Second.bits.b1CellOvp ||
                     g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp);
}

static uint8_t LxPort_AnyCellUvp(void)
{
    return (uint8_t)(g_stCellInfoReport.unMdlFault_First.bits.b1CellUvp ||
                     g_stCellInfoReport.unMdlFault_Second.bits.b1CellUvp ||
                     g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp);
}

static uint8_t LxPort_AnyChargeOcp(void)
{
    return (uint8_t)(g_stCellInfoReport.unMdlFault_First.bits.b1IchgOcp ||
                     g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp ||
                     g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp);
}

static uint8_t LxPort_AnyDischargeOcp(void)
{
    return (uint8_t)(g_stCellInfoReport.unMdlFault_First.bits.b1IdischgOcp ||
                     g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp ||
                     g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp);
}

static uint8_t LxPort_AnyChargeOtp(void)
{
    return (uint8_t)(g_stCellInfoReport.unMdlFault_First.bits.b1CellChgOtp ||
                     g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgOtp ||
                     g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp);
}

static uint8_t LxPort_AnyChargeUtp(void)
{
    return (uint8_t)(g_stCellInfoReport.unMdlFault_First.bits.b1CellChgUtp ||
                     g_stCellInfoReport.unMdlFault_Second.bits.b1CellChgUtp ||
                     g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp);
}

static uint8_t LxPort_AnyDischargeOtp(void)
{
    return (uint8_t)(g_stCellInfoReport.unMdlFault_First.bits.b1CellDischgOtp ||
                     g_stCellInfoReport.unMdlFault_Second.bits.b1CellDischgOtp ||
                     g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp);
}

static uint8_t LxPort_AnyDischargeUtp(void)
{
    return (uint8_t)(g_stCellInfoReport.unMdlFault_First.bits.b1CellDischgUtp ||
                     g_stCellInfoReport.unMdlFault_Second.bits.b1CellDischgUtp ||
                     g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp);
}

static uint8_t LxPort_GetFaultBits(void)
{
    uint8_t value = 0U;

    if (LxPort_AnyCellOvp())
    {
        value |= (uint8_t)(1U << 0);
    }
    if (LxPort_AnyCellUvp())
    {
        value |= (uint8_t)(1U << 1);
    }
    if (LxPort_AnyChargeOtp() || LxPort_AnyDischargeOtp())
    {
        value |= (uint8_t)(1U << 2);
    }
    if (LxPort_AnyChargeOcp())
    {
        value |= (uint8_t)(1U << 4);
    }
    if (LxPort_AnyDischargeOcp())
    {
        value |= (uint8_t)(1U << 5);
    }

    return value;
}

static uint16_t LxPort_GetCurrentRaw(void)
{
    INT32 current_deci_amp;
    INT32 raw;

    /* 项目内部：充/放电电流均为正幅值，单位 0.1 A。 */
    current_deci_amp = (INT32)g_stCellInfoReport.u16Ichg -
                       (INT32)g_stCellInfoReport.u16IDischg;
    raw = 10000L + current_deci_amp * 10L;
    return LxPort_ClampU16(raw, 0U, 20000U);
}

static uint8_t LxPort_GetHighTemperatureRaw(void)
{
    INT32 temperature_c;

    /* AFE 参数内部为 (C + 40) * 10，协议字段为 1 C/LSB、offset 0。 */
    temperature_c = ((INT32)AFE_Parameters_RS485_Struction.u16TChgOTp.curValue / 10L) - 40L;
    if (temperature_c < -40L)
    {
        temperature_c = -40L;
    }
    else if (temperature_c > 125L)
    {
        temperature_c = 125L;
    }
    return (uint8_t)(int8_t)temperature_c;
}

static uint16_t LxPort_GetChargeOcRaw(void)
{
    INT32 raw;

    /* AFE 参数单位 0.1 A；协议 offset 1000 A、0.1 A/LSB。 */
    raw = 10000L + (INT32)AFE_Parameters_RS485_Struction.u16IchgOcp_Second.curValue;
    return LxPort_ClampU16(raw, 0U, 20000U);
}

static uint16_t LxPort_GetDischargeOcRaw(void)
{
    INT32 raw;

    raw = 10000L - (INT32)AFE_Parameters_RS485_Struction.u16IdsgOcp_Second.curValue;
    return LxPort_ClampU16(raw, 0U, 20000U);
}

static void LxPort_GetData1(LX_POWER_PROTOCOL_DATA1 *data)
{
    uint8_t i;

    memset(data, 0, sizeof(*data));
    for (i = 0U; i < LX_POWER_PROTOCOL_CELL_COUNT; ++i)
    {
        data->cell_mv[i] = g_stCellInfoReport.u16VCell[i];
    }

    /* 当前温度内部为 (C + 40) * 10，与协议 raw = C + 40 对齐。 */
    data->temperature_raw = LxPort_ClampU8(
        (INT32)(g_stCellInfoReport.u16Temperature[AFE1_TEMP1] / 10U), 0U, 255U);
    data->balance_bits = (uint8_t)(g_stCellInfoReport.u16BalanceFlag1 & 0x7FU);

    /* 项目内部总压为 0.01 V/LSB，LX 为 0.1 V/LSB。 */
    data->pack_voltage_raw = (uint16_t)(g_stCellInfoReport.u16VCellTotle / 10U);
    data->current_raw = LxPort_GetCurrentRaw();
    data->soc_percent = LxPort_ClampU8((INT32)g_stCellInfoReport.SocElement.u16Soc, 0U, 100U);
    data->fault_bits = LxPort_GetFaultBits();

    /* 309 项目使用实际 AFE 保护参数上报协议中的保护阈值。 */
    data->cell_ov_mv = AFE_Parameters_RS485_Struction.u16VcellOvp.curValue;
    data->cell_uv_mv = AFE_Parameters_RS485_Struction.u16VcellUvp.curValue;
    data->high_temperature_raw = LxPort_GetHighTemperatureRaw();
    data->charge_oc_raw = LxPort_GetChargeOcRaw();
    data->discharge_oc_raw = LxPort_GetDischargeOcRaw();
    data->rated_capacity_raw = OtherElement.u16Soc_Ah;
    data->cell_ov_recovery_mv = AFE_Parameters_RS485_Struction.u16VcellOvp_Rcv.curValue;
    data->cell_uv_recovery_mv = AFE_Parameters_RS485_Struction.u16VcellUvp_Rcv.curValue;
}

static uint8_t LxPort_ProductChar(const UINT8 *text,
                                  UINT16 length,
                                  uint16_t index,
                                  uint8_t padding)
{
    if ((index < length) && (text[index] != 0U))
    {
        return text[index];
    }
    return padding;
}

static void LxPort_GetData2(LX_POWER_PROTOCOL_DATA2 *data)
{
    uint8_t i;

    memset(data, 0, sizeof(*data));
    data->cycle_count = g_stCellInfoReport.SocElement.u16Cycle_times;
    data->software_version[0] = LxPort_ProductChar(ProductionInfor.BMS_SoftWareVersion,
                                                   ProductionInfor.BMS_SoftWareVersionLength,
                                                   0U,
                                                   (uint8_t)' ');
    data->software_version[1] = LxPort_ProductChar(ProductionInfor.BMS_SoftWareVersion,
                                                   ProductionInfor.BMS_SoftWareVersionLength,
                                                   1U,
                                                   (uint8_t)' ');
    for (i = 0U; i < 11U; ++i)
    {
        data->battery_id[i] = LxPort_ProductChar(ProductionInfor.BMS_SerialNumber,
                                                 ProductionInfor.BMS_SerialNumberLength,
                                                 i,
                                                 (uint8_t)' ');
    }
}

static uint16_t LxPort_RemainingMinutes(uint16_t capacity_centi_ah,
                                        uint16_t current_deci_amp)
{
    UINT32 minutes;

    if (current_deci_amp == 0U)
    {
        return 0U;
    }

    /* 0.01 Ah / 0.1 A = 0.1 h = 6 min。 */
    minutes = ((UINT32)capacity_centi_ah * 6U) / current_deci_amp;
    if (minutes > 0xFFFFU)
    {
        minutes = 0xFFFFU;
    }
    return (uint16_t)minutes;
}

static void LxPort_GetData3(LX_POWER_PROTOCOL_DATA3 *data)
{
    uint16_t full_capacity;
    uint16_t now_capacity;
    uint16_t charge_need;

    memset(data, 0, sizeof(*data));
    full_capacity = g_stCellInfoReport.SocElement.u16CapacityFull;
    now_capacity = g_stCellInfoReport.SocElement.u16CapacityNow;
    charge_need = (full_capacity > now_capacity) ?
                  (uint16_t)(full_capacity - now_capacity) : 0U;

    /* 项目容量单位 0.01 Ah，协议单位 0.1 Ah。 */
    data->max_capacity_raw = (uint16_t)(full_capacity / 10U);
    data->charge_remaining_min = LxPort_RemainingMinutes(charge_need,
                                                          g_stCellInfoReport.u16Ichg);
    data->discharge_remaining_min = LxPort_RemainingMinutes(now_capacity,
                                                             g_stCellInfoReport.u16IDischg);
}

static void LxPort_IncrementCounter(uint16_t *counter)
{
    if (*counter != 0xFFFFU)
    {
        ++(*counter);
    }
}

static void LxPort_GetData4Counts(uint16_t counts[11])
{
    static UINT8 records[LX_PORT_EVENT_RECORD_BYTES];
    uint16_t i;
    UINT8 event;

    for (i = 0U; i < 11U; ++i)
    {
        counts[i] = 0U;
    }

    /*
     * 当前工程尚无独立永久累计计数区，因此先从现有 100 条持久化事件记录统计。
     * 这保持现有存储模型不变；若客户要求全寿命累计，应另建独立计数存储。
     */
    Sci_ACK_0x03_ReadRegs_EventRecord(records);
    for (i = 0U; i < FLASH_STORAGE_LOG_RECORD_COUNT; ++i)
    {
        event = records[i * 2U];
        switch ((LogEventArray)event)
        {
        case CBC_ERR:
            LxPort_IncrementCounter(&counts[0]);
            break;
        case CHG_OCP:
            LxPort_IncrementCounter(&counts[1]);
            break;
        case DSG_OCP:
            LxPort_IncrementCounter(&counts[2]);
            break;
        case VCELL_OVP:
            LxPort_IncrementCounter(&counts[3]);
            break;
        case CHG_OTP:
            LxPort_IncrementCounter(&counts[4]);
            break;
        case CHG_UTP:
            LxPort_IncrementCounter(&counts[5]);
            break;
        case DSG_OTP:
            LxPort_IncrementCounter(&counts[6]);
            break;
        case DSG_UTP:
            LxPort_IncrementCounter(&counts[7]);
            break;
        case VBUS_OVP:
            LxPort_IncrementCounter(&counts[8]);
            break;
        case VBUS_UVP:
            LxPort_IncrementCounter(&counts[9]);
            break;
        case BMS_START_UP:
            LxPort_IncrementCounter(&counts[10]);
            break;
        default:
            break;
        }
    }
}

static uint8_t LxPort_ClearData4Counts(void)
{
    return EEPROM_ResetData_EventRecord_ToDefault();
}

static uint8_t LxPort_ChargeMosReleaseAllowed(void)
{
    if (LxPort_AnyCellOvp() ||
        LxPort_AnyChargeOcp() ||
        LxPort_AnyChargeOtp() ||
        LxPort_AnyChargeUtp())
    {
        return 0U;
    }
    return 1U;
}

static uint8_t LxPort_DischargeMosReleaseAllowed(void)
{
    if (LxPort_AnyCellUvp() ||
        LxPort_AnyDischargeOcp() ||
        LxPort_AnyDischargeOtp() ||
        LxPort_AnyDischargeUtp() ||
        System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
    {
        return 0U;
    }
    return 1U;
}

static void LxPort_ServiceMos(void)
{
    uint8_t charge_on;
    uint8_t discharge_on;

    if (!s_lx_mos.active)
    {
        return;
    }

    charge_on = (uint8_t)((!s_lx_mos.charge_disable) && LxPort_ChargeMosReleaseAllowed());
    discharge_on = (uint8_t)((!s_lx_mos.discharge_disable) && LxPort_DischargeMosReleaseAllowed());

    if (SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS != charge_on)
    {
        SH367309_DriverMos_Ctrl(GPIO_CHG, charge_on);
    }
    if (SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS != discharge_on)
    {
        SH367309_DriverMos_Ctrl(GPIO_DSG, discharge_on);
    }
}

static uint8_t LxPort_SetSoftwareMosMode(uint8_t mode)
{
    if (mode > 0x03U)
    {
        return 0U;
    }

    s_lx_mos.active = 1U;
    switch (mode)
    {
    case 0x00U:
        s_lx_mos.charge_disable = 0U;
        s_lx_mos.discharge_disable = 0U;
        break;
    case 0x01U:
        s_lx_mos.charge_disable = 1U;
        s_lx_mos.discharge_disable = 0U;
        break;
    case 0x02U:
        s_lx_mos.charge_disable = 0U;
        s_lx_mos.discharge_disable = 1U;
        break;
    case 0x03U:
        s_lx_mos.charge_disable = 1U;
        s_lx_mos.discharge_disable = 1U;
        break;
    default:
        return 0U;
    }

    LxPort_ServiceMos();
    return 1U;
}

const LX_POWER_PROTOCOL_ADAPTER g_lx_power_protocol_adapter = {
    LxPort_GetData1,
    LxPort_GetData2,
    LxPort_GetData3,
    LxPort_GetData4Counts,
    LxPort_ClearData4Counts,
    LxPort_SetSoftwareMosMode,
    LxPort_ServiceMos};
