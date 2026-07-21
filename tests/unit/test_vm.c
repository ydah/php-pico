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
        "function twice($x) { return $x * 2; } $offset = 1;"
        "$calculate = fn($x) => twice($x) + $offset;"
        "try { echo $calculate(20), ':' , 1.25; throw new Exception('ok'); }"
        "catch (Exception $error) { echo ':', $error->getMessage(); }"
        "finally { echo '!'; }";
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
    ASSERT_STR("41:1.25:ok!", output.bytes);
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

TEST(classes_construct_objects_and_dispatch_methods) {
    const char *source =
        "class Counter {"
        " private int $value = 0;"
        " public function __construct($start) { $this->value = $start; }"
        " public function add($amount) {"
        "   $this->value = $this->value + $amount; return $this->value;"
        " }"
        "}"
        "$counter = new Counter(2); $alias = $counter;"
        "echo $counter->add(3), ':', $alias->add(4), ':', ($counter instanceof Counter);";
    output_buffer output;
    char error[256];
    int result = execute(source, &output, error, sizeof(error));
    if (result != PPHP_OK) fprintf(stderr, "class error: %s\n", error);
    ASSERT_EQ(PPHP_OK, result);
    ASSERT_STR("5:9:1", output.bytes);
}

TEST(class_inheritance_reuses_properties_and_methods) {
    const char *source =
        "class Base { public $value = 7; public function get() { return $this->value; } }"
        "class Child extends Base {}"
        "$child = new Child(); echo $child->get(), ':', ($child instanceof Base);";
    output_buffer output;
    char error[256];
    int result = execute(source, &output, error, sizeof(error));
    if (result != PPHP_OK) fprintf(stderr, "inheritance error: %s\n", error);
    ASSERT_EQ(PPHP_OK, result);
    ASSERT_STR("7:1", output.bytes);
}

TEST(class_visibility_and_readonly_are_enforced) {
    output_buffer output;
    char error[256];
    const char *private_source =
        "class Box { private $value = 1; } $box = new Box(); echo $box->value;";
    ASSERT_EQ(PPHP_E_RUNTIME,
              execute(private_source, &output, error, sizeof(error)));
    ASSERT_TRUE(strstr(error, "non-public property") != NULL);
    {
        const char *readonly_source =
            "class Id { public readonly int $value;"
            " public function set($v) { $this->value = $v; } }"
            "$id = new Id(); $id->set(1); $id->set(2);";
        ASSERT_EQ(PPHP_E_RUNTIME,
                  execute(readonly_source, &output, error, sizeof(error)));
        ASSERT_TRUE(strstr(error, "readonly property") != NULL);
    }
}

TEST(explicit_exceptions_match_hierarchy_and_union_catches) {
    const char *source =
        "try { throw new InvalidArgumentException('bad input', 73); }"
        "catch (TypeError|RuntimeException $error) {"
        " echo $error->getMessage(), ':', $error->getCode(), ':',"
        " $error->getFile(), ':', $error->getLine(), ':',"
        " (strlen($error->getTraceAsString()) > 0), ':',"
        " ($error instanceof Exception);"
        "}";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("bad input:73:test:1:1:1", output.bytes);
}

TEST(runtime_errors_are_converted_to_catchable_errors) {
    const char *source =
        "function one($value) { return $value; }"
        "try { echo 1 / 0; }"
        "catch (DivisionByZeroError $error) { echo $error->getMessage(); }"
        "try { one(); }"
        "catch (ArgumentCountError) { echo ':args'; }"
        "try { throw new Exception('bad', 'code'); }"
        "catch (TypeError) { echo ':type'; }";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("Division by zero:args:type", output.bytes);
}

TEST(out_of_memory_uses_the_preallocated_catchable_exception) {
    const char *source =
        "$items = [];"
        "try { for ($i = 0; $i < 20000; $i++) { $items[] = $i; } }"
        "catch (OutOfMemoryError $error) { echo $error->getMessage(); }";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("Out of memory", output.bytes);
}

