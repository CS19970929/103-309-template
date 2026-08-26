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

#endif /* AFE_PARAM_ACCESS_H */
