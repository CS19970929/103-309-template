#ifndef CAN_HDX_H
#define CAN_HDX_H


#define CAN_ADRESS_STD_ID				0x00


void InitCan(void);
void App_Can(void);
UINT8 Can_HDX_Transmit(CanTxMsg *Msg);
UINT8 Can_IsBusy(void);
void Can_PrepareSleep(void);
UINT8 Can_IsBusActive(void);
UINT32 Can_GetIdleRtcPeriodSeconds(void);
void Can_RtcWakeService(UINT32 elapsed_seconds);

#endif
