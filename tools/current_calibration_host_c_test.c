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

static void test_k_gain(void)
{
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(4000, 1100U, 0), 4297);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(-4000, 1100U, 0), -4297);
}

static void test_b_is_milliamp_offset(void)
{
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(4000, 1024U, 125), 4500);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(-4000, 1024U, 125), -4500);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(4000, 1024U, -125), 3500);
	CHECK_EQ_I32(CurrentCalibration_ApplySignedMilliAmpX4(100, 1024U, -100), 0);
}

static void test_no_two_amp_split(void)
{
	int32_t below = CurrentCalibration_ApplySignedMilliAmpX4(1999 * 4, 1100U, 25);
	int32_t above = CurrentCalibration_ApplySignedMilliAmpX4(2001 * 4, 1100U, 25);

	CHECK_EQ_I32(above - below, 9);
}

static void test_protocol_b_sign_magnitude(void)
{
	CHECK_EQ_I32(CurrentCalibration_EncodeProtocolB(5), 0x0005);
	CHECK_EQ_I32(CurrentCalibration_EncodeProtocolB(-5), 0x8005);
	CHECK_EQ_I32(CurrentCalibration_DecodeProtocolB(0x0005), 5);
	CHECK_EQ_I32(CurrentCalibration_DecodeProtocolB(0x8005), -5);
	CHECK_EQ_I32(CurrentCalibration_DecodeProtocolB(CurrentCalibration_EncodeProtocolB(500)), 500);
	CHECK_EQ_I32(CurrentCalibration_DecodeProtocolB(CurrentCalibration_EncodeProtocolB(-500)), -500);
}

int main(void)
{
	test_identity();
	test_k_gain();
	test_b_is_milliamp_offset();
	test_no_two_amp_split();
	test_protocol_b_sign_magnitude();

	if (s_failures != 0U)
	{
		printf("Current calibration host C tests failed: %u\n", s_failures);
		return 1;
	}
	printf("Current calibration host C tests passed\n");
	return 0;
}
