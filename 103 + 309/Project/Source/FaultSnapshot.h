#ifndef FAULT_SNAPSHOT_H
#define FAULT_SNAPSHOT_H

/* STM32F103xB has only 10 backup data registers. DR1..DR5 are reserved
   for RTC, sleep reason, SOC-save diagnostics; DR6..DR10 record CPU faults. */
#define FAULT_BKP_REASON_REG BKP_DR6
#define FAULT_BKP_REASON_INV_REG BKP_DR7
#define FAULT_BKP_CFSR_LO_REG BKP_DR8
#define FAULT_BKP_CFSR_HI_REG BKP_DR9
#define FAULT_BKP_HFSR_HI_REG BKP_DR10

#define FAULT_REASON_HARD ((UINT16)0x4846U)
#define FAULT_REASON_MEM ((UINT16)0x4D46U)
#define FAULT_REASON_BUS ((UINT16)0x4246U)
#define FAULT_REASON_USAGE ((UINT16)0x5546U)

#endif
