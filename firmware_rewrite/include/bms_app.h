#ifndef BMS_APP_H
#define BMS_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BMS_MAX_CELLS 32u
#define BMS_SOC_TABLE_POINTS 21u
#define BMS_RO_D000_WORDS 63u
#define BMS_RO_D300_WORDS 16u

#define BMS_ADDR_SET_ONCE_SOC 0x1005u
#define BMS_ADDR_SOC_TABLE_START 0x2200u
#define BMS_ADDR_SOC_TABLE_END 0x2229u
#define BMS_ADDR_SOC_PARAM_START 0x2318u
#define BMS_ADDR_SOC_PARAM_END 0x231Bu
#define BMS_ADDR_SOC_TEST_STATUS 0xD300u
#define BMS_ADDR_IAP_CONNECT 0xFFFDu
#define BMS_IAP_REQUEST_VALUE 0x00ABu
#define BMS_SOC_TEST_UNSUPPORTED 1u

#define BMS_FLASH_IAP_START 0x08000000ul
#define BMS_FLASH_APP_START 0x08004800ul
#define BMS_FLASH_STORAGE_START 0x0801C000ul
#define BMS_FLASH_SOC_SLOT_A 0x0801E000ul
#define BMS_FLASH_SOC_SLOT_B 0x0801E800ul

#define BMS_FAULT_CELL_OVP 0x00000001ul
#define BMS_FAULT_CELL_UVP 0x00000002ul
#define BMS_FAULT_CHG_OCP 0x00000004ul
#define BMS_FAULT_DSG_OCP 0x00000008ul
#define BMS_FAULT_TEMP_HIGH 0x00000010ul
#define BMS_FAULT_VOLTAGE_INVALID 0x00000020ul

typedef enum {
    BMS_SOC_MODE_RELAX = 0,
    BMS_SOC_MODE_CHARGE,
    BMS_SOC_MODE_DISCHARGE
} bms_soc_mode_t;

typedef enum {
    BMS_POWER_ACTIVE = 0,
    BMS_POWER_RTC_HICCUP,
    BMS_POWER_DEEP_SLEEP
} bms_power_state_id_t;

typedef enum {
    BMS_KEY_NONE = 0,
    BMS_KEY_PRESSED,
    BMS_KEY_RELEASED,
    BMS_KEY_HELD
} bms_key_event_t;

typedef struct {
    uint16_t voltage_mv;
    uint8_t soc_percent;
} bms_soc_table_point_t;

typedef struct {
    uint16_t capacity_ah10;
    uint16_t cycle_count;
    uint16_t full_cell_mv;
    uint16_t empty_cell_mv;
    uint16_t cell_count;
    uint16_t current_deadband_a10;
    uint16_t rtc_idle_with_can_s;
    uint16_t rtc_idle_without_can_s;
    uint16_t idle_before_rtc_s;
    uint16_t cell_ovp_mv;
    uint16_t cell_ovp_release_mv;
    uint16_t cell_uvp_mv;
    uint16_t cell_uvp_release_mv;
    uint16_t charge_ocp_a10;
    uint16_t discharge_ocp_a10;
    uint16_t temp_high_c;
    uint16_t temp_high_release_c;
} bms_config_t;

typedef struct {
    uint16_t vcell[BMS_MAX_CELLS];
    uint16_t vcell_max_mv;
    uint16_t vcell_min_mv;
    uint16_t vcell_delta_mv;
    uint16_t pack_mv;
    uint16_t temp_max_c;
    uint16_t temp_min_c;
    uint16_t ichg_a10;
    uint16_t idsg_a10;
    uint8_t cell_count;
    bool charger_present;
    bool mcu_wake;
    bool protection_fault;
    bool communication_active;
    bms_key_event_t key_event;
} bms_sample_t;

typedef struct {
    uint8_t soc;
    uint8_t display_soc;
    uint8_t soh;
    uint16_t capacity_now_ah100;
    uint16_t capacity_full_ah100;
    uint16_t capacity_factory_ah100;
    uint16_t cycle_count;
    bool full_anchor;
    bool deferred_ocv_valid;
    uint8_t deferred_ocv_target;
    bms_soc_mode_t mode;
} bms_soc_report_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t soc;
    uint16_t display_soc;
    uint16_t soh;
    uint16_t flags;
    uint32_t cap_now_as10;
    uint32_t cap_full_as10;
    uint32_t cycle_x100;
    uint32_t discharge_acc_as10;
    uint32_t sequence;
    uint32_t crc;
} bms_snapshot_t;

typedef struct {
    bms_snapshot_t slot[2];
} bms_storage_t;

typedef struct {
    int32_t cap_factory_as10;
    int32_t cap_full_as10;
    int32_t cap_now_as10;
    int32_t discharge_acc_as10;
    uint32_t cycle_x100;
    uint32_t rem_charge_ms;
    uint32_t rem_discharge_ms;
    uint32_t full_fast_ms;
    uint32_t full_normal_ms;
    uint32_t low_voltage_ms;
    uint32_t mid_voltage_ms;
    uint32_t sag_hold_ms;
    uint32_t rest_stable_ms;
    uint32_t rest_target_ms;
    uint32_t long_rest_down_ms;
    uint32_t deferred_consume_ms;
    uint16_t rest_ref_min_mv;
    uint16_t rest_ref_max_mv;
    uint32_t display_ms;
    uint8_t soc;
    uint8_t display_soc;
    uint8_t soh;
    uint8_t deferred_ocv_target;
    bool deferred_ocv_valid;
    bool full_anchor;
    bool rest_ref_valid;
    bms_soc_mode_t mode;
    bms_soc_table_point_t ocv_table[BMS_SOC_TABLE_POINTS];
} bms_soc_state_t;

