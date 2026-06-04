#ifndef DEBUG_WATCH_H
#define DEBUG_WATCH_H

#include "main.h"
#include <stdint.h>

#if defined(PROJECT_CFG_DEBUG_WATCH_ENABLE) && (PROJECT_CFG_DEBUG_WATCH_ENABLE != 0)
#define DEBUG_WATCH_ENABLED 1
#else
#define DEBUG_WATCH_ENABLED 0
#endif

#if DEBUG_WATCH_ENABLED
#if defined(__GNUC__) || defined(__CC_ARM)
#define DEBUG_WATCH_USED __attribute__((used))
#else
#define DEBUG_WATCH_USED
#endif

#if defined(PROJECT_CFG_DEBUG_MONITOR_ENABLE) && (PROJECT_CFG_DEBUG_MONITOR_ENABLE != 0)
#define DEBUG_WATCH_SYSTEM_DEBUG_ENABLED 1
#else
#define DEBUG_WATCH_SYSTEM_DEBUG_ENABLED 0
#endif

struct ADC_RUNTIME_TAG;
struct _DATA_RUNTIME;
struct FEIDAO_CAN_TX_RUNTIME_TAG;
struct FEIDAO_CAN_RUNTIME_TAG;
struct FEIDAO_CAN_APP_RUNTIME_TAG;
struct CAN_FEIDAO_FRAME_DISPATCH_TAG;
struct LEDBAR_RUNTIME_TAG;
struct SLEEP_RUNTIME_TAG;
struct FLASH_RUNTIME_TAG;
struct LOG_RECORD_RUNTIME_TAG;
struct FACTORY_AGING_RUNTIME_TAG;
struct RTC_RUNTIME_TAG;
struct APP_RUNTIME_TAG;
struct SCI_PORT_RUNTIME;
struct RS485MSG;
struct DBG_RUNTIME_TAG;
struct SYSTEM_DEBUG;
struct IRQ_DEBUG_STATE;
struct SOC_DEBUG_WATCH;
struct SOC_ENHANCE_ELEMENT;
struct SOC_STATE_TAG;
struct SOC_SAVE_MARK_TAG;
struct SOC_EMPTY_TAIL_RULE_TAG;
struct stCell_Info;
struct OTHER_ELEMENT;
struct PRT_E2ROM_PARAS;
struct SYSTEM_ERROR;
union SYS_TIME;
union System_OnOFF_Function;
union System_Status;

typedef struct DEBUG_WATCH_RUNTIME_DIR_TAG
{
	struct ADC_RUNTIME_TAG *adc;
	struct _DATA_RUNTIME *data;
	struct FEIDAO_CAN_TX_RUNTIME_TAG *can_tx;
	struct FEIDAO_CAN_RUNTIME_TAG *can_runtime;
	struct FEIDAO_CAN_APP_RUNTIME_TAG *can_app;
	struct LEDBAR_RUNTIME_TAG *ledbar;
	struct SLEEP_RUNTIME_TAG *sleep;
	struct FLASH_RUNTIME_TAG *flash;
	struct LOG_RECORD_RUNTIME_TAG *log_record;
	struct FACTORY_AGING_RUNTIME_TAG *factory_aging;
	struct RTC_RUNTIME_TAG *rtc;
	struct APP_RUNTIME_TAG *app;
	struct SOC_DEBUG_WATCH *soc;
	struct SOC_ENHANCE_ELEMENT *soc_public;
	struct SOC_STATE_TAG *soc_state;
	struct SOC_SAVE_MARK_TAG *soc_saved;
	uint32_t *soc_rtc_rest_applied_seconds;
	uint8_t *soc_rest_voltage_stable;
	struct DBG_RUNTIME_TAG *debug_monitor_runtime;
} DEBUG_WATCH_RUNTIME_DIR;

