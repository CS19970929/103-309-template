#include "main.h"
#include "RuntimeLog.h"

#if PROJECT_CFG_RUNTIME_LOG_ENABLE
#include "elog.h"
#endif

typedef struct RUNTIME_LOG_STATE_TAG
{
	uint8_t initialized;
	uint8_t heartbeat_divider;
	uint8_t last_mode;
	uint8_t last_wake_source;
	uint16_t last_fault;
	uint32_t last_block;
} RUNTIME_LOG_STATE;

static RUNTIME_LOG_STATE s_runtime_log;

#if PROJECT_CFG_RUNTIME_LOG_ENABLE
static const char *RuntimeLog_ModeName(uint8_t mode)
{
	switch (mode)
	{
	case NO_SLEEP:
		return "NO";
	case HICCUP_MODE:
		return "HICCUP";
	case NORMAL_MODE:
		return "NORMAL";
	case DEEP_MODE:
		return "DEEP";
	default:
		return "?";
	}
}

static const char *RuntimeLog_WakeName(uint8_t source)
{
	switch (source)
	{
	case PA0_irq:
		return "CHG";
	case soc_key:
		return "SOC_KEY";
	case bms_keyirq:
		return "BMS_KEY";
	default:
		return "NO_IRQ";
	}
}
#endif

void RuntimeLog_Init(void)
{
	s_runtime_log.initialized = 0U;
	s_runtime_log.heartbeat_divider = 0U;
	s_runtime_log.last_mode = 0xFFU;
	s_runtime_log.last_wake_source = 0xFFU;
	s_runtime_log.last_fault = 0xFFFFU;
	s_runtime_log.last_block = 0xFFFFFFFFUL;

#if PROJECT_CFG_RUNTIME_LOG_ENABLE
	if (elog_init() == ELOG_NO_ERR)
	{
		elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
		elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
		elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
		elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
		elog_set_filter_lvl((uint8_t)PROJECT_CFG_RUNTIME_LOG_LEVEL);
		elog_start();
		s_runtime_log.initialized = 1U;
	}
#endif
}

void RuntimeLog_BootReady(void)
{
#if PROJECT_CFG_RUNTIME_LOG_ENABLE
	if (s_runtime_log.initialized == 0U)
	{
		return;
	}

	elog_i("boot",
		   "ready fw=%u.%02u.%02u v%u boot_sleep=%u chg_wake=%u rtc=%u",
		   (unsigned int)PROJECT_CFG_FD_YEAR,
		   (unsigned int)PROJECT_CFG_FD_MONTH,
		   (unsigned int)PROJECT_CFG_FD_DAY,
		   (unsigned int)PROJECT_CFG_VERSION,
		   (unsigned int)SleepDeal_IsBootFromSleepStartup(),
		   (unsigned int)SleepDeal_IsBootFromSleepChargerWakeup(),
		   (unsigned int)PROJECT_CFG_RTC_ENABLE);
#endif
}

void RuntimeLog_Task1s(void)
{
#if PROJECT_CFG_RUNTIME_LOG_ENABLE
	uint16_t fault;
	uint32_t block;

	if ((s_runtime_log.initialized == 0U) ||
		(g_st_SysTimeFlag.bits.b1Sys1000msFlag == 0U))
	{
		return;
	}

	if (++s_runtime_log.heartbeat_divider < (uint8_t)PROJECT_CFG_RUNTIME_LOG_HEARTBEAT_SECONDS)
	{
		return;
	}
	s_runtime_log.heartbeat_divider = 0U;

	block = g_stLowPowerRtcStatus.block;
	if (block != s_runtime_log.last_block)
	{
		elog_i("lp", "block=0x%08lX idle=%lu/%lu mode=%s",
			   (unsigned long)block,
			   (unsigned long)g_stLowPowerRtcStatus.idle,
			   (unsigned long)g_stLowPowerRtcStatus.idleMax,
			   RuntimeLog_ModeName(g_stLowPowerRtcStatus.mode));
		s_runtime_log.last_block = block;
	}

	fault = g_stCellInfoReport.unMdlFault_Third.all;
	if (fault != s_runtime_log.last_fault)
	{
		elog_w("fault", "third=0x%04X vmin=%u vmax=%u",
			   (unsigned int)fault,
			   (unsigned int)g_stCellInfoReport.u16VCellMin,
			   (unsigned int)g_stCellInfoReport.u16VCellMax);
		s_runtime_log.last_fault = fault;
	}

	elog_i("run",
		   "t=%lu soc=%u v=%u/%u ichg=%u idsg=%u fault=0x%04X lp=%s block=0x%08lX sleep=%lu last=%lu",
		   (unsigned long)su32_Interval_S_Tcnt,
		   (unsigned int)g_stCellInfoReport.SocElement.u16Soc,
		   (unsigned int)g_stCellInfoReport.u16VCellMin,
		   (unsigned int)g_stCellInfoReport.u16VCellMax,
		   (unsigned int)g_stCellInfoReport.u16Ichg,
		   (unsigned int)g_stCellInfoReport.u16IDischg,
		   (unsigned int)fault,
		   RuntimeLog_ModeName(g_stLowPowerRtcStatus.mode),
		   (unsigned long)block,
		   (unsigned long)g_stLowPowerRtcStatus.sleep,
		   (unsigned long)g_stLowPowerRtcStatus.last);
#endif
}

