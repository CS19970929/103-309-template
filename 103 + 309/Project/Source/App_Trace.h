#ifndef APP_TRACE_H
#define APP_TRACE_H

#ifndef APP_TRACE_ENABLE
#define APP_TRACE_ENABLE 0
#endif

#if APP_TRACE_ENABLE
typedef enum _APP_TRACE_TASK_ID {
	APP_TRACE_TASK_SYS_TIME = 0,
	APP_TRACE_TASK_WARN_CTRL,
	APP_TRACE_TASK_LED_BAR,
	APP_TRACE_TASK_AFE_GET,
	APP_TRACE_TASK_POWER_UI,
	APP_TRACE_TASK_SCI,
	APP_TRACE_TASK_ANALOG_CAL,
	APP_TRACE_TASK_EEPROM,
	APP_TRACE_TASK_CELL_BALANCE,
	APP_TRACE_TASK_SLEEP,
	APP_TRACE_TASK_SOC,
	APP_TRACE_TASK_BMS_EUAVCAN,
	APP_TRACE_TASK_HEAT_COOL,
	APP_TRACE_TASK_CHARGER_ON,
	APP_TRACE_TASK_FLASH_UPDATE,
	APP_TRACE_TASK_LOG_RECORD,
	APP_TRACE_TASK_PRO_ID,
	APP_TRACE_TASK_NUM
} APP_TRACE_TASK_ID;

#define APP_TRACE_TASK_NONE ((UINT8)0xFF)

typedef enum _APP_WARN_CHECK_ID {
	APP_WARN_CHECK_CELL_OVP_SECOND = 0,
	APP_WARN_CHECK_CELL_OVP_THIRD,
	APP_WARN_CHECK_CELL_UVP_SECOND,
	APP_WARN_CHECK_CELL_UVP_THIRD,
	APP_WARN_CHECK_BAT_OVP_SECOND,
	APP_WARN_CHECK_BAT_OVP_THIRD,
	APP_WARN_CHECK_BAT_UVP_SECOND,
	APP_WARN_CHECK_BAT_UVP_THIRD,
	APP_WARN_CHECK_MOS_OTP_SECOND,
	APP_WARN_CHECK_MOS_OTP_THIRD,
	APP_WARN_CHECK_VDELTA_OP_SECOND,
	APP_WARN_CHECK_VDELTA_OP_THIRD,
	APP_WARN_CHECK_IDISCHG_OCP_SECOND,
	APP_WARN_CHECK_IDISCHG_OCP_THIRD,
	APP_WARN_CHECK_ICHG_OCP_SECOND,
	APP_WARN_CHECK_ICHG_OCP_THIRD,
	APP_WARN_CHECK_CELL_SOC_UP_SECOND,
	APP_WARN_CHECK_CELL_SOC_UP_THIRD,
	APP_WARN_CHECK_CELL_DISCHG_OTP_SECOND,
	APP_WARN_CHECK_CELL_DISCHG_OTP_THIRD,
	APP_WARN_CHECK_CELL_DISCHG_UTP_SECOND,
	APP_WARN_CHECK_CELL_DISCHG_UTP_THIRD,
	APP_WARN_CHECK_CELL_CHG_OTP_SECOND,
	APP_WARN_CHECK_CELL_CHG_OTP_THIRD,
	APP_WARN_CHECK_CELL_CHG_UTP_SECOND,
	APP_WARN_CHECK_CELL_CHG_UTP_THIRD,
	APP_WARN_CHECK_NUM
} APP_WARN_CHECK_ID;

#define APP_WARN_CHECK_NONE ((UINT8)0xFF)

typedef struct _APP_TRACE_TASK {
	volatile UINT32 runCnt;
	volatile UINT32 lastLoopCnt;
	volatile UINT32 last1msTick;
	volatile UINT32 last10msPhaseTick;
	volatile UINT32 last10msFlag1Tick;
	volatile UINT16 lastLoopInterval;
	volatile UINT16 maxLoopInterval;
	volatile UINT16 last10msFlag1Interval;
	volatile UINT16 max10msFlag1Interval;
	volatile UINT8 active;
} APP_TRACE_TASK;

typedef struct _APP_TRACE_WARN_CHECK {
	volatile UINT32 runCnt;
	volatile UINT32 lastWarnCtrlCnt;
	volatile UINT16 lastWarnCtrlInterval;
	volatile UINT16 maxWarnCtrlInterval;
	volatile UINT8 active;
} APP_TRACE_WARN_CHECK;

