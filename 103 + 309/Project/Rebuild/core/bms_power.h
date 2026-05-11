#ifndef BMS_POWER_H
#define BMS_POWER_H

#include "../include/bms_types.h"

void BmsPower_Init(BmsState *state);
void BmsPower_Request(BmsState *state, BmsSleepMode mode);
void BmsPower_ClearRequest(BmsState *state);
void BmsPower_Evaluate(BmsState *state, const BmsConfig *config, const BmsSample *sample, uint32_t idle_seconds);
void BmsPower_HandleWakeup(BmsState *state, BmsWakeReason reason);

#endif
