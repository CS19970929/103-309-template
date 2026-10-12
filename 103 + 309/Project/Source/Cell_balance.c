#include "main.h"

/* Report AFE balance register bits; MCU does not choose or switch cells. */
void CellBalance_UpdateReport(UINT16 afe_balance_mask)
{
    UINT8 cell;
    UINT16 report_mask = 0u;

    if (SeriesNum >= 5u && SeriesNum <= 16u)
    {
        for (cell = 0u; cell < SeriesNum; ++cell)
        {
            UINT8 afe_cell = SeriesSelect_AFE1[SeriesNum - 1u][cell];
            if (afe_cell < 16u && (afe_balance_mask & ((UINT16)1u << afe_cell)))
                report_mask |= (UINT16)((UINT16)1u << cell);
        }
    }

    g_stCellInfoReport.u16BalanceFlag1 = report_mask;
    g_stCellInfoReport.u16BalanceFlag2 = 0u;
}
