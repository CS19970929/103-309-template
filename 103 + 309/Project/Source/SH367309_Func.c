#include "main.h"

/*
 * Compatibility compile slot only.
 * The historical Keil project already compiles SH367309_Func.c. Keeping this
 * file avoids a risky project-file rewrite while the implementation is fully
 * replaced by the SH3673520 coordinated-protection module below.
 */
#include "afe3520/BmsProtection3520.c"
