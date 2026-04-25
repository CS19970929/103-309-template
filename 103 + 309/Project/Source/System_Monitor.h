#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

//ϵͳ�����������
typedef enum {STARTUP_CONT = 0, STARTUP_OVER = !STARTUP_CONT} StartUp_Status;

enum SYSTEM_ERROR_COMMAND {
	ERROR_AFE1 = 1,			//Ϊ�˷���ʹ�ú�������ֵ
	ERROR_AFE2,
	ERROR_CAN,
	ERROR_EEPROM_COM,
	ERROR_SPI,
	ERROR_UPPER,
	ERROR_CLIENT,
	ERROR_SCREEN,
	
	ERROR_WIFI,
	ERROR_BLUETOOTH,
	ERROR_APP,
	ERROR_CBC_CHG,
	ERROR_CBC_DSG,
	ERROR_EEPROM_STORE,
	ERROR_HSE,
	ERROR_LSE,
	ERROR_VDEATLE_OVER,
	
	ERROR_BALANCED,
	ERROR_ADC,
	ERROR_SOC_CAIL,
	ERROR_HEAT,
	ERROR_COOL,
	ERROR_TEMP_BREAK,
	ERROR_NUM = ERROR_TEMP_BREAK,


	ERROR_REMOVE_AFE1,
	ERROR_REMOVE_AFE2,
	ERROR_REMOVE_CAN,
	ERROR_REMOVE_EEPROM_COM,
	ERROR_REMOVE_SPI,
	ERROR_REMOVE_UPPER,
	ERROR_REMOVE_CLIENT,
	ERROR_REMOVE_SCREEN,
	
	ERROR_REMOVE_WIFI,
	ERROR_REMOVE_BLUETOOTH,
	ERROR_REMOVE_APP,
	ERROR_REMOVE_CBC_CHG,
	ERROR_REMOVE_CBC_DSG,
	ERROR_REMOVE_EEPROM_STORE,
	ERROR_REMOVE_HSE,
	ERROR_REMOVE_LSE,
	ERROR_REMOVE_VDEATLE_OVER,
	
	ERROR_REMOVE_BALANCED,
	ERROR_REMOVE_ADC,
	ERROR_REMOVE_HEAT,
	ERROR_REMOVE_COOL,
	ERROR_REMOVE_SOC_CAIL,
	ERROR_REMOVE_TEMP_BREAK,

	ERROR_STATUS_AFE1,
	ERROR_STATUS_AFE2,
	ERROR_STATUS_CAN,
	ERROR_STATUS_EEPROM_COM,
	ERROR_STATUS_SPI,
	ERROR_STATUS_UPPER,
	ERROR_STATUS_CLIENT,
	ERROR_STATUS_SCREEN,
	
	ERROR_STATUS_WIFI,
	ERROR_STATUS_BLUETOOTH,
	ERROR_STATUS_APP,
	ERROR_STATUS_CBC_CHG,
	ERROR_STATUS_CBC_DSG,
	ERROR_STATUS_EEPROM_STORE,
	ERROR_STATUS_HSE,
	ERROR_STATUS_LSE,
	ERROR_STATUS_VDEATLE_OVER,
	
	ERROR_STATUS_BALANCED,
	ERROR_STATUS_ADC,
	ERROR_STATUS_HEAT,
	ERROR_STATUS_COOL,
	ERROR_STATUS_SOC_CAIL,
	ERROR_STATUS_TEMP_BREAK,
};


struct SYSTEM_ERROR {	
	UINT8 u8ErrFlag_Com_AFE1;
	UINT8 u8ErrFlag_Com_AFE2;
	UINT8 u8ErrFlag_Com_Can;
	UINT8 u8ErrFlag_Com_EEPROM;
	
	UINT8 u8ErrFlag_Com_SPI;
	UINT8 u8ErrFlag_Com_Upper;
	UINT8 u8ErrFlag_Com_Client;
	UINT8 u8ErrFlag_Com_Screen;
	
