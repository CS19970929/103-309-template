#include "debug_hub.h"

#include "main.h"
#include "FactoryAging.h"
#include "FaultSnapshot.h"
#include "IrqDebug.h"
#include <string.h>

#if defined(STM32F10X_LD) || defined(STM32F10X_MD) || defined(STM32F10X_HD) || \
	defined(STM32F10X_CL) || defined(STM32F10X_LD_VL) || defined(STM32F10X_MD_VL) || \
	defined(STM32F10X_HD_VL) || defined(__STM32F1__)
#define DBG_HUB_STM32F1 1
#else
#define DBG_HUB_STM32F1 0
#endif

#if defined(STM32F0XX) || defined(STM32F0xx) || defined(__STM32F0__)
#define DBG_HUB_STM32F0 1
#else
#define DBG_HUB_STM32F0 0
#endif

volatile DBG_Hub_t g_dbg;

static uint8_t dbg_bit(uint32_t value, uint32_t mask)
{
	return (uint8_t)(((value & mask) != 0U) ? 1U : 0U);
}

static uint8_t dbg_gpio_in(GPIO_TypeDef *gpio, uint16_t pin)
{
	return (uint8_t)((GPIO_ReadInputDataBit(gpio, pin) != Bit_RESET) ? 1U : 0U);
}

static uint8_t dbg_gpio_out(GPIO_TypeDef *gpio, uint16_t pin)
{
	return (uint8_t)((GPIO_ReadOutputDataBit(gpio, pin) != Bit_RESET) ? 1U : 0U);
}

static volatile DBG_HubUsart_t *dbg_usart_by_port(uint8_t port)
{
	if (port == (uint8_t)DBG_HUB_USART1)
	{
		return &g_dbg.mcu.usart1;
	}
	if (port == (uint8_t)DBG_HUB_USART2)
	{
		return &g_dbg.mcu.usart2;
	}
	return 0;
}

static void dbg_copy_can_frame(volatile DBG_HubCanFrame_t *dst,
							   uint32_t id,
							   uint8_t ide,
							   uint8_t rtr,
							   uint8_t dlc,
							   const uint8_t data[8])
{
	uint8_t i;

	dst->id = id;
	dst->ide = ide;
	dst->rtr = rtr;
	dst->dlc = dlc;
	for (i = 0U; i < 8U; i++)
	{
		dst->data[i] = (data != 0) ? data[i] : 0U;
	}
	dst->tick_10ms = SysTime_Get10msTickCount();
}

void DBG_RecordUsartRx(uint8_t port)
{
	volatile DBG_HubUsart_t *u = dbg_usart_by_port(port);

	if (u != 0)
	{
		u->rx_count++;
		u->last_event_tick_10ms = SysTime_Get10msTickCount();
	}
}

void DBG_RecordUsartTx(uint8_t port)
{
	volatile DBG_HubUsart_t *u = dbg_usart_by_port(port);

	if (u != 0)
	{
		u->tx_count++;
		u->last_event_tick_10ms = SysTime_Get10msTickCount();
	}
}

void DBG_RecordUsartIdle(uint8_t port)
{
	volatile DBG_HubUsart_t *u = dbg_usart_by_port(port);

	if (u != 0)
	{
		u->idle_count++;
		u->last_event_tick_10ms = SysTime_Get10msTickCount();
	}
}

void DBG_RecordUsartError(uint8_t port, uint16_t sr)
{
	volatile DBG_HubUsart_t *u = dbg_usart_by_port(port);

	if (u == 0)
	{
		return;
	}
	if ((sr & USART_SR_ORE) != 0U) u->ore_count++;
	if ((sr & USART_SR_FE) != 0U)  u->fe_count++;
	if ((sr & USART_SR_NE) != 0U)  u->ne_count++;
	if ((sr & USART_SR_PE) != 0U)  u->pe_count++;
	u->last_event_tick_10ms = SysTime_Get10msTickCount();
}

void DBG_RecordCanRxFrame(uint32_t id, uint8_t ide, uint8_t rtr, uint8_t dlc, const uint8_t data[8])
{
	g_dbg.mcu.can.rx_count++;
	g_dbg.comm.can_rx_count++;
	dbg_copy_can_frame(&g_dbg.mcu.can.last_rx, id, ide, rtr, dlc, data);
}

void DBG_RecordCanTxFrame(uint32_t id, uint8_t ide, uint8_t rtr, uint8_t dlc, const uint8_t data[8])
{
	g_dbg.mcu.can.tx_count++;
	g_dbg.comm.can_tx_count++;
	dbg_copy_can_frame(&g_dbg.mcu.can.last_tx, id, ide, rtr, dlc, data);
}

void DBG_RecordAdcSample(void)
{
	g_dbg.mcu.adc.sample_count++;
	g_dbg.mcu.adc.last_sample_tick_10ms = SysTime_Get10msTickCount();
}

void DBG_RecordIwdgFeed(uint8_t source)
{
	uint32_t now_tick = SysTime_Get10msTickCount();
	uint32_t gap = 0U;

	if (g_dbg.mcu.iwdg.feed_count != 0U)
	{
		gap = now_tick - g_dbg.mcu.iwdg.last_feed_tick_10ms;
	}
	g_dbg.mcu.iwdg.feed_count++;
	g_dbg.mcu.iwdg.last_feed_tick_10ms = now_tick;
	g_dbg.mcu.iwdg.last_feed_gap_10ms = gap;
	if (gap > g_dbg.mcu.iwdg.max_feed_gap_10ms)
	{
		g_dbg.mcu.iwdg.max_feed_gap_10ms = gap;
	}
	g_dbg.mcu.iwdg.last_feed_source = source;
}

void DBG_RecordI2c1Status(uint16_t sr1, uint16_t sr2, uint8_t state, uint8_t event)
{
	g_dbg.mcu.i2c1.sr1 = sr1;
	g_dbg.mcu.i2c1.sr2 = sr2;
	g_dbg.mcu.i2c1.state = state;
	g_dbg.mcu.i2c1.last_event_tick_10ms = SysTime_Get10msTickCount();

	switch (event)
	{
	case 1U:
		g_dbg.mcu.i2c1.ack_fail_count++;
		break;
	case 2U:
		g_dbg.mcu.i2c1.busy_timeout_count++;
		break;
	case 3U:
		g_dbg.mcu.i2c1.clock_stretch_timeout_count++;
		break;
	case 4U:
		g_dbg.mcu.i2c1.recovery_count++;
		break;
	default:
		break;
	}
}

static void dbg_capture_core(void)
{
	uint32_t icsr;

	icsr = SCB->ICSR;
	g_dbg.mcu.core.scb_icsr = icsr;
	g_dbg.mcu.core.scb_aircr = SCB->AIRCR;
	g_dbg.mcu.core.scb_scr = SCB->SCR;
	g_dbg.mcu.core.scb_ccr = SCB->CCR;
#if defined(__CORTEX_M) && (__CORTEX_M >= 0x03U)
	g_dbg.mcu.core.scb_shcsr = SCB->SHCSR;
	g_dbg.mcu.core.scb_cfsr = SCB->CFSR;
	g_dbg.mcu.core.scb_hfsr = SCB->HFSR;
	g_dbg.mcu.core.scb_dfsr = SCB->DFSR;
	g_dbg.mcu.core.scb_afsr = SCB->AFSR;
	g_dbg.mcu.core.basepri = __get_BASEPRI();
	g_dbg.mcu.core.faultmask = __get_FAULTMASK();
#else
	g_dbg.mcu.core.scb_shcsr = 0U;
	g_dbg.mcu.core.scb_cfsr = 0U;
	g_dbg.mcu.core.scb_hfsr = 0U;
	g_dbg.mcu.core.scb_dfsr = 0U;
	g_dbg.mcu.core.scb_afsr = 0U;
	g_dbg.mcu.core.basepri = 0U;
	g_dbg.mcu.core.faultmask = 0U;
#endif
	g_dbg.mcu.core.primask = __get_PRIMASK();
	g_dbg.mcu.core.current_exception = (uint16_t)(icsr & 0x01FFU);
	g_dbg.mcu.core.in_isr = (g_dbg.mcu.core.current_exception != 0U) ? 1U : 0U;
	g_dbg.mcu.core.interrupts_masked = (g_dbg.mcu.core.primask != 0U) ? 1U : 0U;
}

