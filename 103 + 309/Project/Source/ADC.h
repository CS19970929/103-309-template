#ifndef ADC_H
#define ADC_H

#define AD_Used_amount      3


#define AD_CalNum			8		//������λ�ƴ���>>2������������Ҹ�
#define AD_CalNum_2			3		//2^3 = 8�������Ǹ�����2�Ķ��ٴη�������λ��

#define AD_CalNum_Cur		32		//������λ�ƴ���>>5������������Ҹ�
#define AD_CalNum_Cur_2		5		//2^5 = 32�������Ǹ�����2�Ķ��ٴη�������λ��

#define AD_CurOffsetCalNum  16
#define AD_CurOffsetCalNum_2 4
#define AD_CurZeroDeadband  4

#define TYPEC_CUR_RSENSE_MOHM       10U     // Type-C current sense resistor: 10 mohm, PA2 samples shunt voltage directly
#define TYPEC_CUR_VDDA_MV           3300U   // ADC reference voltage
#define TYPEC_OUT_VOLTAGE_MV        5000U   // Type-C output voltage used for SOC battery-side equivalent current
#define TYPEC_DCDC_EFFICIENCY_PERMILLE 1000U // 900 means 90% DC/DC efficiency
#define VBC_ADC_VDDA_MV           3300U   // ADC reference voltage for PA1 VBUS divider
#define VBC_DIVIDER_RTOP_KOHM     470U    // Vbat+ to PA1 divider resistor, adjust with hardware
#define VBC_DIVIDER_RBOTTOM_KOHM  15U     // PA1 to GND divider resistor, adjust with hardware

//AD��������ö��
enum tagInfoForADCArray {
    ADC_TEMP_MOS1,      // MOS1�¶�
	ADC_CUR_AMP,		//����������ѹ
    ADC_VBC,            // ĸ�ߵ�ѹ
    ADC_TEMP_EV1,       // �ⲿ�¶�

    ADC_TEMP_MOS2,		// MOS2�¶�
    AD_VREF_AD,			//̧����ѹ
	ADC_TEMP_EV2,		// �ⲿ�¶�
	ADC_TEMP_EV3,		// �ⲿ�¶�
    ADC_EXT_C1,         // ��1�ڵ�ص��
    ADC_EXT_C2,         // ��2�ڵ�ص��
	ADC_CURR,

	ADC_NUM		        // ADC number
};


#define LENGTH_TBLTEMP_PORT_10K    ((UINT16)56)
#define Vbc_scale (((VBC_DIVIDER_RTOP_KOHM + VBC_DIVIDER_RBOTTOM_KOHM) / VBC_DIVIDER_RBOTTOM_KOHM)) // legacy integer divider ratio


INT32 ADC_GetResult(UINT8 index);
UINT16 ADC_GetRaw(UINT8 index);


void InitADC(void);
void ADC_StopForLowPower(void);
void ADC_ResetAnlogCalSchedule(void);
UINT32 ADC_GetVbatMilliVolt(void);
UINT16 ADC_GetTypeCOutCurrentMilliAmp(void);
void App_AnlogCal(void);

#endif	/* ADC_H */
