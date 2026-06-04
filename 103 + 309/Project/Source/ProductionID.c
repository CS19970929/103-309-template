#include "main.h"
#include "DebugWatch.h"

PRODUCTION_ID_INFO ProductionInfor;

#if DEBUG_WATCH_ENABLED
void ProductionID_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->public_data.production = &ProductionInfor;
}
#endif

static void InitProID_DefaultData(void)
{
	UINT8 harewareCount = sizeof(BMS_HARDWARE_VERDION_DEFAULT) > PRODUCT_ID_LENGTH_MAX ? PRODUCT_ID_LENGTH_MAX : sizeof(BMS_HARDWARE_VERDION_DEFAULT);
	UINT8 softwareCount = sizeof(BMS_SOFTWARE_VERDION_DEFAULT) > PRODUCT_ID_LENGTH_MAX ? PRODUCT_ID_LENGTH_MAX : sizeof(BMS_SOFTWARE_VERDION_DEFAULT);
	UINT8 serialNumberCount = sizeof(BMS_SERIAL_NUMBER_DEFAULT) > PRODUCT_ID_LENGTH_MAX ? PRODUCT_ID_LENGTH_MAX : sizeof(BMS_SERIAL_NUMBER_DEFAULT);

	memset(&ProductionInfor, 0, sizeof(PRODUCTION_ID_INFO));
	memcpy(&ProductionInfor.BMS_HardWareVersion[0], BMS_HARDWARE_VERDION_DEFAULT, harewareCount);
	memcpy(&ProductionInfor.BMS_SoftWareVersion[0], BMS_SOFTWARE_VERDION_DEFAULT, softwareCount);
	memcpy(&ProductionInfor.BMS_SerialNumber[0], BMS_SERIAL_NUMBER_DEFAULT, serialNumberCount);

	ProductionInfor.BMS_SerialNumberLength = serialNumberCount;
	ProductionInfor.BMS_HardWareVersionLength = harewareCount;
	ProductionInfor.BMS_SoftWareVersionLength = softwareCount;
}

void InitProID(void)
{
	InitProID_DefaultData();
}


void WriteProID_Default(void)
{
	InitProID_DefaultData();
}

void App_ProID_Deal(void)
{
	/* Production ID is initialized during boot; keep this hook for runtime heartbeat. */
}
