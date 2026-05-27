#ifndef CAN_HDX_H
#define CAN_HDX_H


#define CAN_ADRESS_STD_ID				0x00

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
struct CAN_ERROR_SNAPSHOT {
	UINT8 u8LastErrorCode;
	UINT8 u8ReceiveErrorCounter;
	UINT8 u8TransmitErrorCounter;
	UINT8 u8ErrorWarning;
	UINT8 u8ErrorPassive;
	UINT8 u8BusOff;
	UINT16 u16AckErrorCnt;
	UINT16 u16TxFailedCnt;
	UINT16 u16TxTimeoutCnt;
	UINT16 u16TxAbortCnt;
	UINT16 u16TxNoMailboxCnt;
	UINT16 u16BusOffCnt;
};

extern volatile struct CAN_ERROR_SNAPSHOT g_stCanErrorSnapshot;

struct CAN_LOW_POWER_STATUS {
	UINT8 u8PowerState;
	UINT8 u8BusActive;
	UINT8 u8NoAckCnt;
	UINT8 u8ProbeActive;
	UINT8 u8TxMailbox;
	UINT8 u8RtcServiceActive;
	UINT8 u8LastRtcWakeTxAcked;
	UINT8 u8LastRtcWakeTimeout;
	UINT16 u16PendingMask;
	UINT16 u16RtcWakeServiceCnt;
	UINT16 u16PrepareSleepCnt;
	UINT32 u32LogicalTick;
	UINT32 u32LastRtcElapsedSeconds;
};

extern volatile struct CAN_LOW_POWER_STATUS g_stCanLowPowerStatus;
#endif

void InitCan(void);
void App_Can(void);
UINT8 Can_HDX_Transmit(CanTxMsg *Msg);
UINT8 Can_IsBusy(void);
void Can_PrepareSleep(void);
UINT8 Can_IsBusActive(void);
UINT32 Can_GetIdleRtcPeriodSeconds(void);
void Can_RtcWakeService(UINT32 elapsed_seconds);

#endif
