#include "main.h"

PRODUCTION_ID_INFO ProductionInfor;

static void ProductionID_ApplySerialNumber(const UINT8 *serial)
{
    UINT16 length=0U;
    memcpy(ProductionInfor.BMS_SerialNumber,serial,PRODUCT_ID_LENGTH_MAX);
    while (length<PRODUCT_ID_LENGTH_MAX && serial[length]!=0U) ++length;
    ProductionInfor.BMS_SerialNumberLength=(UINT16)(
        length<PRODUCT_ID_LENGTH_MAX ? length+1U : PRODUCT_ID_LENGTH_MAX);
}

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
	UINT8 serial[PRODUCT_ID_LENGTH_MAX];
	InitProID_DefaultData();
	if (StorageFlash_ReadSerialNumber(serial,sizeof(serial)))
		ProductionID_ApplySerialNumber(serial);
}

UINT8 ProductionID_WriteSerialNumber(const UINT8 *serial)
{
    /* Publish only the value confirmed by the committed A/B Flash record. */
    if (!StorageFlash_WriteSerialNumber(serial,PRODUCT_ID_LENGTH_MAX)) return 0U;
    ProductionID_ApplySerialNumber(serial);
    return 1U;
}
