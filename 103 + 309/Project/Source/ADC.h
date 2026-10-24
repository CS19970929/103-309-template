#ifndef ADC_H
#define ADC_H

/* Keep this order aligned with s_adc_inputs in ADC.c. */
enum ADC_INPUT {
    ADC_INPUT_MOS,
    ADC_INPUT_COUNT
};

void InitADC(void);
void ADC_StopForLowPower(void);
void ADC_Task(void);
/* A complete filtered raw sample is consumed once; no cached-value fallback. */
UINT8 ADC_TakeSample(enum ADC_INPUT input, UINT16 *raw);

#endif
