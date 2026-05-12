#include "bms_app.h"

#include <string.h>

void bms_app_init(bms_app_t *app, const bms_config_t *config, bms_storage_t *storage)
{
    bms_snapshot_t snapshot;
    const bms_snapshot_t *snapshot_ptr = NULL;

    memset(app, 0, sizeof(*app));
    app->config = config != NULL ? *config : bms_config_default();
    app->storage = storage;

    if (storage != NULL && bms_storage_load_latest(storage, &snapshot)) {
        snapshot_ptr = &snapshot;
    }

    bms_soc_init(&app->soc, &app->config, snapshot_ptr);
    bms_can_init(&app->can);
    bms_power_init(&app->power);
}

void bms_app_process_sample(bms_app_t *app, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    app->last_sample = *sample;
    app->has_sample = true;
    bms_soc_update(&app->soc, &app->config, sample, elapsed_ms);
    bms_power_update(&app->power, &app->config, sample, &app->can, elapsed_ms);
}

void bms_app_apply_rtc_wake(bms_app_t *app, uint32_t slept_seconds, const bms_sample_t *sample)
{
    bms_sample_t relaxed = *sample;

    relaxed.communication_active = false;
    relaxed.ichg_a10 = 0u;
    relaxed.idsg_a10 = 0u;

    bms_can_on_rtc_wake(&app->can, &app->config, slept_seconds);
    bms_soc_update(&app->soc, &app->config, &relaxed, slept_seconds * 1000u);
    bms_power_update(&app->power, &app->config, &relaxed, &app->can, slept_seconds * 1000u);
    app->last_sample = relaxed;
    app->has_sample = true;
}

bms_soc_report_t bms_app_report(const bms_app_t *app)
{
    return bms_soc_report(&app->soc);
}

bool bms_app_save_snapshot(bms_app_t *app)
{
    bms_snapshot_t latest;
    uint32_t sequence = 1u;

    if (app->storage == NULL) {
        return false;
    }

    if (bms_storage_load_latest(app->storage, &latest)) {
        sequence = latest.sequence + 1u;
    }

    latest = bms_soc_make_snapshot(&app->soc, sequence);
    return bms_storage_save(app->storage, &latest);
}
