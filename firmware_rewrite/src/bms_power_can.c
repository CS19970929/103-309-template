#include "bms_app.h"

#include <string.h>

#define BMS_CAN_NO_ACK_INACTIVE_LIMIT 6u
#define BMS_CAN_RTC_BUSINESS_FRAMES 2u
#define BMS_CAN_RTC_PROBE_FRAMES 2u
#define BMS_POWER_DEEP_LOW_CELL_MV 2600u
#define BMS_POWER_DEEP_CONFIRM_MS 60000u

void bms_can_init(bms_can_state_t *can)
{
    memset(can, 0, sizeof(*can));
    can->bus_active = true;
}

uint16_t bms_can_idle_rtc_period_seconds(const bms_can_state_t *can, const bms_config_t *config)
{
    return can->bus_active ? config->rtc_idle_with_can_s : config->rtc_idle_without_can_s;
}

void bms_can_prepare_sleep(bms_can_state_t *can)
{
    can->pending_business_frames = 0u;
    can->pending_probe_frames = 0u;
}

void bms_can_on_rx(bms_can_state_t *can)
{
    can->bus_active = true;
    can->no_ack_windows = 0u;
}

void bms_can_finish_power_window(bms_can_state_t *can, bool any_tx_ack)
{
    can->pending_business_frames = 0u;
    can->pending_probe_frames = 0u;

    if (any_tx_ack) {
        bms_can_on_rx(can);
        return;
    }

    if (can->no_ack_windows < 255u) {
        can->no_ack_windows += 1u;
    }

    if (can->no_ack_windows >= BMS_CAN_NO_ACK_INACTIVE_LIMIT) {
        can->bus_active = false;
    }
}

void bms_can_on_rtc_wake(bms_can_state_t *can, const bms_config_t *config, uint32_t slept_seconds)
{
    (void)config;
    can->logic_ms += slept_seconds * 1000u;
    if (can->bus_active) {
        can->pending_business_frames = BMS_CAN_RTC_BUSINESS_FRAMES;
        can->pending_probe_frames = 0u;
    } else {
        can->pending_business_frames = 0u;
        can->pending_probe_frames = BMS_CAN_RTC_PROBE_FRAMES;
    }
}

void bms_power_init(bms_power_state_t *power)
{
    memset(power, 0, sizeof(*power));
    power->state = BMS_POWER_ACTIVE;
}

void bms_power_update(bms_power_state_t *power, const bms_config_t *config, const bms_sample_t *sample, const bms_can_state_t *can, uint32_t elapsed_ms)
{
    const bool has_current = sample->ichg_a10 >= config->current_deadband_a10 ||
                             sample->idsg_a10 >= config->current_deadband_a10;
    const bool active_blocker = has_current ||
                                sample->communication_active ||
                                sample->mcu_wake ||
                                sample->protection_fault ||
                                can->pending_business_frames > 0u ||
                                can->pending_probe_frames > 0u;

    power->rtc_request = false;
    power->deep_request = false;

    if (sample->vcell_min_mv > 0u &&
        sample->vcell_min_mv <= BMS_POWER_DEEP_LOW_CELL_MV &&
        sample->ichg_a10 < config->current_deadband_a10) {
        power->low_voltage_ms += elapsed_ms;
        if (power->low_voltage_ms >= BMS_POWER_DEEP_CONFIRM_MS) {
            power->deep_request = true;
            power->state = BMS_POWER_DEEP_SLEEP;
            return;
        }
    } else {
        power->low_voltage_ms = 0u;
    }

    if (active_blocker) {
        power->idle_ms = 0u;
        power->state = BMS_POWER_ACTIVE;
        return;
    }

    power->idle_ms += elapsed_ms;
    if (power->idle_ms >= (uint32_t)config->idle_before_rtc_s * 1000u) {
        power->rtc_request = true;
        power->state = BMS_POWER_RTC_HICCUP;
    } else {
        power->state = BMS_POWER_ACTIVE;
    }
}
