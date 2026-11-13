#ifndef ADC_H
#define ADC_H

/* Keep this order aligned with g_board_adc_inputs in Board_Config.c. */
enum ADC_INPUT {
    ADC_INPUT_MOS,
    ADC_INPUT_COUNT
};

enum ADC_SAMPLE_STATUS { ADC_SAMPLE_NONE, ADC_SAMPLE_READY, ADC_SAMPLE_INVALID };

void InitADC(void);
void ADC_StopForLowPower(void);
void ADC_Task(void);
/* Consume one status. raw is written only for ADC_SAMPLE_READY. */
UINT8 ADC_TakeSample(enum ADC_INPUT input, UINT16 *raw);

#endif
