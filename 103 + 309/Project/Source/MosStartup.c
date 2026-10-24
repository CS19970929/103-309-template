#include "main.h"
#include "MosStartup.h"

UINT8 IsChargeActive(void)
{
    return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_SET);
}

void MosStartup_OpenChargeCloseDischarge(void)
{
    Fault_ApplyMosPermits(1U, 0U);
}

void MosStartup_OpenDischargeCloseCharge(void)
{
    Fault_ApplyMosPermits(0U, 1U);
}

void MosStartup_EnterFactoryMode(bool on)
{
    (void)on;
    Fault_ApplyMosPermits(1U, 1U);
}

UINT8 MosStartup_ApplyInitialState(void)
{
    return Fault_ApplyMosPermits(1U, 1U);
}

void Mos_OpenAll(void)
{
    Fault_ApplyMosPermits(1U, 1U);
}