void RuntimeLog_LowPowerRequest(uint8_t mode, uint32_t block, uint32_t idle, uint32_t idle_max)
{
#if PROJECT_CFG_RUNTIME_LOG_ENABLE
	if (s_runtime_log.initialized == 0U)
	{
		return;
	}

	if ((mode == s_runtime_log.last_mode) && (block == s_runtime_log.last_block))
	{
		return;
	}

	elog_i("lp", "request mode=%s block=0x%08lX idle=%lu/%lu",
		   RuntimeLog_ModeName(mode),
		   (unsigned long)block,
		   (unsigned long)idle,
		   (unsigned long)idle_max);
	s_runtime_log.last_mode = mode;
	s_runtime_log.last_block = block;
#else
	(void)mode;
	(void)block;
	(void)idle;
	(void)idle_max;
#endif
}

void RuntimeLog_ResetSleepCommit(uint8_t mode)
{
#if PROJECT_CFG_RUNTIME_LOG_ENABLE
	if (s_runtime_log.initialized != 0U)
	{
		elog_w("lp", "reset-sleep commit mode=%s", RuntimeLog_ModeName(mode));
	}
#else
	(void)mode;
#endif
}

void RuntimeLog_RtcStopEnter(uint32_t cycles, uint32_t sleep_seconds)
{
#if PROJECT_CFG_RUNTIME_LOG_ENABLE
	if (s_runtime_log.initialized != 0U)
	{
		elog_i("rtc", "stop enter cycle=%lu sleep=%lu",
			   (unsigned long)cycles,
			   (unsigned long)sleep_seconds);
	}
#else
	(void)cycles;
	(void)sleep_seconds;
#endif
}

void RuntimeLog_RtcStopWake(uint8_t rtc_wake, uint32_t elapsed_seconds, uint32_t sleep_seconds)
{
#if PROJECT_CFG_RUNTIME_LOG_ENABLE
	if (s_runtime_log.initialized != 0U)
	{
		elog_i("rtc", "stop wake rtc=%u elapsed=%lu sleep=%lu",
			   (unsigned int)rtc_wake,
			   (unsigned long)elapsed_seconds,
			   (unsigned long)sleep_seconds);
	}
#else
	(void)rtc_wake;
	(void)elapsed_seconds;
	(void)sleep_seconds;
#endif
}

void RuntimeLog_RtcStopExit(uint8_t wake_source, uint32_t sleep_seconds, uint32_t cycles)
{
#if PROJECT_CFG_RUNTIME_LOG_ENABLE
	if (s_runtime_log.initialized == 0U)
	{
		return;
	}

	if (wake_source != s_runtime_log.last_wake_source)
	{
		elog_w("rtc", "exit wake=%s(%u) sleep=%lu cycles=%lu",
			   RuntimeLog_WakeName(wake_source),
			   (unsigned int)wake_source,
			   (unsigned long)sleep_seconds,
			   (unsigned long)cycles);
		s_runtime_log.last_wake_source = wake_source;
	}
	else
	{
		elog_i("rtc", "exit wake=%s(%u) sleep=%lu cycles=%lu",
			   RuntimeLog_WakeName(wake_source),
			   (unsigned int)wake_source,
			   (unsigned long)sleep_seconds,
			   (unsigned long)cycles);
	}
#else
	(void)wake_source;
	(void)sleep_seconds;
	(void)cycles;
#endif
}
