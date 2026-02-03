#include "sh3673520_port_softspi_stm32f1.h"
#include "main.h"

/* Quick IO */
static inline void hi(GPIO_TypeDef *p, uint16_t pin) { p->BSRR = pin; }
static inline void lo(GPIO_TypeDef *p, uint16_t pin) { p->BRR  = pin; }
static inline bool rd(GPIO_TypeDef *p, uint16_t pin) { return (p->IDR & pin) != 0; }

static inline void nops(uint32_t n)
{
    while (n--) __NOP();
}

static void port_cs_low(void *user)
{
    sh3673520_softspi_t *s = (sh3673520_softspi_t*)user;
    lo(s->cs_port, s->cs_pin);
}

static void port_cs_high(void *user)
{
    sh3673520_softspi_t *s = (sh3673520_softspi_t*)user;
    hi(s->cs_port, s->cs_pin);
}

static void port_delay_us(void *user, uint32_t us)
{
    sh3673520_softspi_t *s = (sh3673520_softspi_t*)user;
    if (s->delay_us_cb) {
        s->delay_us_cb(us);
        return;
    }

    /* Fallback: coarse delay using NOPs. Not accurate but stable enough for short gaps. */
    /* Rough guess: 72MHz => 72 cycles/us; each loop ~3 cycles => 24 iterations/us */
    nops(us * 24u);
}

static uint8_t port_txrx(void *user, uint8_t mosi)
{
    sh3673520_softspi_t *s = (sh3673520_softspi_t*)user;
    uint8_t miso = 0;

    /* Mode3: idle high */
    hi(s->sck_port, s->sck_pin);

    for (int i = 0; i < 8; i++) {
        /* Falling edge: update MOSI */
        lo(s->sck_port, s->sck_pin);
        if (mosi & 0x80) hi(s->mosi_port, s->mosi_pin);
        else             lo(s->mosi_port, s->mosi_pin);
        mosi <<= 1;
        nops(s->half_period_nops);

        /* Rising edge: sample MISO */
        hi(s->sck_port, s->sck_pin);
        nops(s->half_period_nops);
        miso <<= 1;
        if (rd(s->miso_port, s->miso_pin)) miso |= 1;
    }

    return miso;
}

static void enable_gpio_clock(GPIO_TypeDef *port)
{
    /* Minimal mapping for STM32F1: A/B/C/D/E */
    if (port == GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (port == GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else if (port == GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    else if (port == GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    else if (port == GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
}

void sh3673520_softspi_init(sh3673520_softspi_t *s)
{
    GPIO_InitTypeDef gi;

    enable_gpio_clock(s->cs_port);
    enable_gpio_clock(s->sck_port);
    enable_gpio_clock(s->mosi_port);
    enable_gpio_clock(s->miso_port);

    gi.GPIO_Speed = GPIO_Speed_50MHz;

    /* CS/SCK/MOSI: push-pull output */
    gi.GPIO_Mode = GPIO_Mode_Out_PP;

    gi.GPIO_Pin = s->cs_pin;
    GPIO_Init(s->cs_port, &gi);

    gi.GPIO_Pin = s->sck_pin;
    GPIO_Init(s->sck_port, &gi);

    gi.GPIO_Pin = s->mosi_pin;
    GPIO_Init(s->mosi_port, &gi);

    /* MISO: input pull-up (avoid floating) */
    gi.GPIO_Mode = GPIO_Mode_IPU;
    gi.GPIO_Pin = s->miso_pin;
    GPIO_Init(s->miso_port, &gi);

    /* Defaults */
    hi(s->cs_port, s->cs_pin);
    hi(s->sck_port, s->sck_pin);   /* CPOL=1 idle high */
    hi(s->mosi_port, s->mosi_pin);

    if (s->half_period_nops == 0) s->half_period_nops = 20;
}

sh_spi_port_t sh3673520_softspi_make_port(sh3673520_softspi_t *s)
{
    sh_spi_port_t p;
    p.user = s;
    p.cs_low = port_cs_low;
    p.cs_high = port_cs_high;
    p.txrx = port_txrx;
    p.delay_us = port_delay_us;
    return p;
}