	UINT8 u8ErrFlag_Com_Wifi;
	UINT8 u8ErrFlag_Com_BlueTooth;
	UINT8 u8ErrFlag_Com_App;
	UINT8 u8ErrFlag_CBC_CHG;
	
	UINT8 u8ErrFlag_Store_EEPROM;
	UINT8 u8ErrFlag_HSE;
	UINT8 u8ErrFlag_LSE;
	UINT8 u8ErrFlag_Vdelta_OVER;
	
	UINT8 u8ErrFlag_Balanced;
	UINT8 u8ErrFlag_ADC;
	UINT8 u8ErrFlag_Heat;
	UINT8 u8ErrFlag_Cool;
	
	UINT8 u8ErrFlag_CBC_DSG;
	UINT8 u8ErrFlag_SOC_Cail;
	UINT8 u8ErrFlag_TempBreak;
	UINT8 u8Res6;
};


//ϵͳ����ʱ���������
enum SYSTEM_FUNC_STARTUP_COMMAND {
	SYSTEM_FUNC_STARTUP_SOC = 1,			//Ϊ�˷���ʹ�ú�������ֵ
	SYSTEM_FUNC_STARTUP_BALANCE,
	SYSTEM_FUNC_STARTUP_PROTECT,
	SYSTEM_FUNC_STARTUP_MOS,
	SYSTEM_FUNC_STARTUP_RELAY,
	SYSTEM_FUNC_STARTUP_ADC,
	SYSTEM_FUNC_STARTUP_CAN,
	SYSTEM_FUNC_STARTUP_HEAT,	
	SYSTEM_FUNC_STARTUP_COOL,
	SYSTEM_FUNC_STARTUP_NUM = SYSTEM_FUNC_STARTUP_COOL,
};


union System_Function_StartUp {				//���ܳ�ʼ��ͳһ����
    UINT32 all;
    struct System_Func_StartUp_Flag {
		UINT8 b1StartUpFlag_SOC		:1;		//SOC��ʼ����Ŀǰ���ڿ�·��ѹ�Ͱ�ʱ���֣��迹���ٲ����õ�����Ϊ�������ļ�����
		UINT8 b1StartUpFlag_Balance	:1;		//�����ʼ��
		UINT8 b1StartUpFlag_Protect :1;		//�������ܳ�ʼ��
		UINT8 b1StartUpFlag_MOS     :1;		//MOS�ܳ�ʼ��
		
		UINT8 b1StartUpFlag_Relay   :1;		//�̵����ܳ�ʼ��
		UINT8 b1StartUpFlag_ADC     :1;		//ADC��ʼ��
		UINT8 b1StartUpFlag_CAN		:1;		//Can��ʼ��
		UINT8 b1StartUpFlag_Cool	:1;		//�����������Լ�

		UINT8 b1StartUpFlag_Heat	:1;		//ɢ�����������Լ�
		UINT8 b1StartUpFlag_AFE1	:1;		//AFE1		//����AFE���ϵ绽�Ѻ���ʼ���꣬�����������Ҫ����Ȼ̫������
		UINT8 b1StartUpFlag_AFE2	:1;		//AFE2
		UINT8 b1StartUpFlag_BlueT	:1;		//���������Լ�
		
		UINT8 bRcved7				:1;		//res
		UINT8 bRcved8				:1;		//res
		UINT8 bRcved9				:1;		//res
		UINT8 bRcved10				:1;		//res

		UINT8 bRcved11				:8;		//res
		UINT8 bRcved12				:8;		//res
     }bits;
};


//ϵͳ״̬��������
union System_Status {				//TODO�����⣬Heat��Cool��û��
    UINT32 all;
    struct System_Status_Flag {
		UINT8 b1StartUpBMS			:1;		//BMS��һ�ο�����־λ����ʼΪ1��ȷ���ܴ򿪹�����Ϊϵͳ��ʼ�����				//�ڶ���8λ
		UINT8 b1Status_MOS_PRE      :1;		//Ԥ��MOS�ܹ���״̬
		UINT8 b1Status_MOS_CHG      :1;		//���MOS�ܹ���״̬
		UINT8 b1Status_MOS_DSG      :1;		//�ŵ�MOS�ܹ���״̬

