#include "main.h"
#include "DebugWatch.h"

struct PRT_E2ROM_PARAS PRT_E2ROMParas;

union FAULT_FLAG_FIRST Fault_Flag_Fisrt;
union FAULT_FLAG_SECOND Fault_Flag_Second;
union FAULT_FLAG_THIRD Fault_Flag_Third;

UINT16 Fault_record_Third[Record_len];

UINT16 Fault_record_Third2[Record_len];

UINT8 FaultPoint_Third;

UINT8 FaultPoint_Third2;

void FaultWarnRecord2(enum FaultFlag num);

#if DEBUG_WATCH_ENABLED
void Fault_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->fault.protect = &PRT_E2ROMParas;
	watch->public_data.protect = &PRT_E2ROMParas;
	watch->fault.first = &Fault_Flag_Fisrt;
	watch->fault.second = &Fault_Flag_Second;
	watch->fault.third = &Fault_Flag_Third;
	watch->fault.record_third = Fault_record_Third;
	watch->fault.record_third2 = Fault_record_Third2;
	watch->fault.point_third = &FaultPoint_Third;
	watch->fault.point_third2 = &FaultPoint_Third2;
}
#endif

void FaultWarnRecord2(enum FaultFlag num)
{
	if (FaultPoint_Third2 >= Record_len)
	{
		FaultPoint_Third2 = 0;
	}
	Fault_record_Third2[FaultPoint_Third2++] = num;
}
