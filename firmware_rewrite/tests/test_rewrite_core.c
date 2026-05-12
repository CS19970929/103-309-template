#include "bms_app.h"
#include "bms_firmware.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    bool charge_mos;
    bool discharge_mos;
    bool display_on;
    bool charge_icon;
    bool probe_sent;
    bool status_sent;
    bool rtc_entered;
    bool iap_reset;
    uint8_t display_soc;
    uint16_t rtc_seconds;
    bms_sample_t sample;
} mock_outputs_t;

static bool mock_read_sample(void *ctx, bms_sample_t *sample)
{
    *sample = ((mock_outputs_t *)ctx)->sample;
    return true;
}

static void mock_set_charge_mos(void *ctx, bool on)
{
    ((mock_outputs_t *)ctx)->charge_mos = on;
}

static void mock_set_discharge_mos(void *ctx, bool on)
{
    ((mock_outputs_t *)ctx)->discharge_mos = on;
}

static void mock_set_display(void *ctx, bool on, uint8_t soc, bool charge_icon)
{
    mock_outputs_t *outputs = (mock_outputs_t *)ctx;
    outputs->display_on = on;
    outputs->display_soc = soc;
    outputs->charge_icon = charge_icon;
}

static bool mock_can_probe(void *ctx)
{
    ((mock_outputs_t *)ctx)->probe_sent = true;
    return true;
}

static bool mock_can_status(void *ctx, const bms_soc_report_t *report)
{
    (void)report;
    ((mock_outputs_t *)ctx)->status_sent = true;
    return true;
}

static void mock_enter_rtc(void *ctx, uint16_t seconds)
{
    mock_outputs_t *outputs = (mock_outputs_t *)ctx;
    outputs->rtc_entered = true;
    outputs->rtc_seconds = seconds;
}

static void mock_iap_reset(void *ctx)
{
    ((mock_outputs_t *)ctx)->iap_reset = true;
}

