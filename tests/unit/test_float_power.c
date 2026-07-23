#include "test.h"
#include "value_ops.h"

#include <math.h>
#include <stdint.h>

static float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t ordered_float_bits(float value) {
    uint32_t bits = float_bits(value);
    return (bits & UINT32_C(0x80000000)) != 0U
        ? ~bits : bits | UINT32_C(0x80000000);
}

static int power_matches_oracle(float base, float exponent) {
    float expected = powf(base, exponent);
    float actual = pphp_float_power(base, exponent);
    uint32_t expected_bits;
    uint32_t actual_bits;
    uint32_t expected_order;
    uint32_t actual_order;
    uint32_t distance;
    if (isnan(expected) || isnan(actual)) {
        return isnan(expected) && isnan(actual);
    }
    expected_bits = float_bits(expected);
    actual_bits = float_bits(actual);
    if (isinf(expected) || expected == 0.0f) {
        return expected_bits == actual_bits;
    }
    expected_order = ordered_float_bits(expected);
    actual_order = ordered_float_bits(actual);
    distance = expected_order > actual_order
        ? expected_order - actual_order : actual_order - expected_order;
    if (distance <= UINT32_C(4)) return 1;
    fprintf(stderr,
            "base=%08x exponent=%08x expected=%08x actual=%08x ulp=%u\n",
            (unsigned)float_bits(base), (unsigned)float_bits(exponent),
            (unsigned)expected_bits, (unsigned)actual_bits,
            (unsigned)distance);
    return 0;
}

TEST(exponent_parity_uses_the_mantissa_boundary_bit) {
    ASSERT_EQ(UINT32_C(0x80000000),
              float_bits(pphp_float_power(-0.0f, 8388609.0f)));
    ASSERT_EQ(UINT32_C(0xff800000),
              float_bits(pphp_float_power(-0.0f, -8388609.0f)));
    ASSERT_EQ(UINT32_C(0xff800000),
              float_bits(pphp_float_power(-INFINITY, 8388609.0f)));
    ASSERT_EQ(UINT32_C(0x80000000),
              float_bits(pphp_float_power(-INFINITY, -8388609.0f)));
    ASSERT_EQ(UINT32_C(0x00000000),
              float_bits(pphp_float_power(-0.0f, 16777216.0f)));
}

TEST(near_one_large_exponents_match_powf) {
    float below_one = 1.0f - 1.0f / 16777216.0f;
    ASSERT_TRUE(power_matches_oracle(below_one, 16777216.0f));
    ASSERT_TRUE(power_matches_oracle(below_one, -16777216.0f));
    ASSERT_TRUE(power_matches_oracle(1.0000001192092896f, 8388609.0f));
    ASSERT_TRUE(power_matches_oracle(-1.0000001192092896f, 8388609.0f));
}

TEST(random_binary32_pairs_are_within_four_ulp) {
    uint32_t base_bits = UINT32_C(0x243f6a88);
    uint32_t exponent_bits = UINT32_C(0x85a308d3);
    unsigned index;
    for (index = 0U; index < 100000U; index++) {
        base_bits = base_bits * UINT32_C(1664525) + UINT32_C(1013904223);
        exponent_bits =
            exponent_bits * UINT32_C(22695477) + UINT32_C(1);
        ASSERT_TRUE(power_matches_oracle(float_from_bits(base_bits),
                                         float_from_bits(exponent_bits)));
    }
}

int main(void) {
    const test_case tests[] = {
        {"exponent parity boundary",
         exponent_parity_uses_the_mantissa_boundary_bit},
        {"near one with large exponents",
         near_one_large_exponents_match_powf},
        {"100000 binary32 pairs", random_binary32_pairs_are_within_four_ulp},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
