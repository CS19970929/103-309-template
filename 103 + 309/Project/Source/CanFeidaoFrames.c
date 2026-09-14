/*
 * Legacy Keil build bridge.
 *
 * The existing .uvprojx already compiles CanFeidaoFrames.c.  To keep the
 * legacy project file unchanged, this translation unit includes the clearly
 * named protocol modules below.
 *
 * When migrating to CMake/GCC or after cleaning the Keil project file:
 *   1. add these four .c files as normal independent source files;
 *   2. remove the includes below;
 *   3. keep this file as the historical empty Feidao placeholder or remove it.
 */

#include "LxPowerProtocol.c"
#include "LxPowerProtocolPort.c"
#include "LegacyModbusProtocol.c"
#include "SerialProtocolMux.c"