typedef struct DEBUG_WATCH_COMM_DIR_TAG
{
	struct SCI_PORT_RUNTIME *sci1;
	struct SCI_PORT_RUNTIME *sci2;
	struct SCI_PORT_RUNTIME *sci3;
	struct RS485MSG *sci_msg1;
	struct RS485MSG *sci_msg2;
	struct RS485MSG *sci_msg3;
	uint8_t *sci_tx_buffer;
	volatile uint16_t *sci_err1;
	volatile uint16_t *sci_err2;
	volatile uint16_t *sci_err3;
	volatile uint8_t *sci_tx_enable1;
	volatile uint8_t *sci_tx_enable2;
	volatile uint8_t *sci_tx_enable3;
	volatile uint8_t *sci_tx_finish1;
	volatile uint8_t *sci_tx_finish2;
	volatile uint8_t *sci_tx_finish3;
	uint8_t *flash_update_flag;
	uint8_t *flash_update_e2prom;
} DEBUG_WATCH_COMM_DIR;

typedef struct DEBUG_WATCH_SYSTEM_DIR_TAG
{
	struct SYSTEM_DEBUG *snapshot;
	volatile struct IRQ_DEBUG_STATE *irq;
	volatile union SYS_TIME *time_latched;
	volatile union SYS_TIME *time_pending;
	volatile uint32_t *tick_10ms;
	uint8_t *cnt50ms;
	uint8_t *cnt100ms;
	uint8_t *cnt200ms;
	uint8_t *cnt1000ms;
	volatile uint8_t *pending_200ms;
	volatile uint16_t *overflow_200ms;
	volatile union System_OnOFF_Function *feature;
	volatile union System_Status *status;
	volatile struct SYSTEM_ERROR *error;
	volatile struct LOW_POWER_RTC_STATUS *low_power;
	enum irqWakeup *irq_wakeup;
	uint8_t *delay_fac_us;
	uint16_t *delay_fac_ms;
} DEBUG_WATCH_SYSTEM_DIR;

typedef struct DEBUG_WATCH_AFE_DIR_TAG
{
	AFEDATA *registers_afe1;
	struct SH367309_Read *read_afe1;
	SH367309_REG_STORE *reg_store;
	AFE_ROM_PARAMETERS_TypeDef *rom_params;
	AFE_Parameters_RS485_Typedef *rs485_params;
	int *param_write_flag;
	uint8_t *mtp_buffer;
} DEBUG_WATCH_AFE_DIR;

typedef struct DEBUG_WATCH_FAULT_DIR_TAG
{
	struct PRT_E2ROM_PARAS *protect;
	union FAULT_FLAG_FIRST *first;
	union FAULT_FLAG_SECOND *second;
	union FAULT_FLAG_THIRD *third;
	uint16_t *record_third;
	uint16_t *record_third2;
	uint8_t *point_third;
	uint8_t *point_third2;
} DEBUG_WATCH_FAULT_DIR;

typedef struct DEBUG_WATCH_PUBLIC_DIR_TAG
{
	struct stCell_Info *cell_report;
	struct OTHER_ELEMENT *other;
	struct PRT_E2ROM_PARAS *protect;
	PRODUCTION_ID_INFO *production;
	struct RTC_ELEMENT *rtc_time;
	struct SOC_ENHANCE_ELEMENT *soc;
} DEBUG_WATCH_PUBLIC_DIR;

typedef struct DEBUG_WATCH_APP_DIR_TAG
{
	uint8_t *series_num;
	uint32_t *log_interval_s_tcnt;
} DEBUG_WATCH_APP_DIR;

typedef struct DEBUG_WATCH_CALIB_DIR_TAG
{
	uint16_t *coef_k;
	int16_t *coef_b;
	uint32_t *cs_res_afe;
} DEBUG_WATCH_CALIB_DIR;

