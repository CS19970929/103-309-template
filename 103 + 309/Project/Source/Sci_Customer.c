#include "main.h"
#include "Sci_Protocol.h"

#define CUSTOMER_CELL_COUNT 7U
static const UINT8 s_customer_software_version[32]=BMS_SOFTWARE_VERDION_DEFAULT;

static void Customer_PutLE(UINT8 *buffer, UINT16 *index, UINT16 value)
{
    buffer[(*index)++] = (UINT8)value;
    buffer[(*index)++] = (UINT8)(value >> 8);
}

static UINT8 Customer_Bcc(const UINT8 *data, UINT16 length)
{
    UINT8 checksum = 0U;
    while (length-- != 0U)
        checksum ^= *data++;
    return checksum;
}

static UINT16 Customer_Checksum(const UINT8 *data, UINT16 length)
{
    UINT16 sum = 0U;
    while (length-- != 0U)
        sum = (UINT16)(sum + *data++);
    return (UINT16)(0U-sum);
}

void Sci_CustomerUpdateCounters(void)
{
    const struct MDLCHGFAULT_BITS *fault = &g_stCellInfoReport.unMdlFault_Third.bits;
    UINT16 mask = (UINT16)(SH367309_Reg_Store.REG_BSTATUS1.bits.SC |
        ((fault->b1IchgOcp || g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp) << 1) |
        ((fault->b1IdischgOcp || g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp) << 2) |
        (fault->b1CellOvp << 3) | (fault->b1CellChgOtp << 4) |
        (fault->b1CellChgUtp << 5) | (fault->b1CellDischgOtp << 6) |
        (fault->b1CellDischgUtp << 7) | (fault->b1BatOvp << 8) | (fault->b1BatUvp << 9));
    StorageFlash_UpdateFaultStatistics(mask);
}

UINT8 Sci_CustomerFeed(struct RS485MSG *message, UINT8 data)
{
    UINT16 total;
    if (message->ptr_no >= RS485_MAX_BUFFER_SIZE)
    {
        Sci_ResetMessage(message);
        return 0U;
    }
    message->u16Buffer[message->ptr_no++] = data;
    if (message->ptr_no == 2U &&
        ((message->u16Buffer[0] == 0x5AU && data != 0xA5U) ||
         (message->u16Buffer[0] == 0xDDU && data != 0xA5U && data != 0x5AU)))
    {
        Sci_ResetMessage(message);
        return 0U;
    }
    if (message->ptr_no < 4U)
        return 0U;
    total = (UINT16)(message->u16Buffer[3] + (message->u16Buffer[0] == 0xDDU ? 7U : 6U));
    if (total > RS485_MAX_BUFFER_SIZE)
        Sci_ResetMessage(message);
    else if (message->ptr_no == total)
        return 1U;
    return 0U;
}

UINT8 Sci_CustomerFrameValid(const struct RS485MSG *message)
{
    const UINT8 *b = message->u16Buffer;
    UINT16 count = message->ptr_no;
    UINT16 checksum;
    if (count < 6U || count > RS485_MAX_BUFFER_SIZE)
        return 0U;
    if (b[0] == 0x5AU)
        return (UINT8)(count == 7U && b[1] == 0xA5U && b[3] == 1U && b[4] == 1U &&
            (b[2] == 3U || b[2] == 4U || b[2] == 5U) &&
            b[5] == Customer_Bcc(&b[2],3U) && b[6] == 0xF0U);
    if (b[0] != 0xDDU || count != (UINT16)(b[3]+7U) || b[count-1U] != 0x77U ||
        (b[1] != 0xA5U && b[1] != 0x5AU))
        return 0U;
    checksum = (UINT16)((b[count-3U] << 8) | b[count-2U]);
    return (UINT8)(checksum == Customer_Checksum(&b[2],(UINT16)(count-5U)));
}

