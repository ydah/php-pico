#include "test.h"

#include "pphp/pphp.h"

#include <stdint.h>
#include <string.h>

static uint8_t compiler_off_pool[256U * 1024U];

TEST(source_api_reports_compiler_disabled) {
    static const char source[] = "echo 42;";
    pphp_state *state = pphp_open(compiler_off_pool,
                                  sizeof(compiler_off_pool));
    int result;
    ASSERT_TRUE(state != NULL);
    result = pphp_exec_source(state, source, sizeof(source) - 1U, "api-test");
    ASSERT_EQ(PPHP_E_PARSE, result);
    ASSERT_STR("source compiler is disabled; execute PBC instead",
               pphp_last_error(state));
    ASSERT_EQ(0U, pphp_last_error_line(state));
    pphp_close(state);
}

int main(void) {
    static const test_case tests[] = {
        {"compiler-off source ABI", source_api_reports_compiler_disabled}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
