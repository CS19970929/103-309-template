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
    bms_protection_init(&app->protection);
    bms_ui_init(&app->ui);
    bms_iap_init(&app->iap);
}

void bms_app_process_sample(bms_app_t *app, const bms_sample_t *sample, uint32_t elapsed_ms)
{
    bms_sample_t effective_sample = *sample;
    bms_soc_report_t report;

    bms_protection_update(&app->protection, &app->config, &effective_sample);
    effective_sample.protection_fault = app->protection.active_faults != 0u;
    app->last_sample = *sample;
    app->has_sample = true;
    bms_soc_update(&app->soc, &app->config, &effective_sample, elapsed_ms);
    bms_power_update(&app->power, &app->config, &effective_sample, &app->can, elapsed_ms);
    report = bms_soc_report(&app->soc);
    bms_ui_update(&app->ui, &report, &effective_sample, elapsed_ms);
    if (app->ui.deep_sleep_request) {
        app->power.deep_request = true;
        app->power.state = BMS_POWER_DEEP_SLEEP;
    }
}

void bms_app_apply_rtc_wake(bms_app_t *app, uint32_t slept_seconds, const bms_sample_t *sample)
{
    bms_sample_t relaxed = *sample;
    bms_soc_report_t report;

    relaxed.communication_active = false;
    relaxed.ichg_a10 = 0u;
    relaxed.idsg_a10 = 0u;

    bms_can_on_rtc_wake(&app->can, &app->config, slept_seconds);
    bms_protection_update(&app->protection, &app->config, &relaxed);
    relaxed.protection_fault = app->protection.active_faults != 0u;
    bms_soc_update(&app->soc, &app->config, &relaxed, slept_seconds * 1000u);
    bms_power_update(&app->power, &app->config, &relaxed, &app->can, slept_seconds * 1000u);
    report = bms_app_report(app);
    bms_ui_update(&app->ui, &report, &relaxed, slept_seconds * 1000u);
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

void bms_app_apply_outputs(const bms_app_t *app, const bms_platform_ops_t *ops)
{
    bms_soc_report_t report;

    if (ops == NULL) {
        return;
    }

    if (ops->set_charge_mos != NULL) {
        ops->set_charge_mos(ops->ctx, app->protection.charge_mos_on);
    }
    if (ops->set_discharge_mos != NULL) {
        ops->set_discharge_mos(ops->ctx, app->protection.discharge_mos_on);
    }
    if (ops->set_display != NULL) {
        ops->set_display(ops->ctx, app->ui.display_on, app->ui.display_soc, app->ui.charge_icon_on);
    }

    report = bms_app_report(app);
    if (app->can.pending_probe_frames > 0u && ops->can_send_probe != NULL) {
        (void)ops->can_send_probe(ops->ctx);
    } else if (app->can.pending_business_frames > 0u && ops->can_send_status != NULL) {
        (void)ops->can_send_status(ops->ctx, &report);
    }

    if (app->power.rtc_request && ops->enter_rtc_stop != NULL) {
        ops->enter_rtc_stop(ops->ctx, bms_can_idle_rtc_period_seconds(&app->can, &app->config));
    }
    if (app->iap.requested && ops->request_iap_reset != NULL) {
        ops->request_iap_reset(ops->ctx);
    }
}
