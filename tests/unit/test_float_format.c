#include "float_format.h"
#include "test.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

static void assert_format(const char *expected, pphp_float value,
                          char conversion, int precision) {
    char buffer[PPHP_FLOAT_FORMAT_BUFFER_SIZE];
    int length = pphp_format_float(buffer, sizeof(buffer), value,
                                   conversion, precision);
    ASSERT_TRUE(length >= 0);
    ASSERT_EQ(strlen(expected), (size_t)length);
    ASSERT_STR(expected, buffer);
}

static pphp_float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return (pphp_float)value;
}

static int oracle_format(char *buffer, size_t capacity, pphp_float value,
                         char conversion, int precision) {
    if (conversion == 'f') {
        return snprintf(buffer, capacity, "%.*f", precision, (double)value);
    }
    if (conversion == 'e') {
        return snprintf(buffer, capacity, "%.*e", precision, (double)value);
    }
    return snprintf(buffer, capacity, "%.*g", precision, (double)value);
}

static int compare_with_oracle(uint32_t bits) {
    static const char conversions[] = {'f', 'e', 'g'};
    pphp_float value = float_from_bits(bits);
    char expected[PPHP_FLOAT_FORMAT_BUFFER_SIZE];
    char actual[PPHP_FLOAT_FORMAT_BUFFER_SIZE];
    size_t conversion;
    int precision;
    if (((bits >> 23U) & UINT32_C(0xff)) == UINT32_C(0xff)) return 1;
    for (conversion = 0U; conversion < sizeof(conversions); conversion++) {
        for (precision = 0; precision <= 9; precision++) {
            int expected_length = oracle_format(expected, sizeof(expected), value,
                                                conversions[conversion], precision);
            int actual_length = pphp_format_float(actual, sizeof(actual), value,
                                                  conversions[conversion], precision);
            if (expected_length != actual_length ||
                expected_length < 0 || strcmp(expected, actual) != 0) {
                fprintf(stderr,
                        "bits=%08x format=%%.%d%c expected=\"%s\" got=\"%s\"\n",
                        (unsigned)bits, precision, conversions[conversion],
                        expected, actual_length < 0 ? "<error>" : actual);
                return 0;
            }
        }
    }
    return 1;
}

TEST(special_values_and_signed_zero_do_not_need_printf) {
    assert_format("nan", (pphp_float)NAN, 'g', 9);
    assert_format("inf", (pphp_float)INFINITY, 'g', 9);
    assert_format("-inf", (pphp_float)-INFINITY, 'g', 9);
    assert_format("0", (pphp_float)0.0, 'g', 9);
    assert_format("-0", (pphp_float)-0.0, 'g', 9);
    assert_format("-0.000", (pphp_float)-0.0, 'f', 3);
}

TEST(fixed_format_rounds_and_honors_precision) {
    assert_format("1.25", (pphp_float)1.25, 'f', 2);
    assert_format("1.38", (pphp_float)1.375, 'f', 2);
    assert_format("0.62", (pphp_float)0.625, 'f', 2);
    assert_format("2", (pphp_float)2.5, 'f', 0);
    assert_format("4", (pphp_float)3.5, 'f', 0);
    assert_format("10.0", (pphp_float)9.99, 'f', 1);
    assert_format("0.001", (pphp_float)0.000625, 'f', 3);
}

TEST(exponential_format_uses_normalized_exponents) {
    assert_format("1.250000e+01", (pphp_float)12.5, 'e', -1);
    assert_format("-1.250e-02", (pphp_float)-0.0125, 'e', 3);
    assert_format("9.88e+00", (pphp_float)9.875, 'e', 2);
}

TEST(general_format_selects_fixed_or_exponential_and_rounds) {
    assert_format("12.5", (pphp_float)12.5, 'g', -1);
    assert_format("0.000125", (pphp_float)0.000125, 'g', 4);
    assert_format("1.25e-05", (pphp_float)0.0000125, 'g', 3);
    assert_format("1.2e+03", (pphp_float)1250.0, 'g', 2);
    assert_format("1e+03", (pphp_float)999.5, 'g', 2);
    assert_format("9.99999975e-05", (pphp_float)0.0001, 'g', 9);
    assert_format("3.40282347e+38", (pphp_float)FLT_MAX, 'g', 9);
    assert_format("1.17549435e-38", (pphp_float)FLT_MIN, 'g', 9);
}

TEST(regression_values_match_binary32_rounding) {
    assert_format("-2827637.2", float_from_bits(UINT32_C(0xca2c95d5)),
                  'g', 8);
    assert_format("-2.8276372e+06", float_from_bits(UINT32_C(0xca2c95d5)),
                  'e', 7);
    assert_format("247387287485393798868690028986368",
                  float_from_bits(UINT32_C(0x75432777)), 'f', 0);
    assert_format("113638.812", float_from_bits(UINT32_C(0x47ddf368)),
                  'g', 9);
    assert_format("1.40129846e-45", float_from_bits(UINT32_C(0x00000001)),
                  'g', 9);
    assert_format("1.17549421e-38", float_from_bits(UINT32_C(0x007fffff)),
                  'g', 9);
}

TEST(deterministic_binary32_sample_matches_libc) {
    static const uint32_t mantissas[] = {
        UINT32_C(0), UINT32_C(1), UINT32_C(2), UINT32_C(3),
        UINT32_C(0x0000ff), UINT32_C(0x000100), UINT32_C(0x00ffff),
        UINT32_C(0x010000), UINT32_C(0x155555), UINT32_C(0x2aaaaa),
        UINT32_C(0x3fffff), UINT32_C(0x400000), UINT32_C(0x555555),
        UINT32_C(0x7ffffd), UINT32_C(0x7ffffe), UINT32_C(0x7fffff),
    };
    uint32_t exponent;
    size_t mantissa;
    uint32_t random = UINT32_C(0x6d2b79f5);
    size_t i;
    for (exponent = 0U; exponent < 255U; exponent++) {
        for (mantissa = 0U;
             mantissa < sizeof(mantissas) / sizeof(mantissas[0]);
             mantissa++) {
            uint32_t bits = (exponent << 23U) | mantissas[mantissa];
            ASSERT_TRUE(compare_with_oracle(bits));
            ASSERT_TRUE(compare_with_oracle(bits | UINT32_C(0x80000000)));
        }
    }
    for (i = 0U; i < 12000U; i++) {
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        ASSERT_TRUE(compare_with_oracle(random));
    }
}

TEST(invalid_requests_fail_without_writing_past_capacity) {
    char buffer[4];
    ASSERT_EQ(-1, pphp_format_float(buffer, sizeof(buffer), (pphp_float)1.0,
                                    'x', 2));
    ASSERT_EQ(-1, pphp_format_float(buffer, sizeof(buffer), (pphp_float)1.0,
                                    'f', 65));
    ASSERT_EQ(-1, pphp_format_float(buffer, 2U, (pphp_float)12.5, 'g', 3));
}

int main(void) {
    const test_case tests[] = {
        {"special values and signed zero", special_values_and_signed_zero_do_not_need_printf},
        {"fixed format", fixed_format_rounds_and_honors_precision},
        {"exponential format", exponential_format_uses_normalized_exponents},
        {"general format", general_format_selects_fixed_or_exponential_and_rounds},
        {"binary32 regressions", regression_values_match_binary32_rounding},
        {"binary32 oracle sample", deterministic_binary32_sample_matches_libc},
        {"invalid requests", invalid_requests_fail_without_writing_past_capacity},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
