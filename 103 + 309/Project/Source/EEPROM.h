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


#define EEPROM_ADDR_PASS           			((UINT16)0x3FFC)
#define EEPROM_ADDR_SLEEP           		((UINT16)0x3FFA)
#define EEPROM_ADDR_FLASHUPDATE     		((UINT16)0x3FFE)

#define EEPROM_VALUE_SLEEP    				((UINT16)0xABCD)
#define EEPROM_VALUE_SLEEP_RESET    		((UINT16)0xFFFF)
#define EEPROM_VALUE_FLASHUPDATE    		((UINT16)0xABCD)
#define EEPROM_VALUE_FLASHUPDATE_RESET    	((UINT16)0xFFFF)


//#define EEPROM_ADDR_SLEEPMODE     			2036	//ȡ������ΪFLASH
#define EEPROM_ADDR_SWITCH_ONOFF     		2040
#define EEPROM_ADDR_SYS_FUNC_SELECT     	2044



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
#define E2P_ADDR_E2POS_PROTECT 			{0,  2,  4,  6,  8,  10, 12, 14, 16, 18,\
						 				 20, 22, 24, 26, 28, 30, 32, 34, 36, 38,\
						 				 40, 42, 44, 46, 48, 50, 52, 54, 56, 58,\
						 				 \
						 				 60, 62, 64, 66, 68, 70, 72, 74, 76, 78,\
						 				 80, 82, 84, 86, 88, 90, 92, 94, 96, 98,\
						 				 100,102,104,106,108,\
						 				 \
						 				 110,112,114,116,118,120,122,124,126,128}

#define E2P_ADDR_E2POS_RTC 				{130,132,134,136,138,140,\
						   				 142,144,146,148,150,152}


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

#define E2P_ADDR_START_OTHER_ELEMENT1	676		//E2P_ADDR_START_FR_THIRD_RTC + 120 = E2P_ADDR_START_FAULT_RECORD+66+120

#define E2P_ADDR_E2POS_OTHER_ELEMENT1 	{676,678,680,682,684,686,688,690,\
									 	 692,694,696,698,700,702,704,706,\
									 	 708,710,712,714,716,718,720,722,\
									 	 724,726,728,730,\
									 	 732,734,736,738}

#define E2P_ADDR_E2POS_RESERVED_RW_PARAM {740,742,744,746,748,750,752,754,756,758,760,762,764,\
										 766,768,770,772,774,776,778,780,782,784,788}
#if 0
#define E2P_ADDR_E2POS_ENHANCE_SOC 		{790,792,794,796,798,800,802,804,\
										 806,808,810,812,814,816,818,820} 		//����ǲ�������λ���ĵ�
#endif

#define E2P_ADDR_E2POS_ENHANCE_SOC		790		//��828

#define E2P_ADDR_E2POS_SERIAL_NUM		830		//��868
#define E2P_ADDR_E2POS_HAEDWARE_VER		870		//��908
#define E2P_ADDR_E2POS_SOFTWARE_VER		910		//��948

#define E2P_ADDR_START_EVENT_RECORD 	1000	//��1198		//100����
#define E2P_ADDR_E2POS_EVENT_POINT		1200	//��һ��1202

#define E2P_ADDR_SH367309_VALUE		1500	//��һ��1202

UINT8 ReadEEPROM_Byte(UINT16 addr);
UINT8 WriteEEPROM_Byte(UINT16 addr, UINT8 val);
UINT16 ReadEEPROM_Word_NoZone(UINT16 addr);
UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data);

void InitE2PROM(void);
void App_E2promDeal(void);
UINT8 EEPROM_SaveRWParametersToFlash(void);
UINT8 UpgradeParamPolicy_ApplyOnce(void);

void EEPROM_test(void);

extern uint16_t curr_offset;
extern UINT16 OffsetValue_CHG ;
extern UINT16 OffsetValue_DSG;

#endif	/* EEPROM_H */

