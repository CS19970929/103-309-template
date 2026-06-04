#ifndef DEBUG_WATCH_H
#define DEBUG_WATCH_H

#include "Project_Config.h"
#include "rtc_sleep.h"
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

struct ADC_RUNTIME_TAG;
struct _DATA_RUNTIME;
struct FEIDAO_CAN_TX_RUNTIME_TAG;
struct FEIDAO_CAN_RUNTIME_TAG;
struct FEIDAO_CAN_APP_RUNTIME_TAG;
struct LEDBAR_RUNTIME_TAG;
struct SLEEP_RUNTIME_TAG;
struct FLASH_RUNTIME_TAG;
struct LOG_RECORD_RUNTIME_TAG;
struct SOC_DEBUG_WATCH;
struct SOC_ENHANCE_ELEMENT;
struct stCell_Info;
struct OTHER_ELEMENT;
struct PRT_E2ROM_PARAS;
struct SYSTEM_ERROR;
union SYS_TIME;
union System_OnOFF_Function;
union System_Status;

typedef struct DEBUG_WATCH_ROOT_TAG
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