		UINT8 b1Status_Relay_PRE    :1;		//Ԥ��̵�������״̬
		UINT8 b1Status_Relay_CHG    :1;		//�ֿڳ��̵�������״̬
		UINT8 b1Status_Relay_DSG    :1;		//�ֿڷŵ�̵�������״̬
		UINT8 b1Status_Relay_MAIN   :1;		//ͬ�����̵�������״̬

		UINT8 b1Status_Heat         :1;		//���ȹ���״̬					//��һ��8λ
		UINT8 b1Status_Cool         :1;		//���书��״̬
		UINT8 b1Status_AFE1         :1;		//AFE1״̬
		UINT8 b1Status_AFE2	        :1;		//AFE2״̬

		UINT8 b1Status_Balance		:1;		//���⹦��״̬
		UINT8 b1Status_ToSleep		:1;		//������������״̬
		UINT8 b1Status_BnCloseIO	:1;		//�������MOS�رձ�־λ
		UINT8 b1Status_HeatCloseIO	:1;		//����ʱ�ر�MosRelay
		
		UINT8 b1Status_SysLimits	:1;		//res						//���ĸ�8λ
		UINT8 b1Status_CBCCloseIO	:1;		//��ǹ����ǿ�ƹر�����������ģ��ǿ�ƹر�����
		UINT8 b1Status_DriverExtCtrl:1;		//����תΪ�ⲿ���ƣ�����������ã�����
		UINT8 bRcved6				:1;		//res

		UINT8 b4Status_ProjectVer	:4;		//��¼һЩ��Ŀ��Ϣ��Ŀǰ���ߴ���Ϊ1�����Ϊ0

		UINT8 bRcved11				:8;		//res						//������8λ
     }bits;
};


//ϵͳ���ܿ�������
union System_OnOFF_Function {				//TODO
    UINT32 all;
    struct System_OnOFF_Ctrl {
		UINT8 b1OnOFF_Balance		:1;		//���⹦��
		UINT8 b1OnOFF_BMS_Source 	:1;		//BMS��Դ����
		UINT8 b1OnOFF_MOS_Relay     :1;		//����MOS�ͽӴ������ܣ���ŵ��ã���Ҫ��������IO�������ع��ܹرգ�ͨ��Switch���Ʊ��
											//ԭ���Ƿֿ��ģ����Ǻ��淢��ÿ���õ��ĵط���Ҫѡͨ���ɴ�ͺϲ���һ������Ҫѡͨ�ˡ�
		UINT8 b1OnOFF_Relay_Rec     :1;		//�̵�������
		
		UINT8 b1OnOFF_SOC_Fixed     :1;		//Soc�̶�����
		UINT8 b1OnOFF_Heat          :1;		//���ȹ���
		UINT8 b1OnOFF_Cool          :1;		//���书��
		UINT8 b1OnOFF_AFE1         	:1;		//AFE1״̬

		UINT8 b1OnOFF_AFE2	        :1;		//AFE2״̬
		UINT8 b1OnOFF_Sleep			:1;		//���߹���
		UINT8 b1OnOFF_SOC_Zero		:1;		//���߹���
		UINT8 bRcved5				:1;		//
		
		UINT8 bRcved1				:4;		//res

		UINT8 bRcved2				:8;		//res
		UINT8 bRcved3				:8;		//res
     }bits;
};


extern volatile struct SYSTEM_ERROR System_ErrFlag;
extern volatile union System_Function_StartUp System_Func_StartUp;
extern volatile union System_OnOFF_Function System_OnOFF_Func_StartUpRec;

extern volatile union System_OnOFF_Function System_OnOFF_Func;
extern volatile union System_Status SystemStatus;

void InitSystemMonitorData_EEPROM(void);
void SystemMonitorResetData_EEPROM(void);

UINT8 System_ERROR_UserCallback(enum SYSTEM_ERROR_COMMAND errorCode);

#endif	/* SYSTEM_MONITOR_H */

