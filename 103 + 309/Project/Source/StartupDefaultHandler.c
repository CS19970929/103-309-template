

#include "main.h"

/* 启动文件无条件调用该钩子；关闭 IRQ debug 时保持为空实现，确保 Debug 配置也能链接。 */
void IrqDebug_RecordUnhandledVector(void)
{
	IRQ_DEBUG_ENTER(unhandled);
	IRQ_DEBUG_SET(last_unhandled_vector, SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk);
}
