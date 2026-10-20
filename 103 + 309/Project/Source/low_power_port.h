#ifndef LOW_POWER_PORT_H
#define LOW_POWER_PORT_H

#include "low_power.h"

/* Compile-time ports: no heap, callbacks or runtime registration. */
uint32_t BoardLowPower_Lock(void);
void BoardLowPower_Unlock(uint32_t mask);
void BoardLowPower_Init(void);
uint32_t BoardLowPower_NowMs(void);
uint32_t BoardLowPower_RtcCounter(void);
void BoardLowPower_Snapshot(void);
uint32_t BoardLowPower_ActiveWake(void);
uint8_t BoardLowPower_Begin(void);
/* 0 waiting, 1 ready, 2 timeout. */
uint8_t BoardLowPower_WaitQuiet(void);
void BoardLowPower_ArmWake(void);
void BoardLowPower_PrepareNextStop(void);
void BoardLowPower_Stop(void);
uint8_t BoardLowPower_RtcInit(void);
uint8_t BoardLowPower_RtcArm(void);
uint8_t BoardLowPower_CaptureWake(void);
void BoardLowPower_BeginSample(void);
uint8_t BoardLowPower_PollSample(void);
void BoardLowPower_BeginUart1Check(void);
uint8_t BoardLowPower_PollUart1Check(void);
void BoardLowPower_EndUart1Check(uint8_t accepted);
void BoardLowPower_Restore(void);
void BoardLowPower_AccountSeconds(uint32_t seconds);

void BmsSleep_PollRequest(void);
LowPowerTarget BmsSleep_GetTarget(void);
void BmsSleep_Request(LowPowerTarget target);
uint32_t BmsSleep_BlockMask(uint8_t refresh_afe);
uint32_t BmsSleep_SampleExit(void);
uint8_t BmsSleep_CommitReset(void);
void BmsSleep_ResetQualification(void);
void BmsSleep_ConsumeSleepUartActivity(void);

#endif
