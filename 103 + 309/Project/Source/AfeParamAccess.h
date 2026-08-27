#ifndef AFE_PARAM_ACCESS_H
#define AFE_PARAM_ACCESS_H

#include "SH367309_DataDeal.h"
#include <stddef.h>

/*
 * AFE_Parameters_RS485_Typedef is a protocol/persistent image made of 24
 * consecutive AFE_Value_Typedef members. Never walk it with `first_member + i`
 * or by casting the whole object to UINT16*: both rely on pointer arithmetic
 * outside the declared member object and are fragile under optimization.
 *
 * The layout checks make the byte-offset accessor an explicit invariant.
 */
typedef char AFE_ParamImageSizeCheck[
	(sizeof(AFE_Parameters_RS485_Typedef) ==
	 (AFE_PARAMETES_TOTAL_LENGTH * sizeof(AFE_Value_Typedef))) ? 1 : -1];
typedef char AFE_ParamImageLastOffsetCheck[
	(offsetof(AFE_Parameters_RS485_Typedef, u16CBC_DelayT) ==
	 ((AFE_PARAMETES_TOTAL_LENGTH - 1U) * sizeof(AFE_Value_Typedef))) ? 1 : -1];

#define AFE_PARAM_INDEX(member) \
	((UINT16)(offsetof(AFE_Parameters_RS485_Typedef, member) / sizeof(AFE_Value_Typedef)))
#define AFE_PARAM_TEMP_FIRST_INDEX AFE_PARAM_INDEX(u16TChgOTp)
#define AFE_PARAM_TEMP_LAST_INDEX AFE_PARAM_INDEX(u16TdischgUTp_Rcv)
#define AFE_PARAM_TEMP_MAX_ENCODED ((UINT16)1400U)

static __inline AFE_Value_Typedef *AfeParam_At(UINT16 index)
{
	return (AFE_Value_Typedef *)((UINT8 *)&AFE_Parameters_RS485_Struction +
		((UINT32)index * sizeof(AFE_Value_Typedef)));
}

static __inline const AFE_Value_Typedef *AfeParam_AtConst(UINT16 index)
{
	return (const AFE_Value_Typedef *)((const UINT8 *)&AFE_Parameters_RS485_Struction +
		((UINT32)index * sizeof(AFE_Value_Typedef)));
}

static __inline UINT8 AfeParam_ValueIsValid(UINT16 index, UINT16 value)
{
	const AFE_Value_Typedef *param;

	if (index >= AFE_PARAMETES_TOTAL_LENGTH)
	{
		return 0U;
	}

	param = AfeParam_AtConst(index);
	if ((value < param->minValue) || (value > param->maxValue))
	{
		return 0U;
	}

	/* SH367309 temperature encoding indexes iSheldTemp_10K_NTC[value / 10].
	 * The table covers encoded -40..100 C, i.e. 0..1400. Some legacy
	 * per-field maxValue entries are broader than the physical lookup table;
	 * enforce the real driver-domain bound here for every write/load path. */
	if ((index >= AFE_PARAM_TEMP_FIRST_INDEX) &&
		(index <= AFE_PARAM_TEMP_LAST_INDEX) &&
		(value > AFE_PARAM_TEMP_MAX_ENCODED))
	{
		return 0U;
	}

	return 1U;
}

#endif /* AFE_PARAM_ACCESS_H */
