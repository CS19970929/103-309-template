#ifndef LOG_RECORD_H
#define LOG_RECORD_H

/*
 * Event records are exposed as one Modbus register per log item (event, delta).
 * The transport still limits each response to RS485_MAX_BUFFER_SIZE, but the
 * internal read-window scratch must hold the complete 500-record logical area
 * because Sci_Upper.c builds the window first and then slices the requested
 * Modbus range. This only enlarges the shared scratch buffer; it does not
 * enlarge RS485MSG or the on-wire frame size.
 */
#if defined(SCI_TX_BUF_LEN) && defined(FLASH_STORAGE_LOG_RECORD_COUNT)
#if (SCI_TX_BUF_LEN < (FLASH_STORAGE_LOG_RECORD_COUNT * 2U))
#undef SCI_TX_BUF_LEN
#define SCI_TX_BUF_LEN (FLASH_STORAGE_LOG_RECORD_COUNT * 2U)
#endif
#endif

typedef enum _LogEventArray {
	BMS_EVENT_NULL1 = 0,
	BMS_START_UP,
	BMS_SLEEP,
	BALANCE_OPEN,
	EVENT_RESERVED_4,
	EVENT_RESERVED_5,
	
	VCELL_OVP,
	VBUS_OVP,
	CHG_OCP,
	
	VCELL_UVP,
	VBUS_UVP,
	DSG_OCP,

	CHG_UTP,
	DSG_UTP,
	CHG_OTP,
	DSG_OTP,
	VDELTA_OP,
	CBC_ERR,
	AFE1_ERR,
	AFE2_ERR,
	EEPROM_ERR,

	EVENT_NUM
}LogEventArray;


typedef union __LOG_RECORD_FLAG {
    UINT8 all;
    struct _LOG_RECORD_FLAG {
		UINT8 Log_StartUp  		:1;
		UINT8 Log_Sleep     	:1;
		UINT8 BatOvp_Third      :1;
		UINT8 BatUvp_Third      :1;
		
		UINT8 Rcv				:4;
     }bits;
}LOG_RECORD_FLAG;


extern UINT32 su32_Interval_S_Tcnt ;

void App_LogRecord(void);
void LogRecord_RequestStartup(void);
void LogRecord_RequestSleep(void);
void Sci_ACK_0x03_ReadRegs_EventRecord(UINT8 t_u8BuffTemp[]);
void Sci_WrReg_0x06_Reset_EventRecord(struct RS485MSG *s);
UINT8 EEPROM_ResetData_EventRecord_ToDefault(void);
void ReadEEPROM_EventRecord_Parameters(void);

#endif	/* LOG_RECORD_H */
