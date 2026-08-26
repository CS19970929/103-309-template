#ifndef BMS_PARAM_SCHEMA_H
#define BMS_PARAM_SCHEMA_H

#include <stddef.h>

/*
 * One read-only schema for persistent/protocol parameters.
 *
 * The initializer values still live beside the parameter definitions in
 * Fault.h/DataDeal.h, but only one ROM copy is instantiated by EEPROM.c.
 * Protocol code consumes these arrays directly instead of creating private
 * min/default/max struct copies on the stack or in .constdata.
 */
extern const UINT16 g_u16ProtectParamMin[E2P_PARA_NUM_PROTECT];
extern const UINT16 g_u16ProtectParamDefault[E2P_PARA_NUM_PROTECT];
extern const UINT16 g_u16ProtectParamMax[E2P_PARA_NUM_PROTECT];

extern const UINT16 g_u16OtherParamMin[E2P_PARA_NUM_OTHER_ELEMENT1];
extern const UINT16 g_u16OtherParamDefault[E2P_PARA_NUM_OTHER_ELEMENT1];
extern const UINT16 g_u16OtherParamMax[E2P_PARA_NUM_OTHER_ELEMENT1];

/* OTHER_ELEMENT is intentionally a contiguous UINT16 protocol image. Keep
 * field-based code readable without hard-coding register offsets twice. */
#define BMS_OTHER_PARAM_WORD_INDEX(member) \
	((UINT16)(offsetof(struct OTHER_ELEMENT, member) / sizeof(UINT16)))

/* Refresh runtime values derived from persistent OtherElement parameters.
 * All parameter writers/loaders call this single owner instead of duplicating
 * SeriesNum/current-sense calculations. */
void BmsParam_ApplyRuntime(void);

#endif /* BMS_PARAM_SCHEMA_H */