static void dbg_capture_systick(void)
{
	g_dbg.mcu.systick.ctrl = SysTick->CTRL;
	g_dbg.mcu.systick.load = SysTick->LOAD;
	g_dbg.mcu.systick.val = SysTick->VAL;
	g_dbg.mcu.systick.calib = SysTick->CALIB;
}

void DBG_CaptureRcc(void)
{
	g_dbg.mcu.rcc.cr = RCC->CR;
	g_dbg.mcu.rcc.cfgr = RCC->CFGR;
	g_dbg.mcu.rcc.cir = RCC->CIR;
	g_dbg.mcu.rcc.apb2rstr = RCC->APB2RSTR;
	g_dbg.mcu.rcc.apb1rstr = RCC->APB1RSTR;
	g_dbg.mcu.rcc.ahbenr = RCC->AHBENR;
	g_dbg.mcu.rcc.apb2enr = RCC->APB2ENR;
	g_dbg.mcu.rcc.apb1enr = RCC->APB1ENR;
	g_dbg.mcu.rcc.bdcr = RCC->BDCR;
	g_dbg.mcu.rcc.csr = RCC->CSR;
#if defined(STM32F10X_CL)
	g_dbg.mcu.rcc.ahbrstr = RCC->AHBRSTR;
	g_dbg.mcu.rcc.cfgr2 = RCC->CFGR2;
#elif DBG_HUB_STM32F0
	g_dbg.mcu.rcc.ahbrstr = RCC->AHBRSTR;
	g_dbg.mcu.rcc.cfgr2 = RCC->CFGR2;
	g_dbg.mcu.rcc.cfgr3 = RCC->CFGR3;
	g_dbg.mcu.rcc.cr2 = RCC->CR2;
#elif defined(RCC_CFGR2_PREDIV1)
	g_dbg.mcu.rcc.ahbrstr = 0U;
	g_dbg.mcu.rcc.cfgr2 = RCC->CFGR2;
#else
	g_dbg.mcu.rcc.ahbrstr = 0U;
	g_dbg.mcu.rcc.cfgr2 = 0U;
#endif
#if !DBG_HUB_STM32F0
	g_dbg.mcu.rcc.cfgr3 = 0U;
	g_dbg.mcu.rcc.cr2 = 0U;
#endif
}

void DBG_CaptureClock(void)
{
	static const uint16_t ahb_div[16] = {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 2U, 4U, 8U, 16U, 64U, 128U, 256U, 512U};
	static const uint8_t apb_div[8] = {1U, 1U, 1U, 1U, 2U, 4U, 8U, 16U};
	uint32_t cfgr;
	uint32_t bdcr;
	uint32_t cr;
	uint32_t csr;
	uint32_t hclk;
	uint32_t hpre;
	uint32_t ppre1;
	uint32_t ppre2;

	cfgr = RCC->CFGR;
	bdcr = RCC->BDCR;
	cr = RCC->CR;
	csr = RCC->CSR;

	hpre = ahb_div[(cfgr >> 4) & 0x0FU];
	ppre1 = apb_div[(cfgr >> 8) & 0x07U];
	ppre2 = apb_div[(cfgr >> 11) & 0x07U];
	hclk = SystemCoreClock;

	g_dbg.mcu.clock.hclk_hz = hclk;
	g_dbg.mcu.clock.sysclk_hz = hclk * hpre;
	g_dbg.mcu.clock.pclk1_hz = hclk / ppre1;
	g_dbg.mcu.clock.pclk2_hz = hclk / ppre2;
	g_dbg.mcu.clock.sysclk_src = (uint8_t)((cfgr & RCC_CFGR_SWS) >> 2);
	g_dbg.mcu.clock.hse_ready = dbg_bit(cr, RCC_CR_HSERDY);
	g_dbg.mcu.clock.hsi_ready = dbg_bit(cr, RCC_CR_HSIRDY);
	g_dbg.mcu.clock.pll_ready = dbg_bit(cr, RCC_CR_PLLRDY);
	g_dbg.mcu.clock.lsi_ready = dbg_bit(csr, RCC_CSR_LSIRDY);
	g_dbg.mcu.clock.lse_ready = dbg_bit(bdcr, RCC_BDCR_LSERDY);

#if DBG_HUB_STM32F1
	g_dbg.mcu.clock.gpioa_en = dbg_bit(RCC->APB2ENR, RCC_APB2ENR_IOPAEN);
	g_dbg.mcu.clock.gpiob_en = dbg_bit(RCC->APB2ENR, RCC_APB2ENR_IOPBEN);
	g_dbg.mcu.clock.gpioc_en = dbg_bit(RCC->APB2ENR, RCC_APB2ENR_IOPCEN);
	g_dbg.mcu.clock.gpiod_en = dbg_bit(RCC->APB2ENR, RCC_APB2ENR_IOPDEN);
#elif DBG_HUB_STM32F0
#if defined(RCC_AHBENR_GPIOAEN)
	g_dbg.mcu.clock.gpioa_en = dbg_bit(RCC->AHBENR, RCC_AHBENR_GPIOAEN);
	g_dbg.mcu.clock.gpiob_en = dbg_bit(RCC->AHBENR, RCC_AHBENR_GPIOBEN);
	g_dbg.mcu.clock.gpioc_en = dbg_bit(RCC->AHBENR, RCC_AHBENR_GPIOCEN);
	g_dbg.mcu.clock.gpiod_en = dbg_bit(RCC->AHBENR, RCC_AHBENR_GPIODEN);
#elif defined(RCC_AHBPeriph_GPIOA)
	g_dbg.mcu.clock.gpioa_en = dbg_bit(RCC->AHBENR, RCC_AHBPeriph_GPIOA);
	g_dbg.mcu.clock.gpiob_en = dbg_bit(RCC->AHBENR, RCC_AHBPeriph_GPIOB);
	g_dbg.mcu.clock.gpioc_en = dbg_bit(RCC->AHBENR, RCC_AHBPeriph_GPIOC);
	g_dbg.mcu.clock.gpiod_en = dbg_bit(RCC->AHBENR, RCC_AHBPeriph_GPIOD);
#else
	g_dbg.mcu.clock.gpioa_en = 0U;
	g_dbg.mcu.clock.gpiob_en = 0U;
	g_dbg.mcu.clock.gpioc_en = 0U;
	g_dbg.mcu.clock.gpiod_en = 0U;
#endif
#else
	g_dbg.mcu.clock.gpioa_en = 0U;
	g_dbg.mcu.clock.gpiob_en = 0U;
	g_dbg.mcu.clock.gpioc_en = 0U;
	g_dbg.mcu.clock.gpiod_en = 0U;
#endif
#if defined(RCC_AHBENR_DMA1EN)
	g_dbg.mcu.clock.dma1_en = dbg_bit(RCC->AHBENR, RCC_AHBENR_DMA1EN);
#elif defined(RCC_AHBENR_DMAEN)
	g_dbg.mcu.clock.dma1_en = dbg_bit(RCC->AHBENR, RCC_AHBENR_DMAEN);
#elif defined(RCC_AHBPeriph_DMA1)
	g_dbg.mcu.clock.dma1_en = dbg_bit(RCC->AHBENR, RCC_AHBPeriph_DMA1);
#else
	g_dbg.mcu.clock.dma1_en = 0U;
#endif
#if defined(RCC_APB2ENR_ADC1EN)
	g_dbg.mcu.clock.adc1_en = dbg_bit(RCC->APB2ENR, RCC_APB2ENR_ADC1EN);
#elif defined(RCC_APB2ENR_ADCEN)
	g_dbg.mcu.clock.adc1_en = dbg_bit(RCC->APB2ENR, RCC_APB2ENR_ADCEN);
#else
	g_dbg.mcu.clock.adc1_en = 0U;
#endif
	g_dbg.mcu.clock.usart1_en = dbg_bit(RCC->APB2ENR, RCC_APB2ENR_USART1EN);
	g_dbg.mcu.clock.usart2_en = dbg_bit(RCC->APB1ENR, RCC_APB1ENR_USART2EN);
	g_dbg.mcu.clock.i2c1_en = dbg_bit(RCC->APB1ENR, RCC_APB1ENR_I2C1EN);
#if defined(RCC_APB1ENR_CAN1EN)
	g_dbg.mcu.clock.can1_en = dbg_bit(RCC->APB1ENR, RCC_APB1ENR_CAN1EN);
#elif defined(RCC_APB1ENR_CANEN)
	g_dbg.mcu.clock.can1_en = dbg_bit(RCC->APB1ENR, RCC_APB1ENR_CANEN);
#else
	g_dbg.mcu.clock.can1_en = 0U;
#endif
	g_dbg.mcu.clock.rtc_en = dbg_bit(RCC->BDCR, RCC_BDCR_RTCEN);
	g_dbg.mcu.clock.pwr_en = dbg_bit(RCC->APB1ENR, RCC_APB1ENR_PWREN);
#if DBG_HUB_STM32F1
	g_dbg.mcu.clock.bkp_en = dbg_bit(RCC->APB1ENR, RCC_APB1ENR_BKPEN);
#else
	g_dbg.mcu.clock.bkp_en = 0U;
#endif
}

