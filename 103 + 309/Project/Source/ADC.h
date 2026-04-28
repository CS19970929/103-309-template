#ifndef ADC_H
#define ADC_H

#define AD_Used_amount      3


#define AD_CalNum			8		//后面是位移处理>>2，所以这个别乱改
#define AD_CalNum_2			3		//2^3 = 8，上面那个数是2的多少次方，用于位移

#define AD_CalNum_Cur		32		//后面是位移处理>>5，所以这个别乱改
#define AD_CalNum_Cur_2		5		//2^5 = 32，上面那个数是2的多少次方，用于位移

#define AD_CurOffsetCalNum  16
#define AD_CurOffsetCalNum_2 4
#define AD_CurZeroDeadband  4

#define TYPEC_CUR_RSENSE_MOHM       10U     // Type-C current sense resistor: 10 mohm, PA2 samples shunt voltage directly
#define TYPEC_CUR_VDDA_MV           3300U   // ADC reference voltage
#define VBC_ADC_VDDA_MV           3300U   // ADC reference voltage for PA1 VBUS divider
#define VBC_DIVIDER_RTOP_KOHM     300U    // Vbat+ to PA1 divider resistor, adjust with hardware
#define VBC_DIVIDER_RBOTTOM_KOHM  10U     // PA1 to GND divider resistor, adjust with hardware

//AD采样变量枚举
enum tagInfoForADCArray {
    ADC_TEMP_MOS1,      // MOS1温度
	ADC_CUR_AMP,		//电流采样电压
    ADC_VBC,            // 母线电压
    ADC_TEMP_EV1,       // 外部温度

    ADC_TEMP_MOS2,		// MOS2温度
    AD_VREF_AD,			//抬升电压
	ADC_TEMP_EV2,		// 外部温度
	ADC_TEMP_EV3,		// 外部温度
    ADC_EXT_C1,         // 第1节电池电压
    ADC_EXT_C2,         // 第2节电池电压
	ADC_CURR,

	ADC_NUM		        // ADC number
};


#define LENGTH_TBLTEMP_PORT_10K    ((UINT16)56)
#define Vbc_scale (((VBC_DIVIDER_RTOP_KOHM + VBC_DIVIDER_RBOTTOM_KOHM) / VBC_DIVIDER_RBOTTOM_KOHM)) // legacy integer divider ratio


extern INT32 g_i32ADCResult[ADC_NUM];             //ADC数据缓存
//extern __IO UINT16 g_u16ADCValFilter[ADC_NUM];		//这个位数不能改
extern UINT16 g_u16TypeCOutCurrent_mA;
extern UINT16 g_u16TypeCOutCurrent_A10;
extern UINT16 g_u16TypeCOutOffsetAD;
extern UINT16 g_u16TypeCOutStableAD;
extern UINT16 g_u16TypeCOutDelta_mV;
extern UINT16 g_u16VbcStableAD;
extern UINT16 g_u16VbcAdc_mV;
extern UINT32 g_u32Vbat_mV;
extern UINT16 gu16_BusCurr_CHG;
extern UINT16 gu16_BusCurr_DSG;


void InitADC(void);
void ADC_StopForLowPower(void);
void ADC_ResetAnlogCalSchedule(void);
UINT32 ADC_GetVbatMilliVolt(void);
void App_AnlogCal(void);

#endif	/* ADC_H */

