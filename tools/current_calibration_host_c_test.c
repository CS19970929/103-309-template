#include "CurrentCalibration.h"

#include <stdio.h>

static unsigned s_failures;

#define CHECK_EQ_I32(actual, expected) do { \
	int32_t a_ = (int32_t)(actual); \
	int32_t e_ = (int32_t)(expected); \
	if (a_ != e_) { \
		printf("FAIL line %d: actual=%ld expected=%ld\n", __LINE__, (long)a_, (long)e_); \
		++s_failures; \
	} \
} while (0)

static void test_identity(void)
{
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(4000, 1024U, 0), 4000);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(-4000, 1024U, 0), -4000);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(0, 1024U, 0), 0);
}

static void test_k_gain_matches_q10_reference(void)
{
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(4000, 1100U, 0), 4296);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(-4000, 1100U, 0), -4296);
}

static void test_b_is_q10_amp_like_bms_upper(void)
{
	/* BMS-upper UI B=+0.500 A sends +512. */
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(4000, 1024U, 512), 6000);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(-4000, 1024U, 512), -6000);

	/* BMS-upper UI B=-0.500 A sends sign-magnitude 0x8200 -> -512. */
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(4000, 1024U, -512), 2000);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(100, 1024U, -100), 0);
}

static void test_no_two_amp_split(void)
{
	int32_t below = CurrentCalibration_ApplySignedMilliAmpX4(1999 * 4, 1100U, 25);
	int32_t above = CurrentCalibration_ApplySignedMilliAmpX4(2001 * 4, 1100U, 25);

	CHECK_EQ_I32(above - below, 8);
}

static void test_bms_upper_write_protocol(void)
{
	CHECK_EQ_I32(CurrentCalibration_DecodeWriteProtocolB(0x0000U), 0);
	CHECK_EQ_I32(CurrentCalibration_DecodeWriteProtocolB(0x0200U), 512);
	CHECK_EQ_I32(CurrentCalibration_DecodeWriteProtocolB(0x8200U), -512);
	CHECK_EQ_I32(CurrentCalibration_DecodeWriteProtocolB(0x7530U), 30000);
	CHECK_EQ_I32(CurrentCalibration_DecodeWriteProtocolB(0xF530U), -30000);
}

static void test_bms_upper_readback_protocol(void)
{
	CHECK_EQ_I32(CurrentCalibration_EncodeReadbackB(512), 0x0200U);
	CHECK_EQ_I32(CurrentCalibration_EncodeReadbackB(-512), 0xFE00U);
	CHECK_EQ_I32((int16_t)CurrentCalibration_EncodeReadbackB(-512), -512);
}

int main(void)
{
	test_identity();
	test_k_gain_matches_q10_reference();
	test_b_is_q10_amp_like_bms_upper();
	test_no_two_amp_split();
	test_bms_upper_write_protocol();
	test_bms_upper_readback_protocol();

	if (s_failures != 0U)
	{
		printf("Current calibration host C tests failed: %u\n", s_failures);
		return 1;
	}
	printf("Current calibration host C tests passed\n");
	return 0;
}
