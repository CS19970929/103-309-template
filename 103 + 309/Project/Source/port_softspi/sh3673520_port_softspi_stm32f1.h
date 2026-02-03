#pragma once
#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>
#include "../common/sh_spi_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Soft-SPI for SH3673520 (Mode3: CPOL=1, CPHA=1)
 * - SCK idle HIGH
 * - Data changes on falling edge
 * - Data sampled on rising edge
 */

typedef struct
{
    GPIO_TypeDef *cs_port;   uint16_t cs_pin;
    GPIO_TypeDef *sck_port;  uint16_t sck_pin;
    GPIO_TypeDef *mosi_port; uint16_t mosi_pin;
    GPIO_TypeDef *miso_port; uint16_t miso_pin;

    /* Half-period delay in NOP loops (bigger = slower). */
    uint32_t half_period_nops;

    /* Optional: user can provide precise delay_us (NULL ok) */
    void (*delay_us_cb)(uint32_t us);
} sh3673520_softspi_t;

/* Init GPIOs (StdPeriph). CS high by default. */
void sh3673520_softspi_init(sh3673520_softspi_t *s);

/* Build a sh_spi_port_t to plug into sh3673520_spi driver */
sh_spi_port_t sh3673520_softspi_make_port(sh3673520_softspi_t *s);

#ifdef __cplusplus
}
#endif