extern volatile APP_TRACE_TASK g_stAppTraceTask[APP_TRACE_TASK_NUM];
extern volatile APP_TRACE_WARN_CHECK g_stAppTraceWarnCheck[APP_WARN_CHECK_NUM];
extern volatile UINT32 gu32_AppTraceLoopCnt;
extern volatile UINT32 gu32_AppTrace1msTick;
extern volatile UINT32 gu32_AppTrace10msPhaseTick;
extern volatile UINT32 gu32_AppTrace10msFlag1Tick;
extern volatile UINT8 gu8_AppTraceCurrentTask;
extern volatile UINT8 gu8_AppTraceCurrentWarnCheck;
extern volatile UINT8 gu8_AppTraceLast10msPhase;

void AppTrace_LoopBegin(void);
void AppTrace_TaskBegin(UINT8 u8TaskId);
void AppTrace_TaskEnd(UINT8 u8TaskId);
void AppTrace_WarnCheckBegin(UINT8 u8CheckId);
void AppTrace_WarnCheckEnd(UINT8 u8CheckId);
#endif

#endif

#if APP_TRACE_ENABLE && defined(APP_TRACE_WRAP_MAIN_TASKS)
#define APP_TRACE_RUN_TASK(TaskId, TaskFunc) \
	do { \
		AppTrace_TaskBegin((UINT8)(TaskId)); \
		TaskFunc(); \
		AppTrace_TaskEnd((UINT8)(TaskId)); \
	} while (0)

#define App_SysTime() do { AppTrace_LoopBegin(); APP_TRACE_RUN_TASK(APP_TRACE_TASK_SYS_TIME, App_SysTime); } while (0)
#define App_WarnCtrl() APP_TRACE_RUN_TASK(APP_TRACE_TASK_WARN_CTRL, App_WarnCtrl)
#define APP_LedBar() APP_TRACE_RUN_TASK(APP_TRACE_TASK_LED_BAR, APP_LedBar)
#define App_AFEGet() APP_TRACE_RUN_TASK(APP_TRACE_TASK_AFE_GET, App_AFEGet)
#define PowerUi_ProcessRequests() APP_TRACE_RUN_TASK(APP_TRACE_TASK_POWER_UI, PowerUi_ProcessRequests)
#define App_Sci() APP_TRACE_RUN_TASK(APP_TRACE_TASK_SCI, App_Sci)
#define App_AnlogCal() APP_TRACE_RUN_TASK(APP_TRACE_TASK_ANALOG_CAL, App_AnlogCal)
#define App_E2promDeal() APP_TRACE_RUN_TASK(APP_TRACE_TASK_EEPROM, App_E2promDeal)
#define App_CellBalance() APP_TRACE_RUN_TASK(APP_TRACE_TASK_CELL_BALANCE, App_CellBalance)
#define App_SleepDeal() APP_TRACE_RUN_TASK(APP_TRACE_TASK_SLEEP, App_SleepDeal)
#define App_SOC() APP_TRACE_RUN_TASK(APP_TRACE_TASK_SOC, App_SOC)
#define App_BmsEUavcan() APP_TRACE_RUN_TASK(APP_TRACE_TASK_BMS_EUAVCAN, App_BmsEUavcan)
#define App_Heat_Cool_Ctrl() APP_TRACE_RUN_TASK(APP_TRACE_TASK_HEAT_COOL, App_Heat_Cool_Ctrl)
#define AllSeriesDeal_Charger_ON() APP_TRACE_RUN_TASK(APP_TRACE_TASK_CHARGER_ON, AllSeriesDeal_Charger_ON)
#define App_FlashUpdate() APP_TRACE_RUN_TASK(APP_TRACE_TASK_FLASH_UPDATE, App_FlashUpdate)
#define App_LogRecord() APP_TRACE_RUN_TASK(APP_TRACE_TASK_LOG_RECORD, App_LogRecord)
#define App_ProID_Deal() APP_TRACE_RUN_TASK(APP_TRACE_TASK_PRO_ID, App_ProID_Deal)
#endif

#ifdef APP_TRACE_WRAP_MAIN_TASKS
#undef APP_TRACE_WRAP_MAIN_TASKS
#endif

#ifdef APP_TRACE_UNWRAP_MAIN_TASKS
#undef App_SysTime
#undef App_WarnCtrl
#undef APP_LedBar
#undef App_AFEGet
#undef PowerUi_ProcessRequests
#undef App_Sci
#undef App_AnlogCal
#undef App_E2promDeal
#undef App_CellBalance
#undef App_SleepDeal
#undef App_SOC
#undef App_BmsEUavcan
#undef App_Heat_Cool_Ctrl
#undef AllSeriesDeal_Charger_ON
#undef App_FlashUpdate
#undef App_LogRecord
#undef App_ProID_Deal
#undef APP_TRACE_RUN_TASK
#undef APP_TRACE_UNWRAP_MAIN_TASKS
#endif

#if APP_TRACE_ENABLE && defined(APP_TRACE_WRAP_WARN_CHECKS)
#define APP_TRACE_RUN_WARN_CHECK(CheckId, CheckFunc) \
	do { \
		AppTrace_WarnCheckBegin((UINT8)(CheckId)); \
		CheckFunc(); \
		AppTrace_WarnCheckEnd((UINT8)(CheckId)); \
	} while (0)

