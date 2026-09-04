#include "main.h"

/*
 * Compatibility compile slot only.
 * The historical Keil project already compiles SH367309_Func.c. Keeping this
 * file avoids a risky project-file rewrite while the implementation is fully
 * replaced by the SH3673520 coordinated-protection module below.
 */
#include "afe3520/BmsProtection3520.c"

/* Legacy low-power code calls this name after sampling AFE status. The new
 * protection arbiter owns the actual MOS decision and is the single receiver. */
void Fault_ChangeToMCU(void)
{
    Bms3520_ProtectionService();
}
