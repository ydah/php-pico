#include "test.h"

#include "pphp/pphp.h"
#include "state.h"
#include "codegen.h"
#include "parser.h"
#include "pbc.h"
#include "vm.h"

#include <stdint.h>

typedef struct output_buffer {
    char bytes[4096];
    size_t length;
} output_buffer;

static uint8_t vm_pool[256U * 1024U];

static void capture_output(void *context, const char *bytes, size_t length) {
    output_buffer *output = context;
    if (length > sizeof(output->bytes) - output->length - 1U) {
        length = sizeof(output->bytes) - output->length - 1U;
    }
    memcpy(output->bytes + output->length, bytes, length);
    output->length += length;
    output->bytes[output->length] = '\0';
}

static int execute(const char *source, output_buffer *output, char *error,
                   size_t error_size) {
    pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
    int result;
    if (state == NULL) {
        return PPHP_E_NOMEM;
    }
    memset(output, 0, sizeof(*output));
    pphp_set_output(state, capture_output, output);
    result = pphp_exec_source_mode(state, source, strlen(source), "test", 1);
    if (error != NULL && error_size != 0U) {
        (void)snprintf(error, error_size, "%s", pphp_last_error(state));
    }
    pphp_close(state);
    return result;
}

TEST(arithmetic_runs_through_compiler_and_vm) {
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute("echo 1 + 2 * 3, \"\\n\";", &output, NULL, 0U));
    ASSERT_STR("7\n", output.bytes);
}

TEST(variables_conditionals_and_loops_execute) {
    const char *source =
        "$sum = 0;"
        "for ($i = 1; $i <= 10; $i++) {"
        "  if ($i === 8) continue;"
        "  $sum += $i;"
        "}"
        "echo $sum;";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("47", output.bytes);
}

TEST(user_functions_and_recursion_execute) {
    const char *source =
        "function fib($n) {"
        "  if ($n < 2) return $n;"
        "  return fib($n - 1) + fib($n - 2);"
        "}"
        "echo fib(10);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("55", output.bytes);
}

TEST(short_circuit_and_ternary_preserve_values) {
    const char *source =
        "$a = 0; false && ($a = 1); true || ($a = 2);"
        "echo $a, ',', (null ?? 4), ',', (0 ?: 5), ',', (2 ? 6 : 7);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("0,4,5,6", output.bytes);
}

TEST(strings_and_initial_builtins_execute) {
    const char *source =
        "$name = 'pico'; echo \"php-$name\", ':', strlen('abc');"
        "var_dump(true, 42);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("php-pico:3bool(true)\nint(42)\n", output.bytes);
}

TEST(runtime_errors_stop_execution_cleanly) {
    output_buffer output;
    char error[256];
    ASSERT_EQ(PPHP_E_RUNTIME, execute("echo 1 / 0; echo 'bad';", &output,
                                     error, sizeof(error)));
    ASSERT_STR("", output.bytes);
    ASSERT_TRUE(strstr(error, "Division by zero") != NULL);
}

TEST(argument_count_and_stack_limits_are_checked) {
    output_buffer output;
    char error[256];
    ASSERT_EQ(PPHP_E_RUNTIME,
              execute("function f($a) { return $a; } f();", &output,
                      error, sizeof(error)));
    ASSERT_TRUE(strstr(error, "Too few arguments") != NULL);
}

TEST(pbc_serialization_round_trips_through_loader) {
    const char *source =
        "function twice($x) { return $x * 2; } echo twice(21), ':' , 1.25;";
    const char *path = "build/host/test_roundtrip.pbc";
    pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
    pc_arena arena;
    pc_parser parser;
    pc_codegen_error error;
    pc_ast *program;
    pmodule original;
    pmodule loaded;
    output_buffer output;
    ASSERT_TRUE(state != NULL);
    pc_arena_init(&arena, 2048U);
    pc_parser_init(&parser, &arena, source, strlen(source), 1);
    program = pc_parse_program(&parser);
    ASSERT_TRUE(program != NULL);
    ASSERT_TRUE(pc_codegen_program(program, &original, &error));
    pc_arena_destroy(&arena);
    ASSERT_EQ(PPHP_OK, pphp_pbc_write_file(&original, path));
    pmodule_destroy(&original);
    ASSERT_EQ(PPHP_OK, pphp_pbc_read_file(path, &loaded));
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK, pphp_vm_execute(state, &loaded));
    ASSERT_STR("42:1.25", output.bytes);
    pmodule_destroy(&loaded);
    pphp_close(state);
    ASSERT_EQ(0, remove(path));
}

TEST(arrays_use_copy_on_write_and_normalized_keys) {
    const char *source =
        "$a = [1, 2]; $b = $a; $b[] = 3; $b['1'] = 20;"
        "echo count($a), ':', count($b), ':', $a[1], ':', $b[1], ':', array_sum($b);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("2:3:2:20:24", output.bytes);
}

TEST(foreach_preserves_insertion_order_and_supports_break_continue) {
    const char *source =
        "$a = ['x' => 1, 4 => 2, 3]; $sum = 0;"
        "foreach ($a as $k => $v) {"
        "  if ($v === 2) continue;"
        "  $sum += $v; echo $k, '=', $v, ';';"
        "}"
        "echo $sum;";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("x=1;5=3;4", output.bytes);
}

TEST(foreach_uses_snapshot_when_array_is_modified) {
    const char *source =
        "$a = [1, 2]; foreach ($a as $v) { echo $v; $a[] = 9; } echo ':', count($a);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("12:4", output.bytes);
}

int main(void) {
    static const test_case tests[] = {
        {"arithmetic VM", arithmetic_runs_through_compiler_and_vm},
        {"control-flow VM", variables_conditionals_and_loops_execute},
        {"functions and recursion", user_functions_and_recursion_execute},
        {"short circuit", short_circuit_and_ternary_preserve_values},
        {"strings and builtins", strings_and_initial_builtins_execute},
        {"runtime errors", runtime_errors_stop_execution_cleanly},
        {"argument validation", argument_count_and_stack_limits_are_checked},
        {"PBC round trip", pbc_serialization_round_trips_through_loader},
        {"array COW runtime", arrays_use_copy_on_write_and_normalized_keys},
        {"foreach runtime", foreach_preserves_insertion_order_and_supports_break_continue},
        {"foreach snapshot", foreach_uses_snapshot_when_array_is_modified}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