typedef struct {
    bool bus_active;
    uint8_t no_ack_windows;
    uint8_t pending_business_frames;
    uint8_t pending_probe_frames;
    uint32_t logic_ms;
} bms_can_state_t;

typedef struct {
    bms_power_state_id_t state;
    uint32_t idle_ms;
    uint32_t low_voltage_ms;
    bool rtc_request;
    bool deep_request;
} bms_power_state_t;

typedef struct {
    uint32_t active_faults;
    bool charge_mos_on;
    bool discharge_mos_on;
} bms_protection_state_t;

typedef struct {
    bool display_on;
    bool charge_icon_on;
    bool deep_sleep_request;
    uint32_t display_hold_ms;
    uint32_t key_hold_ms;
    uint8_t display_soc;
} bms_ui_state_t;

typedef struct {
    bool requested;
    uint16_t request_value;
} bms_iap_state_t;

typedef struct {
    bms_config_t config;
    bms_soc_state_t soc;
    bms_can_state_t can;
    bms_power_state_t power;
    bms_protection_state_t protection;
    bms_ui_state_t ui;
    bms_iap_state_t iap;
    bms_storage_t *storage;
    bms_sample_t last_sample;
    bool has_sample;
} bms_app_t;

typedef struct {
    bool (*read_sample)(void *ctx, bms_sample_t *sample);
    bool (*save_snapshot)(void *ctx, const bms_snapshot_t *snapshot);
    bool (*load_snapshot)(void *ctx, bms_snapshot_t *snapshot);
    bool (*can_send_probe)(void *ctx);
    bool (*can_send_status)(void *ctx, const bms_soc_report_t *report);
    void (*set_charge_mos)(void *ctx, bool on);
    void (*set_discharge_mos)(void *ctx, bool on);
    void (*set_display)(void *ctx, bool on, uint8_t soc, bool charge_icon);
    void (*enter_rtc_stop)(void *ctx, uint16_t seconds);
    void (*request_iap_reset)(void *ctx);
    void *ctx;
} bms_platform_ops_t;

bms_config_t bms_config_default(void);
bms_sample_t bms_sample_default(void);

void bms_storage_init(bms_storage_t *storage);
bool bms_storage_load_latest(const bms_storage_t *storage, bms_snapshot_t *snapshot);
bool bms_storage_save(bms_storage_t *storage, const bms_snapshot_t *snapshot);
bool bms_storage_validate(const bms_snapshot_t *snapshot);

void bms_soc_init(bms_soc_state_t *soc, const bms_config_t *config, const bms_snapshot_t *snapshot);
void bms_soc_update(bms_soc_state_t *soc, const bms_config_t *config, const bms_sample_t *sample, uint32_t elapsed_ms);
void bms_soc_set_once(bms_soc_state_t *soc, uint8_t soc_percent);
bms_soc_report_t bms_soc_report(const bms_soc_state_t *soc);
bms_snapshot_t bms_soc_make_snapshot(const bms_soc_state_t *soc, uint32_t sequence);

void bms_can_init(bms_can_state_t *can);
uint16_t bms_can_idle_rtc_period_seconds(const bms_can_state_t *can, const bms_config_t *config);
void bms_can_prepare_sleep(bms_can_state_t *can);
void bms_can_on_rx(bms_can_state_t *can);
void bms_can_finish_power_window(bms_can_state_t *can, bool any_tx_ack);
void bms_can_on_rtc_wake(bms_can_state_t *can, const bms_config_t *config, uint32_t slept_seconds);

void bms_power_init(bms_power_state_t *power);
void bms_power_update(bms_power_state_t *power, const bms_config_t *config, const bms_sample_t *sample, const bms_can_state_t *can, uint32_t elapsed_ms);

void bms_protection_init(bms_protection_state_t *protection);
void bms_protection_update(bms_protection_state_t *protection, const bms_config_t *config, const bms_sample_t *sample);

void bms_ui_init(bms_ui_state_t *ui);
void bms_ui_update(bms_ui_state_t *ui, const bms_soc_report_t *report, const bms_sample_t *sample, uint32_t elapsed_ms);

void bms_iap_init(bms_iap_state_t *iap);
bool bms_iap_request(bms_iap_state_t *iap, uint16_t value);

void bms_app_init(bms_app_t *app, const bms_config_t *config, bms_storage_t *storage);
void bms_app_process_sample(bms_app_t *app, const bms_sample_t *sample, uint32_t elapsed_ms);
void bms_app_apply_rtc_wake(bms_app_t *app, uint32_t slept_seconds, const bms_sample_t *sample);
bms_soc_report_t bms_app_report(const bms_app_t *app);
bool bms_app_save_snapshot(bms_app_t *app);
void bms_app_apply_outputs(const bms_app_t *app, const bms_platform_ops_t *ops);

bool bms_comm_write_single(bms_app_t *app, uint16_t address, uint16_t value);
bool bms_comm_write_block(bms_app_t *app, uint16_t address, const uint16_t *values, size_t count);
bool bms_comm_read_d000(const bms_app_t *app, uint16_t *words, size_t count);
bool bms_comm_read_d300(const bms_app_t *app, uint16_t *words, size_t count);

#endif
