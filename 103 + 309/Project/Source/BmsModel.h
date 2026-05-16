#ifndef BMS_MODEL_H
#define BMS_MODEL_H

#include "Project_Types.h"
#include "DataDeal.h"
#include "Sci_Upper.h"
#include "System_Monitor.h"

BMS_INLINE const struct stCell_Info *BmsModel_CellInfoConst(void)
{
	return &g_stCellInfoReport;
}

BMS_INLINE struct stCell_Info *BmsModel_CellInfo(void)
{
	return &g_stCellInfoReport;
}

BMS_INLINE const struct OTHER_ELEMENT *BmsModel_ParamsConst(void)
{
	return &OtherElement;
}

BMS_INLINE struct OTHER_ELEMENT *BmsModel_Params(void)
{
	return &OtherElement;
}

BMS_INLINE const volatile union System_Status *BmsModel_SystemStatusConst(void)
{
	return &SystemStatus;
}

BMS_INLINE UINT16 BmsModel_GetPackVoltage10mV(void)
{
	return g_stCellInfoReport.u16VCellTotle;
}

BMS_INLINE UINT32 BmsModel_GetPackVoltageMv(void)
{
	return (UINT32)g_stCellInfoReport.u16VCellTotle * 10U;
}

BMS_INLINE UINT16 BmsModel_GetChargeCurrentA10(void)
{
	return g_stCellInfoReport.u16Ichg;
}

BMS_INLINE UINT16 BmsModel_GetDischargeCurrentA10(void)
{
	return g_stCellInfoReport.u16IDischg;
}

BMS_INLINE UINT8 BmsModel_GetSocPercent(void)
{
	return (UINT8)g_stCellInfoReport.SocElement.u16Soc;
}

BMS_INLINE UINT8 BmsModel_GetSohPercent(void)
{
	return (UINT8)g_stCellInfoReport.SocElement.u16Soh;
}

BMS_INLINE UINT16 BmsModel_GetCapacityNowAh100(void)
{
	return g_stCellInfoReport.SocElement.u16CapacityNow;
}

BMS_INLINE UINT16 BmsModel_GetCapacityFactoryAh100(void)
{
	return g_stCellInfoReport.SocElement.u16CapacityFactory;
}

BMS_INLINE UINT16 BmsModel_GetCycleTimes(void)
{
	return g_stCellInfoReport.SocElement.u16Cycle_times;
}

BMS_INLINE UINT16 BmsModel_GetMaxTemperatureT10Offset40(void)
{
	return g_stCellInfoReport.u16TempMax;
}

BMS_INLINE UINT16 BmsModel_GetSeriesCount(void)
{
	return (UINT16)SNum;
}

#endif