static void dbg_capture_reset(void)
{
	uint32_t csr = RCC->CSR;

	g_dbg.mcu.reset.raw_rcc_csr = csr;
	g_dbg.mcu.reset.pin = dbg_bit(csr, RCC_CSR_PINRSTF);
	g_dbg.mcu.reset.por = dbg_bit(csr, RCC_CSR_PORRSTF);
	g_dbg.mcu.reset.software = dbg_bit(csr, RCC_CSR_SFTRSTF);
	g_dbg.mcu.reset.iwdg = dbg_bit(csr, RCC_CSR_IWDGRSTF);
	g_dbg.mcu.reset.wwdg = dbg_bit(csr, RCC_CSR_WWDGRSTF);
	g_dbg.mcu.reset.low_power = dbg_bit(csr, RCC_CSR_LPWRRSTF);
	g_dbg.mcu.reset.option_byte = 0U;
	g_dbg.mcu.reset.backup_domain = 0U;
	g_dbg.mcu.iwdg.reset_flag = g_dbg.mcu.reset.iwdg;
}

void DBG_CaptureNvic(void)
{
	uint8_t i;

	g_dbg.mcu.nvic.iser[0] = NVIC->ISER[0];
	g_dbg.mcu.nvic.ispr[0] = NVIC->ISPR[0];
	g_dbg.mcu.nvic.iabr[0] = NVIC->IABR[0];
#if DBG_HUB_STM32F1
	g_dbg.mcu.nvic.iser[1] = NVIC->ISER[1];
	g_dbg.mcu.nvic.ispr[1] = NVIC->ISPR[1];
	g_dbg.mcu.nvic.iabr[1] = NVIC->IABR[1];
#else
	g_dbg.mcu.nvic.iser[1] = 0U;
	g_dbg.mcu.nvic.ispr[1] = 0U;
	g_dbg.mcu.nvic.iabr[1] = 0U;
#endif

#if PROJECT_CFG_IRQ_DEBUG_ENABLE
	for (i = 0U; (i < DBG_HUB_IRQ_SLOTS) && (i < (uint8_t)IRQDBG_COUNT); i++)
	{
		g_dbg.mcu.nvic.irq_enter_count[i] = g_stIrqDebug.total[i];
	}
	g_dbg.mcu.nvic.last_irq_id = g_stIrqDebug.last_id;
	g_dbg.mcu.nvic.last_vectactive = g_stIrqDebug.last_vectactive;
	g_dbg.mcu.nvic.current_phase = g_stIrqDebug.current_phase;
	g_dbg.mcu.nvic.last_phase = g_stIrqDebug.last_phase;
	g_dbg.mcu.nvic.event_count = g_stIrqDebug.event_count;
#else
	for (i = 0U; i < DBG_HUB_IRQ_SLOTS; i++)
	{
		g_dbg.mcu.nvic.irq_enter_count[i] = 0U;
	}
	g_dbg.mcu.nvic.last_irq_id = 0U;
	g_dbg.mcu.nvic.last_vectactive = 0U;
	g_dbg.mcu.nvic.current_phase = 0U;
	g_dbg.mcu.nvic.last_phase = 0U;
	g_dbg.mcu.nvic.event_count = 0U;
#endif
}

void DBG_CaptureExti(void)
{
	g_dbg.mcu.exti.imr = EXTI->IMR;
	g_dbg.mcu.exti.emr = EXTI->EMR;
	g_dbg.mcu.exti.rtsr = EXTI->RTSR;
	g_dbg.mcu.exti.ftsr = EXTI->FTSR;
	g_dbg.mcu.exti.pr = EXTI->PR;

#if PROJECT_CFG_IRQ_DEBUG_ENABLE
	g_dbg.mcu.exti.line_trigger_count[0] = g_stIrqDebug.total[IRQDBG_EXTI0_CHG_IN];
	g_dbg.mcu.exti.line_trigger_count[2] = g_stIrqDebug.total[IRQDBG_EXTI2_STRAY];
	g_dbg.mcu.exti.line_trigger_count[3] = g_stIrqDebug.total[IRQDBG_EXTI3_STRAY];
	g_dbg.mcu.exti.line_trigger_count[5] = g_stIrqDebug.total[IRQDBG_EXTI5_STRAY];
	g_dbg.mcu.exti.line_trigger_count[6] = g_stIrqDebug.total[IRQDBG_EXTI6_STRAY];
	g_dbg.mcu.exti.line_trigger_count[7] = g_stIrqDebug.total[IRQDBG_EXTI7_UART1_WAKE];
	g_dbg.mcu.exti.line_trigger_count[9] = g_stIrqDebug.total[IRQDBG_EXTI9_SW_KEY];
	g_dbg.mcu.exti.line_trigger_count[12] = g_stIrqDebug.total[IRQDBG_EXTI12_CMNT_WAKE];
	g_dbg.mcu.exti.line_trigger_count[13] = g_stIrqDebug.total[IRQDBG_EXTI13_MCU_WAKE];
	g_dbg.mcu.exti.spurious_count = g_stIrqDebug.total[IRQDBG_EXTI_GROUP_SPURIOUS];
#else
	g_dbg.mcu.exti.line_trigger_count[0] = sys_time.cnt_PA0_irq;
	g_dbg.mcu.exti.line_trigger_count[9] = sys_time.cnt_bms1_keyirq;
	g_dbg.mcu.exti.spurious_count = 0U;
#endif
}

static void dbg_capture_gpio_port(GPIO_TypeDef *gpio, volatile DBG_HubGpioPort_t *dst)
{
#if DBG_HUB_STM32F1
	dst->crl = gpio->CRL;
	dst->crh = gpio->CRH;
	dst->moder = 0U;
	dst->otyper = 0U;
	dst->ospeedr = 0U;
	dst->pupdr = 0U;
	dst->afr[0] = 0U;
	dst->afr[1] = 0U;
#elif DBG_HUB_STM32F0
	dst->crl = 0U;
	dst->crh = 0U;
	dst->moder = gpio->MODER;
	dst->otyper = gpio->OTYPER;
	dst->ospeedr = gpio->OSPEEDR;
	dst->pupdr = gpio->PUPDR;
	dst->afr[0] = gpio->AFR[0];
	dst->afr[1] = gpio->AFR[1];
#else
	dst->crl = 0U;
	dst->crh = 0U;
	dst->moder = 0U;
	dst->otyper = 0U;
	dst->ospeedr = 0U;
	dst->pupdr = 0U;
	dst->afr[0] = 0U;
	dst->afr[1] = 0U;
#endif
	dst->idr = gpio->IDR;
	dst->odr = gpio->ODR;
	dst->lckr = gpio->LCKR;
}

