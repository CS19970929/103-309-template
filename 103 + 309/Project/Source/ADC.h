#ifndef ADC_H
#define ADC_H

/* ADC1 temperature sampling: 100 ms main-loop task, no timer or DMA.
 * Add 10k NTC inputs to s_sensors[] in ADC.c.
 */
void InitADC(void);
void App_TempADC(void);
UINT8 TempADC_HasFault(void);

#endif /* ADC_H */
