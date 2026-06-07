#ifndef ADC_H
#define ADC_H

#define AD_Used_amount      2


#define AD_CurZeroDeadband  4

#define VBC_ADC_VDDA_MV           3300U   // ADC reference voltage for PA1 VBUS divider
#define VBC_DIVIDER_RTOP_KOHM     470U    // Vbat+ to PA1 divider resistor, adjust with hardware
#define VBC_DIVIDER_RBOTTOM_KOHM  15U     // PA1 to GND divider resistor, adjust with hardware

//AD��������ö��
enum tagInfoForADCArray {
    ADC_TEMP_MOS1,      // MOS1�¶�
    ADC_VBC,            // ĸ�ߵ�ѹ
    ADC_TEMP_EV1,       // �ⲿ�¶�

    ADC_TEMP_MOS2,		// MOS2�¶�
    AD_VREF_AD,			//̧����ѹ
	ADC_TEMP_EV2,		// �ⲿ�¶�
	ADC_TEMP_EV3,		// �ⲿ�¶�
    ADC_EXT_C1,         // ��1�ڵ�ص��
    ADC_EXT_C2,         // ��2�ڵ�ص��

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
UINT8 ADC_IsReady(void);
void App_AnlogCal(void);

#endif	/* ADC_H */