void DBG_CaptureGpio(void)
{
	dbg_capture_gpio_port(GPIOA, &g_dbg.mcu.gpio.porta);
	dbg_capture_gpio_port(GPIOB, &g_dbg.mcu.gpio.portb);
	dbg_capture_gpio_port(GPIOC, &g_dbg.mcu.gpio.portc);
	dbg_capture_gpio_port(GPIOD, &g_dbg.mcu.gpio.portd);

	g_dbg.mcu.gpio.named.key_raw = dbg_gpio_in(GPIO_SW, PIN_SW);
	g_dbg.mcu.gpio.named.key_pressed = (g_dbg.mcu.gpio.named.key_raw == 0U) ? 1U : 0U;
	g_dbg.mcu.gpio.named.charger_det_raw = dbg_gpio_in(GPIO_CHG_IN, PIN_CHG_IN);
	g_dbg.mcu.gpio.named.charger_connected = (g_dbg.mcu.gpio.named.charger_det_raw == 0U) ? 1U : 0U;
	g_dbg.mcu.gpio.named.load_det_raw = dbg_gpio_in(GPIO_MCU_WK, PIN_MCU_WK);
	g_dbg.mcu.gpio.named.load_present = g_dbg.mcu.gpio.named.load_det_raw;
	g_dbg.mcu.gpio.named.afe_alert_raw = 0xFFU;
	g_dbg.mcu.gpio.named.afe_alert_valid = 0U;
	g_dbg.mcu.gpio.named.chg_mos = (uint8_t)SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS;
	g_dbg.mcu.gpio.named.dsg_mos = (uint8_t)SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS;
	g_dbg.mcu.gpio.named.pre_mos = (uint8_t)SH367309_Reg_Store.REG_MTP_CONF.bits.PCHMOS;
	g_dbg.mcu.gpio.named.rs485_dir = 0xFFU;
	g_dbg.mcu.gpio.named.mcc_c_gpio = dbg_gpio_out(GPIO_MCC_C, PIN_MCC_C);
	g_dbg.mcu.gpio.named.cmnt_en_gpio = dbg_gpio_out(GPIO_CMNT_EN, PIN_CMNT_EN);
	g_dbg.mcu.gpio.named.dc_en_gpio = dbg_gpio_out(GPIO_DC_EN, PIN_DC_EN);
	g_dbg.mcu.gpio.named.afe_ctl_gpio = dbg_gpio_out(GPIO_AFE1_CTL, PIN_AFE1_CTL);
}

static void dbg_capture_usart(USART_TypeDef *uart, volatile DBG_HubUsart_t *dst, uint8_t enabled, uint8_t irq_id)
{
	uint32_t sr;

	if (enabled == 0U)
	{
		dst->sr = 0U;
		dst->brr = 0U;
		dst->cr1 = 0U;
		dst->cr2 = 0U;
		dst->cr3 = 0U;
	}
	else
	{
		sr = uart->SR;
		dst->sr = sr;
		dst->brr = uart->BRR;
		dst->cr1 = uart->CR1;
		dst->cr2 = uart->CR2;
		dst->cr3 = uart->CR3;
		dst->rxne = dbg_bit(sr, USART_SR_RXNE);
		dst->txe = dbg_bit(sr, USART_SR_TXE);
		dst->tc = dbg_bit(sr, USART_SR_TC);
		dst->idle = dbg_bit(sr, USART_SR_IDLE);
		dst->ore = dbg_bit(sr, USART_SR_ORE);
		dst->fe = dbg_bit(sr, USART_SR_FE);
		dst->ne = dbg_bit(sr, USART_SR_NE);
		dst->pe = dbg_bit(sr, USART_SR_PE);
	}

#if PROJECT_CFG_IRQ_DEBUG_ENABLE
	if (irq_id < (uint8_t)IRQDBG_COUNT)
	{
		dst->irq_count = g_stIrqDebug.total[irq_id];
	}
#else
	if (irq_id == (uint8_t)IRQDBG_USART1)
	{
		dst->irq_count = sys_time.sci1_irq_cnt;
	}
	else if (irq_id == (uint8_t)IRQDBG_USART2)
	{
		dst->irq_count = sys_time.sci2_irq_cnt;
	}
#endif
}

static void dbg_capture_dma(void)
{
	g_dbg.mcu.dma.isr = DMA1->ISR;
	g_dbg.mcu.dma.ch[0].ccr = DMA1_Channel1->CCR;
	g_dbg.mcu.dma.ch[0].cndtr = DMA1_Channel1->CNDTR;
	g_dbg.mcu.dma.ch[0].cpar = DMA1_Channel1->CPAR;
	g_dbg.mcu.dma.ch[0].cmar = DMA1_Channel1->CMAR;
	g_dbg.mcu.dma.ch[1].ccr = DMA1_Channel2->CCR;
	g_dbg.mcu.dma.ch[1].cndtr = DMA1_Channel2->CNDTR;
	g_dbg.mcu.dma.ch[1].cpar = DMA1_Channel2->CPAR;
	g_dbg.mcu.dma.ch[1].cmar = DMA1_Channel2->CMAR;
	g_dbg.mcu.dma.ch[2].ccr = DMA1_Channel3->CCR;
	g_dbg.mcu.dma.ch[2].cndtr = DMA1_Channel3->CNDTR;
	g_dbg.mcu.dma.ch[2].cpar = DMA1_Channel3->CPAR;
	g_dbg.mcu.dma.ch[2].cmar = DMA1_Channel3->CMAR;
	g_dbg.mcu.dma.ch[3].ccr = DMA1_Channel4->CCR;
	g_dbg.mcu.dma.ch[3].cndtr = DMA1_Channel4->CNDTR;
	g_dbg.mcu.dma.ch[3].cpar = DMA1_Channel4->CPAR;
	g_dbg.mcu.dma.ch[3].cmar = DMA1_Channel4->CMAR;
	g_dbg.mcu.dma.ch[4].ccr = DMA1_Channel5->CCR;
	g_dbg.mcu.dma.ch[4].cndtr = DMA1_Channel5->CNDTR;
	g_dbg.mcu.dma.ch[4].cpar = DMA1_Channel5->CPAR;
	g_dbg.mcu.dma.ch[4].cmar = DMA1_Channel5->CMAR;
	g_dbg.mcu.dma.ch[5].ccr = DMA1_Channel6->CCR;
	g_dbg.mcu.dma.ch[5].cndtr = DMA1_Channel6->CNDTR;
	g_dbg.mcu.dma.ch[5].cpar = DMA1_Channel6->CPAR;
	g_dbg.mcu.dma.ch[5].cmar = DMA1_Channel6->CMAR;
	g_dbg.mcu.dma.ch[6].ccr = DMA1_Channel7->CCR;
	g_dbg.mcu.dma.ch[6].cndtr = DMA1_Channel7->CNDTR;
	g_dbg.mcu.dma.ch[6].cpar = DMA1_Channel7->CPAR;
	g_dbg.mcu.dma.ch[6].cmar = DMA1_Channel7->CMAR;
	g_dbg.mcu.dma.adc_dma_channel = 1U;
	g_dbg.mcu.dma.uart_rx_dma_channel = 0U;
	g_dbg.mcu.dma.uart_tx_dma_channel = 0U;
}

