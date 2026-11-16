#ifndef SOC_H
#define SOC_H

#include <stdint.h>

/* Retained for the existing read-only protocol table and parameter layout. */
#define SOC_TABLE_SIZE 42
enum SOC_TABLE_SELECT { SOC_TABLE_LIFEPO = 1, SOC_TABLE_TERNARYLI = 2 };

void InitData_SOC(void);
void App_SOC(uint8_t sample_valid, uint8_t fault_valid);
void SOC_PauseCalibration(void);
void SOC_ResetVoltageCalibration(void);
/* Records a target only; never changes capacity or waits before protective sleep. */
/* Call only after a fresh, plausible UV frame is confirmed by SleepDeal. */
void SOC_NotifyOverdischarge(void);
uint8_t SOC_CellVoltagesValid(void);
uint8_t SOC_HasPendingFull(void);
uint8_t SOC_SaveSnapshot(void);
void SOC_ApplyParameters(uint8_t cycle_count_written);
/* Persist a smooth target; success never means an immediate capacity change. */
uint8_t SOC_RequestSetOnce(uint8_t soc);
/* 0xFFFF: unavailable; 0..0xFFFE: minutes (opposite direction returns zero). */
uint16_t SOC_GetRemainingMinutes(uint8_t charging);

#endif
