#ifndef SOC_H
#define SOC_H

#include <stdint.h>

/* Retained for the existing read-only protocol table and parameter layout. */
#define SOC_TABLE_SIZE 42
enum SOC_TABLE_SELECT { SOC_TABLE_LIFEPO = 1, SOC_TABLE_TERNARYLI = 2 };

void InitData_SOC(void);
void App_SOC(void);
uint8_t SOC_SaveSnapshot(void);
void SOC_ApplyParameters(uint8_t cycle_count_written);
uint8_t SOC_RequestSetOnce(uint8_t soc);
/* 0xFFFF: unavailable; 0..0xFFFE: minutes (opposite direction returns zero). */
uint16_t SOC_GetRemainingMinutes(uint8_t charging);

#endif
