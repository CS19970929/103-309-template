#ifndef ADC_H
#define ADC_H

#define AD_Used_amount      3

#define AD_CalNum           8
#define AD_CalNum_2         3
#define AD_CalNum_Cur       32
#define AD_CalNum_Cur_2     5
#define AD_CurOffsetCalNum  16
#define AD_CurOffsetCalNum_2 4
#define AD_CurZeroDeadband  4

#define TYPEC_CUR_RSENSE_MOHM       10U
#define TYPEC_CUR_VDDA_MV           3300U
#define TYPEC_OUT_VOLTAGE_MV        9000U
#define TYPEC_DCDC_EFFICIENCY_PERMILLE 900U
#define VBC_ADC_VDDA_MV           3300U
#define VBC_DIVIDER_RTOP_KOHM     470U
#define VBC_DIVIDER_RBOTTOM_KOHM  15U

enum tagInfoForADCArray {
    ADC_TEMP_MOS1,
    ADC_CUR_AMP,
    ADC_VBC,
    ADC_TEMP_EV1,
    ADC_TEMP_MOS2,
    AD_VREF_AD,
    ADC_TEMP_EV2,
    ADC_TEMP_EV3,
    ADC_EXT_C1,
    ADC_EXT_C2,
    ADC_CURR,
    ADC_NUM
};

#define LENGTH_TBLTEMP_PORT_10K    ((UINT16)56)

extern INT32   g_i32ADCResult[ADC_NUM];
extern UINT32  g_u32Vbat_mV;

void     InitADC(void);
void     ADC_StopForLowPower(void);
void     ADC_ResetAnlogCalSchedule(void);
UINT32   ADC_GetVbatMilliVolt(void);
UINT16   ADC_GetTypeCOutCurrentMilliAmp(void);
void     App_AnlogCal(void);

#endif  /* ADC_H */
