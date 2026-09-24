#ifndef CURRENT_CALIBRATION_H
#define CURRENT_CALIBRATION_H

#include <stdint.h>

#define CURRENT_CAL_Q10_ONE       ((uint16_t)1024U)
#define CURRENT_CAL_MA_X4_SCALE   ((int32_t)4)

/*
 * Apply one linear current calibration to a signed mA*4 value.
 * K is Q10 (1024 = 1.0000), B is signed mA.
 * The correction is applied to magnitude; the original charge/discharge sign
 * is restored afterwards.
 */
static __inline int32_t CurrentCalibration_ApplySignedMilliAmpX4(int32_t nominal_mA_x4,
                                                        uint16_t k_q10,
                                                        int16_t b_mA)
{
	uint32_t magnitude;
	int64_t corrected;
	int32_t result;

	if (nominal_mA_x4 == 0)
	{
		return 0;
	}

	magnitude = (nominal_mA_x4 < 0) ?
		((uint32_t)(-(nominal_mA_x4 + 1)) + 1U) :
		(uint32_t)nominal_mA_x4;

	corrected = ((int64_t)magnitude * (int64_t)k_q10 +
		((int64_t)CURRENT_CAL_Q10_ONE / 2)) /
		(int64_t)CURRENT_CAL_Q10_ONE;
	corrected += (int64_t)b_mA * (int64_t)CURRENT_CAL_MA_X4_SCALE;

	if (corrected <= 0)
	{
		return 0;
	}
	if (corrected > 0x7FFFFFFFLL)
	{
		corrected = 0x7FFFFFFFLL;
	}

	result = (int32_t)corrected;
	return (nominal_mA_x4 > 0) ? result : -result;
}

/* Historical K/B protocol: bit15 is B sign, low15 bits are magnitude in mA. */
static __inline uint16_t CurrentCalibration_EncodeProtocolB(int16_t b_mA)
{
	uint16_t magnitude;

	if (b_mA < 0)
	{
		magnitude = (uint16_t)(-(int32_t)b_mA);
		return (uint16_t)(0x8000U | magnitude);
	}
	return (uint16_t)b_mA;
}

static __inline int16_t CurrentCalibration_DecodeProtocolB(uint16_t raw)
{
	int32_t magnitude = (int32_t)(raw & 0x7FFFU);

	return ((raw & 0x8000U) != 0U) ?
		(int16_t)(-magnitude) :
		(int16_t)magnitude;
}

#endif /* CURRENT_CALIBRATION_H */
