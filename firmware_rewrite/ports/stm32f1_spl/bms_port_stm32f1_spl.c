#include "bms_port_stm32f1_spl.h"

#include <string.h>

#if defined(__CC_ARM)
#define BMS_WEAK __weak
#elif defined(__GNUC__)
#define BMS_WEAK __attribute__((weak))
#else
#define BMS_WEAK
#endif

typedef struct {
    bms_sample_t last_sample;
    bool has_sample;
} bms_stm32f1_port_state_t;

static bms_stm32f1_port_state_t s_port;

BMS_WEAK bool bms_stm32f1_board_read_sample(bms_sample_t *sample)
{
    (void)sample;
    return false;
}

BMS_WEAK bool bms_stm32f1_board_save_snapshot(const bms_snapshot_t *snapshot)
{
    (void)snapshot;
    return false;
}

BMS_WEAK bool bms_stm32f1_board_load_snapshot(bms_snapshot_t *snapshot)
{
    (void)snapshot;
    return false;
}

BMS_WEAK bool bms_stm32f1_board_can_send_probe(void)
{
    return false;
}

BMS_WEAK bool bms_stm32f1_board_can_send_status(const bms_soc_report_t *report)
{
    (void)report;
    return false;
}

BMS_WEAK void bms_stm32f1_board_set_charge_mos(bool on)
{
    (void)on;
}

BMS_WEAK void bms_stm32f1_board_set_discharge_mos(bool on)
{
    (void)on;
}

BMS_WEAK void bms_stm32f1_board_set_display(bool on, uint8_t soc, bool charge_icon)
{
    (void)on;
    (void)soc;
    (void)charge_icon;
}

BMS_WEAK void bms_stm32f1_board_enter_rtc_stop(uint16_t seconds)
{
    (void)seconds;
}

BMS_WEAK void bms_stm32f1_board_request_iap_reset(void)
{
}

BMS_WEAK bool bms_stm32f1_board_wait_tick(uint32_t tick_ms)
{
    (void)tick_ms;
    return true;
}

static bool port_read_sample(void *ctx, bms_sample_t *sample)
{
    bms_stm32f1_port_state_t *port = (bms_stm32f1_port_state_t *)ctx;

    if (sample == NULL) {
        return false;
    }

    if (bms_stm32f1_board_read_sample(sample)) {
        port->last_sample = *sample;
        port->has_sample = true;
        return true;
    }

    if (!port->has_sample) {
        return false;
    }

    *sample = port->last_sample;
    return true;
}

static bool port_save_snapshot(void *ctx, const bms_snapshot_t *snapshot)
{
    (void)ctx;
    return bms_stm32f1_board_save_snapshot(snapshot);
}

static bool port_load_snapshot(void *ctx, bms_snapshot_t *snapshot)
{
    (void)ctx;
    return bms_stm32f1_board_load_snapshot(snapshot);
}

static bool port_can_send_probe(void *ctx)
{
    (void)ctx;
    return bms_stm32f1_board_can_send_probe();
}

static bool port_can_send_status(void *ctx, const bms_soc_report_t *report)
{
    (void)ctx;
    return bms_stm32f1_board_can_send_status(report);
}

static void port_set_charge_mos(void *ctx, bool on)
{
    (void)ctx;
    bms_stm32f1_board_set_charge_mos(on);
}

static void port_set_discharge_mos(void *ctx, bool on)
{
    (void)ctx;
    bms_stm32f1_board_set_discharge_mos(on);
}

static void port_set_display(void *ctx, bool on, uint8_t soc, bool charge_icon)
{
    (void)ctx;
    bms_stm32f1_board_set_display(on, soc, charge_icon);
}

static void port_enter_rtc_stop(void *ctx, uint16_t seconds)
{
    (void)ctx;
    bms_stm32f1_board_enter_rtc_stop(seconds);
}

static void port_request_iap_reset(void *ctx)
{
    (void)ctx;
    bms_stm32f1_board_request_iap_reset();
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

bool bms_stm32f1_wait_tick(uint32_t tick_ms)
{
    return bms_stm32f1_board_wait_tick(tick_ms);
}
