# MCU ADC temperature sampling

Hardware target: STM32F103, ADC1. Current board uses **PA1 / ADC1_IN1** for a
10k NTC on the MOS heatsink. This module deliberately avoids TIM2, DMA and ADC
interrupts: the existing TIM3-based main-loop scheduler samples every 100 ms.

## Behavior

- `InitADC()` configures the analog pins and ADC1 once (also on STOP wake),
  then performs bounded ADC calibration.
- `App_TempADC()` reads each configured channel with software triggering,
  averaging four conversions; each conversion has a bounded EOC wait.
- The existing NTC lookup table and the temperature K/B coefficients are
  retained. Reports use the legacy `(degrees_C + 40) * 10` representation.
- Temperature results are written independently of `App_AFEGet()`, so an AFE
  sampling skip does not also skip the MOS temperature update.
- An ADC timeout, calibration failure or out-of-range NTC sample publishes
  the last valid temperature unchanged, sets `ERROR_ADC`, and keeps MOS CTLC disabled through
  `TempADC_HasFault()` / `isforceClose()`; recovery clears `ERROR_ADC`.
  The AFE sensor's existing `ERROR_TEMP_BREAK` handling remains separate.
- STOP wake uses the same `InitADC()` function to reconfigure ADC1. RTC
  monitoring while still in STOP cycles does not require a fresh MOS ADC sample.

## Adding channels

Add one `s_sensors[]` entry in `Source/ADC.c` with GPIO port, pin,
`ADC_Channel_x`, an unused temperature report index, and its K/B calibration
index. Ensure the GPIO is not occupied by another peripheral. All channels
currently use the same 10k NTC table and are treated as required safety
sensors; adapt that policy if optional sensors are added. The existing
`DataLoad_TemperatureMaxMinFind()` includes AFE and environment slots but
excludes MOS, whose overtemperature protection is evaluated separately.

## Validation before release

1. Check the NTC resistor network and actual open/short ADC levels.
2. Test normal temperature points, channel switching, K/B calibration,
   intentional open/short faults, and sensor reconnect.
3. Verify both MOS overtemperature protection stages and immediate CTLC
   disable on invalid ADC readings, including during AFE/EEPROM traffic.
4. Repeat after STOP wake cycles and check the 100 ms sampling cadence.
5. Measure worst-case execution time on actual hardware; use a longer ADC
   sample time if the input source impedance requires it.
