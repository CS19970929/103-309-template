#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

/* System error counters retained for protocol/debug compatibility. */
enum SYSTEM_ERROR_COMMAND {
	ERROR_AFE1 = 1,
	ERROR_AFE2,
	ERROR_EEPROM_COM,
	ERROR_SPI,
	ERROR_UPPER,
	ERROR_CLIENT,
	ERROR_SCREEN,
	ERROR_WIFI,
	ERROR_BLUETOOTH,
	ERROR_APP,
	ERROR_CBC_CHG,
	ERROR_CBC_DSG,
	ERROR_EEPROM_STORE,
	ERROR_HSE,
	ERROR_LSE,
	ERROR_VDEATLE_OVER,
	ERROR_BALANCED,
	ERROR_ADC,
	ERROR_SOC_CAIL,
	ERROR_RESERVED_21,
	ERROR_RESERVED_22,
	ERROR_TEMP_BREAK,
	ERROR_NUM = ERROR_TEMP_BREAK,

	ERROR_REMOVE_AFE1,
	ERROR_REMOVE_AFE2,
	ERROR_REMOVE_CAN,
	ERROR_REMOVE_EEPROM_COM,
	ERROR_REMOVE_SPI,
	ERROR_REMOVE_UPPER,
	ERROR_REMOVE_CLIENT,
	ERROR_REMOVE_SCREEN,
	ERROR_REMOVE_WIFI,
	ERROR_REMOVE_BLUETOOTH,
	ERROR_REMOVE_APP,
	ERROR_REMOVE_CBC_CHG,
	ERROR_REMOVE_CBC_DSG,
	ERROR_REMOVE_EEPROM_STORE,
	ERROR_REMOVE_HSE,
	ERROR_REMOVE_LSE,
	ERROR_REMOVE_VDEATLE_OVER,
	ERROR_REMOVE_BALANCED,
	ERROR_REMOVE_ADC,
	ERROR_REMOVE_RESERVED_21,
	ERROR_REMOVE_RESERVED_22,
	ERROR_REMOVE_SOC_CAIL,
	ERROR_REMOVE_TEMP_BREAK,

	ERROR_STATUS_AFE1,
	ERROR_STATUS_AFE2,
	ERROR_STATUS_CAN,
	ERROR_STATUS_EEPROM_COM,
	ERROR_STATUS_SPI,
	ERROR_STATUS_UPPER,
	ERROR_STATUS_CLIENT,
	ERROR_STATUS_SCREEN,
	ERROR_STATUS_WIFI,
	ERROR_STATUS_BLUETOOTH,
	ERROR_STATUS_APP,
	ERROR_STATUS_CBC_CHG,
	ERROR_STATUS_CBC_DSG,
	ERROR_STATUS_EEPROM_STORE,
	ERROR_STATUS_HSE,
	ERROR_STATUS_LSE,
	ERROR_STATUS_VDEATLE_OVER,
	ERROR_STATUS_BALANCED,
	ERROR_STATUS_ADC,
	ERROR_STATUS_RESERVED_21,
	ERROR_STATUS_RESERVED_22,
	ERROR_STATUS_SOC_CAIL,
	ERROR_STATUS_TEMP_BREAK,
};

struct SYSTEM_ERROR {
	UINT8 u8ErrFlag_Com_AFE1;
	UINT8 u8ErrFlag_Com_AFE2;
	UINT8 u8ErrFlag_Com_Can;
	UINT8 u8ErrFlag_Com_EEPROM;
	UINT8 u8ErrFlag_Com_SPI;
	UINT8 u8ErrFlag_Com_Upper;
	UINT8 u8ErrFlag_Com_Client;
	UINT8 u8ErrFlag_Com_Screen;
	UINT8 u8ErrFlag_Com_Wifi;
	UINT8 u8ErrFlag_Com_BlueTooth;
	UINT8 u8ErrFlag_Com_App;
	UINT8 u8ErrFlag_CBC_CHG;
	UINT8 u8ErrFlag_Store_EEPROM;
	UINT8 u8ErrFlag_HSE;
	UINT8 u8ErrFlag_LSE;
	UINT8 u8ErrFlag_Vdelta_OVER;
	UINT8 u8ErrFlag_Balanced;
	UINT8 u8ErrFlag_ADC;
	UINT8 u8ErrFlag_Reserved21;
	UINT8 u8ErrFlag_Reserved22;
	UINT8 u8ErrFlag_CBC_DSG;
	UINT8 u8ErrFlag_SOC_Cail;
	UINT8 u8ErrFlag_TempBreak;
	UINT8 u8Res6;
};