static void dbg_capture_adc(void)
{
	uint8_t i;

#if DBG_HUB_STM32F1
	if ((RCC->APB2ENR & RCC_APB2ENR_ADC1EN) != 0U)
	{
		g_dbg.mcu.adc.sr = ADC1->SR;
		g_dbg.mcu.adc.cr1 = ADC1->CR1;
		g_dbg.mcu.adc.cr2 = ADC1->CR2;
		g_dbg.mcu.adc.smpr1 = ADC1->SMPR1;
		g_dbg.mcu.adc.smpr2 = ADC1->SMPR2;
		g_dbg.mcu.adc.sqr1 = ADC1->SQR1;
		g_dbg.mcu.adc.sqr2 = ADC1->SQR2;
		g_dbg.mcu.adc.sqr3 = ADC1->SQR3;
	}
#elif DBG_HUB_STM32F0
#if defined(RCC_APB2ENR_ADC1EN)
	if ((RCC->APB2ENR & RCC_APB2ENR_ADC1EN) != 0U)
#elif defined(RCC_APB2ENR_ADCEN)
	if ((RCC->APB2ENR & RCC_APB2ENR_ADCEN) != 0U)
#else
	if (1)
#endif
	{
		g_dbg.mcu.adc.sr = ADC1->ISR;
		g_dbg.mcu.adc.cr1 = ADC1->CR;
		g_dbg.mcu.adc.cr2 = ADC1->CFGR1;
		g_dbg.mcu.adc.smpr1 = ADC1->SMPR;
		g_dbg.mcu.adc.smpr2 = 0U;
		g_dbg.mcu.adc.sqr1 = ADC1->CHSELR;
		g_dbg.mcu.adc.sqr2 = ADC1->CFGR2;
		g_dbg.mcu.adc.sqr3 = 0U;
	}
#endif
	g_dbg.mcu.adc.dr_read_skipped = 1U;
	g_dbg.mcu.adc.ready = ADC_IsReady();
	g_dbg.mcu.adc.vbat_mv = ADC_GetVbatMilliVolt();
	g_dbg.mcu.adc.typec_current_ma = ADC_GetTypeCOutCurrentMilliAmp();
	for (i = 0U; i < DBG_HUB_ADC_KEYS; i++)
	{
		g_dbg.mcu.adc.raw[i] = ADC_GetRaw(i);
		g_dbg.mcu.adc.value[i] = ADC_GetResult(i);
	}
}

static void dbg_capture_i2c1(void)
{
#if DBG_HUB_STM32F1
	if ((RCC->APB1ENR & RCC_APB1ENR_I2C1EN) != 0U)
	{
		g_dbg.mcu.i2c1.cr1 = I2C1->CR1;
		g_dbg.mcu.i2c1.cr2 = I2C1->CR2;
		g_dbg.mcu.i2c1.ccr = I2C1->CCR;
		g_dbg.mcu.i2c1.trise = I2C1->TRISE;
		g_dbg.mcu.i2c1.busy = (uint8_t)(((g_dbg.mcu.i2c1.sr2 & I2C_SR2_BUSY) != 0U) ? 1U : 0U);
	}
	g_dbg.mcu.i2c1.sr_read_skipped = (g_dbg.ctrl.i2c_sr_auto_read == 0U) ? 1U : 0U;
	if (g_dbg.ctrl.i2c_sr_auto_read != 0U)
	{
		g_dbg.mcu.i2c1.sr1 = I2C1->SR1;
		g_dbg.mcu.i2c1.sr2 = I2C1->SR2;
		g_dbg.mcu.i2c1.sr_read_skipped = 0U;
	}
#elif DBG_HUB_STM32F0
	if ((RCC->APB1ENR & RCC_APB1ENR_I2C1EN) != 0U)
	{
		g_dbg.mcu.i2c1.cr1 = I2C1->CR1;
		g_dbg.mcu.i2c1.cr2 = I2C1->CR2;
		g_dbg.mcu.i2c1.sr1 = I2C1->ISR;
		g_dbg.mcu.i2c1.sr2 = 0U;
		g_dbg.mcu.i2c1.ccr = I2C1->TIMINGR;
		g_dbg.mcu.i2c1.trise = I2C1->TIMEOUTR;
#if defined(I2C_ISR_BUSY)
		g_dbg.mcu.i2c1.busy = (uint8_t)(((g_dbg.mcu.i2c1.sr1 & I2C_ISR_BUSY) != 0U) ? 1U : 0U);
#else
		g_dbg.mcu.i2c1.busy = 0U;
#endif
	}
	g_dbg.mcu.i2c1.sr_read_skipped = 0U;
#endif
}

static void dbg_capture_can(void)
{
#if defined(CAN1)
	uint32_t esr;

#if defined(RCC_APB1ENR_CAN1EN)
	if ((RCC->APB1ENR & RCC_APB1ENR_CAN1EN) == 0U)
#elif defined(RCC_APB1ENR_CANEN)
	if ((RCC->APB1ENR & RCC_APB1ENR_CANEN) == 0U)
#else
	if (0)
#endif
	{
		return;
	}

	esr = CAN1->ESR;
	g_dbg.mcu.can.mcr = CAN1->MCR;
	g_dbg.mcu.can.msr = CAN1->MSR;
	g_dbg.mcu.can.tsr = CAN1->TSR;
	g_dbg.mcu.can.rf0r = CAN1->RF0R;
	g_dbg.mcu.can.rf1r = CAN1->RF1R;
	g_dbg.mcu.can.ier = CAN1->IER;
	g_dbg.mcu.can.esr = esr;
	g_dbg.mcu.can.btr = CAN1->BTR;
	g_dbg.mcu.can.bus_off = dbg_bit(esr, CAN_ESR_BOFF);
	g_dbg.mcu.can.error_passive = dbg_bit(esr, CAN_ESR_EPVF);
	g_dbg.mcu.can.error_warning = dbg_bit(esr, CAN_ESR_EWGF);
	g_dbg.mcu.can.lec = (uint8_t)((esr & CAN_ESR_LEC) >> 4);
	g_dbg.mcu.can.tec = (uint8_t)((esr & CAN_ESR_TEC) >> 16);
	g_dbg.mcu.can.rec = (uint8_t)((esr & CAN_ESR_REC) >> 24);
	g_dbg.mcu.can.fifo0_pending = (uint8_t)(CAN1->RF0R & 0x03U);
	g_dbg.mcu.can.fifo1_pending = (uint8_t)(CAN1->RF1R & 0x03U);
#else
	g_dbg.mcu.can.bus_off = 0U;
	g_dbg.mcu.can.error_passive = 0U;
	g_dbg.mcu.can.error_warning = 0U;
#endif
}

static void dbg_capture_pwr_iwdg(void)
{
	if ((RCC->APB1ENR & RCC_APB1ENR_PWREN) != 0U)
	{
		g_dbg.mcu.pwr.cr = PWR->CR;
		g_dbg.mcu.pwr.csr = PWR->CSR;
		g_dbg.mcu.pwr.dbp = dbg_bit(PWR->CR, PWR_CR_DBP);
		g_dbg.mcu.pwr.wuf = dbg_bit(PWR->CSR, PWR_CSR_WUF);
		g_dbg.mcu.pwr.sbf = dbg_bit(PWR->CSR, PWR_CSR_SBF);
		g_dbg.mcu.pwr.regulator_stop_ready = 0U;
	}
	g_dbg.mcu.iwdg.pr = IWDG->PR;
	g_dbg.mcu.iwdg.rlr = IWDG->RLR;
	g_dbg.mcu.iwdg.sr = IWDG->SR;
#if PROJECT_CFG_WDOG_ENABLE
	g_dbg.mcu.iwdg.enabled_by_config = 1U;
#else
	g_dbg.mcu.iwdg.enabled_by_config = 0U;
#endif
}

