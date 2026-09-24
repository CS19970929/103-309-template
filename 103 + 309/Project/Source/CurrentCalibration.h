#ifndef CURRENT_CALIBRATION_H
#define CURRENT_CALIBRATION_H

#include <stdint.h>

#define CURRENT_CAL_Q10_ONE       ((uint16_t)1024U)
#define CURRENT_CAL_MA_X4_SCALE   ((int32_t)4)
#define CURRENT_CAL_MA_PER_A      ((int32_t)1000)

/*
 * Historical BMS-upper current calibration semantics:
 *
 *   K_raw = K * 1024
 *   B_raw = B(A) * 1024
 *
 * g_i16CalibCoefB stores signed Q10(A), not mA.  The current path stays in
 * mA*4 internally, so the reference formula
 *
 *   I_cal_mA = (I_nominal_mA * K_raw + B_raw * 1000) / 1024
 *
 * becomes the expression below.  Calibration is applied to current magnitude,
 * then the original charge/discharge sign is restored.  No 2 A split is used.
 */
static __inline int32_t CurrentCalibration_ApplySignedMilliAmpX4(int32_t nominal_mA_x4,
                                                                 uint16_t k_q10,
                                                                 int16_t b_q10_a)
{
	uint32_t magnitude;
	int64_t numerator;
	int64_t corrected;
	int32_t result;

	if (nominal_mA_x4 == 0)
	{
		return 0;
	}

	magnitude = (nominal_mA_x4 < 0) ?
		((uint32_t)(-(nominal_mA_x4 + 1)) + 1U) :
		(uint32_t)nominal_mA_x4;

	numerator = (int64_t)magnitude * (int64_t)k_q10;
	numerator += (int64_t)b_q10_a *
	             (int64_t)CURRENT_CAL_MA_PER_A *
	             (int64_t)CURRENT_CAL_MA_X4_SCALE;

	if (numerator <= 0)
	{
		return 0;
	}

	/* Match the historical positive-magnitude Q10 truncation semantics. */
	corrected = numerator / (int64_t)CURRENT_CAL_Q10_ONE;
	if (corrected > 0x7FFFFFFFLL)
	{
		corrected = 0x7FFFFFFFLL;
	}

	result = (int32_t)corrected;
	return (nominal_mA_x4 > 0) ? result : -result;
}

/*
 * BMS-upper write protocol is sign-magnitude:
 *   positive B: 0x0000..0x7FFF
 *   negative B: bit15=1, low15=absolute Q10(A) magnitude.
 */
static __inline int16_t CurrentCalibration_DecodeWriteProtocolB(uint16_t raw)
{
	int32_t magnitude = (int32_t)(raw & 0x7FFFU);

	return ((raw & 0x8000U) != 0U) ?
		(int16_t)(-magnitude) :
		(int16_t)magnitude;
}

/*
 * BMS-upper read path interprets the returned word directly as Int16 and then
 * divides by 1024, so readback is ordinary int16 two's-complement.
 */
static __inline uint16_t CurrentCalibration_EncodeReadbackB(int16_t b_q10_a)
{
	return (uint16_t)b_q10_a;
}

#endif /* CURRENT_CALIBRATION_H */
