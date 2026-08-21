#ifndef PRODUCTIONID_H
#define PRODUCTIONID_H

#define PRODUCT_ID_LENGTH_MAX 32

#define PRODUCTION_ID_FIELD_SERIAL_NUMBER     ((UINT8)0U)
#define PRODUCTION_ID_FIELD_HARDWARE_VERSION  ((UINT8)1U)
#define PRODUCTION_ID_FIELD_SOFTWARE_VERSION  ((UINT8)2U)

typedef struct {
	//均为阿斯克码
	UINT8 BMS_SerialNumber[PRODUCT_ID_LENGTH_MAX];			//BMS序列号
	UINT8 BMS_HardWareVersion[PRODUCT_ID_LENGTH_MAX];		//BMS硬件版本号
	UINT8 BMS_SoftWareVersion[PRODUCT_ID_LENGTH_MAX];		//BMS软件版本号

	UINT16 BMS_SerialNumberLength;			//BMS序列号地址				//意义不大，末端全部填0，阿斯克码为空
	UINT16 BMS_HardWareVersionLength;		//BMS硬件版本号地址			//意义不大，末端全部填0，阿斯克码为空
	UINT16 BMS_SoftWareVersionLength;		//BMS软件版本号地址			//意义不大，末端全部填0，阿斯克码为空
	
	UINT16 BMS_SerialNumberHeadAdress;		//BMS序列号地址
	UINT16 BMS_HardWareVersionHeadAdress;	//BMS硬件版本号地址
	UINT16 BMS_SoftWareVersionHeadAdress;	//BMS软件版本号地址
}PRODUCTION_ID_INFO;

extern PRODUCTION_ID_INFO ProductionInfor;


void InitProID(void);
void WriteProID_Default(void);
void App_ProID_Deal(void);
UINT8 ProductionID_UpdateField(UINT8 field, const UINT8 *data, UINT16 length);


#endif	/* PRODUCTIONID_H */
