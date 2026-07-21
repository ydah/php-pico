#ifndef PPHP_TEST_H
#define PPHP_TEST_H

#include <stdio.h>
#include <string.h>

typedef void (*test_function)(void);

typedef struct test_case {
    const char *name;
    test_function function;
} test_case;

static int test_failures;

#define TEST(name) static void name(void)
#define ASSERT_TRUE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #value); \
        test_failures++; \
        return; \
    } \
} while (0)
#define ASSERT_EQ(expected, actual) do { \
    long long test_expected = (long long)(expected); \
    long long test_actual = (long long)(actual); \
    if (test_expected != test_actual) { \
        fprintf(stderr, "%s:%d: expected %lld, got %lld\n", __FILE__, __LINE__, test_expected, test_actual); \
        test_failures++; \
        return; \
    } \
} while (0)
#define ASSERT_STR(expected, actual) do { \
    if (strcmp((expected), (actual)) != 0) { \
        fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, (expected), (actual)); \
        test_failures++; \
        return; \
    } \
} while (0)

static int run_tests(const test_case *tests, size_t count) {
    size_t i;
    for (i = 0U; i < count; i++) {
        int before = test_failures;
        tests[i].function();
        fprintf(stderr, "%s %s\n", before == test_failures ? "ok" : "not ok", tests[i].name);
    }
    fprintf(stderr, "%zu tests, %d failures\n", count, test_failures);
    return test_failures == 0 ? 0 : 1;
}

#endif