static UINT16 Customer_Data1(UINT8 *b)
{
    const struct MDLCHGFAULT_BITS *f = &g_stCellInfoReport.unMdlFault_Third.bits;
    UINT16 i, index=4U;
    INT32 current = AfeCurrent_GetCurrent_mA();
    INT32 temperature = (INT32)AFE_Parameters_RS485_Struction.u16TChgOTp.curValue/10-40;
    for (i=0U; i<CUSTOMER_CELL_COUNT; ++i)
        Customer_PutLE(b,&index,i<SeriesNum ? g_stCellInfoReport.u16VCell[i] : 61001U);
    b[index++] = (UINT8)(g_stCellInfoReport.u16TempMax > 1650U ? 165U : g_stCellInfoReport.u16TempMax/10U);
    b[index++] = (UINT8)(g_stCellInfoReport.u16BalanceFlag1 & 0x7FU);
    Customer_PutLE(b,&index,(UINT16)(g_stCellInfoReport.u16VCellTotle/10U));
    if (current > 100000) current=100000;
    if (current < -100000) current=-100000;
    Customer_PutLE(b,&index,(UINT16)(10000+current/10));
    b[index++] = (UINT8)(g_stCellInfoReport.SocElement.u16Soc > 100U ? 100U : g_stCellInfoReport.SocElement.u16Soc);
    b[index++] = (UINT8)(f->b1CellOvp | (f->b1CellUvp << 1) |
        ((f->b1CellChgOtp || f->b1CellDischgOtp || f->b1TmosOtp) << 2) |
        ((f->b1IchgOcp || g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp) << 4) |
        ((f->b1IdischgOcp || g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp) << 5));
    Customer_PutLE(b,&index,AFE_Parameters_RS485_Struction.u16VcellOvp.curValue);
    Customer_PutLE(b,&index,AFE_Parameters_RS485_Struction.u16VcellUvp.curValue);
    if (temperature > 125) temperature=125;
    if (temperature < -40) temperature=-40;
    b[index++] = (UINT8)temperature;
    Customer_PutLE(b,&index,(UINT16)(10000U +
        (AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue > 10000U ? 10000U : AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue)));
    Customer_PutLE(b,&index,(UINT16)(10000U -
        (AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue > 10000U ? 10000U : AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue)));
    Customer_PutLE(b,&index,(UINT16)(g_stCellInfoReport.SocElement.u16CapacityFactory/10U));
    Customer_PutLE(b,&index,AFE_Parameters_RS485_Struction.u16VcellOvp_Rcv.curValue);
    Customer_PutLE(b,&index,AFE_Parameters_RS485_Struction.u16VcellUvp_Rcv.curValue);
    return index;
}

void Sci_CustomerProcessFrame(struct RS485MSG *message)
{
    UINT8 *b=message->u16Buffer;
    UINT8 command=b[2], status=0U;
    UINT16 index=4U, i, checksum;
    UINT32 counters[FLASH_STATISTICS_COUNT];
    message->AckLenth=0U;
    if (!Sci_CustomerFrameValid(message))
        return;
    if (b[0] == 0x5AU)
    {
        if (command == 5U)
        {
            b[0]=0xAAU; b[1]=0xEDU; b[2]=15U; index=3U;
            Customer_PutLE(b,&index,g_stCellInfoReport.SocElement.u16Cycle_times);
            b[index++]=s_customer_software_version[0];
            b[index++]=s_customer_software_version[1];
            for (i=0U; i<11U; ++i) b[index++]=ProductionInfor.BMS_SerialNumber[i];
            b[index]=Customer_Bcc(&b[3],15U); ++index;
            b[index++]=0xFAU;
        }
        else
        {
            b[2]=(UINT8)(command == 4U ? 1U : 3U);
            if (command == 4U)
                index=Customer_Data1(b);
            else
            {
                Customer_PutLE(b,&index,(UINT16)(g_stCellInfoReport.SocElement.u16CapacityFull/10U));
                Customer_PutLE(b,&index,SOC_GetRemainingMinutes(1U));
                Customer_PutLE(b,&index,SOC_GetRemainingMinutes(0U));
                for (i=0U; i<10U; ++i) b[index++]=0U;
            }
            b[3]=(UINT8)(index-4U);
            b[index]=Customer_Bcc(&b[2],(UINT16)(index-2U)); ++index;
            b[index++]=0xF0U;
        }
    }
    else
    {
        if (b[1] == 0xA5U && command == 0xAAU && b[3] == 0U)
        {
            if (!StorageFlash_ReadStatistics(counters)) status=0x80U;
            else for (i=0U; i<FLASH_STATISTICS_COUNT; ++i) {
                UINT16 count=(UINT16)(counters[i]>0xFFFFU ? 0xFFFFU : counters[i]);
                b[index++]=(UINT8)(count >> 8);
                b[index++]=(UINT8)count;
            }
        }
        else
        {
#if PROJECT_CFG_HOST_WRITE_ENABLE
            if (b[1] == 0x5AU && command == 1U && b[3] == 2U && b[4] == 0x28U && b[5] == 0x28U)
            {
                status=StorageFlash_ClearFaultStatistics() ? 0U : 0x80U;
            }
            else if (b[1] == 0x5AU && command == 0xE1U && b[3] == 2U && b[4] == 0U && b[5] <= 3U)
                status = Fault_SetHostMosOff(b[5]) ? 0U : 0x80U;
            else
#endif
                status=0x80U;
        }
        b[1]=command; b[2]=status; b[3]=(UINT8)(index-4U);
        checksum=Customer_Checksum(&b[2],(UINT16)(index-2U));
        b[index++]=(UINT8)(checksum >> 8); b[index++]=(UINT8)checksum; b[index++]=0x77U;
    }
    message->AckLenth=(UINT8)index;
}
