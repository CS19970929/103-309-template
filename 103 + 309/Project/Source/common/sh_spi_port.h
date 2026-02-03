#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sh_spi_port
{
    void *user;

    void (*cs_low)(void *user);
    void (*cs_high)(void *user);

    /* Full-duplex transfer one byte (clock 8 bits). Returns MISO byte. */
    uint8_t (*txrx)(void *user, uint8_t mosi);

    /* Small delay between edges/bytes; can be NULL. */
    void (*delay_us)(void *user, uint32_t us);
} sh_spi_port_t;

#ifdef __cplusplus
}
#endif