TEST(finally_runs_for_normal_return_caught_and_rethrown_paths) {
    const char *source =
        "function returned() { try { return 7; } finally { echo 'R'; } }"
        "try { echo 'A'; } finally { echo 'F'; }"
        "try { throw new Exception('handled'); }"
        "catch (Exception $error) { echo ':C'; } finally { echo 'Z'; }"
        "echo ':', returned();";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("AF:CZR:7", output.bytes);
    {
        char error[256];
        const char *rethrow =
            "try { throw new Exception('old'); }"
            "catch (Exception $error) { echo 'C'; throw new RuntimeException('new'); }"
            "finally { echo 'F'; }";
        ASSERT_EQ(PPHP_E_RUNTIME,
                  execute(rethrow, &output, error, sizeof(error)));
        ASSERT_STR("CF", output.bytes);
        ASSERT_TRUE(strstr(error, "Uncaught RuntimeException: new") != NULL);
    }
}

TEST(nested_finally_preserves_the_original_pending_exception) {
    const char *source =
        "try { throw new Exception('outer'); }"
        "finally {"
        " try { throw new Exception('inner'); }"
        " catch (Exception $error) { echo $error->getMessage(), ':'; }"
        "}";
    output_buffer output;
    char error[256];
    ASSERT_EQ(PPHP_E_RUNTIME, execute(source, &output, error, sizeof(error)));
    ASSERT_STR("inner:", output.bytes);
    ASSERT_TRUE(strstr(error, "Uncaught Exception: outer") != NULL);
}

TEST(loop_transfers_run_only_the_finally_blocks_they_cross) {
    const char *source =
        "try { while (true) { echo 'B'; break; } echo 'A'; }"
        "finally { echo 'F'; }"
        "$i = 0; while ($i < 2) {"
        " try { $i++; if ($i === 1) continue; echo 'C'; }"
        " finally { echo 'G'; }"
        "}"
        "while (true) { try { echo 'D'; break; } finally { echo 'H'; } }";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("BAFGCGDH", output.bytes);
}

TEST(multilevel_continue_releases_inner_foreach_iterators) {
    const char *source =
        "foreach ([1, 2] as $outer) {"
        " foreach ([3] as $inner) { echo $outer; continue 2; }"
        " echo 'bad';"
        "} echo ':done';";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("12:done", output.bytes);
}

TEST(switch_uses_loose_cases_fallthrough_and_control_levels) {
    const char *source =
        "$value = '2';"
        "switch ($value) {"
        " case 1: echo 'A'; break;"
        " case 2: echo 'B';"
        " case 3: echo 'C'; break;"
        " default: echo 'bad';"
        "}"
        "switch (9) { default: echo 'D'; case 8: echo 'N'; break; }"
        "while (true) { switch (1) { case 1: echo 'X'; break 2; } echo 'bad'; }"
        "echo 'Y';";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("BCDNXY", output.bytes);
}

TEST(switch_transfers_interact_with_continue_and_finally) {
    const char *source =
        "$i = 0;"
        "while ($i < 2) {"
        " $i++; switch ($i) { case 1: continue 2; default: echo 'C'; }"
        "}"
        "try { switch (1) { case 1: break; } echo 'A'; } finally { echo 'F'; }"
        "switch (1) { case 1: try { break; } finally { echo 'G'; } }"
        "echo 'E';";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("CAFGE", output.bytes);
}

TEST(match_is_strict_and_evaluates_its_subject_once) {
    const char *source =
        "$count = 0;"
        "echo match ($count++) { 0 => 'Z', default => 'bad' }, ':', $count, ':';"
        "echo match ('1') { 1 => 'int', '1' => 'string', default => 'bad' }, ':';"
        "echo match (3) { 1, 2 => 'small', default => 'other' };";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("Z:1:string:other", output.bytes);
}

TEST(match_without_a_matching_arm_throws_unhandled_match_error) {
    const char *source =
        "try { echo match (9) { 1 => 'one', 2 => 'two' }; }"
        "catch (UnhandledMatchError $error) {"
        " echo 'caught:', ($error instanceof Error);"
        "}";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("caught:1", output.bytes);
}

