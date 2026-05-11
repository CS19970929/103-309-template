#ifndef BMS_SOC_H
#define BMS_SOC_H

#include "../include/bms_types.h"

void BmsSoc_Init(BmsState *state, const BmsConfig *config, uint8_t initial_soc_percent);
void BmsSoc_Update(BmsState *state, const BmsConfig *config, const BmsSample *sample, uint32_t dt_ms);
void BmsSoc_SetPercent(BmsState *state, const BmsConfig *config, uint8_t soc_percent);

#endif
