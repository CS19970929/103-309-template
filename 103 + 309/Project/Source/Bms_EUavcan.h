#ifndef BMS_EUAVCAN_H
#define BMS_EUAVCAN_H

#include "stm32f10x.h"

#define BMS_EUAVCAN_SOURCE_NODE_ID        ((UINT8)0x16)
#define BMS_EUAVCAN_MESSAGE_TYPE_ID       ((UINT16)0x1092)
#define BMS_EUAVCAN_SERVICE_MESSAGE_BIT   ((UINT32)0)

/* PDF red text gives frame id "1109216"; treat it as hex 0x01109216. */
#define BMS_EUAVCAN_PRIORITY              ((UINT32)1)

#define BMS_EUAVCAN_CAN_ID \
	((((UINT32)BMS_EUAVCAN_PRIORITY & 0x1FU) << 24) | \
	 (((UINT32)BMS_EUAVCAN_MESSAGE_TYPE_ID) << 8) | \
	 ((BMS_EUAVCAN_SERVICE_MESSAGE_BIT & 0x01U) << 7) | \
	 ((UINT32)BMS_EUAVCAN_SOURCE_NODE_ID & 0x7FU))

#define BMS_EUAVCAN_MANUFACTURER_ID       ((UINT16)0)
#define BMS_EUAVCAN_BATTERY_MODEL_ID      ((UINT16)0)

/* PDF marks both capacity fields as 0. Set to 1 after confirming mAh reporting. */
#define BMS_EUAVCAN_REPORT_CAPACITY_MAH   0

void InitBmsEUavcan(void);
void App_BmsEUavcan(void);

#endif