TEST(closures_capture_values_and_arrow_functions_capture_automatically) {
    const char *source =
        "$offset = 10;"
        "$add = function ($value) use ($offset) { return $value + $offset; };"
        "$multiply = fn($value) => $value * $offset;"
        "$offset = 99;"
        "echo $add(5), ':', $multiply(2);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("15:20", output.bytes);
}

TEST(nested_arrows_retain_transitive_captures) {
    const char *source =
        "$base = 5;"
        "$factory = fn($left) => fn($right) => $base + $left + $right;"
        "$sum = $factory(2); $base = 100; echo $sum(3);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("10", output.bytes);
}

TEST(method_closures_bind_this_by_value) {
    const char *source =
        "class Box {"
        " public $value = 7;"
        " public function reader() { return fn() => $this->value; }"
        " public function classic() { return function () { return $this->value + 1; }; }"
        "}"
        "$box = new Box(); $reader = $box->reader(); $classic = $box->classic();"
        "echo $reader(), ':', $classic();";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("7:8", output.bytes);
}

TEST(call_value_supports_strings_method_arrays_and_invokable_objects) {
    const char *source =
        "function twice($value) { return $value * 2; }"
        "class Worker {"
        " public function run($value) { return $value + 1; }"
        " public function __invoke($value) { return $value + 2; }"
        "}"
        "$named = 'twice'; $builtin = 'strlen'; $worker = new Worker();"
        "$method = [$worker, 'run'];"
        "echo $named(4), ':', $builtin('pico'), ':',"
        " $method(5), ':', $worker(5);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("8:4:6:7", output.bytes);
}

TEST(static_closures_reject_this_binding) {
    const char *source =
        "class Invalid { public function make() { return static fn() => $this; } }";
    output_buffer output;
    char error[256];
    ASSERT_EQ(PPHP_E_PARSE, execute(source, &output, error, sizeof(error)));
    ASSERT_TRUE(strstr(error, "cannot use $this") != NULL);
}

TEST(clone_copies_property_slots_and_invokes_clone_hook) {
    const char *source =
        "class Copyable {"
        " public $value = 1;"
        " public function __clone() { $this->value = $this->value + 1; }"
        "}"
        "$original = new Copyable(); $copy = clone $original;"
        "echo $original->value, ':', $copy->value, ':',"
        " (($original === $copy) ? 1 : 0);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("1:2:0", output.bytes);
    ASSERT_EQ(PPHP_OK,
              execute("try { clone 1; } catch (Error) { echo 'caught'; }",
                      &output, NULL, 0U));
    ASSERT_STR("caught", output.bytes);
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
        {"foreach snapshot", foreach_uses_snapshot_when_array_is_modified},
        {"object methods", classes_construct_objects_and_dispatch_methods},
        {"class inheritance", class_inheritance_reuses_properties_and_methods},
        {"visibility and readonly", class_visibility_and_readonly_are_enforced},
        {"explicit exceptions", explicit_exceptions_match_hierarchy_and_union_catches},
        {"catchable runtime errors", runtime_errors_are_converted_to_catchable_errors},
        {"preallocated OOM exception", out_of_memory_uses_the_preallocated_catchable_exception},
        {"finally control flow", finally_runs_for_normal_return_caught_and_rethrown_paths},
        {"nested finally", nested_finally_preserves_the_original_pending_exception},
        {"finally loop transfers", loop_transfers_run_only_the_finally_blocks_they_cross},
        {"multilevel foreach continue", multilevel_continue_releases_inner_foreach_iterators},
        {"switch runtime", switch_uses_loose_cases_fallthrough_and_control_levels},
        {"switch transfers", switch_transfers_interact_with_continue_and_finally},
        {"match runtime", match_is_strict_and_evaluates_its_subject_once},
        {"match exhaustiveness", match_without_a_matching_arm_throws_unhandled_match_error},
        {"closure captures", closures_capture_values_and_arrow_functions_capture_automatically},
        {"nested arrow captures", nested_arrows_retain_transitive_captures},
        {"bound this closures", method_closures_bind_this_by_value},
        {"callable values", call_value_supports_strings_method_arrays_and_invokable_objects},
        {"static closure this", static_closures_reject_this_binding},
        {"object clone", clone_copies_property_slots_and_invokes_clone_hook}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