typedef struct DEBUG_WATCH_TABLE_DIR_TAG
{
	const uint16_t *adc_ntc_10k;
	const uint16_t *afe_ntc_10k;
	const uint8_t *afe_crc8;
	const uint16_t *sh_ntc_10k;
	const uint16_t *sh_afe_scv;
	const uint16_t *sh_afe_sct;
	const uint16_t *sh_afe_ocd1v_occv;
	const uint16_t *sh_afe_ocd2v;
	const uint16_t *sh_afe_ovt_uvt;
	const uint16_t *sh_afe_ocd1t;
	const uint16_t *sh_afe_occt_ocd2t;
	const uint16_t *soc_lifepo;
	const uint16_t *soc_ternary;
	const struct SOC_EMPTY_TAIL_RULE_TAG *soc_empty_tail;
	uint16_t soc_empty_tail_count;
	const uint8_t *ledbar_digit_map;
	uint16_t ledbar_digit_map_count;
	const void *ledbar_routes;
	uint16_t ledbar_routes_count;
	const void *ledbar_pins;
	uint16_t ledbar_pins_count;
	const uint8_t *rtc_month_days;
	uint16_t rtc_month_days_count;
	const uint8_t *system_error_field_offset;
	uint16_t system_error_field_offset_count;
	const uint16_t *bq_afe_scv;
	const uint16_t *bq_afe_sct;
	const struct CAN_FEIDAO_FRAME_DISPATCH_TAG *can_feidao_dispatch;
	uint16_t can_feidao_dispatch_count;
} DEBUG_WATCH_TABLE_DIR;

typedef struct DEBUG_WATCH_ROOT_TAG
{
	DEBUG_WATCH_RUNTIME_DIR runtime;
	DEBUG_WATCH_COMM_DIR comm;
	DEBUG_WATCH_SYSTEM_DIR system;
	DEBUG_WATCH_AFE_DIR afe;
	DEBUG_WATCH_FAULT_DIR fault;
	DEBUG_WATCH_PUBLIC_DIR public_data;
	DEBUG_WATCH_APP_DIR app;
	DEBUG_WATCH_CALIB_DIR calib;
	DEBUG_WATCH_TABLE_DIR tables;

	struct ADC_RUNTIME_TAG *adc;
	struct _DATA_RUNTIME *data;
	struct FEIDAO_CAN_TX_RUNTIME_TAG *can_tx;
	struct FEIDAO_CAN_RUNTIME_TAG *can_runtime;
	struct FEIDAO_CAN_APP_RUNTIME_TAG *can_app;
	struct LEDBAR_RUNTIME_TAG *ledbar;
	struct SLEEP_RUNTIME_TAG *sleep;
	struct FLASH_RUNTIME_TAG *flash;
	struct LOG_RECORD_RUNTIME_TAG *log_record;
	struct SOC_DEBUG_WATCH *soc;
	struct SOC_ENHANCE_ELEMENT *soc_public;

	volatile union SYS_TIME *sys_time_latched;
	volatile union SYS_TIME *sys_time_pending;
	volatile uint32_t *sys_10ms_tick_count;
	uint8_t *sys_cnt50ms;
	uint8_t *sys_cnt100ms;
	uint8_t *sys_cnt200ms;
	uint8_t *sys_cnt1000ms;
	volatile uint8_t *sys_200ms_pending_periods;
	volatile uint16_t *sys_200ms_overflow_count;

	volatile union System_OnOFF_Function *system_feature;
	volatile union System_Status *system_status;
	volatile struct SYSTEM_ERROR *system_error;
	volatile struct LOW_POWER_RTC_STATUS *low_power;
	enum irqWakeup *irq_wakeup;

	struct stCell_Info *cell_report;
	struct OTHER_ELEMENT *other;
	struct PRT_E2ROM_PARAS *protect;
} DEBUG_WATCH_ROOT;

extern DEBUG_WATCH_ROOT g_dbg_watch;

void DebugWatch_BindAll(void);
#else
#define DebugWatch_BindAll() ((void)0)
#endif

#endif /* DEBUG_WATCH_H */
