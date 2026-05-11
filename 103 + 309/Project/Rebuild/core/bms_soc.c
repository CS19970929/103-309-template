#include "bms_soc.h"

static int64_t s_integral_ma_ms;

static int32_t BmsSoc_ClampRemain(int32_t remain_mah, uint16_t capacity_mah)
{
    if (remain_mah < 0)
    {
        return 0;
    }

    if (remain_mah > (int32_t)capacity_mah)
    {
        return (int32_t)capacity_mah;
    }

    return remain_mah;
}

static void BmsSoc_Publish(BmsState *state, const BmsConfig *config)
{
    uint32_t permille;
    uint8_t target_percent;

    if ((state == 0) || (config == 0) || (config->capacity_mah == 0U))
    {
        return;
    }

    state->remain_mah = BmsSoc_ClampRemain(state->remain_mah, config->capacity_mah);
    permille = ((uint32_t)state->remain_mah * 1000UL) / (uint32_t)config->capacity_mah;

    if (permille > 1000UL)
    {
        permille = 1000UL;
    }

    state->soc_permille = (uint16_t)permille;
    target_percent = (uint8_t)((permille + 5UL) / 10UL);

    if (state->display_soc_percent < target_percent)
    {
        ++state->display_soc_percent;
    }
    else if (state->display_soc_percent > target_percent)
    {
        --state->display_soc_percent;
    }
}

void BmsSoc_Init(BmsState *state, const BmsConfig *config, uint8_t initial_soc_percent)
{
    if ((state == 0) || (config == 0) || (config->capacity_mah == 0U))
    {
        return;
    }

    if (initial_soc_percent > 100U)
    {
        initial_soc_percent = 100U;
    }

    s_integral_ma_ms = 0;
    state->remain_mah = ((int32_t)config->capacity_mah * (int32_t)initial_soc_percent) / 100;
    state->display_soc_percent = initial_soc_percent;
    state->soh_percent = 100U;
    BmsSoc_Publish(state, config);
}

void BmsSoc_SetPercent(BmsState *state, const BmsConfig *config, uint8_t soc_percent)
{
    BmsSoc_Init(state, config, soc_percent);
}

void BmsSoc_Update(BmsState *state, const BmsConfig *config, const BmsSample *sample, uint32_t dt_ms)
{
    int64_t delta_mah;

    if ((state == 0) || (config == 0) || (sample == 0) || (config->capacity_mah == 0U))
    {
        return;
    }

    s_integral_ma_ms += ((int64_t)sample->current_ma * (int64_t)dt_ms);
    delta_mah = s_integral_ma_ms / 3600000LL;
    s_integral_ma_ms -= (delta_mah * 3600000LL);

    state->remain_mah += (int32_t)delta_mah;

    if ((sample->charger_present != false) && (sample->vcell_min_mv >= config->soc_full_mv))
    {
        state->remain_mah = (int32_t)config->capacity_mah;
    }
    else if (sample->vcell_min_mv <= config->soc_empty_mv)
    {
        state->remain_mah = 0;
    }

    BmsSoc_Publish(state, config);
}
