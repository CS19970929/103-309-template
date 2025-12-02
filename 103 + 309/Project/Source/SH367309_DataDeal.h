#ifndef _SH367309_DATADEAL_H
#define _SH367309_DATADEAL_H


#define E2P_ADDR_E2POS_AFE_Parameters			950		//��974��AFE�������浽eeprom����ʼ��ַ
#define RS485_CMD_ADDR_AFE_ROM_PARAMETERS_START 0x2400  
#define RS485_CMD_ADDR_AFE_ROM_PARAMETERS_END 	0x2417
#define AFE_PARAMETES_TOTAL_LENGTH  			24    //��λ���Ĵ���


#define RS485_ADDR_RW_AFE_PARAMETER  0x2400

//��Ԫ��
#ifdef TERNARYLI
/*curValue*/  /*defaultValue*/ /*maxValue*/ /*minValue*/
#define AFE_PARAMETERS_RS485_STRUCTION_DEFAULT  {\
	/*���ڹ�ѹ*/			4200,	4200,	5000,	1000,\
	/*���ڹ�ѹ�ָ�*/			4000,	4000,	5000,	1000,\
	/*���ڹ�ѹ��ʱ*/			100,	100,	50000,	1,\
	/*���ڵ�ѹ*/			800,	2800,	5000,	1000,\
	/*���ڵ�ѹ�ָ�*/			2900,	2900,	5000,	1000,\
	/*���ڵ�ѹ��ʱ*/			100,	100,	50000,	1,\
	/*һ��������*/			1200,	1200,	50000,	10,\
	/*һ����������ʱ*/100,			100,	50000,	1,\
	/*����������*/			1300,	1300,	50000,	10,\
	/*������������ʱ*/100,			100,	50000,	1,\
	/*һ���ŵ����*/			1200,	1200,	50000,	10,\
	/*һ���ŵ������ʱ*/100,			100,	50000,	1,\
	/*�����ŵ����*/			1300,	1300,	50000,	10,\
	/*�����ŵ������ʱ*/100,			100,	50000,	1,\
	/*������*/			1200,	1200,	2000,	400,\
	/*�����»ָ�*/			1150,	1150,	50000,	1,\
	/*������*/			410,	410,	800,	0,\
	/*�����»ָ�*/			450,	450,	50000,	1,\
	/*�ŵ����*/			1200,	1200,	2000,	400,\
	/*�ŵ���»ָ�*/			1150,	1150,	50000,	1,\
	/*�ŵ����*/			410,	410,	800,	0,\
	/*�ŵ���»ָ�*/			450,	450,	50000,	1,\
	/*��·����*/			100,	100,	65000,	0,\
	/*��·��ʱ*/			64,		64,		65000,	0,\
}
//�������
#elif (defined(LIFEPO))
/*curValue*/  /*defaultValue*/ /*maxValue*/ /*minValue*/
#define AFE_PARAMETERS_RS485_STRUCTION_DEFAULT  {\
	/*���ڹ�ѹ*/			3650,	3650,	5000,	1000,\
	/*���ڹ�ѹ�ָ�*/			3600,	3600,	5000,	1000,\
	/*���ڹ�ѹ��ʱ*/			100,	100,	50000,	1,\
	/*���ڵ�ѹ*/			2500,	2500,	5000,	1000,\
	/*���ڵ�ѹ�ָ�*/			2600,	2600,	5000,	1000,\
	/*���ڵ�ѹ��ʱ*/			100,	100,	50000,	1,\
	/*һ��������*/			1200,	1200,	50000,	10,\
	/*һ����������ʱ*/100,			100,	50000,	1,\
	/*����������*/			1300,	1300,	50000,	10,\
	/*������������ʱ*/100,			100,	50000,	1,\
	/*һ���ŵ����*/			1200,	1200,	50000,	10,\
	/*һ���ŵ������ʱ*/100,			100,	50000,	1,\
	/*�����ŵ����*/			1300,	1300,	50000,	10,\
	/*�����ŵ������ʱ*/100,			100,	50000,	1,\
	/*������*/			1050,	1050,	2000,	400,\
	/*�����»ָ�*/			900,	900,	50000,	1,\
	/*������*/			200,	200,	800,	0,\
	/*�����»ָ�*/			300,	300,	50000,	1,\
	/*�ŵ����*/			1050,	1050,	2000,	400,\
	/*�ŵ���»ָ�*/			900,	900,	50000,	1,\
	/*�ŵ����*/			200,	200,	800,	0,\
	/*�ŵ���»ָ�*/			300,	300,	50000,	1,\
	/*��·����*/			10000,	10000,	65000,	0,\
	/*��·��ʱ*/			640,		640,		65000,	0,\
}

