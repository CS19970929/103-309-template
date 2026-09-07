#include "stm32f10x.h"
#include "conf/Project_Config.h"
#include "afe3520/Afe3520Config.h"
void SleepDeal_CommitEmergencyBoot(void);

#if PROJECT_CFG_IRQ_DEBUG_ENABLE
/* Available in the debugger if an unhandled vector reaches Default_Handler. */
volatile uint32_t g_unhandledVector;
#endif

void IrqDebug_RecordUnhandledVector(void)
{
#if PROJECT_CFG_IRQ_DEBUG_ENABLE
    g_unhandledVector = SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk;
#endif
#if AFE3520_CFG_FATAL_SLEEP_ENABLE
    SleepDeal_CommitEmergencyBoot();
#endif
}
