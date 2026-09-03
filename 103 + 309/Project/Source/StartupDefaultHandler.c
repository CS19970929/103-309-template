

/* 启动文件无条件调用该钩子；关闭 IRQ debug 时保持为空实现，确保 Debug 配置也能链接。 */
void IrqDebug_RecordUnhandledVector(void)
{
}
