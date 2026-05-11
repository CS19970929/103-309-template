#ifndef ADC_H
#define ADC_H

#define AD_Used_amount 2

#define AD_CalNum   8
#define AD_CalNum_2 3

#define VBC_ADC_VDDA_MV          3300U
#define VBC_DIVIDER_RTOP_KOHM    470U
#define VBC_DIVIDER_RBOTTOM_KOHM 15U

enum tagInfoForADCArray {
    ADC_TEMP_MOS1,
    ADC_VBC,
    ADC_TEMP_EV1,
    ADC_TEMP_MOS2,
    AD_VREF_AD,
    ADC_TEMP_EV2,
    ADC_TEMP_EV3,
    ADC_EXT_C1,
    ADC_EXT_C2,
    ADC_NUM
};

#define LENGTH_TBLTEMP_PORT_10K ((UINT16)56)

extern INT32 g_i32ADCResult[ADC_NUM];
extern UINT16 g_u16VbcStableAD;
extern UINT16 g_u16VbcAdc_mV;
extern UINT32 g_u32Vbat_mV;

void InitADC(void);
void ADC_StopForLowPower(void);
void ADC_ResetAnlogCalSchedule(void);
UINT32 ADC_GetVbatMilliVolt(void);
void App_AnlogCal(void);

#endif
