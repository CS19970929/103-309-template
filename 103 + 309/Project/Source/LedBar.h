#ifndef LEDBAR_H
#define LEDBAR_H

typedef enum _LEDBAR_COMMAND {
	LED_BAR_STARTUP = 0,
    LED_BAR_NORMAL,
    LED_BAR_CHG,
    LED_BAR_DSG,
	LED_BAR_FAULT,
}LEDBAR_COMMAND;

#define PORT_SOC_20             GPIOB
#define PIN_SOC_20	            GPIO_Pin_1

#define PORT_SOC_40             GPIOA
#define PIN_SOC_40	            GPIO_Pin_7

#define PORT_SOC_60             GPIOA
#define PIN_SOC_60	            GPIO_Pin_6

#define PORT_SOC_80             GPIOA
#define PIN_SOC_80	            GPIO_Pin_5

#define PORT_SOC_100            GPIOB
#define PIN_SOC_100	            GPIO_Pin_5

// #define PORT_SOC_RUN             GPIOC
// #define PIN_SOC_RUN	            GPIO_Pin_14

// #define PORT_SOC_ALM             GPIOC
// #define PIN_SOC_ALM	            GPIO_Pin_15

// #define PORT_SOC_BLE             GPIOF
// #define PIN_SOC_BLE	            GPIO_Pin_7

#define PORT_SOC_KEY             GPIOC
#define PIN_SOC_KEY              GPIO_Pin_13


#define MCUO_SOC_20 		(PBout(1))
#define MCUO_SOC_40 		(PAout(7))
#define MCUO_SOC_60 		(PAout(6))
#define MCUO_SOC_80 		(PAout(5))
#define MCUO_SOC_100 		(PBout(5))

// #define MCUO_SOC_RUN 		(PORT_OUT_GPIOC->bit14)
// #define MCUO_SOC_ALARM 		(PORT_OUT_GPIOC->bit15)
//ble run����       �����滻
#define MCUI_SOC_KEY 		(PCin(13))
//FIXME IN OR OUT
// #define MCUO_SOC_BLE 		(PORT_OUT_GPIOF->bit7)

// #define MCUO_SOC_BLE 		(PORT_OUT_GPIOC->bit14)
// #define MCUO_SOC_RUN 		(PORT_OUT_GPIOF->bit7)

void APP_LedBar(void);

#endif	/* LEDBAR_H */
