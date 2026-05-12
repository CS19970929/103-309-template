#ifndef BMS_FIRMWARE_H
#define BMS_FIRMWARE_H

#include "bms_app.h"

bool bms_firmware_run_once(bms_app_t *app, const bms_platform_ops_t *ops, uint32_t elapsed_ms);
bool bms_firmware_save_if_needed(bms_app_t *app, const bms_platform_ops_t *ops);

#endif
