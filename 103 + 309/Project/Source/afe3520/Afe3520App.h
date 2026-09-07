#ifndef AFE3520_APP_H
#define AFE3520_APP_H

#include "afe3520/Afe3520.h"

/* Application units preserve the existing telemetry and current calibration. */
struct Afe3520Measurements
{
    UINT16 u16VCell[AFE3520_CELL_MAX];
    UINT16 u16TempBat[AFE3520_TEMP_MAX];
    UINT16 u16Current; /* Calibrated-pipeline code, not the native CADC register. */
};
extern struct Afe3520Measurements g_afe3520Measurements;

void Afe3520_AppInit(void);
void Afe3520_RestorePort(void);
UINT8 Afe3520_UpdateMeasurements(void); /* 0=success, 1=failure */
UINT8 Afe3520_ReadCalibratedCurrentCode(UINT16 *code); /* 1=success */

#endif
