#ifndef BMS_PARAM_SCHEMA_H
#define BMS_PARAM_SCHEMA_H

#include <stddef.h>

/* Shared defaults and range validation for persistent/protocol parameters.
 * Bounds remain next to their definitions in Fault.h / DataDeal.h. */
extern const UINT16 g_u16ProtectParamDefault[E2P_PARA_NUM_PROTECT];

extern const UINT16 g_u16OtherParamDefault[E2P_PARA_NUM_OTHER_ELEMENT1];

/* protect: 1 selects the legacy 0x2100 bank; 0 selects OtherElement. */
UINT8 BmsParam_ValueInRange(UINT8 protect, UINT16 index, UINT16 value);

/* OTHER_ELEMENT is intentionally a contiguous UINT16 protocol image. Keep
 * field-based code readable without hard-coding register offsets twice. */
#define BMS_OTHER_PARAM_WORD_INDEX(member) \
	((UINT16)(offsetof(struct OTHER_ELEMENT, member) / sizeof(UINT16)))

/* Refresh runtime values derived from persistent OtherElement parameters.
 * All parameter writers/loaders call this single owner instead of duplicating
 * SeriesNum/current-sense calculations. */
void BmsParam_ApplyRuntime(void);

#endif /* BMS_PARAM_SCHEMA_H */
