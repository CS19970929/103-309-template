#ifndef FAULT_SNAPSHOT_H
#define FAULT_SNAPSHOT_H

#define FAULT_REASON_HARD ((UINT16)0x4846U)
#define FAULT_REASON_MEM ((UINT16)0x4D46U)
#define FAULT_REASON_BUS ((UINT16)0x4246U)
#define FAULT_REASON_USAGE ((UINT16)0x5546U)

extern volatile UINT16 g_u16FaultReasonSnapshot;
extern volatile UINT16 g_u16FaultReasonSnapshotInv;

#endif
