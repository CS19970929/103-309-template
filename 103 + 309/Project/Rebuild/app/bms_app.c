#include "bms_app.h"
#include "../core/bms_power.h"
#include "../core/bms_soc.h"

static BmsSnapshot s_bms;
static uint32_t s_idle_seconds;

void BmsApp_Init(void)
{
    s_bms.config.cell_count = 10U;
    s_bms.config.capacity_mah = 27000U;
    s_bms.config.soc_full_mv = 4180U;
    s_bms.config.soc_empty_mv = 3000U;
    s_bms.config.sleep_idle_seconds = 60U;
    s_bms.config.rtc_period_seconds = 30U;
    s_bms.config.current_idle_ma = 100U;

    s_bms.state.charge_allowed = true;
    s_bms.state.discharge_allowed = true;

    BmsPower_Init(&s_bms.state);
    BmsSoc_Init(&s_bms.state, &s_bms.config, 60U);
}

void BmsApp_Task10ms(void)
{
}

void BmsApp_Task200ms(const BmsSample *sample)
{
    if (sample == 0)
    {
        return;
    }

    s_bms.sample = *sample;
    BmsSoc_Update(&s_bms.state, &s_bms.config, &s_bms.sample, 200U);
}

void BmsApp_Task1000ms(void)
{
    if ((s_bms.sample.charger_present == false) && (s_bms.sample.load_present == false))
    {
        ++s_idle_seconds;
    }
    else
    {
        s_idle_seconds = 0U;
    }

    BmsPower_Evaluate(&s_bms.state, &s_bms.config, &s_bms.sample, s_idle_seconds);
}

const BmsSnapshot *BmsApp_GetSnapshot(void)
{
    return &s_bms;
}
