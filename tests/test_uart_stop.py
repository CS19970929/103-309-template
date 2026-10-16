"""Static regression checks for USART/STOP ordering and fault diagnostics.

Run: python -B tests/test_uart_stop.py
This checks source-level invariants only. It does not replace target compilation
or physical-board STOP/wake validation.
"""

from pathlib import Path
import re

from test_current_calibration import function_source


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"
IT_SOURCE = (
    ROOT
    / "103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c"
)


def read(path):
    return path.read_bytes().decode("latin1").replace("\r\n", "\n")


def ordered(body, *tokens):
    positions = [body.index(token) for token in tokens]
    assert positions == sorted(positions), (tokens, positions)


def main():
    sci = read(SOURCE / "Sci_Upper.c")
    conf = read(SOURCE / "conf/conf.c")
    rtc = read(SOURCE / "rtc_sleep.c")
    it = read(IT_SOURCE)

    prepare = function_source(sci, "Sci_CommonUpperPrepareStop")
    pending = function_source(sci, "Sci_CommonUpperHasPendingWork")
    assert "USART1->SR &" in pending and "USART2->SR &" in pending
    assert "USART_SR_RXNE" in pending and "USART_SR_FE" in pending
    assert "Sci_CommonUpperHasPendingWork()" in prepare
    for port, irqn in (("USART1", "USART1_IRQn"), ("USART2", "USART2_IRQn")):
        ordered(
            prepare,
            f"USART_ITConfig({port}, USART_IT_RXNE, DISABLE)",
            f"NVIC_DisableIRQ({irqn})",
            f"USART_Cmd({port}, DISABLE)",
            f"NVIC_ClearPendingIRQ({irqn})",
        )

    for port in (1, 2):
        irq = function_source(sci, f"Sci{port}_CommonUpper_IRQHandler")
        assert f"status = USART{port}->SR" in irq
        ordered(irq, f"status = USART{port}->SR", f"data = USART{port}->DR")
        assert "USART_SR_ORE" in irq and "USART_SR_FE" in irq
        assert "return 0U" in irq
        assert f"Sci{port}_CommonUpper_Rx_Deal" not in irq

    usart1_irq = function_source(it, "USART1_IRQHandler")
    usart2_irq = function_source(it, "USART2_IRQHandler")
    assert "Sci1_CommonUpper_IRQHandler" in usart1_irq
    assert "Sci2_CommonUpper_IRQHandler" in usart2_irq
    assert "RTC_ExtComCnt++" in usart1_irq and "RTC_ExtComCnt++" in usart2_irq
    assert "rtc_sleep_record_wakeup(uart1_irq)" in usart1_irq
    assert "rtc_sleep_record_wakeup(uart2_irq)" in usart2_irq

    assert not re.search(r"USART[123]->SR\s*&=", sci)

    sci1_init = function_source(sci, "InitSCI1_CommonUpper_rtc")
    sci2_init = function_source(sci, "InitSCI2_CommonUpper_rtc")
    assert "GPIO_Mode_IPU" in sci1_init and "GPIO_Mode_IPU" in sci2_init
    ordered(sci1_init, "USART_Cmd(USART1, ENABLE)", "NVIC_Init(&NVIC_InitStructure)")
    ordered(sci2_init, "USART_Cmd(USART2, ENABLE)", "NVIC_Init(&NVIC_InitStructure)")
    assert "NVIC_IRQChannelPreemptionPriority = 1" in sci1_init
    assert "NVIC_IRQChannelPreemptionPriority = 1" in sci2_init

    wake_base = function_source(conf, "InitWakeUp_Base")
    wake_normal = function_source(conf, "InitWakeUp_NormalMode")
    wake_rtc = function_source(conf, "InitWakeUp_RTCMode")
    assert "GPIO_Mode_IPD" in wake_base
    assert "GPIO_Mode_IPU" in wake_base
    assert "GPIO_Mode_IPD" in wake_normal
    assert "GPIO_PinSource7" in wake_normal
    assert "GPIO_PinSource3" in wake_normal
    assert wake_normal.count("EXTI_Trigger_Falling") >= 2
    assert "NVIC_ClearPendingIRQ(EXTI0_IRQn)" in wake_base
    assert "NVIC_ClearPendingIRQ(EXTI9_5_IRQn)" in wake_base
    assert "NVIC_ClearPendingIRQ(EXTI15_10_IRQn)" in wake_normal
    assert "EXTI_Line3 | EXTI_Line7" in wake_rtc
    assert "InitWakeUp_Base()" in wake_rtc
    assert "InitWakeUp_NormalMode()" not in wake_rtc
    assert "GPIO_PinSource3" not in wake_rtc
    assert "GPIO_PinSource7" not in wake_rtc
    assert "__delay_ms(RTC_WAKE_IO_SETTLE_MS)" in wake_rtc
    assert wake_rtc.count("EXTI_Mode = EXTI_Mode_Interrupt") >= 2
    assert "NVIC_ClearPendingIRQ(EXTI9_5_IRQn)" not in wake_rtc
    ordered(
        wake_rtc,
        "__delay_ms(RTC_WAKE_IO_SETTLE_MS)",
        "EXTI_ClearITPendingBit(EXTI_Line12)",
        "NVIC_Init(&NVIC_InitStructure)",
    )

    sleep_run = function_source(rtc, "sleep_run_rtc")
    active_wake = function_source(rtc, "sleep_has_active_wakeup")
    assert "rtc_sleep_record_wakeup(PA0_irq)" in active_wake
    assert "rtc_sleep_record_wakeup(rs485_irq)" in active_wake
    assert "rtc_sleep_record_wakeup(bms_keyirq)" in active_wake
    ordered(
        sleep_run,
        "__disable_irq()",
        "Sci_CommonUpperPrepareStop()",
        "IOstatus_RTCMode()",
        "InitWakeUp_RTCMode()",
        "Sys_StopMode()",
    )
    assert sleep_run.index("Sys_StopMode()") < sleep_run.rindex("__set_PRIMASK(primask)")
    assert sleep_run.index("Sys_StopMode()") < sleep_run.index("RTC_LatchAlarmWake()")
    assert sleep_run.index("RTC_LatchAlarmWake()") < sleep_run.rindex("__set_PRIMASK(primask)")
    assert "if (!is_rtc_wakekup)" in sleep_run
    assert "continue;" in sleep_run
    assert "GPIO_ReadInputDataBit(GPIO_INT_WK_CMNT, PIN_INT_WK_CMNT)" in active_wake
    assert "GPIO_ReadInputDataBit(GPIO_SCI1_RX, PIN_SCI1_RX)" not in sleep_run
    assert "GPIO_ReadInputDataBit(GPIO_SCI2_RX, PIN_SCI2_RX)" not in sleep_run
    assert "sleep_restore_run(stop_io_prepared)" in sleep_run

    exti3_irq = function_source(it, "EXTI3_IRQHandler")
    exti9_irq = function_source(it, "EXTI9_5_IRQHandler")
    assert "rtc_sleep_record_wakeup(uart2_irq)" in exti3_irq
    assert "rtc_sleep_record_wakeup(uart1_irq)" in exti9_irq

    hardfault = function_source(it, "HardFault_Record")
    assert "0x48465231UL" in hardfault
    for register in ("CFSR", "HFSR", "DFSR", "MMFAR", "BFAR", "AFSR"):
        assert f"SCB->{register}" in hardfault
    assert "g_hardfault_record.pc" in hardfault
    assert "g_hardfault_record.xpsr" in hardfault

    print("PASS: USART/STOP sequencing, wake bias, IRQ filtering and HardFault capture")


if __name__ == "__main__":
    main()
