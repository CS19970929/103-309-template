#ifndef CAN_HDX_H
#define CAN_HDX_H


#define CAN_ADRESS_STD_ID				0x00


void InitCan(void);
void App_Can(void);
UINT8 Can_HDX_Transmit(CanTxMsg *Msg);
UINT8 Can_PeekBusy(void);
UINT8 Can_IsBusy(void);
void Can_PrepareSleep(void);

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE
void Can_GetDebugSnapshot(uint8_t *power_on,
                          uint8_t *bus_off,
                          uint8_t *tx_queue,
                          uint16_t *esr);
#endif

#endif
