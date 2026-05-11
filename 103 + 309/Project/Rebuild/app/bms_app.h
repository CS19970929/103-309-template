#ifndef BMS_APP_H
#define BMS_APP_H

#include "../include/bms_types.h"

void BmsApp_Init(void);
void BmsApp_Task10ms(void);
void BmsApp_Task200ms(const BmsSample *sample);
void BmsApp_Task1000ms(void);
const BmsSnapshot *BmsApp_GetSnapshot(void);

#endif