void DBG_CaptureRtcBkp(void)
{
#if DBG_HUB_STM32F1
	uint32_t counter;

	g_dbg.mcu.rtc.crh = RTC->CRH;
	g_dbg.mcu.rtc.crl = RTC->CRL;
	g_dbg.mcu.rtc.prlh = RTC->PRLH;
	g_dbg.mcu.rtc.prll = RTC->PRLL;
	g_dbg.mcu.rtc.divh = RTC->DIVH;
	g_dbg.mcu.rtc.divl = RTC->DIVL;
	g_dbg.mcu.rtc.cnth = RTC->CNTH;
	g_dbg.mcu.rtc.cntl = RTC->CNTL;
	g_dbg.mcu.rtc.alrh = RTC->ALRH;
	g_dbg.mcu.rtc.alrl = RTC->ALRL;
	counter = ((RTC->CNTH & 0xFFFFUL) << 16) | (RTC->CNTL & 0xFFFFUL);
	g_dbg.mcu.rtc.counter = counter;
	g_dbg.mcu.rtc.running = dbg_bit(RCC->BDCR, RCC_BDCR_RTCEN);
	g_dbg.mcu.rtc.wake_flag = dbg_bit(RTC->CRL, RTC_CRL_SECF);
	g_dbg.mcu.rtc.alarm_flag = dbg_bit(RTC->CRL, RTC_CRL_ALRF);
	g_dbg.mcu.rtc.sync_ready = dbg_bit(RTC->CRL, RTC_CRL_RSF);

	g_dbg.mcu.bkp.raw[0] = BKP->DR1;
	g_dbg.mcu.bkp.raw[1] = BKP->DR2;
	g_dbg.mcu.bkp.raw[2] = BKP->DR3;
	g_dbg.mcu.bkp.raw[3] = BKP->DR4;
	g_dbg.mcu.bkp.raw[4] = BKP->DR5;
	g_dbg.mcu.bkp.raw[5] = BKP->DR6;
	g_dbg.mcu.bkp.raw[6] = BKP->DR7;
	g_dbg.mcu.bkp.raw[7] = BKP->DR8;
	g_dbg.mcu.bkp.raw[8] = BKP->DR9;
	g_dbg.mcu.bkp.raw[9] = BKP->DR10;
	g_dbg.mcu.bkp.raw[10] = BKP->DR11;
	g_dbg.mcu.bkp.raw[11] = BKP->DR12;
	g_dbg.mcu.bkp.raw[12] = BKP->DR13;
	g_dbg.mcu.bkp.raw[13] = BKP->DR14;
	g_dbg.mcu.bkp.raw[14] = BKP->DR15;
	g_dbg.mcu.bkp.raw[15] = BKP->DR16;
	g_dbg.mcu.bkp.magic = BKP->DR1;
	g_dbg.mcu.bkp.boot_flag = BKP->DR2;
	g_dbg.mcu.bkp.boot_flag_inv = BKP->DR3;
	g_dbg.mcu.bkp.sleep_flag = BKP->DR4;
	g_dbg.mcu.bkp.fault_flag = BKP->DR11;
	g_dbg.mcu.bkp.fault_flag_inv = BKP->DR12;
#elif DBG_HUB_STM32F0
	g_dbg.mcu.rtc.tr = RTC->TR;
	g_dbg.mcu.rtc.dr = RTC->DR;
	g_dbg.mcu.rtc.cr = RTC->CR;
	g_dbg.mcu.rtc.isr = RTC->ISR;
	g_dbg.mcu.rtc.prer = RTC->PRER;
	g_dbg.mcu.rtc.wutr = RTC->WUTR;
	g_dbg.mcu.rtc.alrmar = RTC->ALRMAR;
	g_dbg.mcu.rtc.running = dbg_bit(RCC->BDCR, RCC_BDCR_RTCEN);
#if defined(RTC_ISR_WUTF)
	g_dbg.mcu.rtc.wake_flag = dbg_bit(RTC->ISR, RTC_ISR_WUTF);
#else
	g_dbg.mcu.rtc.wake_flag = 0U;
#endif
#if defined(RTC_ISR_ALRAF)
	g_dbg.mcu.rtc.alarm_flag = dbg_bit(RTC->ISR, RTC_ISR_ALRAF);
#else
	g_dbg.mcu.rtc.alarm_flag = 0U;
#endif
#if defined(RTC_ISR_RSF)
	g_dbg.mcu.rtc.sync_ready = dbg_bit(RTC->ISR, RTC_ISR_RSF);
#else
	g_dbg.mcu.rtc.sync_ready = 0U;
#endif
	g_dbg.mcu.bkp.raw[0] = RTC->BKP0R;
	g_dbg.mcu.bkp.raw[1] = RTC->BKP1R;
	g_dbg.mcu.bkp.raw[2] = RTC->BKP2R;
	g_dbg.mcu.bkp.raw[3] = RTC->BKP3R;
	g_dbg.mcu.bkp.raw[4] = RTC->BKP4R;
	g_dbg.mcu.bkp.magic = (uint16_t)RTC->BKP0R;
	g_dbg.mcu.bkp.boot_flag = (uint16_t)RTC->BKP1R;
	g_dbg.mcu.bkp.boot_flag_inv = (uint16_t)RTC->BKP2R;
	g_dbg.mcu.bkp.sleep_flag = (uint16_t)RTC->BKP3R;
	g_dbg.mcu.bkp.fault_flag = (uint16_t)RTC->BKP4R;
	g_dbg.mcu.bkp.fault_flag_inv = 0U;
#else
	g_dbg.mcu.rtc.running = 0U;
#endif
	g_dbg.mcu.bkp.last_wakeup_source = (uint16_t)g_irq_t;
	g_dbg.mcu.bkp.boot_flag_valid =
		(((uint16_t)(g_dbg.mcu.bkp.boot_flag ^ g_dbg.mcu.bkp.boot_flag_inv)) == 0xFFFFU) ? 1U : 0U;
	g_dbg.mcu.bkp.fault_flag_valid =
		(((uint16_t)(g_dbg.mcu.bkp.fault_flag ^ g_dbg.mcu.bkp.fault_flag_inv)) == 0xFFFFU) ? 1U : 0U;
}

