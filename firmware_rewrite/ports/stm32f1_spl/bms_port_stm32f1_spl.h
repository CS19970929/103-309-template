#ifndef BMS_PORT_STM32F1_SPL_H
#define BMS_PORT_STM32F1_SPL_H

#include "bms_app.h"

bool bms_stm32f1_board_read_sample(bms_sample_t *sample);
bool bms_stm32f1_board_save_snapshot(const bms_snapshot_t *snapshot);
bool bms_stm32f1_board_load_snapshot(bms_snapshot_t *snapshot);
bool bms_stm32f1_board_can_send_probe(void);
bool bms_stm32f1_board_can_send_status(const bms_soc_report_t *report);
void bms_stm32f1_board_set_charge_mos(bool on);
void bms_stm32f1_board_set_discharge_mos(bool on);
void bms_stm32f1_board_set_display(bool on, uint8_t soc, bool charge_icon);
void bms_stm32f1_board_enter_rtc_stop(uint16_t seconds);
void bms_stm32f1_board_request_iap_reset(void);
bool bms_stm32f1_board_wait_tick(uint32_t tick_ms);

void bms_stm32f1_platform_init(void);
bms_platform_ops_t bms_stm32f1_platform_ops(void);
bool bms_stm32f1_wait_tick(uint32_t tick_ms);

#endif
