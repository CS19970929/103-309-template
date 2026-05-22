#include "main.h"

struct PRT_E2ROM_PARAS PRT_E2ROMParas;

union FAULT_FLAG_FIRST Fault_Flag_Fisrt;
union FAULT_FLAG_SECOND Fault_Flag_Second;
union FAULT_FLAG_THIRD Fault_Flag_Third;

UINT16 Fault_record_Third[Record_len];

UINT16 Fault_record_First2[Record_len];
UINT16 Fault_record_Second2[Record_len];
UINT16 Fault_record_Third2[Record_len];

UINT8 FaultPoint_First;
UINT8 FaultPoint_Second;
UINT8 FaultPoint_Third;

UINT8 FaultPoint_First2;
UINT8 FaultPoint_Second2;
UINT8 FaultPoint_Third2;

void FaultWarnRecord(enum FaultFlag num);
void FaultWarnRecord2(enum FaultFlag num);

void FaultWarnRecord(enum FaultFlag num)
{
}

void FaultWarnRecord2(enum FaultFlag num)
{
	if (num >= 1 && num <= 13)
	{
		if (FaultPoint_First2 >= Record_len)
		{
			FaultPoint_First2 = 0;
		}
		Fault_record_First2[FaultPoint_First2++] = num;
	}
	else if (num >= 14 && num <= 26)
	{
		if (FaultPoint_Second2 >= Record_len)
		{
			FaultPoint_Second2 = 0;
		}
		Fault_record_Second2[FaultPoint_Second2++] = num;
	}
	else
	{
		if (FaultPoint_Third2 >= Record_len)
		{
			FaultPoint_Third2 = 0;
		}
		Fault_record_Third2[FaultPoint_Third2++] = num;
	}
}
