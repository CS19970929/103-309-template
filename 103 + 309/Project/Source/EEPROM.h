#ifndef EEPROM_H
#define EEPROM_H

//Mini STM32������ʹ�õ���24c02
//#define AT24C02
#define DELAY_US_IIC_EEPROM		2	//4Ϊ100KHz��2Ϊ150KHz

#define sEEAddress   0xA0			//E2 = E1 = E0 = 0

#define AZONE				0x0000              // A��: 0x0000~0x0799		//2Kһ������
#define BZONE				0x0800				// B��: 0x0800~0x0999
#define CZONE				0x1000				// C��: 0x1000~0x1800

//IO��������
//�����������Ǹ���X<<2
#define SDA_IN_SEE()  {GPIOB->CRH&=0xFFFF0FFF;GPIOB->CRH|=(UINT32)8<<(3<<2);}
#define SDA_OUT_SEE() {GPIOB->CRH&=0xFFFF0FFF;GPIOB->CRH|=(UINT32)3<<(3<<2);}

//IO��������	
#define IIC_SCL_SEE    PBout(10) //SCL
#define IIC_SDA_SEE    PBout(11) //SDA
#define READ_SDA_SEE   PBin(11)  //����SDA





//����ΪEEPROM�������ݵ�˳��͸���һ����
#define E2P_PARA_NUM_PROTECT 		 		65				//Ϊ����������ӣ�Ϊ��Ҫ�ֿ�����Ϊ���32λ�����⣬��������ôʵ��
#define E2P_PARA_NUM_RTC		 			12
#define E2P_PARA_NUM_CALIB_K 		 		KB_NUM			//47
#define E2P_PARA_NUM_CALIB_B 		 		KB_NUM
#define E2P_PARA_NUM_SOC_TABLE 		 		SOC_TABLE_SIZE	//42����GetEndValue��Ե��ֻ�ܺ���һ��
#define E2P_PARA_NUM_COPPERLOSS 		 	CompensateNUM	//16
#define E2P_PARA_NUM_COPPERLOSS_NUM 		CompensateNUM
#define E2P_PARA_NUM_FAULT_RECORD 		 	(3*Record_len+ 3 +Record_len*6)//Ϊʲôdefine�����������ʱҪ�������أ���Ϊ����������������define�����ȼ��߾ͻ����
#define E2P_PARA_NUM_OTHER_ELEMENT1 		32				//�����ӣ��ٴ����ӱ�ע������������
#define E2P_PARA_NUM_RESERVED_RW_PARAM		24


//�ṹ�����ͱ��������
#define	E2P_ADDR_START_CALIB_K		    154    	//47����
#define	E2P_ADDR_START_CALIB_B		    248		//47����
#define	E2P_ADDR_START_SOC_TABLE		342    	//42����
#define	E2P_ADDR_START_COPPERLOSS		426    	//16����
#define	E2P_ADDR_START_COPPERLOSS_NUM	458    	//16����

#define E2P_ADDR_START_FAULT_RECORD 	490
#define	E2P_ADDR_START_FR_FIRST 		E2P_ADDR_START_FAULT_RECORD				//10����
#define	E2P_ADDR_START_FR_SECOND  		(E2P_ADDR_START_FAULT_RECORD+20)		//10����
#define	E2P_ADDR_START_FR_THIRD  		(E2P_ADDR_START_FAULT_RECORD+40)		//10����
//����2���ֽڵ�ָ�뱣��
#define E2P_ADDR_E2POS_FR_TEMP_FIRST	(E2P_ADDR_START_FAULT_RECORD+60)		//2����
#define E2P_ADDR_E2POS_FR_TEMP_SECOND  	(E2P_ADDR_START_FAULT_RECORD+62)		//2����
#define E2P_ADDR_E2POS_FR_TEMP_THIRD	(E2P_ADDR_START_FAULT_RECORD+64)		//2����
#define E2P_ADDR_START_FR_THIRD_RTC 	(E2P_ADDR_START_FAULT_RECORD+66)		//�ںм�¼��60����






UINT8 ReadEEPROM_Byte(UINT16 addr);
UINT8 WriteEEPROM_Byte(UINT16 addr, UINT8 val);
UINT16 ReadEEPROM_Word_NoZone(UINT16 addr);
UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data);

void InitE2PROM(void);
UINT8 EEPROM_SaveRWParametersToFlash(void);
UINT8 UpgradeParamPolicy_ApplyOnce(void);


#endif	/* EEPROM_H */
