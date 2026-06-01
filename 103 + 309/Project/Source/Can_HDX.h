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

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE
void Can_GetDebugSnapshot(uint8_t *bus_active, uint8_t *power_on,
                          uint8_t *bus_off,  uint8_t *no_ack_cnt,
                          uint8_t *tx_queue, uint8_t *probe,
                          uint8_t *rtc_svc,  uint16_t *esr);
#endif

#endif
