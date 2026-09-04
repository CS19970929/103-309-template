#ifndef AFE3520_BOARD_H
#define AFE3520_BOARD_H

#include "stm32f10x.h"

/*
 * Initial board mapping follows codex/afe-spi-refactor-debug SH3673520 wiring,
 * with the PA6/PA7 ALARM/MODE collision removed. The reference project uses
 * GPIO bit-banged SPI on PA5..PA7, which is the validated path for this board.
 */
#define AFE3520_GPIO_SPI                 GPIOA
#define AFE3520_PIN_CS                   GPIO_Pin_4
#define AFE3520_PIN_SCK                  GPIO_Pin_5
#define AFE3520_PIN_MISO                 GPIO_Pin_6
#define AFE3520_PIN_MOSI                 GPIO_Pin_7
#define AFE3520_USE_SOFTWARE_SPI        1U

#define AFE3520_GPIO_SHIP                GPIOA
#define AFE3520_PIN_SHIP                 GPIO_Pin_10

#define AFE3520_GPIO_CTLC                GPIOB
#define AFE3520_PIN_CTLC                 GPIO_Pin_14
#define AFE3520_GPIO_PRO_EN              GPIOB
#define AFE3520_PIN_PRO_EN               GPIO_Pin_0

#define AFE3520_GPIO_DSG_DET             GPIOA
#define AFE3520_PIN_DSG_DET              GPIO_Pin_8
#define AFE3520_GPIO_CHG_DET             GPIOA
#define AFE3520_PIN_CHG_DET              GPIO_Pin_9

/* Reference branch assigns ALARM to PA6, which is also SPI1 MISO. That mapping
 * is electrically impossible. Protection therefore never depends on ALARM;
 * FLAG1/FLAG2/BSTATUS are polled every 200ms. Set these only after schematic
 * confirmation if an independent ALARM pin becomes available. */
#define AFE3520_ALARM_EXTI_ENABLE        0U

#define AFE3520_BOARD_SPI_MAX_HZ         1000000UL
#define AFE3520_BOARD_MAPPING_REV        0x0001U

#endif /* AFE3520_BOARD_H */
