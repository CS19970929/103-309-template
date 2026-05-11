#ifndef BMS_REGISTERS_H
#define BMS_REGISTERS_H

#include <stdint.h>

#define BMS_REG_RUNTIME_STATUS_BASE      ((uint16_t)0xD000U)
#define BMS_REG_SOC_TEST_STATUS_BASE     ((uint16_t)0xD300U)
#define BMS_REG_CMD_SET_SOC_ONCE         ((uint16_t)0x1005U)
#define BMS_REG_SOC_TABLE_BASE           ((uint16_t)0x2200U)
#define BMS_REG_OTHER_CAPACITY           ((uint16_t)0x2318U)
#define BMS_REG_OTHER_CYCLE              ((uint16_t)0x2319U)
#define BMS_REG_OTHER_SOC_V100           ((uint16_t)0x231AU)
#define BMS_REG_OTHER_SOC_V0             ((uint16_t)0x231BU)
#define BMS_REG_AFE_PARAM_BASE           ((uint16_t)0x2400U)

#endif