#endif


//typedef  unsigned char UINT8;
//typedef  unsigned short UINT16;

/******************************* AFE�Ĵ����ṹ�� *****************************/
typedef struct {
	UINT8 CN 		:4;  		//5-15����Ӧ���������Ϊ16��
	UINT8 BAL 		:1;			//0��ƽ�⹦�����ڲ�SH367309���ƣ�1:���ⲿMCU��
	UINT8 OCPM 		:1;			//0:������ֻ�رճ��MOS���ŵ����ֻ�طŵ�MOS��1��ͬʱ��
	UINT8 ENMOS 	:1;			/*0:��ֹ���MOS�ָ�����λ��1:�������MOS�ָ�����λ��
												(�������/�¶ȱ���(�¶�ʵ�ʹ�2��)�رճ��MOS�������⵽�ŵ�״̬���������MOS) */									
	UINT8 ENPCH 	:1;			//0:��ֹԤ��繦�ܣ�1������Ԥ�书��
	
	UINT8 EUVR 		:1;			//0�����ű���״̬�ͷ��븺���ͷ��޹أ���ζ�Ÿ����ͷ�ֻ�͵��������й���
	UINT8 OCRA 		:1;			/*0����������1������---������������ʱ�ָ������ܣ�
												Ҳ����ζ��ֻ�ܸ����ͷŲ��ܽ�����������������Զ��ָ� */
	UINT8 CTLC 		:2;			//
	UINT8 DIS_PF 	:1;			//
	UINT8 UV_OP 	:1;			//
	UINT8 Reserve 	:1;			//
	UINT8 E0VB 		:1;			//
	
}BYTE_00H_01H_TypeDef;


typedef struct {
	UINT8 OVH 		:2;  		//���䱣��ǰ2λ
	UINT8 LDRT 		:2;			//11�������ͷ���ʱ2000ms������Ͷ�·�����ͷ���ʱ�й�ϵ
	UINT8 OVT		:4;			//����籣����ʱ
	UINT8 OVL;					//���䱣����8λ
}BYTE_02H_03H_TypeDef;


typedef struct {
	UINT8 OVRH 		:2;  		//���䱣���ָ�ǰ2λ
	UINT8 Reserve 	:2;		
	UINT8 UVT		:4;			//��ѹ������ʱ
	UINT8 OVRL;					//���䱣���ָ���8λ
}BYTE_04H_05H_TypeDef;

typedef struct {
	UINT8 UV;					//��ѹ����
	UINT8 UVR;					//��ѹ�����ָ�
}BYTE_06H_07H_TypeDef;


typedef struct {
	UINT8 BALV;					//���⿪����ѹ
	UINT8 PREV;					//Ԥ���ѹ
}BYTE_08H_09H_TypeDef;

typedef struct {
	UINT8 L0V;				//�͵�ѹ��ֹ����ѹ
	UINT8 PFV;				//���ι���籣����ѹ���üĴ������Ŵ�һЩ���������
}BYTE_0AH_0BH_TypeDef;


typedef struct {
	UINT8 OCD1T		:4;			//�ŵ����1������ʱ
	UINT8 OCD1V		:4;			//�ŵ��������1������ѹ
	UINT8 OCD2T		:4;			//�ŵ����2������ʱ
	UINT8 OCD2V		:4;			//�ŵ��������2������ѹ
}BYTE_0CH_0DH_TypeDef;

typedef struct {
	UINT8 SCT		:4;			//��·������ʱ
	UINT8 SCV		:4;			//��·������ѹ
	UINT8 OCCT		:4;			//������������ʱ
	UINT8 OCCV		:4;			//������������ѹ
}BYTE_0EH_0FH_TypeDef;

typedef struct {
	UINT8 PFT		:2;			//��·������ʱ
	UINT8 OCRT		:2;			//��·������ѹ
	UINT8 MOST		:2;			//������������ʱ
	UINT8 CHS		:2;			//������������ѹ
}BYTE_10H_TypeDef;

