#ifndef SOC_AUTO_TEST_H
#define SOC_AUTO_TEST_H

#include "stm32f10x.h"

typedef struct _SOC_AUTO_TEST_REPORT {
    UINT8 u8Enabled;
    UINT8 u8Running;
    UINT8 u8Done;
    UINT8 u8Passed;
    UINT16 u16CaseTotal;
    UINT16 u16CaseIndex;
    UINT16 u16CasePassed;
    UINT16 u16CaseFailed;
    UINT16 u16FailCode;
    UINT16 u16Step;
    UINT32 u32TickTotal;
    UINT8 u8ActualSoc;
    UINT8 u8TruthSoc;
    UINT8 u8ExpectedMinSoc;
    UINT8 u8ExpectedMaxSoc;
    UINT16 u16LastVMin_mV;
    UINT16 u16LastVMax_mV;
    UINT16 u16LastIchg_A10;
    UINT16 u16LastIDsg_A10;
    UINT16 u16ObservedVMin_mV;
    UINT16 u16ObservedVMax_mV;
    UINT16 u16ObservedIDsgMin_A10;
    UINT16 u16ObservedIDsgMax_A10;
} SOC_AUTO_TEST_REPORT;

extern SOC_AUTO_TEST_REPORT g_stSocAutoTestReport;

void SocAutoTest_Task(void);
void SocAutoTest_Reset(void);

#endif