union System_Status {
    UINT32 all;
    struct System_Status_Flag {
		UINT8 b1StartUpBMS             :1;
		UINT8 b1Status_MOS_PRE         :1;
		UINT8 b1Status_MOS_CHG         :1;
		UINT8 b1Status_MOS_DSG         :1;
		UINT8 b1Status_Relay_PRE       :1;
		UINT8 b1Status_Relay_CHG       :1;
		UINT8 b1Status_Relay_DSG       :1;
		UINT8 b1Status_Relay_MAIN      :1;
		UINT8 b1Status_ReservedHeat    :1;
		UINT8 b1Status_ReservedCool    :1;
		UINT8 b1Status_AFE1            :1;
		UINT8 b1Status_AFE2            :1;
		UINT8 b1Status_Balance         :1;
		UINT8 b1Status_ToSleep         :1;
		UINT8 b1Status_BnCloseIO       :1;
		UINT8 b1Status_ReservedHeatCloseIO :1;
		UINT8 b1Status_SysLimits       :1;
		UINT8 b1Status_CBCCloseIO      :1;
		UINT8 b1Status_DriverExtCtrl   :1;
		UINT8 bRcved6                  :1;
		UINT8 b4Status_ProjectVer      :4;
		UINT8 bRcved11                 :8;
     }bits;
};

/*
 * BMS core state is deliberately small. It separates measured facts,
 * protection decisions, requested actuator state and actual hardware state.
 */
#define BMS_MOS_BLOCK_CELL_VOLTAGE   (1UL << 0)
#define BMS_MOS_BLOCK_CURRENT        (1UL << 1)
#define BMS_MOS_BLOCK_TEMPERATURE    (1UL << 2)
#define BMS_MOS_BLOCK_SHORT_CIRCUIT  (1UL << 3)
#define BMS_MOS_BLOCK_SOC_LOW        (1UL << 4)
#define BMS_MOS_BLOCK_USER           (1UL << 5)
#define BMS_MOS_BLOCK_AFE            (1UL << 6)

/*
 * Coalescing pending events, not a message queue. Repeated identical events
 * remain one bit until a consumer takes them. Safety decisions never depend
 * on event delivery; they are recomputed from current state.
 */
#define BMS_CORE_EVT_FAULT_CHANGED        (1UL << 0)
#define BMS_CORE_EVT_MOS_REQUEST_CHANGED  (1UL << 1)
#define BMS_CORE_EVT_MOS_ACTUAL_CHANGED   (1UL << 2)
#define BMS_CORE_EVT_AFE_RECOVERED        (1UL << 3)
#define BMS_CORE_EVT_CONFIG_CHANGED       (1UL << 4)
#define BMS_CORE_EVT_WAKEUP               (1UL << 5)
#define BMS_CORE_EVT_SLEEP_BLOCK_CHANGED  (1UL << 6)

struct BMS_MEASUREMENT_STATE {
	UINT16 cellMinMv;
	UINT16 cellMaxMv;
	UINT16 chargeCurrentA10;
	UINT16 dischargeCurrentA10;
	UINT16 socPercent;
};

struct BMS_PROTECTION_STATE {
	UINT32 swChargeBlock;
	UINT32 swDischargeBlock;
	UINT32 hwChargeBlock;
	UINT32 hwDischargeBlock;
};

struct BMS_MOS_STATE {
	UINT8 chargeRequest;
	UINT8 dischargeRequest;
	UINT8 chargeActual;
	UINT8 dischargeActual;
};

struct BMS_CORE_STATE {
	struct BMS_MEASUREMENT_STATE measurement;
	struct BMS_PROTECTION_STATE protection;
	struct BMS_MOS_STATE mos;
	UINT32 sleepBlockReason;
};

extern volatile struct SYSTEM_ERROR System_ErrFlag;
extern volatile union System_Status s_system_status;

void SystemRuntime_Init(void);
void InitSystemMonitorData_EEPROM(void); /* Legacy wrapper. */
UINT8 System_ERROR_UserCallback(enum SYSTEM_ERROR_COMMAND errorCode);
void SystemRuntime_MarkBootReady(void);
void SystemRuntime_SetProjectVersion(UINT8 project_version);
void SystemRuntime_SetAfeStatus(UINT8 afe_index, UINT8 is_ok);
void SystemRuntime_SetMosStatus(UINT8 charge_on, UINT8 discharge_on);
UINT8 SystemRuntime_IsChargeMosOpen(void);
UINT8 SystemRuntime_IsDischargeMosOpen(void);
UINT32 SystemRuntime_GetStatusSnapshot(void);

void BmsCore_Init(void);
void BmsCore_UpdateMeasurement(UINT16 cell_min_mv,
							   UINT16 cell_max_mv,
							   UINT16 charge_current_a10,
							   UINT16 discharge_current_a10,
							   UINT16 soc_percent);
void BmsCore_SetProtectionBlocks(UINT32 sw_charge_block,
								UINT32 sw_discharge_block,
								UINT32 hw_charge_block,
								UINT32 hw_discharge_block);
UINT8 BmsCore_SetMosRequest(UINT8 charge_on, UINT8 discharge_on);
void BmsCore_SetSleepBlockReason(UINT32 reason);
UINT32 BmsCore_GetSleepBlockReason(void);
void BmsCore_GetState(struct BMS_CORE_STATE *state);

void BmsEvent_Set(UINT32 events);
UINT32 BmsEvent_Take(UINT32 mask);
UINT32 BmsEvent_Peek(void);

#endif /* SYSTEM_MONITOR_H */
