#include "bms_power.h"

static BmsSleepMode s_requested_sleep = BMS_SLEEP_NONE;

void BmsPower_Init(BmsState *state)
{
    s_requested_sleep = BMS_SLEEP_NONE;

    if (state != 0)
    {
        state->power_state = BMS_POWER_RUN;
        state->wake_reason = BMS_WAKE_RESET;
    }
}

void BmsPower_Request(BmsState *state, BmsSleepMode mode)
{
    s_requested_sleep = mode;

    if ((state != 0) && (mode == BMS_SLEEP_DEEP))
    {
        state->power_state = BMS_POWER_DEEP_PENDING;
    }
}

void BmsPower_ClearRequest(BmsState *state)
{
    s_requested_sleep = BMS_SLEEP_NONE;

    if (state != 0)
    {
        state->power_state = BMS_POWER_RUN;
    }
}

void BmsPower_Evaluate(BmsState *state, const BmsConfig *config, const BmsSample *sample, uint32_t idle_seconds)
{
    uint32_t idle_ma;

    if ((state == 0) || (config == 0) || (sample == 0))
    {
        return;
    }

    idle_ma = (sample->current_ma < 0) ? (uint32_t)(0LL - (int64_t)sample->current_ma) : (uint32_t)sample->current_ma;

    if ((state->faults != 0UL) || (sample->charger_present != false) || (sample->load_present != false))
    {
        BmsPower_ClearRequest(state);
        return;
    }

    if (idle_ma > (uint32_t)config->current_idle_ma)
    {
        BmsPower_ClearRequest(state);
        return;
    }

    if (s_requested_sleep == BMS_SLEEP_DEEP)
    {
        state->power_state = BMS_POWER_DEEP_PENDING;
        return;
    }

    if ((s_requested_sleep == BMS_SLEEP_RTC_IDLE) || (idle_seconds >= (uint32_t)config->sleep_idle_seconds))
    {
        state->power_state = BMS_POWER_RTC_IDLE;
    }
    else
    {
        state->power_state = BMS_POWER_RUN;
    }
}

void BmsPower_HandleWakeup(BmsState *state, BmsWakeReason reason)
{
    if (state == 0)
    {
        return;
    }

    state->wake_reason = reason;
    state->power_state = BMS_POWER_RUN;
    s_requested_sleep = BMS_SLEEP_NONE;
}
