#include "test.h"
#include "value_ops.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

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

static void assert_decimal_bits(uint32_t expected, const char *text) {
    ASSERT_EQ(expected, float_bits(pphp_decimal_to_float(text, strlen(text))));
}

TEST(decimal_boundaries_round_to_nearest_even) {
    assert_decimal_bits(UINT32_C(0x0a4fb11f), "1e-32");
    assert_decimal_bits(UINT32_C(0x7f7fffff), "3.4028235e38");
    assert_decimal_bits(
        UINT32_C(0x7f7fffff),
        "3.40282356779733661637539395458142568447e38");
    assert_decimal_bits(
        UINT32_C(0x7f800000),
        "3.40282356779733661637539395458142568448e38");
    assert_decimal_bits(
        UINT32_C(0x00000000),
        "7.0064923216240853546186479164495806564013097093825788587853414"
        "1944895541342930300743319094181060791015625e-46");
    assert_decimal_bits(
        UINT32_C(0x00800000),
        "1.1754942807573642917278829910357665133228589927589904276829631"
        "184250030649651730385585324256680905818939208984375e-38");
    assert_decimal_bits(UINT32_C(0x00000001), "1e-45");
    assert_decimal_bits(UINT32_C(0x00000000), "1e-46");
    assert_decimal_bits(UINT32_C(0x0c01ceb3), "1_0e-3_2");
}

TEST(long_decimal_positions_and_exponents_do_not_saturate_early) {
    const size_t zero_count = 20000U;
    char *text = malloc(zero_count + 32U);
    ASSERT_TRUE(text != NULL);

    text[0] = '1';
    text[1] = '.';
    memset(text + 2, '0', zero_count);
    text[zero_count + 2U] = '\0';
    assert_decimal_bits(UINT32_C(0x3f800000), text);

    text[zero_count + 2U] = '1';
    text[zero_count + 3U] = '\0';
    assert_decimal_bits(UINT32_C(0x3f800000), text);

    text[0] = '0';
    memcpy(text + zero_count + 3U, "e15000", 7U);
    assert_decimal_bits(UINT32_C(0x00000000), text);

    memcpy(text + zero_count + 3U, "e19999", 7U);
    assert_decimal_bits(UINT32_C(0x3c23d70a), text);

    memcpy(text + zero_count + 3U, "e20001", 7U);
    assert_decimal_bits(UINT32_C(0x3f800000), text);

    text[0] = '1';
    memset(text + 1, '0', zero_count);
    memcpy(text + zero_count + 1U, "e-20000", 8U);
    assert_decimal_bits(UINT32_C(0x3f800000), text);

    assert_decimal_bits(UINT32_C(0x7f800000), "1e2147483648");
    assert_decimal_bits(UINT32_C(0x00000000), "1e-2147483648");
    free(text);
}

TEST(random_nine_digit_decimals_match_strtof_bits) {
    uint32_t random = UINT32_C(0x6a09e667);
    unsigned index;
    for (index = 0U; index < 20000U; index++) {
        char text[32];
        unsigned first;
        unsigned fraction;
        int exponent;
        float expected;
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        first = random % 9U + 1U;
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        fraction = random % UINT32_C(100000000);
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        exponent = (int)(random % 106U) - 55;
        (void)snprintf(text, sizeof(text), "%u.%08ue%d",
                       first, fraction, exponent);
        expected = strtof(text, NULL);
        if (float_bits(expected) !=
            float_bits(pphp_decimal_to_float(text, strlen(text)))) {
            fprintf(stderr, "decimal=%s expected=%08x actual=%08x\n", text,
                    (unsigned)float_bits(expected),
                    (unsigned)float_bits(
                        pphp_decimal_to_float(text, strlen(text))));
            ASSERT_TRUE(0);
        }
    }
}

int main(void) {
    const test_case tests[] = {
        {"exponent parity boundary",
         exponent_parity_uses_the_mantissa_boundary_bit},
        {"near one with large exponents",
         near_one_large_exponents_match_powf},
        {"100000 binary32 pairs", random_binary32_pairs_are_within_four_ulp},
        {"decimal RNE boundaries", decimal_boundaries_round_to_nearest_even},
        {"long decimal positions",
         long_decimal_positions_and_exponents_do_not_saturate_early},
        {"20000 decimal oracle cases",
         random_nine_digit_decimals_match_strtof_bits},
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
