#include "bms_port_stm32f1_spl.h"

#include <string.h>

typedef struct {
    bms_sample_t last_sample;
    bool has_sample;
} bms_stm32f1_port_state_t;

static bms_stm32f1_port_state_t s_port;

static bool port_read_sample(void *ctx, bms_sample_t *sample)
{
    bms_stm32f1_port_state_t *port = (bms_stm32f1_port_state_t *)ctx;
    if (sample == NULL || !port->has_sample) {
        return false;
    }
    *sample = port->last_sample;
    return true;
}

static bool port_save_snapshot(void *ctx, const bms_snapshot_t *snapshot)
{
    (void)ctx;
    (void)snapshot;
    return false;
}

static bool port_load_snapshot(void *ctx, bms_snapshot_t *snapshot)
{
    (void)ctx;
    (void)snapshot;
    return false;
}

static bool port_can_send_probe(void *ctx)
{
    (void)ctx;
    return false;
}

static bool port_can_send_status(void *ctx, const bms_soc_report_t *report)
{
    (void)ctx;
    (void)report;
    return false;
}

static void port_set_charge_mos(void *ctx, bool on)
{
    (void)ctx;
    (void)on;
}

static void port_set_discharge_mos(void *ctx, bool on)
{
    (void)ctx;
    (void)on;
}

static void port_set_display(void *ctx, bool on, uint8_t soc, bool charge_icon)
{
    (void)ctx;
    (void)on;
    (void)soc;
    (void)charge_icon;
}

static void port_enter_rtc_stop(void *ctx, uint16_t seconds)
{
    (void)ctx;
    (void)seconds;
}

static void port_request_iap_reset(void *ctx)
{
    (void)ctx;
}

void bms_stm32f1_platform_init(void)
{
    memset(&s_port, 0, sizeof(s_port));
    s_port.last_sample = bms_sample_default();
    s_port.has_sample = true;
}

bms_platform_ops_t bms_stm32f1_platform_ops(void)
{
    bms_platform_ops_t ops;
    memset(&ops, 0, sizeof(ops));
    ops.ctx = &s_port;
    ops.read_sample = port_read_sample;
    ops.save_snapshot = port_save_snapshot;
    ops.load_snapshot = port_load_snapshot;
    ops.can_send_probe = port_can_send_probe;
    ops.can_send_status = port_can_send_status;
    ops.set_charge_mos = port_set_charge_mos;
    ops.set_discharge_mos = port_set_discharge_mos;
    ops.set_display = port_set_display;
    ops.enter_rtc_stop = port_enter_rtc_stop;
    ops.request_iap_reset = port_request_iap_reset;
    return ops;
}
