#ifndef BMS_PORT_STM32F1_SPL_H
#define BMS_PORT_STM32F1_SPL_H

#include "bms_app.h"

void bms_stm32f1_platform_init(void);
bms_platform_ops_t bms_stm32f1_platform_ops(void);

#endif
