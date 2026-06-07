#ifndef LEDBAR_H
#define LEDBAR_H

#include <stdint.h>
#include "Project_Config.h"

#define LEDBAR_SOC_DISPLAY_10MS PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS

void LedBar_Init(void);
void LedBar_SetSleep(uint8_t enable);
void LedBar_SaveSleepSoc(void);
void LedBar_ShowSleepSocPreview(void);
void LedBar_RequestSocDisplay(void);
void LedBar_PrepareForStop(void);
uint8_t LedBar_IsActiveForLowPower(void);
void APP_LedBar(void);

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE
void LedBar_GetDebugSnapshot(uint8_t *sleep, uint8_t *blank,
                             uint8_t *number, uint8_t *indicators,
                             uint16_t *disp_10ms, uint8_t *frame_len,
                             uint8_t *scan_idx, uint8_t *key_active,
                             uint8_t *charge_icon, uint8_t *percent_icon);
#endif

#endif