typedef struct {
	UINT8 OTC;					//�����±���
	UINT8 OTCR;					//�����±����ָ�
	UINT8 UTC;					//�����±���
	UINT8 UTCR;					//�����±����ָ�
	UINT8 OTD;					//�ŵ���±���
	UINT8 OTDR;					//�ŵ���±����ָ�
	UINT8 UTD;					//�ŵ���±���
	UINT8 UTDR;					//�ŵ���±����ָ�
	UINT8 TR;
}BYTE_11H_19H_TypeDef;


typedef	struct {
	BYTE_00H_01H_TypeDef 	m00H_01H;
	BYTE_02H_03H_TypeDef	m02H_03H;
	BYTE_04H_05H_TypeDef	m04H_05H;
	BYTE_06H_07H_TypeDef 	m06H_07H;
	BYTE_08H_09H_TypeDef 	m08H_09H;
	BYTE_0AH_0BH_TypeDef 	m0AH_0BH;
	BYTE_0CH_0DH_TypeDef	m0CH_0DH;
	BYTE_0EH_0FH_TypeDef 	m0EH_0FH;
	BYTE_10H_TypeDef 			m10H;
	BYTE_11H_19H_TypeDef	m11H_19H;
}AFE_ROM_PARAMETERS_TypeDef;



/******************************* AFE���������ṹ�� *****************************/
typedef struct {
	UINT16 curValue;			//��ǰֵ
	UINT16 defaultValue;		//Ĭ��ֵ
	UINT16 maxValue;			//���ֵ
	UINT16 minValue;			//��Сֵ
}AFE_Value_Typedef;

typedef struct{
	AFE_Value_Typedef	u16VcellOvp;  		//���ڹ�ѹ mv
	AFE_Value_Typedef	u16VcellOvp_Rcv;	//��ѹ�ָ� mv
	AFE_Value_Typedef	u16VcellOvp_Filter;	//��ѹ��ʱ 10ms
	
	AFE_Value_Typedef	u16VcellUvp;		//���ڵ�ѹ
	AFE_Value_Typedef	u16VcellUvp_Rcv;
	AFE_Value_Typedef	u16VcellUvp_Filter;
	
	AFE_Value_Typedef	u16IchgOcp_First;	//һ�������� A*10
	AFE_Value_Typedef	u16IchgOcp_Filter_First;
	
	AFE_Value_Typedef	u16IchgOcp_Second;	//����������
	AFE_Value_Typedef	u16IchgOcp_Filter_Second;
	
	AFE_Value_Typedef	u16IdsgOcp_First;	//һ���ŵ����
	AFE_Value_Typedef	u16IdsgOcp_Filter_First;
	
	AFE_Value_Typedef	u16IdsgOcp_Second;	//�����ŵ����
	AFE_Value_Typedef	u16IdsgOcp_Filter_Second;
	
	AFE_Value_Typedef	u16TChgOTp;			//������ (��*10+400)
	AFE_Value_Typedef	u16TChgOTp_Rcv;
	AFE_Value_Typedef	u16TchgUTp;			//������
	AFE_Value_Typedef	u16TchgUTp_Rcv;
	AFE_Value_Typedef	u16TdischgOTp;		//�ŵ����
	AFE_Value_Typedef	u16TdischgOTp_Rcv;
	AFE_Value_Typedef	u16TdischgUTp;		//�ŵ����
	AFE_Value_Typedef	u16TdischgUTp_Rcv;
	AFE_Value_Typedef 	u16CBC_Cur_DSG;
	AFE_Value_Typedef 	u16CBC_DelayT;
}AFE_Parameters_RS485_Typedef; 



void App_SH367309_Supplement(void);

void Sci_ACK_0x03_RW_AFE_Parameters(struct RS485MSG *s,UINT8 t_u8BuffTemp[]);
UINT8 Sci_WrRegs_0x10_AFE_Parameters(UINT16 u16Channel,struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_AFE_Parameters(struct RS485MSG *s);

void EEPROM_ResetData_AFE_ParametersToDefault(void);
void ReadEEPROM_AFE_Parameters(void);


extern int AFE_PARAM_WRITE_Flag;
extern int AFE_ResetFlag;
extern AFE_Parameters_RS485_Typedef  AFE_Parameters_RS485_Struction;
bool fac_sh367309_param_init_first_powerup(void);

#endif