#define App_CellOvp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_OVP_SECOND, App_CellOvp_SecondCheck)
#define App_CellOvp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_OVP_THIRD, App_CellOvp_ThirdCheck)
#define App_CellUvp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_UVP_SECOND, App_CellUvp_SecondCheck)
#define App_CellUvp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_UVP_THIRD, App_CellUvp_ThirdCheck)
#define App_BatOvp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_BAT_OVP_SECOND, App_BatOvp_SecondCheck)
#define App_BatOvp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_BAT_OVP_THIRD, App_BatOvp_ThirdCheck)
#define App_BatUvp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_BAT_UVP_SECOND, App_BatUvp_SecondCheck)
#define App_BatUvp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_BAT_UVP_THIRD, App_BatUvp_ThirdCheck)
#define App_MosOtp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_MOS_OTP_SECOND, App_MosOtp_SecondCheck)
#define App_MosOtp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_MOS_OTP_THIRD, App_MosOtp_ThirdCheck)
#define App_VdeltaOp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_VDELTA_OP_SECOND, App_VdeltaOp_SecondCheck)
#define App_VdeltaOp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_VDELTA_OP_THIRD, App_VdeltaOp_ThirdCheck)
#define App_IdischgOcp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_IDISCHG_OCP_SECOND, App_IdischgOcp_SecondCheck)
#define App_IdischgOcp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_IDISCHG_OCP_THIRD, App_IdischgOcp_ThirdCheck)
#define App_IchgOcp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_ICHG_OCP_SECOND, App_IchgOcp_SecondCheck)
#define App_IchgOcp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_ICHG_OCP_THIRD, App_IchgOcp_ThirdCheck)
#define App_CellSocUp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_SOC_UP_SECOND, App_CellSocUp_SecondCheck)
#define App_CellSocUp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_SOC_UP_THIRD, App_CellSocUp_ThirdCheck)
#define App_CellDisChgOtp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_DISCHG_OTP_SECOND, App_CellDisChgOtp_SecondCheck)
#define App_CellDisChgOtp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_DISCHG_OTP_THIRD, App_CellDisChgOtp_ThirdCheck)
#define App_CellDischgUtp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_DISCHG_UTP_SECOND, App_CellDischgUtp_SecondCheck)
#define App_CellDischgUtp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_DISCHG_UTP_THIRD, App_CellDischgUtp_ThirdCheck)
#define App_CellChgOtp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_CHG_OTP_SECOND, App_CellChgOtp_SecondCheck)
#define App_CellChgOtp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_CHG_OTP_THIRD, App_CellChgOtp_ThirdCheck)
#define App_CellChgUtp_SecondCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_CHG_UTP_SECOND, App_CellChgUtp_SecondCheck)
#define App_CellChgUtp_ThirdCheck() APP_TRACE_RUN_WARN_CHECK(APP_WARN_CHECK_CELL_CHG_UTP_THIRD, App_CellChgUtp_ThirdCheck)
#endif

#ifdef APP_TRACE_WRAP_WARN_CHECKS
#undef APP_TRACE_WRAP_WARN_CHECKS
#endif

#ifdef APP_TRACE_UNWRAP_WARN_CHECKS
#undef App_CellOvp_SecondCheck
#undef App_CellOvp_ThirdCheck
#undef App_CellUvp_SecondCheck
#undef App_CellUvp_ThirdCheck
#undef App_BatOvp_SecondCheck
#undef App_BatOvp_ThirdCheck
#undef App_BatUvp_SecondCheck
#undef App_BatUvp_ThirdCheck
#undef App_MosOtp_SecondCheck
#undef App_MosOtp_ThirdCheck
#undef App_VdeltaOp_SecondCheck
#undef App_VdeltaOp_ThirdCheck
#undef App_IdischgOcp_SecondCheck
#undef App_IdischgOcp_ThirdCheck
#undef App_IchgOcp_SecondCheck
#undef App_IchgOcp_ThirdCheck
#undef App_CellSocUp_SecondCheck
#undef App_CellSocUp_ThirdCheck
#undef App_CellDisChgOtp_SecondCheck
#undef App_CellDisChgOtp_ThirdCheck
#undef App_CellDischgUtp_SecondCheck
#undef App_CellDischgUtp_ThirdCheck
#undef App_CellChgOtp_SecondCheck
#undef App_CellChgOtp_ThirdCheck
#undef App_CellChgUtp_SecondCheck
#undef App_CellChgUtp_ThirdCheck
#undef APP_TRACE_RUN_WARN_CHECK
#undef APP_TRACE_UNWRAP_WARN_CHECKS
#endif
