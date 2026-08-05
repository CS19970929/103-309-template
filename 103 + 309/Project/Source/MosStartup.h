#ifndef MOS_STARTUP_H
#define MOS_STARTUP_H

#include <stdbool.h>
#include "stm32f10x.h"

UINT8 IsChargeActive(void);
void MosStartup_OpenChargeCloseDischarge(void);
void MosStartup_OpenDischargeCloseCharge(void);
void MosStartup_EnterFactoryMode(bool on);
void MosStartup_ApplyInitialState(void);
void Mos_OpenAll(void);

/* Keep legacy call sites readable while the module boundary is tightened. */
#define open_chg_close_dsg() MosStartup_OpenChargeCloseDischarge()
#define open_dsg_close_chg() MosStartup_OpenDischargeCloseCharge()
#define enter_fac_mode(on)   MosStartup_EnterFactoryMode(on)

#endif
