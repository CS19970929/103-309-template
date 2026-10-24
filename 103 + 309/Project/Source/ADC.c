#include "main.h"

#define ADC_CALIBRATION_WAIT_LOOPS 100000U
#define ADC_SETTLE_TICKS           2U     /* At least 10 ms, regardless of tick phase. */
#define ADC_SAMPLE_PERIOD_TICKS    10U    /* 100 ms between sweeps. */
#define ADC_CONVERSION_TIMEOUT_TICKS 3U   /* Bounded polling, including loop latency. */
#define ADC_FILTER_SAMPLES         8U

typedef struct {
    UINT32 gpio_clock;
    GPIO_TypeDef *gpio;
    UINT16 pin;
    UINT8 channel;
} ADC_INPUT_CONFIG;

static const ADC_INPUT_CONFIG s_adc_inputs[] = {
    { RCC_APB2Periph_GPIOB, GPIO_ADC_NMOS, PIN_ADC_NMOS, ADC_Channel_9 }
};
typedef char ADC_InputCountCheck[(sizeof(s_adc_inputs) / sizeof(s_adc_inputs[0]) == ADC_INPUT_COUNT) ? 1 : -1];

enum ADC_STATE { ADC_OFF, ADC_SETTLING, ADC_WAIT, ADC_DISCARD, ADC_SAMPLING };

static struct {
    UINT32 tick;
    UINT32 sum;
    UINT16 min;
    UINT16 max;
    UINT16 raw[ADC_INPUT_COUNT];
    UINT8 ready[ADC_INPUT_COUNT];
    UINT8 input;
    UINT8 count;
    enum ADC_STATE state;
} s_adc;

void ADC_StopForLowPower(void)
{
    s_adc.state = ADC_OFF;
    memset(s_adc.ready, 0, sizeof(s_adc.ready));
    s_adc.count = 0U;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    ADC_Cmd(ADC1, DISABLE);
    ADC_DeInit(ADC1);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, DISABLE);
}

void InitADC(void)
{
    ADC_InitTypeDef adc;
    GPIO_InitTypeDef gpio;
    UINT32 timeout;
    UINT8 i;

    ADC_StopForLowPower();
    for (i = 0U; i < ADC_INPUT_COUNT; ++i)
    {
        RCC_APB2PeriphClockCmd(s_adc_inputs[i].gpio_clock, ENABLE);
        GPIO_StructInit(&gpio);
        gpio.GPIO_Mode = GPIO_Mode_AIN;
        gpio.GPIO_Pin = s_adc_inputs[i].pin;
        GPIO_Init(s_adc_inputs[i].gpio, &gpio);
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8); /* <= 9 MHz even with PCLK2 = 72 MHz. */
    ADC_StructInit(&adc);
    adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc.ADC_NbrOfChannel = 1U;
    ADC_Init(ADC1, &adc);
    ADC_Cmd(ADC1, ENABLE);
    __delay_us(2U); /* STM32F103 ADC tSTAB >= 1 us before calibration. */

    ADC_ResetCalibration(ADC1);
    timeout = ADC_CALIBRATION_WAIT_LOOPS;
    while (ADC_GetResetCalibrationStatus(ADC1) != RESET)
    {
        if (--timeout == 0U)
            goto failed;
    }
    ADC_StartCalibration(ADC1);
    timeout = ADC_CALIBRATION_WAIT_LOOPS;
    while (ADC_GetCalibrationStatus(ADC1) != RESET)
    {
        if (--timeout == 0U)
            goto failed;
    }

    s_adc.input = 0U;
    s_adc.tick = SysTime_Get10msTickCount();
    s_adc.state = ADC_SETTLING;
    return;

failed:
    ADC_StopForLowPower();
    System_ERROR_UserCallback(ERROR_ADC);
}

static void ADC_StartConversion(void)
{
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    s_adc.tick = SysTime_Get10msTickCount();
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

void ADC_Task(void)
{
    UINT16 raw;
    UINT32 elapsed;

    if (s_adc.state == ADC_OFF)
        return;

    elapsed = SysTime_Get10msTickCount() - s_adc.tick;
    if (s_adc.state == ADC_SETTLING || s_adc.state == ADC_WAIT)
    {
        if (elapsed < (s_adc.state == ADC_SETTLING ? ADC_SETTLE_TICKS : ADC_SAMPLE_PERIOD_TICKS))
            return;
        s_adc.input = 0U;
        ADC_RegularChannelConfig(ADC1, s_adc_inputs[0].channel, 1U, ADC_SampleTime_239Cycles5);
        s_adc.state = ADC_DISCARD;
        ADC_StartConversion();
        return;
    }

    /* A late EOC is also discarded: it must not time an old sample as fresh. */
    if (elapsed >= ADC_CONVERSION_TIMEOUT_TICKS)
        goto invalid;
    if (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
        return;
    raw = ADC_GetConversionValue(ADC1);

    if (s_adc.state == ADC_DISCARD)
    {
        s_adc.sum = 0U;
        s_adc.min = 4095U;
        s_adc.max = 0U;
        s_adc.count = 0U;
        s_adc.state = ADC_SAMPLING;
    }
    else
    {
        if (raw == 0U || raw >= 4095U)
            goto invalid; /* Rail readings are diagnostic, never a temperature. */
        s_adc.sum += raw;
        if (raw < s_adc.min) s_adc.min = raw;
        if (raw > s_adc.max) s_adc.max = raw;
        if (++s_adc.count == ADC_FILTER_SAMPLES)
        {
            s_adc.raw[s_adc.input] = (UINT16)((s_adc.sum - s_adc.min - s_adc.max) /
                                              (ADC_FILTER_SAMPLES - 2U));
            s_adc.ready[s_adc.input] = ADC_SAMPLE_READY;
            System_ERROR_UserCallback(ERROR_REMOVE_ADC);
            if (++s_adc.input == ADC_INPUT_COUNT)
            {
                s_adc.tick = SysTime_Get10msTickCount();
                s_adc.state = ADC_WAIT;
                return;
            }
            ADC_RegularChannelConfig(ADC1, s_adc_inputs[s_adc.input].channel, 1U,
                                     ADC_SampleTime_239Cycles5);
            s_adc.state = ADC_DISCARD;
        }
    }
    ADC_StartConversion();
    return;

invalid:
    s_adc.ready[s_adc.input] = ADC_SAMPLE_INVALID;
    s_adc.tick = SysTime_Get10msTickCount();
    s_adc.state = ADC_WAIT;
    System_ERROR_UserCallback(ERROR_ADC);
}

UINT8 ADC_TakeSample(enum ADC_INPUT input, UINT16 *raw)
{
    UINT8 status;
    if ((UINT32)input >= ADC_INPUT_COUNT || raw == 0)
        return ADC_SAMPLE_NONE;
    status = s_adc.ready[input];
    s_adc.ready[input] = ADC_SAMPLE_NONE;
    if (status == ADC_SAMPLE_READY)
        *raw = s_adc.raw[input];
    return status;
}