static void require_true(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void run_for(bms_app_t *app, bms_sample_t sample, uint32_t total_ms, uint32_t step_ms)
{
    uint32_t elapsed = 0u;
    while (elapsed < total_ms) {
        bms_app_process_sample(app, &sample, step_ms);
        elapsed += step_ms;
    }
}

static bms_sample_t sample_with_voltage(uint16_t min_mv, uint16_t max_mv)
{
    bms_sample_t sample = bms_sample_default();
    uint8_t i;

    sample.vcell_min_mv = min_mv;
    sample.vcell_max_mv = max_mv;
    sample.vcell_delta_mv = (uint16_t)(max_mv - min_mv);
    sample.pack_mv = (uint16_t)(((uint32_t)min_mv * sample.cell_count + (uint32_t)sample.vcell_delta_mv) / 1u);
    for (i = 0u; i < sample.cell_count; ++i) {
        sample.vcell[i] = min_mv;
    }
    sample.vcell[0] = max_mv;
    return sample;
}

static void test_set_once_and_d000(void)
{
    bms_storage_t storage;
    bms_app_t app;
    bms_sample_t sample = sample_with_voltage(3700u, 3710u);
    uint16_t words[BMS_RO_D000_WORDS];

    bms_storage_init(&storage);
    bms_app_init(&app, NULL, &storage);
    require_true(bms_comm_write_single(&app, BMS_ADDR_SET_ONCE_SOC, 80u), "0x1005 set once SOC");
    bms_app_process_sample(&app, &sample, 200u);
    require_true(bms_comm_read_d000(&app, words, BMS_RO_D000_WORDS), "read 0xD000 status");
    require_true(words[52] == 80u, "0xD000 offset 52 reports display SOC");
    require_true(words[53] == 100u, "0xD000 offset 53 reports SOH");
    require_true(words[55] == 2700u, "0xD000 offset 55 reports full capacity Ah*100");
}

static void test_full_reaches_100(void)
{
    bms_app_t app;
    bms_sample_t sample = sample_with_voltage(4160u, 4170u);

    bms_app_init(&app, NULL, NULL);
    require_true(bms_comm_write_single(&app, BMS_ADDR_SET_ONCE_SOC, 96u), "seed SOC 96");
    sample.ichg_a10 = 20u;
    run_for(&app, sample, 26000u, 200u);
    require_true(bms_app_report(&app).soc == 100u, "full voltage anchor reaches internal 100");
    require_true(bms_app_report(&app).display_soc == 100u, "full voltage anchor reaches display 100");
    require_true(bms_app_report(&app).full_anchor, "full anchor flag set");
}

static void test_low_voltage_reaches_0(void)
{
    bms_app_t app;
    bms_sample_t sample = sample_with_voltage(2950u, 2960u);

    bms_app_init(&app, NULL, NULL);
    require_true(bms_comm_write_single(&app, BMS_ADDR_SET_ONCE_SOC, 40u), "seed SOC 40");
    sample.idsg_a10 = 100u;
    run_for(&app, sample, 10000u, 200u);
    require_true(bms_app_report(&app).soc == 0u, "low voltage table converges to internal 0");
    require_true(bms_app_report(&app).display_soc == 0u, "low voltage table converges display to 0");
}

static void test_rest_ocv_no_jump_over_one(void)
{
    bms_app_t app;
    bms_sample_t sample = sample_with_voltage(3725u, 3735u);

    bms_app_init(&app, NULL, NULL);
    require_true(bms_comm_write_single(&app, BMS_ADDR_SET_ONCE_SOC, 80u), "seed SOC 80");
    bms_app_process_sample(&app, &sample, 600000u);
    bms_app_process_sample(&app, &sample, 600000u);
    require_true(bms_app_report(&app).soc == 80u, "stable rest records target without immediate jump");
    require_true(bms_app_report(&app).deferred_ocv_valid, "stable rest has deferred target");
    bms_app_process_sample(&app, &sample, 1800000u);
    require_true(bms_app_report(&app).soc == 79u, "long rest correction moves only one percent per update");
}

static void test_sag_hold_blocks_wide_low_table(void)
{
    bms_app_t app;
    bms_sample_t sample = sample_with_voltage(3300u, 3320u);

    bms_app_init(&app, NULL, NULL);
    require_true(bms_comm_write_single(&app, BMS_ADDR_SET_ONCE_SOC, 50u), "seed SOC 50");
    sample.idsg_a10 = 200u;
    run_for(&app, sample, 30000u, 200u);
    require_true(bms_app_report(&app).soc >= 49u, "heavy sag hold blocks wide low-voltage calibration");
}

static void test_can_idle_strategy(void)
{
    bms_app_t app;
    uint8_t i;

    bms_app_init(&app, NULL, NULL);
    require_true(bms_can_idle_rtc_period_seconds(&app.can, &app.config) == 1u, "CAN starts as active bus");
    for (i = 0u; i < 6u; ++i) {
        bms_can_finish_power_window(&app.can, false);
    }
    require_true(!app.can.bus_active, "CAN becomes inactive after repeated no-ACK windows");
    require_true(bms_can_idle_rtc_period_seconds(&app.can, &app.config) == 10u, "inactive CAN uses 10s RTC period");
    bms_can_on_rtc_wake(&app.can, &app.config, 10u);
    require_true(app.can.pending_probe_frames == 2u, "inactive RTC wake schedules probe frames");
    bms_can_finish_power_window(&app.can, true);
    require_true(app.can.bus_active, "ACK restores active CAN bus");
}

static void test_storage_dual_slot(void)
{
    bms_storage_t storage;
    bms_app_t app;
    bms_app_t restored;

    bms_storage_init(&storage);
    bms_app_init(&app, NULL, &storage);
    require_true(bms_comm_write_single(&app, BMS_ADDR_SET_ONCE_SOC, 73u), "seed snapshot SOC");
    require_true(bms_app_save_snapshot(&app), "save snapshot");
    bms_app_init(&restored, NULL, &storage);
    require_true(bms_app_report(&restored).soc == 73u, "restore SOC from storage");
}

static void test_protection_and_outputs(void)
{
    bms_app_t app;
    bms_sample_t sample = sample_with_voltage(2700u, 2710u);
    uint16_t words[BMS_RO_D000_WORDS];
    mock_outputs_t outputs = {0};
    bms_platform_ops_t ops = {0};

    ops.ctx = &outputs;
    ops.set_charge_mos = mock_set_charge_mos;
    ops.set_discharge_mos = mock_set_discharge_mos;
    ops.set_display = mock_set_display;

    bms_app_init(&app, NULL, NULL);
    bms_app_process_sample(&app, &sample, 200u);
    require_true((app.protection.active_faults & BMS_FAULT_CELL_UVP) != 0u, "UVP fault is latched");
    require_true(!app.protection.discharge_mos_on, "UVP turns discharge MOS off");
    require_true(bms_comm_read_d000(&app, words, BMS_RO_D000_WORDS), "D000 readable with fault");
    require_true((words[58] & BMS_FAULT_CELL_UVP) != 0u, "D000 exposes fault low word");

    bms_app_apply_outputs(&app, &ops);
    require_true(outputs.charge_mos, "charge MOS remains on for UVP");
    require_true(!outputs.discharge_mos, "platform output turns discharge MOS off");
}

static void test_ui_key_and_mcu_wake(void)
{
    bms_app_t app;
    bms_sample_t sample = sample_with_voltage(3700u, 3710u);

    bms_app_init(&app, NULL, NULL);
    require_true(bms_comm_write_single(&app, BMS_ADDR_SET_ONCE_SOC, 66u), "seed UI SOC");
    sample.key_event = BMS_KEY_PRESSED;
    bms_app_process_sample(&app, &sample, 200u);
    require_true(app.ui.display_on, "short press turns display on");
    require_true(app.ui.display_soc == 66u, "display uses current SOC");

    sample.key_event = BMS_KEY_NONE;
    run_for(&app, sample, 5200u, 200u);
    require_true(!app.ui.display_on, "display turns off after hold window");

    sample.mcu_wake = true;
    bms_app_process_sample(&app, &sample, 200u);
    require_true(app.ui.display_on, "MCU_WK keeps display on");
    require_true(app.ui.charge_icon_on, "MCU_WK shows charge icon");

    sample.mcu_wake = false;
    sample.key_event = BMS_KEY_HELD;
    run_for(&app, sample, 3200u, 200u);
    require_true(app.power.deep_request, "long press requests deep sleep");
}

static void test_iap_and_rtc_outputs(void)
{
    bms_app_t app;
    bms_sample_t sample = sample_with_voltage(3700u, 3710u);
    mock_outputs_t outputs = {0};
    bms_platform_ops_t ops = {0};

    ops.ctx = &outputs;
    ops.can_send_probe = mock_can_probe;
    ops.can_send_status = mock_can_status;
    ops.enter_rtc_stop = mock_enter_rtc;
    ops.request_iap_reset = mock_iap_reset;

    bms_app_init(&app, NULL, NULL);
    run_for(&app, sample, 10200u, 200u);
    require_true(app.power.rtc_request, "idle system requests RTC");
    bms_app_apply_outputs(&app, &ops);
    require_true(outputs.rtc_entered && outputs.rtc_seconds == 1u, "platform enters 1s RTC when CAN active");

    outputs.rtc_entered = false;
    bms_can_finish_power_window(&app.can, false);
    bms_can_finish_power_window(&app.can, false);
    bms_can_finish_power_window(&app.can, false);
    bms_can_finish_power_window(&app.can, false);
    bms_can_finish_power_window(&app.can, false);
    bms_can_finish_power_window(&app.can, false);
    bms_app_apply_rtc_wake(&app, 10u, &sample);
    bms_app_apply_outputs(&app, &ops);
    require_true(outputs.probe_sent, "inactive CAN sends probe through platform");

    require_true(bms_comm_write_single(&app, BMS_ADDR_IAP_CONNECT, BMS_IAP_REQUEST_VALUE), "IAP command accepted");
    bms_app_apply_outputs(&app, &ops);
    require_true(outputs.iap_reset, "IAP output callback requested");
}

static void test_firmware_run_once_entry(void)
{
    bms_app_t app;
    mock_outputs_t outputs = {0};
    bms_platform_ops_t ops = {0};

    outputs.sample = sample_with_voltage(3700u, 3710u);
    outputs.sample.key_event = BMS_KEY_PRESSED;
    ops.ctx = &outputs;
    ops.read_sample = mock_read_sample;
    ops.set_charge_mos = mock_set_charge_mos;
    ops.set_discharge_mos = mock_set_discharge_mos;
    ops.set_display = mock_set_display;

    bms_app_init(&app, NULL, NULL);
    require_true(bms_comm_write_single(&app, BMS_ADDR_SET_ONCE_SOC, 55u), "seed firmware entry SOC");
    require_true(bms_firmware_run_once(&app, &ops, 200u), "firmware run once processes sample");
    require_true(outputs.display_on && outputs.display_soc == 55u, "firmware entry applies display output");
    require_true(outputs.charge_mos && outputs.discharge_mos, "firmware entry applies MOS outputs");
}

int main(void)
{
    test_set_once_and_d000();
    test_full_reaches_100();
    test_low_voltage_reaches_0();
    test_rest_ocv_no_jump_over_one();
    test_sag_hold_blocks_wide_low_table();
    test_can_idle_strategy();
    test_storage_dual_slot();
    test_protection_and_outputs();
    test_ui_key_and_mcu_wake();
    test_iap_and_rtc_outputs();
    test_firmware_run_once_entry();
    puts("rewrite host tests passed");
    return 0;
}
