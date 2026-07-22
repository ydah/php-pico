#include "float_format.h"
#include "test.h"

#include <float.h>
#include <math.h>

static void assert_format(const char *expected, pphp_float value,
                          char conversion, int precision) {
    char buffer[PPHP_FLOAT_FORMAT_BUFFER_SIZE];
    int length = pphp_format_float(buffer, sizeof(buffer), value,
                                   conversion, precision);
    ASSERT_TRUE(length >= 0);
    ASSERT_EQ(strlen(expected), (size_t)length);
    ASSERT_STR(expected, buffer);
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
    assert_format("1.250000e+1", (pphp_float)12.5, 'e', -1);
    assert_format("-1.250e-2", (pphp_float)-0.0125, 'e', 3);
    assert_format("9.88e+0", (pphp_float)9.875, 'e', 2);
}

TEST(general_format_selects_fixed_or_exponential_and_rounds) {
    assert_format("12.5", (pphp_float)12.5, 'g', -1);
    assert_format("0.000125", (pphp_float)0.000125, 'g', 4);
    assert_format("1.25e-5", (pphp_float)0.0000125, 'g', 3);
    assert_format("1.2e+3", (pphp_float)1250.0, 'g', 2);
    assert_format("1e+3", (pphp_float)999.5, 'g', 2);
    assert_format("9.99999975e-5", (pphp_float)0.0001, 'g', 9);
    assert_format("3.40282347e+38", (pphp_float)FLT_MAX, 'g', 9);
    assert_format("1.17549435e-38", (pphp_float)FLT_MIN, 'g', 9);
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
        {"invalid requests", invalid_requests_fail_without_writing_past_capacity},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
