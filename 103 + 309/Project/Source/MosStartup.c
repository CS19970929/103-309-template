#include "main.h"
#include "MosStartup.h"

UINT8 MosStartup_ApplyInitialState(void)
{
    return Fault_ApplyMosPermits(1U, 1U);
}
