#include "main.h"

/* ADC1: configured 10k NTC inputs only; add entries to sample more sensors. */
typedef struct {
    GPIO_TypeDef *port;
    UINT16 pin;
    UINT8 adc_channel;
    UINT8 report_index;
    UINT8 calibration_index;
} TempAdcSensor;

static const TempAdcSensor s_sensors[] = {
    {GPIOA, GPIO_Pin_1, ADC_Channel_1, MOS_TEMP1, MDL_TEMP_MOS1},
};
#define TEMP_SENSOR_COUNT (sizeof(s_sensors) / sizeof(s_sensors[0]))
#define TEMP_ADC_AVG_SAMPLES 4U
#define TEMP_ADC_POLL_LIMIT 10000UL
#define TEMP_ADC_CAL_POLL_LIMIT 100000UL

static const UINT16 s_ntc_table[] = {
	// AD		(Temp+40)*10
	3771, 100, //-30
	3683, 150, //-25
	3580, 200, //-20
	3460, 250, //-15
	3323, 300, //-10
	3169, 350, //-5
	3004, 400, // 0
	2820, 450, // 5
	2633, 500, // 10
	2437, 550, // 15
	2241, 600, // 20
	2048, 650, // 25
	1859, 700, // 30
	1679, 750, // 35
	1509, 800, // 40
	1351, 850, // 45
	1204, 900, // 50
	1073, 950, // 55
	953, 1000, // 60
	845, 1050, // 65
	749, 1100, // 70
	664, 1150, // 75
	588, 1200, // 80
	522, 1250, // 85
	463, 1300, // 90
	411, 1350, // 95
	366, 1400, // 100
	326, 1450, // 105

};
#define NTC_TABLE_LEN (sizeof(s_ntc_table) / sizeof(s_ntc_table[0]))

static UINT8 s_adc_ready;
static UINT8 s_temperature_fault = 1U;
static UINT8 s_error_reported;

UINT8 TempADC_HasFault(void)
{
    return s_temperature_fault;
}

static UINT8 TempADC_Read(UINT8 channel, UINT16 *value)
{
    UINT32 sum = 0U;
    UINT8 sample;

    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_55Cycles5);
    for (sample = 0U; sample < TEMP_ADC_AVG_SAMPLES; ++sample)
    {
        UINT32 polls = TEMP_ADC_POLL_LIMIT;

        ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
        {
            if (--polls == 0U)
                return 0U;
        }
        sum += ADC_GetConversionValue(ADC1);
    }

    *value = (UINT16)((sum + TEMP_ADC_AVG_SAMPLES / 2U) / TEMP_ADC_AVG_SAMPLES);
    return 1U;
}

static UINT8 TempADC_Convert(UINT8 sensor, UINT16 raw, UINT16 *value)
{
    const TempAdcSensor *cfg = &s_sensors[sensor];
    UINT16 gain = g_u16CalibCoefK[cfg->calibration_index];
    INT16 offset = g_i16CalibCoefB[cfg->calibration_index];
    INT32 temperature_c;
    INT32 report_value;

    /* Outside the NTC table is a sensor fault, not a clamped temperature. */
    if (raw < s_ntc_table[NTC_TABLE_LEN - 2U] || raw > s_ntc_table[0])
        return 0U;

    if (gain < SYSKMIN || gain > SYSKMAX)
        gain = SYSKDEFAULT;
    if (offset < SYSBMIN || offset > SYSBMAX)
        offset = SYSBDEFAULT;

    /* Keep legacy degree-quantized K/B calibration and (T + 40) * 10 units. */
    temperature_c = (INT32)GetEndValue(s_ntc_table, (UINT16)NTC_TABLE_LEN, raw) / 10 - 40;
    temperature_c = (temperature_c * (INT32)gain + offset) >> 10;
    report_value = temperature_c * 10 + 400;

    /* Legacy MOS sensor-break check rejected values below -29 C. */
    if (report_value < 110 || report_value > 65535)
        return 0U;

    *value = (UINT16)report_value;
    return 1U;
}

void InitADC(void)
{
    GPIO_InitTypeDef gpio;
    ADC_InitTypeDef adc;
    UINT8 i;
    UINT32 polls;

    s_adc_ready = 0U;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                               RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    for (i = 0U; i < TEMP_SENSOR_COUNT; ++i)
    {
        gpio.GPIO_Pin = s_sensors[i].pin;
        GPIO_Init(s_sensors[i].port, &gpio);
    }

    ADC_DeInit(ADC1);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8); /* 8 MHz PCLK2 -> 1 MHz ADC clock */
    ADC_StructInit(&adc);
    adc.ADC_Mode = ADC_Mode_Independent;
    adc.ADC_ScanConvMode = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel = 1U;
    ADC_Init(ADC1, &adc);
    ADC_Cmd(ADC1, ENABLE);
    __delay_us(2U);

    ADC_ResetCalibration(ADC1);
    polls = TEMP_ADC_CAL_POLL_LIMIT;
    while (ADC_GetResetCalibrationStatus(ADC1) != RESET)
    {
        if (--polls == 0U)
        {
            s_temperature_fault = 1U;
            return;
        }
    }

    ADC_StartCalibration(ADC1);
    polls = TEMP_ADC_CAL_POLL_LIMIT;
    while (ADC_GetCalibrationStatus(ADC1) != RESET)
    {
        if (--polls == 0U)
        {
            s_temperature_fault = 1U;
            return;
        }
    }
    s_adc_ready = 1U;
}

void App_TempADC(void)
{
    UINT8 i;
    UINT8 fault = 0U;

    if (!g_st_SysTimeFlag.bits.b1Sys100msFlag)
        return;

    for (i = 0U; i < TEMP_SENSOR_COUNT; ++i)
    {
        UINT16 raw;
        UINT16 temperature;
        const TempAdcSensor *cfg = &s_sensors[i];

        if (!s_adc_ready || !TempADC_Read(cfg->adc_channel, &raw) ||
            !TempADC_Convert(i, raw, &temperature))
        {
            g_stCellInfoReport.u16Temperature[cfg->report_index] = 0U;
            fault = 1U;
        }
        else
        {
            g_stCellInfoReport.u16Temperature[cfg->report_index] = temperature;
        }
    }

    s_temperature_fault = fault;
    if (fault)
    {
        /* Close CTLC even if the AFE task is blocked by communications. */
        MCUO_AFE_CTLC = 0;
        if (!s_error_reported)
        {
            System_ERROR_UserCallback(ERROR_ADC);
            s_error_reported = 1U;
        }
    }
    else if (s_error_reported)
    {
        System_ERROR_UserCallback(ERROR_REMOVE_ADC);
        s_error_reported = 0U;
    }
}