static void dbg_capture_business(void)
{
	const volatile uint8_t *err;
	uint8_t i;

	g_dbg.bms.system_status = SystemRuntime_GetStatusSnapshot();
	g_dbg.bms.feature_mask = SystemFeature_GetMask();
	err = (const volatile uint8_t *)&System_ErrFlag;
	g_dbg.bms.sys_error_word0 = ((uint32_t)err[0]) |
		((uint32_t)err[1] << 8) |
		((uint32_t)err[2] << 16) |
		((uint32_t)err[3] << 24);
	g_dbg.bms.sys_error_word1 = ((uint32_t)err[4]) |
		((uint32_t)err[5] << 8) |
		((uint32_t)err[6] << 16) |
		((uint32_t)err[7] << 24);
	g_dbg.bms.main_cycle = (uint32_t)sys_time.test_main_cycle;
	g_dbg.bms.series_num = SeriesNum;
	g_dbg.bms.pack_mv_x100 = g_stCellInfoReport.u16VCellTotle;
	g_dbg.bms.cell_min_mv = g_stCellInfoReport.u16VCellMin;
	g_dbg.bms.cell_max_mv = g_stCellInfoReport.u16VCellMax;
	g_dbg.bms.cell_delta_mv = g_stCellInfoReport.u16VCellDelta;
	g_dbg.bms.temp_max = g_stCellInfoReport.u16TempMax;
	g_dbg.bms.temp_min = g_stCellInfoReport.u16TempMin;
	g_dbg.bms.ichg_a10 = g_stCellInfoReport.u16Ichg;
	g_dbg.bms.idsg_a10 = g_stCellInfoReport.u16IDischg;
	g_dbg.bms.adc_ready = ADC_IsReady();
	g_dbg.bms.afe_ready = AFE_IsReady();
	g_dbg.bms.flash_busy = StorageFlash_IsBusy();
	g_dbg.bms.aging_state = FactoryAging_GetState();
	g_dbg.bms.aging_remaining_sec = FactoryAging_GetRemainingSeconds();

	g_dbg.protect.fault_first = Fault_Flag_Fisrt.all;
	g_dbg.protect.fault_second = Fault_Flag_Second.all;
	g_dbg.protect.fault_third = Fault_Flag_Third.all;
	g_dbg.protect.mdl_fault_first = g_stCellInfoReport.unMdlFault_First.all;
	g_dbg.protect.mdl_fault_second = g_stCellInfoReport.unMdlFault_Second.all;
	g_dbg.protect.mdl_fault_third = g_stCellInfoReport.unMdlFault_Third.all;
	g_dbg.protect.cov_first_mv = PRT_E2ROMParas.u16VcellOvp_First;
	g_dbg.protect.cuv_first_mv = PRT_E2ROMParas.u16VcellUvp_First;
	g_dbg.protect.bov_first_v10 = PRT_E2ROMParas.u16VbusOvp_First;
	g_dbg.protect.buv_first_v10 = PRT_E2ROMParas.u16VbusUvp_First;
	g_dbg.protect.ichg_ocp_first_a10 = PRT_E2ROMParas.u16IchgOcp_First;
	g_dbg.protect.idsg_ocp_first_a10 = PRT_E2ROMParas.u16IdsgOcp_First;
	g_dbg.protect.tchg_otp_first = PRT_E2ROMParas.u16TChgOTp_First;
	g_dbg.protect.tdsg_otp_first = PRT_E2ROMParas.u16TdischgOTp_First;
	g_dbg.protect.tmos_otp_first = PRT_E2ROMParas.u16TmosOTp_First;
	g_dbg.protect.vdelta_ovp_first_mv = PRT_E2ROMParas.u16VdeltaOvp_First;

	g_dbg.mos.sw_chg = SystemRuntime_IsChargeMosOpen();
	g_dbg.mos.sw_dsg = SystemRuntime_IsDischargeMosOpen();
	g_dbg.mos.afe_chg_fet = (uint8_t)SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;
	g_dbg.mos.afe_dsg_fet = (uint8_t)SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;
	g_dbg.mos.afe_pchg_fet = (uint8_t)SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET;
	g_dbg.mos.mtp_chgmos = (uint8_t)SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS;
	g_dbg.mos.mtp_dsgmos = (uint8_t)SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS;
	g_dbg.mos.mtp_pchmos = (uint8_t)SH367309_Reg_Store.REG_MTP_CONF.bits.PCHMOS;
	g_dbg.mos.mcc_c_gpio = dbg_gpio_out(GPIO_MCC_C, PIN_MCC_C);
	g_dbg.mos.rf_en_gpio = dbg_gpio_out(GPIO_RF_EN, PIN_RF_EN);
	g_dbg.mos.dc_en_gpio = dbg_gpio_out(GPIO_DC_EN, PIN_DC_EN);
	g_dbg.mos.boost_en_gpio = dbg_gpio_out(GPIO_2727_EN, PIN_2737_EN);

	g_dbg.soc.soc_pct = g_stCellInfoReport.SocElement.u16Soc;
	g_dbg.soc.soh_pct = g_stCellInfoReport.SocElement.u16Soh;
	g_dbg.soc.cap_now_ah100 = g_stCellInfoReport.SocElement.u16CapacityNow;
	g_dbg.soc.cap_full_ah100 = g_stCellInfoReport.SocElement.u16CapacityFull;
	g_dbg.soc.cap_factory_ah100 = g_stCellInfoReport.SocElement.u16CapacityFactory;
	g_dbg.soc.cycle_times = g_stCellInfoReport.SocElement.u16Cycle_times;
	g_dbg.soc.vcell_min_mv = g_stCellInfoReport.u16VCellMin;
	g_dbg.soc.vcell_max_mv = g_stCellInfoReport.u16VCellMax;
	g_dbg.soc.vcell_total_v100 = g_stCellInfoReport.u16VCellTotle;
	g_dbg.soc.ichg_a10 = g_stCellInfoReport.u16Ichg;
	g_dbg.soc.idsg_a10 = g_stCellInfoReport.u16IDischg;
	g_dbg.soc.typec_current_ma = ADC_GetTypeCOutCurrentMilliAmp();
	g_dbg.soc.vbat_mv = ADC_GetVbatMilliVolt();

	g_dbg.afe.bstatus1 = SH367309_Reg_Store.REG_BSTATUS1.all;
	g_dbg.afe.bstatus2 = SH367309_Reg_Store.REG_BSTATUS2.all;
	g_dbg.afe.bstatus3 = SH367309_Reg_Store.REG_BSTATUS3.all;
	g_dbg.afe.mtp_conf = SH367309_Reg_Store.REG_MTP_CONF.all;
	g_dbg.afe.current_raw = SH367309_Read_AFE1.u16Current;
	g_dbg.afe.cell_min_mv = g_stCellInfoReport.u16VCellMin;
	g_dbg.afe.cell_max_mv = g_stCellInfoReport.u16VCellMax;
	g_dbg.afe.pec_error_count = sys_time.pec_err_cnt;
	g_dbg.afe.afe_seq = AfeCurrent_GetSeq();
	g_dbg.afe.current_zero_ready = AfeCurrent_IsStartupZeroDone();
	g_dbg.afe.current_zero_state = g_dbg.afe.current_zero_ready ? 2U : 1U;
	g_dbg.afe.startup_cold_boot = 0U;

	g_dbg.comm.sci1_irq_count = sys_time.sci1_irq_cnt;
	g_dbg.comm.sci2_irq_count = sys_time.sci2_irq_cnt;
	g_dbg.comm.sci3_irq_count = sys_time.sci3_irq_cnt;
	g_dbg.comm.flash_update_flag = u8FlashUpdateFlag;
	g_dbg.comm.flash_update_e2prom = u8FlashUpdateE2PROM;
	g_dbg.comm.can_busy = Can_PeekBusy();
	g_dbg.comm.can_bus_off = g_dbg.mcu.can.bus_off;

	g_dbg.sleep.mode = g_stLowPowerRtcStatus.mode;
	g_dbg.sleep.rtc_wake = g_stLowPowerRtcStatus.rtc;
	g_dbg.sleep.boot_from_sleep = (g_dbg.mcu.bkp.boot_flag_valid != 0U) &&
		(g_dbg.mcu.bkp.boot_flag != BOOT_FLAG_RESET_VALUE) ? 1U : 0U;
	g_dbg.sleep.boot_charger_wake = (g_dbg.mcu.bkp.boot_flag == FLASH_SLEEP_CHARGER_WAKE_VALUE) ? 1U : 0U;
	g_dbg.sleep.block_reason = g_stLowPowerRtcStatus.block;
	g_dbg.sleep.idle_count = g_stLowPowerRtcStatus.idle;
	g_dbg.sleep.sleep_seconds = g_stLowPowerRtcStatus.sleep;
	g_dbg.sleep.last_sleep_seconds = g_stLowPowerRtcStatus.last;
	g_dbg.sleep.cycles = g_stLowPowerRtcStatus.cycles;
	g_dbg.sleep.rtc_sleep_count = sys_time.rtc_sleep_cnt;
	g_dbg.sleep.last_wakeup_source = (uint8_t)g_irq_t;
	g_dbg.sleep.charger_active = g_dbg.mcu.gpio.named.charger_connected;
	g_dbg.sleep.key_active = g_dbg.mcu.gpio.named.key_pressed;
	g_dbg.sleep.load_active = g_dbg.mcu.gpio.named.load_present;

	g_dbg.fault.cortex_fault_reason = g_dbg.mcu.bkp.fault_flag;
	g_dbg.fault.cortex_fault_reason_inv = g_dbg.mcu.bkp.fault_flag_inv;
	g_dbg.fault.cortex_fault_valid = g_dbg.mcu.bkp.fault_flag_valid;
#if PROJECT_CFG_IRQ_DEBUG_ENABLE
	g_dbg.fault.hardfault_count = (uint8_t)g_stIrqDebug.total[IRQDBG_HARDFAULT];
	g_dbg.fault.memfault_count = (uint8_t)g_stIrqDebug.total[IRQDBG_MEMMANAGE];
	g_dbg.fault.busfault_count = (uint8_t)g_stIrqDebug.total[IRQDBG_BUSFAULT];
	g_dbg.fault.usagefault_count = (uint8_t)g_stIrqDebug.total[IRQDBG_USAGEFAULT];
#else
	g_dbg.fault.hardfault_count = 0U;
	g_dbg.fault.memfault_count = 0U;
	g_dbg.fault.busfault_count = 0U;
	g_dbg.fault.usagefault_count = 0U;
#endif
	g_dbg.fault.scb_cfsr = g_dbg.mcu.core.scb_cfsr;
	g_dbg.fault.scb_hfsr = g_dbg.mcu.core.scb_hfsr;
	g_dbg.fault.scb_dfsr = g_dbg.mcu.core.scb_dfsr;
	g_dbg.fault.scb_afsr = g_dbg.mcu.core.scb_afsr;
	for (i = 0U; i < 12U; i++)
	{
		g_dbg.fault.system_error_flags[i] =
			(uint16_t)(((uint16_t)err[(uint8_t)(2U * i + 1U)] << 8) | err[(uint8_t)(2U * i)]);
	}
}

