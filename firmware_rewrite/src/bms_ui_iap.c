#include "bms_app.h"

#include <string.h>

#define BMS_UI_DISPLAY_HOLD_MS 5000u
#define BMS_UI_LONG_PRESS_MS 3000u

void bms_ui_init(bms_ui_state_t *ui)
{
    memset(ui, 0, sizeof(*ui));
}

void bms_ui_update(bms_ui_state_t *ui, const bms_soc_report_t *report, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    ui->display_soc = report->display_soc;
    ui->charge_icon_on = sample->charger_present || sample->ichg_a10 > 0u || sample->mcu_wake;
    ui->deep_sleep_request = false;

    if (sample->mcu_wake) {
        ui->display_on = true;
        ui->display_hold_ms = BMS_UI_DISPLAY_HOLD_MS;
        ui->key_hold_ms = 0u;
        return;
    }

    switch (sample->key_event) {
    case BMS_KEY_PRESSED:
        ui->display_on = true;
        ui->display_hold_ms = BMS_UI_DISPLAY_HOLD_MS;
        ui->key_hold_ms = 0u;
        break;
    case BMS_KEY_HELD:
        ui->display_on = true;
        ui->display_hold_ms = BMS_UI_DISPLAY_HOLD_MS;
        ui->key_hold_ms += elapsed_ms;
        if (ui->key_hold_ms >= BMS_UI_LONG_PRESS_MS) {
            ui->deep_sleep_request = true;
        }
        break;
    case BMS_KEY_RELEASED:
        ui->key_hold_ms = 0u;
        ui->display_on = true;
        if (ui->display_hold_ms == 0u) {
            ui->display_hold_ms = BMS_UI_DISPLAY_HOLD_MS;
        }
        break;
    case BMS_KEY_NONE:
    default:
        ui->key_hold_ms = 0u;
        break;
    }

    if (ui->display_on && sample->key_event == BMS_KEY_NONE) {
        if (ui->display_hold_ms > elapsed_ms) {
            ui->display_hold_ms -= elapsed_ms;
        } else {
            ui->display_hold_ms = 0u;
            ui->display_on = false;
        }
    }
}

void bms_iap_init(bms_iap_state_t *iap)
{
    memset(iap, 0, sizeof(*iap));
}

bool bms_iap_request(bms_iap_state_t *iap, uint16_t value)
{
    if (value != BMS_IAP_REQUEST_VALUE) {
        return false;
    }
    iap->requested = true;
    iap->request_value = value;
    return true;
}
