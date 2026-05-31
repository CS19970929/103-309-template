#ifndef CAN_H
#define CAN_H

#include "stm32f10x.h"

#define CAN_ADRESS_STD_ID               0x00

/* ── Periodic message IDs ── */
#define CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS ((UINT16)0x0001U)
#define CAN_FEIDAO_MSG_SOC_1000MS             ((UINT16)0x0002U)
#define CAN_FEIDAO_MSG_CAP_5000MS             ((UINT16)0x0004U)
#define CAN_FEIDAO_MSG_SOH_5000MS             ((UINT16)0x0008U)
#define CAN_FEIDAO_MSG_VERSION_5000MS         ((UINT16)0x0010U)
#define CAN_FEIDAO_MSG_STATUS_5000MS          ((UINT16)0x0020U)
#define CAN_FEIDAO_MSG_FACTORY_TIME_5000MS    ((UINT16)0x0040U)

#define CAN_FEIDAO_1000MS_MSG_MASK \
    (CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS | CAN_FEIDAO_MSG_SOC_1000MS)

#define CAN_FEIDAO_5000MS_MSG_MASK \
    (CAN_FEIDAO_MSG_CAP_5000MS | \
     CAN_FEIDAO_MSG_SOH_5000MS | \
     CAN_FEIDAO_MSG_VERSION_5000MS | \
     CAN_FEIDAO_MSG_STATUS_5000MS | \
     CAN_FEIDAO_MSG_FACTORY_TIME_5000MS)

#define CAN_FEIDAO_RTC_PROBE_MSG_MASK CAN_FEIDAO_MSG_VOLTAGE_CURRENT_1000MS

/* ── Debug watch structures ── */
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
struct CAN_ERROR_SNAPSHOT {
    UINT8  u8LastErrorCode;
    UINT8  u8ReceiveErrorCounter;
    UINT8  u8TransmitErrorCounter;
    UINT8  u8ErrorWarning;
    UINT8  u8ErrorPassive;
    UINT8  u8BusOff;
    UINT16 u16AckErrorCnt;
    UINT16 u16TxFailedCnt;
    UINT16 u16TxTimeoutCnt;
    UINT16 u16TxAbortCnt;
    UINT16 u16TxNoMailboxCnt;
    UINT16 u16BusOffCnt;
};

extern volatile struct CAN_ERROR_SNAPSHOT g_stCanErrorSnapshot;

struct CAN_LOW_POWER_STATUS {
    UINT8  u8PowerState;
    UINT8  u8BusActive;
    UINT8  u8NoAckCnt;
    UINT8  u8ProbeActive;
    UINT8  u8TxMailbox;
    UINT8  u8RtcServiceActive;
    UINT8  u8LastRtcWakeTxAcked;
    UINT8  u8LastRtcWakeTimeout;
    UINT16 u16PendingMask;
    UINT16 u16RtcWakeServiceCnt;
    UINT16 u16PrepareSleepCnt;
    UINT32 u32LogicalTick;
    UINT32 u32LastRtcElapsedSeconds;
};

extern volatile struct CAN_LOW_POWER_STATUS g_stCanLowPowerStatus;
#endif

/* ── Public API ── */
void     InitCan(void);
void     App_Can(void);
UINT8    Can_HDX_Transmit(CanTxMsg *Msg);
UINT8    Can_IsBusy(void);
void     Can_PrepareSleep(void);
UINT8    Can_IsBusActive(void);
UINT32   Can_GetIdleRtcPeriodSeconds(void);
void     Can_RtcWakeService(UINT32 elapsed_seconds);
UINT8    CanFeidao_SendNextPending(UINT16 *pending_mask);

#endif  /* CAN_H */