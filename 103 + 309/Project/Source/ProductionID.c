#include "main.h"

PRODUCTION_ID_INFO ProductionInfor;

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
	ProductionInfor.BMS_SerialNumber_WriteFlag = 0;
	ProductionInfor.BMS_HardWareVersion_WriteFlag = 0;
	ProductionInfor.BMS_SoftWareVersion_WriteFlag = 0;
}

void InitProID(void)
{
	InitProID_DefaultData();
}

void WriteProID(void)
{
	ProductionInfor.BMS_SerialNumber_WriteFlag = 0;
	ProductionInfor.BMS_HardWareVersion_WriteFlag = 0;
	ProductionInfor.BMS_SoftWareVersion_WriteFlag = 0;
}

void WriteProID_Default(void)
{
	InitProID_DefaultData();
}

void App_ProID_Deal(void)
{
	static UINT8 su8_StartUpFlag = 0;

	if (!su8_StartUpFlag)
	{
		InitProID();
		su8_StartUpFlag = 1;
	}
	else
	{
		WriteProID();
	}
}
