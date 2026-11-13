#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include "stm32f10x.h"
#include "ADC.h"

/* PB8/PB9 also use direct register operations in the software I2C driver. */
#define BOARD_AFE_I2C_GPIO GPIOB
#define BOARD_AFE_I2C_SCL_SOURCE 8U
#define BOARD_AFE_I2C_SDA_SOURCE 9U

typedef enum {
    BOARD_IO_AFE_PRO,
    BOARD_IO_AFE_CTL,
    BOARD_IO_CHG_IN,
    BOARD_IO_SWITCH,
    BOARD_IO_RF_EN,
    BOARD_IO_M_STB,
    BOARD_IO_AD_EN,
    BOARD_IO_BLE_EN,
    BOARD_IO_CMNT_EN,
    BOARD_IO_SW_EN,
    BOARD_IO_DEBUG_LED,
    BOARD_IO_UART1_TX,
    BOARD_IO_UART1_RX,
    BOARD_IO_UART2_TX,
    BOARD_IO_UART2_RX,
    BOARD_IO_CAN_RX,
    BOARD_IO_CAN_TX,
    BOARD_IO_CMNT_WAKE,
    BOARD_IO_MOS_NTC,
    BOARD_IO_AFE_SCL,
    BOARD_IO_AFE_SDA,
    BOARD_IO_COUNT
} BoardIoId;

typedef enum {
    BOARD_INIT_AFE_CONTROL,
    BOARD_INIT_RUN_SHARED,
    BOARD_INIT_POWER,
    BOARD_INIT_DEBUG_LED,
    BOARD_INIT_UART1,
    BOARD_INIT_UART2,
    BOARD_INIT_CAN,
    BOARD_INIT_ADC,
    BOARD_INIT_AFE_I2C,
    BOARD_INIT_WAKE_ONLY
} BoardIoGroup;

typedef enum {
    BOARD_LEVEL_KEEP,
    BOARD_LOW_BEFORE_MODE,
    BOARD_HIGH_BEFORE_MODE,
    BOARD_LOW_AFTER_MODE,
    BOARD_HIGH_AFTER_MODE
} BoardInitialLevel;

enum {
    BOARD_KEEP_NONE = 0U,
    BOARD_KEEP_NORMAL = 1U,
    BOARD_KEEP_HICCUP = 2U,
    BOARD_KEEP_RTC = BOARD_KEEP_NORMAL | BOARD_KEEP_HICCUP
};

typedef struct {
    GPIO_TypeDef *gpio;
    uint8_t pin_source;             /* Pin number 0..15, also used by EXTI. */
    GPIOMode_TypeDef mode;
    GPIOSpeed_TypeDef speed;
    BoardInitialLevel initial_level;
    BoardIoGroup init_group;
    uint8_t stop_keep;              /* Keep the current mode AND output latch. */
} BoardIoConfig;

typedef struct {
    uint8_t channel;
    uint8_t preemption_priority;
    uint8_t sub_priority;
} BoardIrqConfig;

typedef enum {
    BOARD_WAKE_CHARGER,
    BOARD_WAKE_SWITCH,
    BOARD_WAKE_UART1,
    BOARD_WAKE_UART2,
    BOARD_WAKE_CMNT,
    BOARD_WAKE_COUNT
} BoardWakeupId;

typedef struct {
    BoardIoId io;
    EXTITrigger_TypeDef trigger;
    BoardIrqConfig irq;
} BoardWakeupConfig;

typedef struct {
    USART_TypeDef *usart;
    uint32_t clock;
    uint32_t remap;
    uint8_t apb2;
    BoardIoGroup io_group;
    BoardIrqConfig irq;
} BoardUartConfig;

typedef struct {
    CAN_InitTypeDef init;
    BoardIrqConfig irq;
    BitAction power_on_level;
    BitAction power_off_level;
} BoardCanConfig;

typedef struct {
    uint8_t channel;
    uint8_t sample_time;
} BoardAdcInputConfig;

typedef struct {
    uint32_t counter_hz;
    uint16_t period;
    BoardIrqConfig irq;
} BoardTimerConfig;

typedef struct {
    uint8_t prescaler;
    uint16_t reload;
} BoardIwdgConfig;

extern const BoardIoConfig g_board_io[BOARD_IO_COUNT];
extern const BoardWakeupConfig g_board_wakeup[BOARD_WAKE_COUNT];
extern const BoardUartConfig g_board_uart1;
extern const BoardUartConfig g_board_uart2;
extern const BoardCanConfig g_board_can;
extern const BoardAdcInputConfig g_board_adc_inputs[ADC_INPUT_COUNT];
extern const BoardTimerConfig g_board_timer3;
extern const BoardIwdgConfig g_board_iwdg;

#define BOARD_IO_GPIO(id) (g_board_io[(id)].gpio)
#define BOARD_IO_PIN(id) ((uint16_t)(1U << g_board_io[(id)].pin_source))

void Board_InitIoGroup(BoardIoGroup group);
void Board_EnableIrq(const BoardIrqConfig *config);

#endif
