#ifndef RUNTIME_LOG_H
#define RUNTIME_LOG_H

#include <stdint.h>

void RuntimeLog_Init(void);
void RuntimeLog_BootReady(void);
void RuntimeLog_Task1s(void);
void RuntimeLog_LowPowerRequest(uint8_t mode, uint32_t block, uint32_t idle, uint32_t idle_max);
void RuntimeLog_ResetSleepCommit(uint8_t mode);
void RuntimeLog_RtcStopEnter(uint32_t cycles, uint32_t sleep_seconds);
void RuntimeLog_RtcStopWake(uint8_t rtc_wake, uint32_t elapsed_seconds, uint32_t sleep_seconds);
void RuntimeLog_RtcStopExit(uint8_t wake_source, uint32_t sleep_seconds, uint32_t cycles);

#endif /* RUNTIME_LOG_H */
