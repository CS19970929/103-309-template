#ifndef AFE_PARAM_ACCESS_H
#define AFE_PARAM_ACCESS_H

#include "BmsParameters.h"
#include <stddef.h>

/*
 * BMS_PARAMETERS is a protocol/persistent image made of 24
 * consecutive BMS_PARAMETER_VALUE members. Never walk it with `first_member + i`
 * or by casting the whole object to UINT16*: both rely on pointer arithmetic
 * outside the declared member object and are fragile under optimization.
 *
 * The layout checks make the byte-offset accessor an explicit invariant.
 */
typedef char AFE_ParamImageSizeCheck[
	(sizeof(BMS_PARAMETERS) ==
	 (BMS_PARAMETER_COUNT * sizeof(BMS_PARAMETER_VALUE))) ? 1 : -1];
typedef char AFE_ParamImageLastOffsetCheck[
	(offsetof(BMS_PARAMETERS, u16CBC_DelayT) ==
	 ((BMS_PARAMETER_COUNT - 1U) * sizeof(BMS_PARAMETER_VALUE))) ? 1 : -1];

#define AFE_PARAM_INDEX(member) \
	((UINT16)(offsetof(BMS_PARAMETERS, member) / sizeof(BMS_PARAMETER_VALUE)))
#define AFE_PARAM_TEMP_FIRST_INDEX AFE_PARAM_INDEX(u16TChgOTp)
#define AFE_PARAM_TEMP_LAST_INDEX AFE_PARAM_INDEX(u16TdischgUTp_Rcv)
#define AFE_PARAM_TEMP_MAX_ENCODED ((UINT16)1400U)

static __inline BMS_PARAMETER_VALUE *AfeParam_At(UINT16 index)
{
	return (BMS_PARAMETER_VALUE *)((UINT8 *)&g_bmsParameters +
		((UINT32)index * sizeof(BMS_PARAMETER_VALUE)));
}

static __inline const BMS_PARAMETER_VALUE *AfeParam_AtConst(UINT16 index)
{
	return (const BMS_PARAMETER_VALUE *)((const UINT8 *)&g_bmsParameters +
		((UINT32)index * sizeof(BMS_PARAMETER_VALUE)));
}

static __inline UINT8 AfeParam_ValueIsValid(UINT16 index, UINT16 value)
{
	const BMS_PARAMETER_VALUE *param;

	if (index >= BMS_PARAMETER_COUNT)
	{
		return 0U;
	}

	param = AfeParam_AtConst(index);
	if ((value < param->minValue) || (value > param->maxValue))
	{
		return 0U;
	}

	/* Software temperature encoding indexes iSheldTemp_10K_NTC[value / 10].
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