void DBG_CaptureMcuFast(void)
{
	dbg_capture_core();
	dbg_capture_systick();
	dbg_capture_usart(USART1, &g_dbg.mcu.usart1, g_dbg.mcu.clock.usart1_en, (uint8_t)IRQDBG_USART1);
	dbg_capture_usart(USART2, &g_dbg.mcu.usart2, g_dbg.mcu.clock.usart2_en, (uint8_t)IRQDBG_USART2);
	dbg_capture_adc();
	dbg_capture_can();
	dbg_capture_pwr_iwdg();
	g_dbg.ctrl.fast_capture_count++;
	g_dbg.ctrl.last_fast_tick_10ms = SysTime_Get10msTickCount();
}

void DBG_CaptureMcuSlow(void)
{
	DBG_CaptureRcc();
	DBG_CaptureClock();
	dbg_capture_reset();
	DBG_CaptureNvic();
	DBG_CaptureExti();
	DBG_CaptureGpio();
	dbg_capture_dma();
	dbg_capture_i2c1();
	DBG_CaptureRtcBkp();
	g_dbg.ctrl.slow_capture_count++;
	g_dbg.ctrl.last_slow_tick_10ms = SysTime_Get10msTickCount();
}

void DBG_CaptureMcuAll(void)
{
	DBG_CaptureMcuSlow();
	DBG_CaptureMcuFast();
	dbg_capture_business();
}

void DBG_ClearCounters(void)
{
	uint32_t primask;

	g_dbg.mcu.usart1.rx_count = 0U;
	g_dbg.mcu.usart1.tx_count = 0U;
	g_dbg.mcu.usart1.idle_count = 0U;
	g_dbg.mcu.usart1.ore_count = 0U;
	g_dbg.mcu.usart1.fe_count = 0U;
	g_dbg.mcu.usart1.ne_count = 0U;
	g_dbg.mcu.usart1.pe_count = 0U;
	g_dbg.mcu.usart2.rx_count = 0U;
	g_dbg.mcu.usart2.tx_count = 0U;
	g_dbg.mcu.usart2.idle_count = 0U;
	g_dbg.mcu.usart2.ore_count = 0U;
	g_dbg.mcu.usart2.fe_count = 0U;
	g_dbg.mcu.usart2.ne_count = 0U;
	g_dbg.mcu.usart2.pe_count = 0U;
	g_dbg.mcu.can.rx_count = 0U;
	g_dbg.mcu.can.tx_count = 0U;
	g_dbg.comm.can_rx_count = 0U;
	g_dbg.comm.can_tx_count = 0U;
	g_dbg.mcu.adc.sample_count = 0U;
	g_dbg.mcu.i2c1.ack_fail_count = 0U;
	g_dbg.mcu.i2c1.busy_timeout_count = 0U;
	g_dbg.mcu.i2c1.clock_stretch_timeout_count = 0U;
	g_dbg.mcu.i2c1.recovery_count = 0U;
	g_dbg.mcu.iwdg.feed_count = 0U;
	g_dbg.mcu.iwdg.last_feed_tick_10ms = 0U;
	g_dbg.mcu.iwdg.last_feed_gap_10ms = 0U;
	g_dbg.mcu.iwdg.max_feed_gap_10ms = 0U;
	g_dbg.mcu.iwdg.last_feed_source = 0U;

#if PROJECT_CFG_IRQ_DEBUG_ENABLE
	primask = __get_PRIMASK();
	__disable_irq();
	memset((void *)g_stIrqDebug.total, 0, sizeof(g_stIrqDebug.total));
	memset((void *)g_stIrqDebug.phase, 0, sizeof(g_stIrqDebug.phase));
	g_stIrqDebug.event_seq = 0U;
	g_stIrqDebug.event_head = 0U;
	g_stIrqDebug.event_count = 0U;
	if (primask == 0U)
	{
		__enable_irq();
	}
#else
	primask = 0U;
	(void)primask;
#endif

	g_dbg.ctrl.clear_count++;
}

void DBG_Init(void)
{
	uint32_t feed_count;
	uint32_t last_feed_tick;
	uint32_t max_feed_gap;
	uint8_t last_feed_source;

	feed_count = g_dbg.mcu.iwdg.feed_count;
	last_feed_tick = g_dbg.mcu.iwdg.last_feed_tick_10ms;
	max_feed_gap = g_dbg.mcu.iwdg.max_feed_gap_10ms;
	last_feed_source = g_dbg.mcu.iwdg.last_feed_source;

	memset((void *)&g_dbg, 0, sizeof(g_dbg));
	g_dbg.ctrl.magic = DBG_HUB_MAGIC;
	g_dbg.ctrl.version = DBG_HUB_VERSION;
	g_dbg.ctrl.size = (uint16_t)sizeof(g_dbg);
	g_dbg.ctrl.enable = 1U;
	g_dbg.ctrl.fast_period_10ms = 1U;
	g_dbg.ctrl.slow_period_10ms = 100U;
	g_dbg.ctrl.init_count = 1U;
	g_dbg.mcu.iwdg.feed_count = feed_count;
	g_dbg.mcu.iwdg.last_feed_tick_10ms = last_feed_tick;
	g_dbg.mcu.iwdg.max_feed_gap_10ms = max_feed_gap;
	g_dbg.mcu.iwdg.last_feed_source = last_feed_source;
	DBG_CaptureMcuAll();
}

void DBG_Task(void)
{
	uint32_t now_tick;
	uint8_t fast_period;
	uint8_t slow_period;

	g_dbg.ctrl.task_count++;
	now_tick = SysTime_Get10msTickCount();
	g_dbg.ctrl.last_task_tick_10ms = now_tick;

	if (g_dbg.ctrl.clear_counters != 0U)
	{
		g_dbg.ctrl.clear_counters = 0U;
		DBG_ClearCounters();
	}

	if (g_dbg.ctrl.capture_once != 0U)
	{
		g_dbg.ctrl.capture_once = 0U;
		DBG_CaptureMcuAll();
		g_dbg.ctrl.manual_capture_count++;
		return;
	}

	if ((g_dbg.ctrl.enable == 0U) || (g_dbg.ctrl.freeze != 0U))
	{
		return;
	}

	fast_period = (g_dbg.ctrl.fast_period_10ms == 0U) ? 1U : g_dbg.ctrl.fast_period_10ms;
	slow_period = (g_dbg.ctrl.slow_period_10ms == 0U) ? 100U : g_dbg.ctrl.slow_period_10ms;

	if ((uint32_t)(now_tick - g_dbg.ctrl.last_fast_tick_10ms) >= (uint32_t)fast_period)
	{
		DBG_CaptureMcuFast();
		dbg_capture_business();
	}
	if ((uint32_t)(now_tick - g_dbg.ctrl.last_slow_tick_10ms) >= (uint32_t)slow_period)
	{
		DBG_CaptureMcuSlow();
		dbg_capture_business();
	}
}
