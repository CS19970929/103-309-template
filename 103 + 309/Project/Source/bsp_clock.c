#include "main.h"
#include "bsp_clock.h"

void BSP_Clock_RestoreAfterStop(void)
{
    cpu_frequency_conf();
}
