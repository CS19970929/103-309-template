#include "bms_firmware.h"

bool bms_firmware_run_once(bms_app_t *app, const bms_platform_ops_t *ops, uint32_t elapsed_ms)
{
    bms_sample_t sample;

    if (app == NULL || ops == NULL || ops->read_sample == NULL || elapsed_ms == 0u) {
        return false;
    }

    sample = bms_sample_default();
    if (!ops->read_sample(ops->ctx, &sample)) {
        return false;
    }

    bms_app_process_sample(app, &sample, elapsed_ms);
    bms_app_apply_outputs(app, ops);
    return true;
}

bool bms_firmware_save_if_needed(bms_app_t *app, const bms_platform_ops_t *ops)
{
    bms_snapshot_t snapshot;

    if (app == NULL) {
        return false;
    }

    if (app->storage != NULL) {
        return bms_app_save_snapshot(app);
    }

    if (ops == NULL || ops->save_snapshot == NULL) {
        return false;
    }

    snapshot = bms_soc_make_snapshot(&app->soc, 1u);
    return ops->save_snapshot(ops->ctx, &snapshot);
}
