#include "test.h"

#include "pphp/pphp.h"
#include "pphp/hal.h"
#include "state.h"
#include "codegen.h"
#include "parser.h"
#include "pbc.h"
#include "opcode.h"
#include "vm.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct output_buffer {
    char bytes[4096];
    size_t length;
} output_buffer;

static uint8_t vm_pool[256U * 1024U];
#if PPHP_TYPECHECK
static uint8_t pbc_oom_pool[64U * 1024U];
#endif
static pclass *native_test_class;
static int native_finalized;

static int native_add(pphp_ctx *context) {
    pphp_ret_int(context, pphp_arg_int(context, 0) +
                              pphp_arg_int(context, 1));
    return 0;
}

static void native_box_finalizer(void *data) {
    (void)data;
    native_finalized++;
}

static int native_make(pphp_ctx *context) {
    pobject *object = pphp_obj_new_with(context, native_test_class,
                                        sizeof(pphp_int),
                                        native_box_finalizer);
    pphp_int *value;
    if (object == NULL) return -1;
    value = pphp_obj_data(object);
    *value = 40;
    pphp_ret_object(context, object);
    return 0;
}

static int native_increment(pphp_ctx *context) {
    pphp_int *value = pphp_obj_data(pphp_this(context));
    if (value == NULL) return pphp_raise(context, "RuntimeException",
                                         "native data is unavailable");
    *value += pphp_arg_int(context, 0);
    pphp_ret_int(context, *value);
    return 0;
}

static int native_label(pphp_ctx *context) {
    pphp_ret_strn(context, "native", 6U);
    return 0;
}

static int native_fail(pphp_ctx *context) {
    return pphp_raise(context, "RuntimeException", "native failure");
}

static void test_put_u16(uint8_t *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (uint8_t)(value & 0xffU);
    bytes[offset + 1U] = (uint8_t)(value >> 8U);
}

static void test_put_u32(uint8_t *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)(value & 0xffU);
    bytes[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
    bytes[offset + 2U] = (uint8_t)((value >> 16U) & 0xffU);
    bytes[offset + 3U] = (uint8_t)(value >> 24U);
}

#if PPHP_TYPECHECK
static uint16_t test_get_u16(const uint8_t *bytes, size_t offset) {
    return (uint16_t)((uint16_t)bytes[offset] |
                      (uint16_t)((uint16_t)bytes[offset + 1U] << 8U));
}

static uint32_t test_get_u32(const uint8_t *bytes, size_t offset) {
    uint32_t value = bytes[offset];
    value |= (uint32_t)bytes[offset + 1U] << 8U;
    value |= (uint32_t)bytes[offset + 2U] << 16U;
    value |= (uint32_t)bytes[offset + 3U] << 24U;
    return value;
}

static size_t test_align4(size_t value) {
    return (value + 3U) & ~(size_t)3U;
}

static size_t test_proto_type_offset(const uint8_t *bytes,
                                     size_t proto_offset) {
    uint16_t code_length = test_get_u16(bytes, proto_offset + 8U);
    uint16_t constant_count = test_get_u16(bytes, proto_offset + 10U);
    uint16_t catch_count = test_get_u16(bytes, proto_offset + 12U);
    uint8_t local_count = bytes[proto_offset + 3U];
    size_t offset = proto_offset + 16U + test_align4(code_length);
    size_t i;
    for (i = 0U; i < constant_count; i++) {
        uint8_t tag = bytes[offset];
        offset += tag == 3U || tag == 4U ? 16U : 8U;
    }
    return offset + (size_t)local_count * 2U + (size_t)catch_count * 10U;
}

static size_t test_instruction_size(const uint8_t *code, size_t length,
                                    size_t pc) {
    uint8_t opcode = code[pc];
    size_t operands;
    switch ((pphp_opcode)opcode) {
        case OP_LOAD_I8: case OP_LOAD_LOCAL: case OP_LOAD_LOCAL_QUIET:
        case OP_STORE_LOCAL: case OP_UNSET_LOCAL: case OP_BIND_GLOBAL:
        case OP_ECHO: case OP_CALL_VALUE: case OP_INCLUDE: case OP_CAST:
        case OP_NEW_OBJ_DYNAMIC: case OP_MCALL_DYNAMIC:
            operands = 1U; break;
        case OP_LOAD_CONST: case OP_LOAD_NAMED_CONST: case OP_DEF_CONST:
        case OP_DEF_FUNC: case OP_CALL_ARRAY: case OP_MCALL_ARRAY:
        case OP_NEW_OBJ_ARRAY: case OP_DEF_CCONST: case OP_DEF_INTERFACE:
        case OP_JMP: case OP_JMP_IF: case OP_JMP_UNLESS:
        case OP_JMP_IF_KEEP: case OP_JMP_UNLESS_KEEP:
        case OP_JMP_NOTNULL_KEEP: case OP_JMP_IFNULL_KEEP: case OP_LINE:
        case OP_NEW_ARRAY: case OP_PROP_GET: case OP_PROP_GET_QUIET:
        case OP_PROP_SET: case OP_INSTANCEOF:
            operands = 2U; break;
        case OP_CALL: case OP_NEW_OBJ: case OP_MCALL: case OP_FE_NEXT:
        case OP_STATIC_INIT:
            operands = 3U; break;
        case OP_SPROP_GET: case OP_SPROP_SET: case OP_CLSCONST:
        case OP_SCALL_ARRAY: case OP_LOAD_I32:
            operands = 4U; break;
        case OP_SCALL: case OP_DEF_CLASS: case OP_DEF_METHOD:
            operands = 5U; break;
        case OP_DEF_PROP:
            operands = 6U; break;
        case OP_CLOSURE:
            if (pc + 4U > length) return 0U;
            operands = 3U + (size_t)code[pc + 3U] * 2U; break;
        default: operands = 0U; break;
    }
    return pc + operands < length ? operands + 1U : 0U;
}

static size_t test_find_opcode(const uint8_t *bytes, size_t proto_offset,
                               uint8_t opcode, size_t occurrence) {
    size_t code = proto_offset + 16U;
    size_t length = test_get_u16(bytes, proto_offset + 8U);
    size_t pc = 0U;
    while (pc < length) {
        size_t size = test_instruction_size(bytes + code, length, pc);
        if (size == 0U) return SIZE_MAX;
        if (bytes[code + pc] == opcode && occurrence-- == 0U) return code + pc;
        pc += size;
    }
    return SIZE_MAX;
}

static size_t test_constant_offset(const uint8_t *bytes, size_t proto_offset,
                                   uint16_t index) {
    size_t offset = proto_offset + 16U +
                    test_align4(test_get_u16(bytes, proto_offset + 8U));
    uint16_t i;
    for (i = 0U; i < index; i++) {
        uint8_t tag = bytes[offset];
        offset += tag == 3U || tag == 4U ? 16U : 8U;
    }
    return offset;
}
#endif

static void minimal_pbc(uint8_t *bytes, size_t length) {
    uint16_t flags = (uint16_t)(
        (PPHP_INT64 ? PPHP_PBC_FLAG_INT64 : 0U) |
        (PPHP_USE_DOUBLE ? PPHP_PBC_FLAG_DOUBLE : 0U) |
        (PPHP_LINE_INFO ? PPHP_PBC_FLAG_LINE_INFO : 0U) |
        (PPHP_TYPECHECK ? PPHP_PBC_FLAG_TYPECHECK : 0U) |
        (PPHP_ENABLE_FLOAT ? PPHP_PBC_FLAG_FLOAT : 0U) |
        PPHP_PBC_FLAG_FEATURES);
    memset(bytes, 0, 128U);
    memcpy(bytes, "PPBC", 4U);
    test_put_u16(bytes, 4U, (uint16_t)PPHP_PBC_FORMAT_VERSION);
    test_put_u16(bytes, 6U, flags);
    test_put_u32(bytes, 8U, (uint32_t)length);
    test_put_u16(bytes, 12U, 1U);
    test_put_u16(bytes, 14U, 1U);
    test_put_u32(bytes, 16U, 24U);
    test_put_u32(bytes, 20U, 28U);
    test_put_u16(bytes, 24U, 0U);
    test_put_u16(bytes, 42U, 0U);
#if PPHP_TYPECHECK
    if (length >= 48U) bytes[44U] = 0U;
#endif
}

static int load_test_pbc(uint8_t *bytes, size_t length) {
    pmodule module;
    int result = pphp_pbc_load(bytes, length, &module);
    if (result == PPHP_OK) pmodule_destroy(&module);
    return result;
}

static int write_source_pbc(const char *source, const char *path) {
    pc_arena arena;
    pc_parser parser;
    pc_codegen_error error;
    pc_ast *program;
    pmodule module;
    int result;
    pc_arena_init(&arena, 2048U);
    pc_parser_init(&parser, &arena, source, strlen(source), 1);
    program = pc_parse_program(&parser);
    if (program == NULL || !pc_codegen_program(program, &module, &error)) {
        pc_arena_destroy(&arena);
        return 0;
    }
    pc_arena_destroy(&arena);
    result = pphp_pbc_write_file(&module, path) == PPHP_OK;
    pmodule_destroy(&module);
    return result;
}

static uint8_t *read_pbc_from_pool(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    long file_size;
    uint8_t *bytes;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 ||
        (file_size = ftell(file)) < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        if (file != NULL) (void)fclose(file);
        return NULL;
    }
    bytes = pphp_alloc((size_t)file_size);
    if (bytes == NULL || fread(bytes, 1U, (size_t)file_size, file) !=
                             (size_t)file_size) {
        pphp_free(bytes);
        (void)fclose(file);
        return NULL;
    }
    (void)fclose(file);
    *length = (size_t)file_size;
    return bytes;
}

static uint8_t *read_pbc_from_libc(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    long file_size;
    uint8_t *bytes;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 ||
        (file_size = ftell(file)) < 0L || fseek(file, 0L, SEEK_SET) != 0) {
        if (file != NULL) (void)fclose(file);
        return NULL;
    }
    bytes = malloc((size_t)file_size);
    if (bytes == NULL || fread(bytes, 1U, (size_t)file_size, file) !=
                             (size_t)file_size) {
        free(bytes);
        (void)fclose(file);
        return NULL;
    }
    (void)fclose(file);
    *length = (size_t)file_size;
    return bytes;
}

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
        "function twice($x = 20, ...$unused) { return $x * 2 + count($unused); }"
        "$offset = 1; $calculate = fn($x = 20) => twice(...[$x]) + $offset;"
        "try { echo $calculate(...[]), ':' , 1.25; throw new Exception('ok'); }"
        "catch (Exception $error) { echo ':', $error->getMessage(); }"
        "finally { echo '!'; }"
        "if (true) { function loadedConditionally() { return 5; } }"
        "echo ':', loadedConditionally();";
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
    {
        FILE *file = fopen(path, "r+b");
        int flags = (PPHP_INT64 ? 1 : 0) |
                    (PPHP_USE_DOUBLE ? 2 : 0) |
                    (PPHP_LINE_INFO ? 4 : 0) |
                    (PPHP_TYPECHECK ? 8 : 0);
        ASSERT_TRUE(file != NULL);
        ASSERT_EQ(0, fseek(file, 6L, SEEK_SET));
        ASSERT_EQ(flags ^ 1, fputc(flags ^ 1, file));
        ASSERT_EQ(0, fclose(file));
        ASSERT_EQ(PPHP_E_PARSE, pphp_pbc_read_file(path, &loaded));
        ASSERT_EQ(PPHP_OK, pphp_pbc_write_file(&original, path));
    }
    pmodule_destroy(&original);
    ASSERT_EQ(PPHP_OK, pphp_pbc_read_file(path, &loaded));
    ASSERT_TRUE(loaded.owns_backing != 0U);
    ASSERT_TRUE(loaded.protos[0]->owns_code == 0U);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK, pphp_vm_execute(state, &loaded));
    ASSERT_STR("41:1.25:ok!:5", output.bytes);
    pmodule_destroy(&loaded);
    pphp_close(state);
    ASSERT_EQ(0, remove(path));
}

TEST(pbc_string_constants_are_zero_copy_image_views) {
    const char *path = "build/host/test_xip_strings.pbc";
    const size_t literal_length = 12000U;
    const char prefix[] = "$x = '";
    const char suffix[] = "'; echo strlen($x);";
    char *source = malloc(sizeof(prefix) - 1U + literal_length +
                          sizeof(suffix));
    uint8_t *bytes;
    size_t length;
    pmodule module;
    pphp_pool_stats before;
    pphp_pool_stats after;
    const pstring *literal = NULL;
    size_t i;
    ASSERT_EQ(12, sizeof(pstring));
    ASSERT_TRUE(source != NULL);
    memcpy(source, prefix, sizeof(prefix) - 1U);
    memset(source + sizeof(prefix) - 1U, 'x', literal_length);
    memcpy(source + sizeof(prefix) - 1U + literal_length,
           suffix, sizeof(suffix));
    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(source, path));
    bytes = read_pbc_from_pool(path, &length);
    ASSERT_TRUE(bytes != NULL);
    before = pphp_pool_get_stats();
    ASSERT_EQ(PPHP_OK, pphp_pbc_load(bytes, length, &module));
    after = pphp_pool_get_stats();
    ASSERT_TRUE(module.image == bytes);
    ASSERT_EQ(length, module.image_length);
    ASSERT_TRUE(module.ro_string_count != 0U);
    for (i = 0U; i < module.ro_string_count; i++) {
        const pstring *string = &module.ro_strings[i].base;
        ASSERT_EQ(PT_ROSTRING, string->header.type);
        ASSERT_TRUE((const uint8_t *)ps_data(string) >= bytes);
        ASSERT_TRUE((const uint8_t *)ps_data(string) + string->length <
                    bytes + length);
        ASSERT_EQ(0, ps_data(string)[string->length]);
        if (string->length == literal_length) literal = string;
    }
    ASSERT_TRUE(literal != NULL);
    ASSERT_TRUE(after.used - before.used < literal_length / 4U);
    {
        pvalue view = pv_heap(PT_STRING, (pheader *)&literal->header);
        uint16_t refcnt = literal->header.refcnt;
        pv_retain(view);
        pv_release(view);
        ASSERT_EQ(refcnt, literal->header.refcnt);
    }
    pmodule_destroy(&module);
    pphp_free(bytes);
    free(source);
    ASSERT_EQ(0, remove(path));
}

TEST(pbc_string_records_are_binary_safe_and_nul_terminated) {
    uint8_t bytes[128];
    pmodule module;
#if PPHP_TYPECHECK
    minimal_pbc(bytes, 52U);
#else
    minimal_pbc(bytes, 48U);
#endif
    test_put_u32(bytes, 20U, 32U);
    test_put_u16(bytes, 24U, 3U);
    bytes[26U] = 'a';
    bytes[27U] = 0U;
    bytes[28U] = 'b';
    bytes[29U] = 0U;
#if PPHP_TYPECHECK
    bytes[48U] = 0U;
    ASSERT_EQ(PPHP_OK, pphp_pbc_load(bytes, 52U, &module));
#else
    ASSERT_EQ(PPHP_OK, pphp_pbc_load(bytes, 48U, &module));
#endif
    ASSERT_EQ(1, module.ro_string_count);
    ASSERT_EQ(3, module.ro_strings[0].base.length);
    ASSERT_EQ(0, module.ro_strings[0].data[1]);
    ASSERT_EQ('b', module.ro_strings[0].data[2]);
    pmodule_destroy(&module);

    bytes[29U] = 1U;
#if PPHP_TYPECHECK
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 52U));
#else
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 48U));
#endif
}

TEST(pbc_strings_work_across_runtime_consumers) {
    const char *path = "build/host/test_pbc_string_consumers.pbc";
    const char *source =
        "$empty = ''; $nul = \"a\\x00b\";"
        "echo strlen($empty), ':', strlen($nul), ':',"
        " ($nul === \"a\\x00b\" ? 1 : 0), ':',"
        " json_encode(['x' => $nul]), ':',"
        " sprintf('%04d-%s', 7, 'ok'), ':', ('a' <=> 'b');";
    pphp_state *state;
    output_buffer output;
    uint8_t *bytes;
    size_t length;

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(source, path));
    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    bytes = read_pbc_from_pool(path, &length);
    ASSERT_TRUE(bytes != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK, pphp_exec_pbc_owned(state, bytes, length));
    ASSERT_STR("0:3:1:{\"x\":\"a\\u0000b\"}:0007-ok:-1", output.bytes);
    pphp_close(state);
    ASSERT_EQ(0, remove(path));
}

TEST(pbc_writer_rejects_oversized_record_counts) {
    const char *path = "build/host/test_pbc_oversized_writer.pbc";
    pmodule module;
    pproto *entry;
    ASSERT_TRUE(pmodule_init(&module));
    entry = pproto_new("{main}", 6U);
    ASSERT_TRUE(entry != NULL);
    ASSERT_TRUE(pmodule_add(&module, entry));
    entry->catch_count = (size_t)UINT16_MAX + 1U;
    ASSERT_EQ(PPHP_E_NOMEM, pphp_pbc_write_file(&module, path));
    entry->catch_count = 0U;
    pmodule_destroy(&module);
}

TEST(pbc_modules_keep_classes_closures_and_owned_backings_alive) {
    const char *definitions_path = "build/host/test_pbc_lifetime_defs.pbc";
    const char *use_path = "build/host/test_pbc_lifetime_use.pbc";
    const char *failure_path = "build/host/test_pbc_lifetime_failure.pbc";
    const char *duplicate_path = "build/host/test_pbc_lifetime_duplicate.pbc";
    const char *definitions =
        "class XipBox {"
        " public $value = 'property';"
        " function text($suffix) { return $this->value . $suffix; }"
        " function __destruct() { echo ':destroy'; }"
        "}"
        "function xip_echo($value) { return $value; }"
        "$xipObject = new XipBox();"
        "$xipCaptured = 'captured';"
        "$xipClosure = function($suffix) use ($xipObject, $xipCaptured) {"
        " return $xipObject->text($suffix) . ':' . $xipCaptured;"
        "};"
        "$xipCallable = 'xip_echo';"
        "$xipArray = ['image-key' => 'image-value'];";
    const char *use =
        "$xipMethodCallable = [$xipObject, 'text'];"
        "echo $xipObject->text(':method'), ':',"
        " ($xipClosure)(':closure'), ':', $xipArray['image-key'], ':',"
        " ($xipCallable)('callable'), ':',"
        " ($xipMethodCallable)(':array-call');";
    const char *failure =
        "class FailedPbcClass {"
        " function f() { return 'safe'; }"
        " function __destruct() { echo ':failed-destroy'; }"
        "}"
        "$failedPbcObject = new FailedPbcClass();"
        "throw new Exception('stop');";
    pphp_state *state;
    output_buffer output;
    uint8_t *bytes;
    size_t length;

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(definitions, definitions_path));
    ASSERT_TRUE(write_source_pbc(use, use_path));
    ASSERT_TRUE(write_source_pbc(failure, failure_path));
    ASSERT_TRUE(write_source_pbc("class XipBox {}", duplicate_path));

    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    bytes = read_pbc_from_pool(definitions_path, &length);
    ASSERT_TRUE(bytes != NULL);
    ASSERT_EQ(PPHP_OK, pphp_exec_pbc_owned(state, bytes, length));
    bytes = read_pbc_from_pool(use_path, &length);
    ASSERT_TRUE(bytes != NULL);
    ASSERT_EQ(PPHP_OK, pphp_exec_pbc_owned(state, bytes, length));
    ASSERT_STR("property:method:property:closure:captured:image-value:callable:property:array-call",
               output.bytes);
    pphp_close(state);
    ASSERT_STR("property:method:property:closure:captured:image-value:callable:property:array-call:destroy",
               output.bytes);

    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    bytes = read_pbc_from_pool(failure_path, &length);
    ASSERT_TRUE(bytes != NULL);
    ASSERT_EQ(PPHP_E_RUNTIME, pphp_exec_pbc_owned(state, bytes, length));
    pphp_close(state);
    ASSERT_STR(":failed-destroy", output.bytes);

    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    bytes = read_pbc_from_pool(definitions_path, &length);
    ASSERT_TRUE(bytes != NULL);
    ASSERT_EQ(PPHP_OK, pphp_exec_pbc_owned(state, bytes, length));
    bytes = read_pbc_from_pool(duplicate_path, &length);
    ASSERT_TRUE(bytes != NULL);
    ASSERT_EQ(PPHP_E_RUNTIME, pphp_exec_pbc_owned(state, bytes, length));
    ASSERT_TRUE(strstr(pphp_last_error(state), "cannot register class") != NULL);
    pphp_close(state);
    ASSERT_STR(":destroy", output.bytes);

    ASSERT_EQ(0, remove(definitions_path));
    ASSERT_EQ(0, remove(use_path));
    ASSERT_EQ(0, remove(failure_path));
    ASSERT_EQ(0, remove(duplicate_path));
}

TEST(pbc_xip_images_can_be_used_by_sequential_states) {
    const char *path = "build/host/test_pbc_two_states.pbc";
    uint8_t *bytes;
    uint8_t *snapshot;
    size_t length;
    size_t i;
    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc("echo 'state';", path));
    bytes = read_pbc_from_libc(path, &length);
    ASSERT_TRUE(bytes != NULL);
    snapshot = malloc(length);
    ASSERT_TRUE(snapshot != NULL);
    memcpy(snapshot, bytes, length);
    for (i = 0U; i < 2U; i++) {
        pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
        output_buffer output;
        ASSERT_TRUE(state != NULL);
        memset(&output, 0, sizeof(output));
        pphp_set_output(state, capture_output, &output);
        ASSERT_EQ(PPHP_OK, pphp_exec_pbc(state, bytes, length));
        ASSERT_STR("state", output.bytes);
        pphp_close(state);
        ASSERT_EQ(0, memcmp(bytes, snapshot, length));
    }
    free(snapshot);
    free(bytes);
    ASSERT_EQ(0, remove(path));
}

TEST(source_modules_outlive_objects_kept_in_globals) {
    const char *source =
        "<?php class SourceLifetime {"
        " function value() { return 'alive'; }"
        " function __destruct() { echo ':source-destroy'; }"
        "}"
        "$sourceLifetime = new SourceLifetime();"
        "echo $sourceLifetime->value();";
    pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
    output_buffer output;
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source(state, source, strlen(source), "source-lifetime"));
    ASSERT_STR("alive", output.bytes);
    pphp_close(state);
    ASSERT_STR("alive:source-destroy", output.bytes);
}

TEST(shutdown_destructors_run_before_class_metadata_is_released) {
    const char *path = "build/host/test_shutdown_class_lifetime.pbc";
    const char *source =
        "<?php class ShutdownHolder { public static $value; }"
        "class ShutdownVictim {"
        " function __destruct() { echo ShutdownHelper::$message; }"
        "}"
        "class ShutdownHelper { public static $message = ':cross'; }"
        "class ShutdownSelf {"
        " public static $value; public static $message = ':self';"
        " function __destruct() { echo self::$message; }"
        "}"
        "ShutdownHolder::$value = new ShutdownVictim();"
        "ShutdownSelf::$value = new ShutdownSelf();";
    pphp_state *state;
    output_buffer output;
    uint8_t *bytes;
    size_t length;

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source(state, source, strlen(source), "shutdown-source"));
    pphp_close(state);
    ASSERT_STR(":self:cross", output.bytes);

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(source + 6U, path));
    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    bytes = read_pbc_from_pool(path, &length);
    ASSERT_TRUE(bytes != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK, pphp_exec_pbc_owned(state, bytes, length));
    pphp_close(state);
    ASSERT_STR(":self:cross", output.bytes);
    ASSERT_EQ(0, remove(path));
}

TEST(transient_pbc_modules_are_released_after_success_and_failure) {
    const char *success_path = "build/host/test_transient_success.pbc";
    const char *failure_path = "build/host/test_transient_failure.pbc";
    uint8_t *success;
    uint8_t *failure;
    size_t success_length;
    size_t failure_length;
    size_t baseline_modules;
    size_t baseline_used;
    size_t i;
    pphp_state *state;
    const char *source_success = "<?php $transientSource = 3 + 4;";
    const char *source_failure =
        "<?php function transient_source_failed_definition() { return 3; }"
        "missing_transient_source_call();";

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc("$transient = 1 + 2;", success_path));
    ASSERT_TRUE(write_source_pbc(
        "function transient_failed_definition() { return 1; }"
        "if (true) { function transient_failed_conditional() { return 2; } }"
        "missing_transient_call();", failure_path));
    success = read_pbc_from_libc(success_path, &success_length);
    failure = read_pbc_from_libc(failure_path, &failure_length);
    ASSERT_TRUE(success != NULL);
    ASSERT_TRUE(failure != NULL);
    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);

    ASSERT_EQ(PPHP_OK, pphp_exec_pbc(state, success, success_length));
    ASSERT_EQ(PPHP_E_RUNTIME, pphp_exec_pbc(state, failure, failure_length));
    baseline_modules = state->repl_module_count;
    baseline_used = pphp_pool_get_stats().used;
    for (i = 0U; i < 64U; i++) {
        ASSERT_EQ(PPHP_OK, pphp_exec_pbc(state, success, success_length));
        ASSERT_EQ(PPHP_E_RUNTIME,
                  pphp_exec_pbc(state, failure, failure_length));
        ASSERT_EQ(baseline_modules, state->repl_module_count);
        ASSERT_EQ(baseline_used, pphp_pool_get_stats().used);
    }

    ASSERT_EQ(PPHP_OK,
              pphp_exec_source(state, source_success, strlen(source_success),
                               "transient-source-success"));
    ASSERT_EQ(PPHP_E_RUNTIME,
              pphp_exec_source(state, source_failure, strlen(source_failure),
                               "transient-source-failure"));
    baseline_modules = state->repl_module_count;
    baseline_used = pphp_pool_get_stats().used;
    for (i = 0U; i < 32U; i++) {
        ASSERT_EQ(PPHP_OK,
                  pphp_exec_source(state, source_success,
                                   strlen(source_success),
                                   "transient-source-success"));
        ASSERT_EQ(PPHP_E_RUNTIME,
                  pphp_exec_source(state, source_failure,
                                   strlen(source_failure),
                                   "transient-source-failure"));
        ASSERT_EQ(baseline_modules, state->repl_module_count);
        ASSERT_EQ(baseline_used, pphp_pool_get_stats().used);
    }
    pphp_close(state);
    free(success);
    free(failure);
    ASSERT_EQ(0, remove(success_path));
    ASSERT_EQ(0, remove(failure_path));
}

TEST(closure_frames_retain_last_callable_owners) {
    static const char *const invokes[] = {
        "<?php echo (array_shift($frameOwners))('normal');",
        "<?php try { (array_shift($frameOwners))('throw'); }"
        " catch (Exception $e) { echo 'X'; }",
        "<?php try { (array_shift($frameOwners))('error'); }"
        " catch (Error $e) { echo 'E'; }",
        "<?php (array_shift($frameOwners))('exit');"
    };
    static const char *const expected[] = {"Fok", "FX", "FE", ""};
    static const char *const invoke_paths[] = {
        "build/host/test_frame_owner_normal.pbc",
        "build/host/test_frame_owner_throw.pbc",
        "build/host/test_frame_owner_error.pbc",
        "build/host/test_frame_owner_exit.pbc"
    };
    const char *setup_path = "build/host/test_frame_owner_setup.pbc";
    const char *setup =
        "<?php $frameOwners = [function ($mode) {"
        " try {"
        "  if ($mode === 'throw') { throw new Exception('frame'); }"
        "  if ($mode === 'error') { missing_frame_owner_call(); }"
        "  if ($mode === 'exit') { exit(7); }"
        "  return 'ok';"
        " } finally { echo 'F'; }"
        "}];";
    uint8_t *setup_pbc;
    uint8_t *invoke_pbc[4];
    size_t setup_length;
    size_t invoke_lengths[4];
    size_t i;

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(setup + 6U, setup_path));
    for (i = 0U; i < 4U; i++) {
        ASSERT_TRUE(write_source_pbc(invokes[i] + 6U, invoke_paths[i]));
    }
    setup_pbc = read_pbc_from_libc(setup_path, &setup_length);
    ASSERT_TRUE(setup_pbc != NULL);
    for (i = 0U; i < 4U; i++) {
        invoke_pbc[i] = read_pbc_from_libc(invoke_paths[i],
                                           &invoke_lengths[i]);
        ASSERT_TRUE(invoke_pbc[i] != NULL);
    }

    for (i = 0U; i < 4U; i++) {
        pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
        output_buffer output;
        ASSERT_TRUE(state != NULL);
        memset(&output, 0, sizeof(output));
        pphp_set_output(state, capture_output, &output);
        ASSERT_EQ(PPHP_OK,
                  pphp_exec_source(state, setup, strlen(setup),
                                   "frame-owner-setup"));
        ASSERT_EQ(PPHP_OK,
                  pphp_exec_source(state, invokes[i], strlen(invokes[i]),
                                   "frame-owner-invoke"));
        ASSERT_STR(expected[i], output.bytes);
        ASSERT_EQ(i == 3U, pphp_exit_requested(state));
        if (i == 3U) ASSERT_EQ(7, pphp_exit_status(state));
        pphp_close(state);
    }
    for (i = 0U; i < 4U; i++) {
        pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
        output_buffer output;
        ASSERT_TRUE(state != NULL);
        memset(&output, 0, sizeof(output));
        pphp_set_output(state, capture_output, &output);
        ASSERT_EQ(PPHP_OK, pphp_exec_pbc(state, setup_pbc, setup_length));
        ASSERT_EQ(PPHP_OK,
                  pphp_exec_pbc(state, invoke_pbc[i], invoke_lengths[i]));
        ASSERT_STR(expected[i], output.bytes);
        ASSERT_EQ(i == 3U, pphp_exit_requested(state));
        if (i == 3U) ASSERT_EQ(7, pphp_exit_status(state));
        pphp_close(state);
    }

    free(setup_pbc);
    ASSERT_EQ(0, remove(setup_path));
    for (i = 0U; i < 4U; i++) {
        free(invoke_pbc[i]);
        ASSERT_EQ(0, remove(invoke_paths[i]));
    }
}

TEST(closure_frames_retain_called_scope) {
    const char *setup_path = "build/host/test_frame_scope_setup.pbc";
    const char *invoke_path = "build/host/test_frame_scope_invoke.pbc";
    const char *setup =
        "<?php class FrameOwnerScope {"
        " private static function hidden() { return 'scope'; }"
        " public static function make() {"
        "  return function () { return self::hidden(); };"
        " }"
        "} $frameOwners = [FrameOwnerScope::make()];";
    const char *invoke =
        "<?php echo (array_shift($frameOwners))();";
    uint8_t *setup_pbc;
    uint8_t *invoke_pbc;
    size_t setup_length;
    size_t invoke_length;
    size_t mode;

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(setup + 6U, setup_path));
    ASSERT_TRUE(write_source_pbc(invoke + 6U, invoke_path));
    setup_pbc = read_pbc_from_libc(setup_path, &setup_length);
    invoke_pbc = read_pbc_from_libc(invoke_path, &invoke_length);
    ASSERT_TRUE(setup_pbc != NULL);
    ASSERT_TRUE(invoke_pbc != NULL);
    for (mode = 0U; mode < 2U; mode++) {
        pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
        output_buffer output;
        ASSERT_TRUE(state != NULL);
        memset(&output, 0, sizeof(output));
        pphp_set_output(state, capture_output, &output);
        if (mode == 0U) {
            ASSERT_EQ(PPHP_OK,
                      pphp_exec_source(state, setup, strlen(setup),
                                       "frame-scope-setup"));
            ASSERT_EQ(PPHP_OK,
                      pphp_exec_source(state, invoke, strlen(invoke),
                                       "frame-scope-invoke"));
        } else {
            ASSERT_EQ(PPHP_OK,
                      pphp_exec_pbc(state, setup_pbc, setup_length));
            ASSERT_EQ(PPHP_OK,
                      pphp_exec_pbc(state, invoke_pbc, invoke_length));
        }
        ASSERT_STR("scope", output.bytes);
        pphp_close(state);
    }
    free(setup_pbc);
    free(invoke_pbc);
    ASSERT_EQ(0, remove(setup_path));
    ASSERT_EQ(0, remove(invoke_path));
}

TEST(class_reachability_sweeps_dead_graphs_beside_live_graphs) {
    const char *success_path = "build/host/test_class_reach_success.pbc";
    const char *failure_path = "build/host/test_class_reach_failure.pbc";
    const char *live_setup =
        "<?php interface LiveMarker {}"
        "class LiveParent {}"
        "class LiveChild extends LiveParent implements LiveMarker {"
        " public function value() { return 'live'; }"
        "}"
        "class SharedNode {}"
        "$liveObject = new LiveChild();"
        "$sharedNode = new SharedNode();";
    const char *success =
        "<?php class TempShared { public static $value; }"
        "TempShared::$value = $sharedNode;"
        "interface TempInterface {}"
        "class TempParent {}"
        "class TempChild extends TempParent implements TempInterface {}"
        "class TempClosureCycle {"
        " public static $values = [];"
        " private static function hidden() { return 1; }"
        " public static function init() {"
        "  self::$values = [function () { return self::hidden(); }];"
        " }"
        "}"
        "TempClosureCycle::init();"
        "class TempHolder { public static $value; }"
        "class TempVictim {"
        " public function __destruct() { echo TempHelper::$message; }"
        "}"
        "class TempHelper { public static $message = 'D'; }"
        "TempHolder::$value = new TempVictim();"
        "class TempSelf {"
        " public static $value;"
        " public function __destruct() { echo 'S'; }"
        "}"
        "TempSelf::$value = new TempSelf();";
    const char *failure =
        "<?php class TempFailure {"
        " public static $value; public static $message = 'F';"
        " public function __destruct() { echo self::$message; }"
        "}"
        "TempFailure::$value = new TempFailure();"
        "throw new Exception('expected failure');";
    uint8_t *success_pbc;
    uint8_t *failure_pbc;
    size_t success_length;
    size_t failure_length;
    size_t baseline_classes;
    size_t baseline_modules;
    size_t baseline_used;
    size_t i;
    pphp_state *state;
    output_buffer output;

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(success + 6U, success_path));
    ASSERT_TRUE(write_source_pbc(failure + 6U, failure_path));
    success_pbc = read_pbc_from_libc(success_path, &success_length);
    failure_pbc = read_pbc_from_libc(failure_path, &failure_length);
    ASSERT_TRUE(success_pbc != NULL);
    ASSERT_TRUE(failure_pbc != NULL);
    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source(state, live_setup, strlen(live_setup),
                               "class-reach-live"));

    ASSERT_EQ(PPHP_OK,
              pphp_exec_source(state, success, strlen(success),
                               "class-reach-success"));
    ASSERT_EQ(PPHP_E_RUNTIME,
              pphp_exec_source(state, failure, strlen(failure),
                               "class-reach-failure"));
    ASSERT_EQ(PPHP_OK, pphp_exec_pbc(state, success_pbc, success_length));
    ASSERT_EQ(PPHP_E_RUNTIME,
              pphp_exec_pbc(state, failure_pbc, failure_length));
    baseline_classes = state->class_count;
    baseline_modules = state->repl_module_count;
    baseline_used = pphp_pool_get_stats().used;

    for (i = 0U; i < 32U; i++) {
        ASSERT_EQ(PPHP_OK,
                  pphp_exec_source(state, success, strlen(success),
                                   "class-reach-success"));
        ASSERT_EQ(PPHP_E_RUNTIME,
                  pphp_exec_source(state, failure, strlen(failure),
                                   "class-reach-failure"));
        ASSERT_EQ(baseline_classes, state->class_count);
        ASSERT_EQ(baseline_modules, state->repl_module_count);
        ASSERT_EQ(baseline_used, pphp_pool_get_stats().used);
    }
    for (i = 0U; i < 32U; i++) {
        ASSERT_EQ(PPHP_OK,
                  pphp_exec_pbc(state, success_pbc, success_length));
        ASSERT_EQ(PPHP_E_RUNTIME,
                  pphp_exec_pbc(state, failure_pbc, failure_length));
        ASSERT_EQ(baseline_classes, state->class_count);
        ASSERT_EQ(baseline_modules, state->repl_module_count);
        ASSERT_EQ(baseline_used, pphp_pool_get_stats().used);
    }
    ASSERT_TRUE(pphp_find_class(state, "LiveChild", 9U) != NULL);
    ASSERT_TRUE(pphp_find_class(state, "LiveParent", 10U) != NULL);
    ASSERT_TRUE(pphp_find_class(state, "LiveMarker", 10U) != NULL);
    ASSERT_TRUE(pphp_find_class(state, "SharedNode", 10U) != NULL);
    ASSERT_TRUE(pphp_find_class(state, "TempShared", 10U) == NULL);
    ASSERT_TRUE(pphp_find_class(state, "TempClosureCycle", 16U) == NULL);
    ASSERT_TRUE(output.length >= 4U + 64U * 3U);
    pphp_close(state);
    free(success_pbc);
    free(failure_pbc);
    ASSERT_EQ(0, remove(success_path));
    ASSERT_EQ(0, remove(failure_path));
}

TEST(pbc_loader_rejects_truncated_and_wrapping_sections) {
    uint8_t bytes[128];
    size_t valid_length;

#if PPHP_TYPECHECK
    valid_length = 48U;
#else
    valid_length = 44U;
#endif
    minimal_pbc(bytes, valid_length);
    ASSERT_EQ(PPHP_OK, load_test_pbc(bytes, valid_length));
#if PPHP_ENABLE_FLOAT
    {
        uint16_t flags = (uint16_t)((uint16_t)bytes[6U] |
                                    (uint16_t)((uint16_t)bytes[7U] << 8U));
        test_put_u16(bytes, 6U,
                     (uint16_t)(flags &
                         (uint16_t)~(PPHP_PBC_FLAG_FEATURES |
                                     PPHP_PBC_FLAG_FLOAT)));
        ASSERT_EQ(PPHP_OK, load_test_pbc(bytes, valid_length));
        minimal_pbc(bytes, valid_length);
        test_put_u16(bytes, 6U,
                     (uint16_t)(flags &
                               (uint16_t)~PPHP_PBC_FLAG_FLOAT));
        ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, valid_length));
    }
#endif

    minimal_pbc(bytes, 20U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 20U));

    minimal_pbc(bytes, 44U);
    test_put_u32(bytes, 16U, UINT32_C(0xfffffff8));
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 44U));

    minimal_pbc(bytes, 44U);
    test_put_u32(bytes, 16U, 43U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 44U));

    minimal_pbc(bytes, 44U);
    test_put_u32(bytes, 20U, 29U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 44U));

    minimal_pbc(bytes, 44U);
    test_put_u32(bytes, 20U, 24U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 44U));

    minimal_pbc(bytes, 44U);
    test_put_u16(bytes, 24U, 3U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 44U));

    minimal_pbc(bytes, 44U);
    test_put_u32(bytes, 16U, 40U);
    test_put_u16(bytes, 40U, 3U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 44U));

    minimal_pbc(bytes, 44U);
    test_put_u32(bytes, 20U, UINT32_C(0xfffffff8));
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 44U));

    minimal_pbc(bytes, 44U);
    test_put_u32(bytes, 20U, 36U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 44U));

    minimal_pbc(bytes, 47U);
    test_put_u16(bytes, 36U, 1U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 47U));

    minimal_pbc(bytes, 51U);
    test_put_u16(bytes, 38U, 1U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 51U));

    minimal_pbc(bytes, 52U);
    test_put_u16(bytes, 38U, 1U);
    bytes[44U] = 3U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 52U));

    minimal_pbc(bytes, 52U);
    test_put_u16(bytes, 38U, 1U);
    bytes[44U] = 4U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 52U));

    minimal_pbc(bytes, 45U);
    bytes[31U] = 1U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 45U));

    minimal_pbc(bytes, 53U);
    test_put_u16(bytes, 40U, 1U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(bytes, 53U));
}

#if PPHP_TYPECHECK
TEST(pbc_loader_rejects_corrupt_declared_type_metadata) {
    const char *path = "build/host/test_type_metadata.pbc";
    const char *source =
        "class TypeBlobParent {}"
        "class TypeBlobClass extends TypeBlobParent {"
        " public parent $parent; public int $value = 1;"
        " public function __construct(): void {}"
        " public function method(int $value): int { return $value; }"
        " public function __destruct() {}"
        " public function destroyTen(int $value): int { return $value; }"
        "}"
        "readonly class ReadonlyBlob { public int $item; }"
        "function typed(TypeBlobClass|int $value = 1): string { return 'ok'; }"
        "$outside = 2;"
        "$closure = function (int $value) use ($outside): int {"
        " return $value + $outside;"
        "};"
        "$otherClosure = function (): int { return 1; };"
        "if (true) { echo typed(new TypeBlobClass()); }"
        "$cast = (int)'1'; include 'not-executed.php';";
    uint8_t *bytes;
    uint8_t *mutated;
    size_t length;
    uint16_t n_strings;
    uint16_t n_protos;
    size_t proto_table;
    size_t main_proto;
    size_t typed_proto;
    size_t typed_type;
    size_t property_opcode = SIZE_MAX;
    size_t second_property;
    size_t readonly_property;
    size_t child_class;
    size_t call_opcode;
    size_t jump_opcode;
    size_t closure_opcode;
    size_t other_closure_opcode;
    size_t cast_opcode;
    size_t include_opcode;
    size_t typecheck_opcode;
    size_t return_opcode;
    size_t method_opcode;
    size_t constructor_opcode;
    size_t destructor_opcode;
    size_t argument_method_opcode;
    size_t constructor_proto;
    size_t constructor_type;
    size_t method_proto;
    size_t method_name_record;
    size_t destructor_proto;
    size_t argument_method_proto;
    size_t argument_method_proto_name_record;
    size_t store_opcode;
    size_t string_record;
    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(source, path));
    bytes = read_pbc_from_libc(path, &length);
    ASSERT_TRUE(bytes != NULL);
    mutated = malloc(length);
    ASSERT_TRUE(mutated != NULL);
    n_strings = test_get_u16(bytes, 14U);
    n_protos = test_get_u16(bytes, 12U);
    ASSERT_TRUE(n_protos >= 2U);
    proto_table = 16U + (size_t)n_strings * 4U;
    main_proto = test_get_u32(bytes, proto_table);
    typed_proto = test_get_u32(bytes, proto_table + 4U);
    ASSERT_EQ(1, bytes[typed_proto]);
    typed_type = test_proto_type_offset(bytes, typed_proto);
    ASSERT_EQ(2, bytes[typed_type]);
    ASSERT_EQ(PTYPE_NAMED, bytes[typed_type + 1U]);
    ASSERT_EQ(PPHP_OK, load_test_pbc(bytes, length));

    property_opcode = test_find_opcode(bytes, main_proto, OP_DEF_PROP, 0U);
    second_property = test_find_opcode(bytes, main_proto, OP_DEF_PROP, 1U);
    readonly_property = test_find_opcode(bytes, main_proto, OP_DEF_PROP, 2U);
    child_class = test_find_opcode(bytes, main_proto, OP_DEF_CLASS, 1U);
    method_opcode = test_find_opcode(bytes, main_proto, OP_DEF_METHOD, 1U);
    constructor_opcode = test_find_opcode(bytes, main_proto, OP_DEF_METHOD, 0U);
    destructor_opcode = test_find_opcode(bytes, main_proto, OP_DEF_METHOD, 2U);
    argument_method_opcode = test_find_opcode(bytes, main_proto,
                                              OP_DEF_METHOD, 3U);
    call_opcode = test_find_opcode(bytes, main_proto, OP_CALL, 0U);
    jump_opcode = test_find_opcode(bytes, main_proto, OP_JMP_UNLESS, 0U);
    closure_opcode = test_find_opcode(bytes, main_proto, OP_CLOSURE, 0U);
    other_closure_opcode = test_find_opcode(bytes, main_proto, OP_CLOSURE, 1U);
    cast_opcode = test_find_opcode(bytes, main_proto, OP_CAST, 0U);
    include_opcode = test_find_opcode(bytes, main_proto, OP_INCLUDE, 0U);
    typecheck_opcode = test_find_opcode(bytes, typed_proto,
                                        OP_TYPECHECK_ARGS, 0U);
    return_opcode = test_find_opcode(bytes, typed_proto, OP_RET, 0U);
    store_opcode = test_find_opcode(bytes, typed_proto, OP_STORE_LOCAL, 0U);
    ASSERT_TRUE(property_opcode != SIZE_MAX);
    ASSERT_TRUE(second_property != SIZE_MAX);
    ASSERT_TRUE(readonly_property != SIZE_MAX);
    ASSERT_TRUE(child_class != SIZE_MAX);
    ASSERT_TRUE(method_opcode != SIZE_MAX);
    ASSERT_TRUE(constructor_opcode != SIZE_MAX);
    ASSERT_TRUE(destructor_opcode != SIZE_MAX);
    ASSERT_TRUE(argument_method_opcode != SIZE_MAX);
    ASSERT_TRUE(call_opcode != SIZE_MAX);
    ASSERT_TRUE(jump_opcode != SIZE_MAX);
    ASSERT_TRUE(closure_opcode != SIZE_MAX);
    ASSERT_TRUE(other_closure_opcode != SIZE_MAX);
    ASSERT_TRUE(cast_opcode != SIZE_MAX);
    ASSERT_TRUE(include_opcode != SIZE_MAX);
    ASSERT_TRUE(typecheck_opcode != SIZE_MAX);
    ASSERT_TRUE(return_opcode != SIZE_MAX);
    ASSERT_TRUE(store_opcode != SIZE_MAX);
    {
        uint16_t constructor_index;
        constructor_index = test_get_u16(bytes, constructor_opcode + 3U);
        ASSERT_TRUE(constructor_index < n_protos);
        constructor_proto = test_get_u32(
            bytes, proto_table + (size_t)constructor_index * 4U);
        constructor_type = test_proto_type_offset(bytes, constructor_proto);
        ASSERT_EQ(1, bytes[constructor_type]);
        ASSERT_EQ(PTYPE_VOID, bytes[constructor_type + 1U]);
    }
    {
        uint16_t destructor_index = test_get_u16(bytes,
                                                 destructor_opcode + 3U);
        uint16_t argument_method_index = test_get_u16(
            bytes, argument_method_opcode + 3U);
        uint16_t argument_method_name_sid;
        ASSERT_TRUE(destructor_index < n_protos);
        ASSERT_TRUE(argument_method_index < n_protos);
        destructor_proto = test_get_u32(
            bytes, proto_table + (size_t)destructor_index * 4U);
        argument_method_proto = test_get_u32(
            bytes, proto_table + (size_t)argument_method_index * 4U);
        argument_method_name_sid = test_get_u16(
            bytes, argument_method_proto + 14U);
        ASSERT_TRUE(argument_method_name_sid < n_strings);
        argument_method_proto_name_record = test_get_u32(
            bytes, 16U + (size_t)argument_method_name_sid * 4U);
    }
    {
        uint16_t method_index = test_get_u16(bytes, method_opcode + 3U);
        uint16_t method_name_sid;
        ASSERT_TRUE(method_index < n_protos);
        method_proto = test_get_u32(
            bytes, proto_table + (size_t)method_index * 4U);
        method_name_sid = test_get_u16(bytes, method_proto + 14U);
        ASSERT_TRUE(method_name_sid < n_strings);
        method_name_record = test_get_u32(
            bytes, 16U + (size_t)method_name_sid * 4U);
    }

    memcpy(mutated, bytes, length);
    mutated[typed_type] = 17U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[typed_type + 1U] = 0U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, typed_type + 2U, n_strings);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[typed_proto] = 2U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u32(mutated, 8U, (uint32_t)(length - 1U));
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length - 1U));

    memcpy(mutated, bytes, length);
    test_put_u32(mutated, proto_table + 4U, (uint32_t)(typed_proto + 1U));
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u32(mutated, proto_table + 4U, (uint32_t)main_proto);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, property_opcode + 4U,
                 test_get_u16(bytes, main_proto + 10U));
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[typecheck_opcode] = OP_NOP;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[return_opcode] = OP_TYPECHECK_ARGS;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[typed_proto + 16U] = OP_NOP;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[store_opcode + 1U] = bytes[typed_proto + 3U];
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[call_opcode + 3U] = 32U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, call_opcode + 1U,
                 test_get_u16(bytes, main_proto + 10U));
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, jump_opcode + 1U, UINT16_MAX);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, jump_opcode + 1U, UINT16_MAX - 1U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    {
        size_t code_start = main_proto + 16U;
        size_t jump_pc = jump_opcode - code_start;
        size_t code_length = test_get_u16(bytes, main_proto + 8U);
        int16_t relative = (int16_t)(code_length - (jump_pc + 3U));
        memcpy(mutated, bytes, length);
        test_put_u16(mutated, jump_opcode + 1U, (uint16_t)relative);
        ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));
    }

    ASSERT_EQ(1, bytes[closure_opcode + 3U]);
    memcpy(mutated, bytes, length);
    test_put_u16(mutated, closure_opcode + 1U, n_protos);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, closure_opcode + 1U, 1U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, other_closure_opcode + 1U,
                 test_get_u16(bytes, closure_opcode + 1U));
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[closure_opcode + 3U] = UINT8_MAX;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[closure_opcode + 4U] = 1U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[closure_opcode + 5U] = bytes[main_proto + 3U];
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[cast_opcode + 1U] = PT_OBJECT;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[include_opcode + 1U] = 0U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[child_class + 5U] = PC_MOD_PUBLIC;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, method_opcode + 3U, n_protos);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[method_opcode + 5U] = PC_MOD_READONLY;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[constructor_opcode + 5U] |= PC_MOD_STATIC;
    mutated[constructor_proto + 2U] &= (uint8_t)~2U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[destructor_opcode + 5U] |= PC_MOD_STATIC;
    mutated[destructor_proto + 2U] &= (uint8_t)~2U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, argument_method_opcode + 1U,
                 test_get_u16(bytes, destructor_opcode + 1U));
    memcpy(mutated + argument_method_proto_name_record + 2U +
               strlen("TypeBlobClass::"),
           "__destruct", 10U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[method_name_record + 2U] = 'X';
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[property_opcode + 3U] =
        (uint8_t)(PC_MOD_PUBLIC | PC_MOD_STATIC | PC_MOD_READONLY);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[property_opcode + 6U] = 2U;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[readonly_property + 3U] = PC_MOD_PUBLIC;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, readonly_property + 4U, UINT16_MAX);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    {
        uint16_t type_index = test_get_u16(bytes, second_property + 4U);
        size_t constant = test_constant_offset(bytes, main_proto, type_index);
        uint32_t sid;
        ASSERT_EQ(2, bytes[constant]);
        sid = test_get_u32(bytes, constant + 4U);
        ASSERT_TRUE(sid < n_strings);
        string_record = test_get_u32(bytes, 16U + (size_t)sid * 4U);
        ASSERT_EQ(3, test_get_u16(bytes, string_record));
        memcpy(mutated, bytes, length);
        mutated[string_record + 2U] = 'i';
        mutated[string_record + 3U] = '|';
        mutated[string_record + 4U] = '|';
        ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

        memcpy(mutated, bytes, length);
        mutated[string_record + 2U] = '1';
        ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));
    }

    memcpy(mutated, bytes, length);
    test_put_u16(mutated, child_class + 3U, UINT16_MAX);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    memcpy(mutated + typed_type + 4U, mutated + typed_type + 1U, 3U);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[typed_type + 1U] = PTYPE_MIXED;
    test_put_u16(mutated, typed_type + 2U, UINT16_MAX);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[typed_type + 8U] = PTYPE_STATIC;
    test_put_u16(mutated, typed_type + 9U, UINT16_MAX);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[typed_type + 8U] = PTYPE_VOID;
    test_put_u16(mutated, typed_type + 9U, UINT16_MAX);
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    memcpy(mutated, bytes, length);
    mutated[constructor_type + 1U] = PTYPE_INT;
    ASSERT_EQ(PPHP_E_PARSE, load_test_pbc(mutated, length));

    free(mutated);
    free(bytes);
    ASSERT_EQ(0, remove(path));
}

TEST(pbc_type_context_reports_allocation_failure) {
    const char *path = "build/host/test_type_context_oom.pbc";
    const char *source =
        "function typed_oom(int $value): int { return $value; }";
    uint8_t *bytes;
    size_t length;
    size_t pool_size;
    int saw_nomem = 0;
    int loaded = 0;
    int last_failure = PPHP_OK;

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(source, path));
    bytes = read_pbc_from_libc(path, &length);
    ASSERT_TRUE(bytes != NULL);
    for (pool_size = 256U; pool_size <= sizeof(pbc_oom_pool);
         pool_size += 8U) {
        pmodule module;
        int status;
        pphp_pool_init(pbc_oom_pool, pool_size);
        status = pphp_pbc_load(bytes, length, &module);
        if (status == PPHP_OK) {
            pmodule_destroy(&module);
            loaded = 1;
            break;
        }
        if (status == PPHP_E_NOMEM) {
            saw_nomem = 1;
        }
        last_failure = status;
    }
    pphp_pool_init(vm_pool, sizeof(vm_pool));
    free(bytes);
    ASSERT_EQ(0, remove(path));
    ASSERT_EQ(PPHP_E_NOMEM, last_failure);
    ASSERT_TRUE(saw_nomem);
    ASSERT_TRUE(loaded);
}

TEST(failed_forward_variance_rolls_back_definitions) {
    const char *pbc_path = "build/host/test_variance_rollback.pbc";
    const char *include_path = "build/host/test_variance_rollback.php";
    const char *stable =
        "function RollbackStableFunction() { return 1; }"
        "class RollbackStableClass {}"
        "$rollbackStableObject = new RollbackStableClass();";
    const char *bad_source =
        "function RollbackFailedSourceFunction() { return 1; }"
        "class RollbackSourceBase {"
        " public function f(RollbackSourceA $v): RollbackSourceB {"
        "  return new RollbackSourceB();"
        " }"
        "}"
        "class RollbackSourceBad extends RollbackSourceBase {"
        " public function f(RollbackSourceC $v): RollbackSourceD {"
        "  return new RollbackSourceD();"
        " }"
        "}"
        "class RollbackSourceA {} class RollbackSourceB {}"
        "class RollbackSourceC {} class RollbackSourceD {}";
    const char *bad_pbc =
        "function RollbackFailedPbcFunction() { return 1; }"
        "class RollbackPbcBase {"
        " public function f(RollbackPbcA $v): RollbackPbcB {"
        "  return new RollbackPbcB();"
        " }"
        "}"
        "class RollbackPbcBad extends RollbackPbcBase {"
        " public function f(RollbackPbcC $v): RollbackPbcD {"
        "  return new RollbackPbcD();"
        " }"
        "}"
        "class RollbackPbcA {} class RollbackPbcB {}"
        "class RollbackPbcC {} class RollbackPbcD {}";
    const char *bad_include =
        "<?php function RollbackFailedIncludeFunction() { return 1; }"
        "class RollbackIncludeBase {"
        " public function f(RollbackIncludeA $v): RollbackIncludeB {"
        "  return new RollbackIncludeB();"
        " }"
        "}"
        "class RollbackIncludeBad extends RollbackIncludeBase {"
        " public function f(RollbackIncludeC $v): RollbackIncludeD {"
        "  return new RollbackIncludeD();"
        " }"
        "}"
        "class RollbackIncludeA {} class RollbackIncludeB {}"
        "class RollbackIncludeC {} class RollbackIncludeD {}";
    const char *good_include =
        "<?php class RollbackIncludeRecovered {} echo 'R';";
    uint8_t *pbc;
    size_t pbc_length;
    pphp_state *state;
    output_buffer output;
    FILE *file;
    size_t baseline_classes;
    size_t baseline_functions;
    size_t baseline_modules;
    size_t baseline_used;
    size_t i;

    pphp_pool_init(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(write_source_pbc(bad_pbc, pbc_path));
    pbc = read_pbc_from_libc(pbc_path, &pbc_length);
    ASSERT_TRUE(pbc != NULL);
    state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source_mode(state, stable, strlen(stable),
                                    "rollback-stable", 1));
    baseline_classes = state->class_count;
    baseline_functions = state->runtime_function_count;
    baseline_modules = state->repl_module_count;
    ASSERT_EQ(PPHP_E_RUNTIME,
              pphp_exec_source_mode(state, bad_source, strlen(bad_source),
                                    "rollback-source", 1));
    ASSERT_TRUE(pphp_find_class(state, "RollbackStableClass", 19U) != NULL);
    ASSERT_TRUE(pphp_find_class(state, "RollbackSourceBad", 17U) == NULL);
    ASSERT_EQ(baseline_classes, state->class_count);
    ASSERT_EQ(baseline_functions, state->runtime_function_count);
    ASSERT_EQ(baseline_modules, state->repl_module_count);
    baseline_used = pphp_pool_get_stats().used;
    for (i = 0U; i < 8U; i++) {
        ASSERT_EQ(PPHP_E_RUNTIME,
                  pphp_exec_source_mode(state, bad_source,
                                        strlen(bad_source),
                                        "rollback-source-repeat", 1));
        ASSERT_EQ(baseline_classes, state->class_count);
        ASSERT_EQ(baseline_functions, state->runtime_function_count);
        ASSERT_EQ(baseline_modules, state->repl_module_count);
        ASSERT_EQ(baseline_used, pphp_pool_get_stats().used);
    }

    ASSERT_EQ(PPHP_E_RUNTIME, pphp_exec_pbc(state, pbc, pbc_length));
    ASSERT_TRUE(pphp_find_class(state, "RollbackStableClass", 19U) != NULL);
    ASSERT_TRUE(pphp_find_class(state, "RollbackPbcBad", 14U) == NULL);
    ASSERT_EQ(baseline_functions, state->runtime_function_count);

    file = fopen(include_path, "wb");
    ASSERT_TRUE(file != NULL);
    ASSERT_EQ((long)strlen(bad_include),
              (long)fwrite(bad_include, 1U, strlen(bad_include), file));
    ASSERT_EQ(0, fclose(file));
    ASSERT_EQ(PPHP_E_RUNTIME,
              pphp_exec_source_mode(
                  state,
                  "include_once 'build/host/test_variance_rollback.php';",
                  strlen("include_once 'build/host/test_variance_rollback.php';"),
                  "rollback-include", 1));
    ASSERT_TRUE(pphp_find_class(state, "RollbackIncludeBad", 18U) == NULL);
    ASSERT_EQ(baseline_functions, state->runtime_function_count);

    file = fopen(include_path, "wb");
    ASSERT_TRUE(file != NULL);
    ASSERT_EQ((long)strlen(good_include),
              (long)fwrite(good_include, 1U, strlen(good_include), file));
    ASSERT_EQ(0, fclose(file));
    memset(&output, 0, sizeof(output));
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source_mode(
                  state,
                  "include_once 'build/host/test_variance_rollback.php';",
                  strlen("include_once 'build/host/test_variance_rollback.php';"),
                  "rollback-include-retry", 1));
    ASSERT_STR("R", output.bytes);
    ASSERT_TRUE(pphp_find_class(state, "RollbackIncludeRecovered", 24U) != NULL);

    pphp_close(state);
    free(pbc);
    ASSERT_EQ(0, remove(pbc_path));
    ASSERT_EQ(0, remove(include_path));
}
#endif

TEST(arrays_use_copy_on_write_and_normalized_keys) {
    const char *source =
        "$a = [1, 2]; $b = $a; $b[] = 3; $b['1'] = 20;"
        "echo count($a), ':', count($b), ':', $a[1], ':', $b[1], ':', array_sum($b);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("2:3:2:20:24", output.bytes);
}

TEST(language_conformance_covers_casts_lvalues_nullsafe_and_interpolation) {
    const char *source =
        "declare(strict_types=1);"
        "$source = array('node' => array('value' => 1)); $copy = $source;"
        "$copy['node']['value'] += 2; $copy['node']['next'] ?" "?= 4;"
        "$key = 'value'; $object = null;"
        "echo (int)'1.9', ':', '12tail' + 1, ':',"
        " $source['node']['value'], ':', $copy['node']['value'], ':',"
        " ++$copy['node']['next'], ':', $object?->missing(), ':',"
        " \"{$copy['node'][$key]}\";";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
#if PPHP_WARNINGS
    ASSERT_STR("Warning: A non-numeric value encountered on line 1\n"
               "1:13:1:3:5::3", output.bytes);
#else
    ASSERT_STR("1:13:1:3:5::3", output.bytes);
#endif
}

TEST(member_lvalues_and_dynamic_names_preserve_cow_and_evaluation_order) {
    const char *source =
        "class MemberBox {"
        " public $items = [3, 1, 2]; public static $shared = [2, 1];"
        " public function total($extra) { return array_sum($this->items) + $extra; }"
        "}"
        "$box = new MemberBox(); $original = $box->items;"
        "$property = 'items'; $method = 'total'; $evaluations = 0;"
        "$box->{$property}[$evaluations++] += 4;"
        "sort($box->items); rsort(MemberBox::$shared);"
        "echo $original[0], ':', implode(',', $box->items), ':',"
        " implode(',', MemberBox::$shared), ':', $box->$method(1), ':',"
        " isset($box->{$property}), ':', $evaluations;";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("3:1,2,7:2,1:11:1:1", output.bytes);
}

TEST(conditional_functions_register_only_when_their_declaration_executes) {
    const char *source =
        "if (false) { function skippedFunction() { return 1; } }"
        "if (true) { function selectedFunction() { return 7; } }"
        "function outerDeclaration() {"
        " function nestedDeclaration() { return 9; }"
        "}"
        "echo function_exists('skippedFunction') ? 1 : 0, ':',"
        " selectedFunction(), ':',"
        " function_exists('nestedDeclaration') ? 1 : 0;"
        "outerDeclaration(); echo ':', nestedDeclaration();";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("0:7:0:9", output.bytes);
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

TEST(type_conversion_and_predicate_builtins_follow_php_values) {
    const char *source =
        "function callable_fn() {}"
        "class CallableTarget {"
        " public static function run() {}"
        " private static function hidden() {}"
        " public static function checksHidden() {"
        "  return is_callable([self::class, 'hidden']);"
        " }"
        "}"
        "$callable = fn() => 1;"
        "echo intval('ff', 16), ':', floatval('1.5'), ':',"
        " ((strval(false) === '') ? 1 : 0), ':',"
        " (boolval('0') ? 1 : 0), ':',"
        " (is_int(1) ? 1 : 0), (is_float(1.0) ? 1 : 0),"
        " (is_string('x') ? 1 : 0), (is_bool(false) ? 1 : 0),"
        " (is_array([]) ? 1 : 0), (is_object($callable) ? 1 : 0),"
        " (is_null(null) ? 1 : 0), (is_numeric('1.5e2') ? 1 : 0),"
        " (is_numeric('no') ? 1 : 0), (is_callable($callable) ? 1 : 0), ':';"
        "echo (is_callable('callable_fn') ? 1 : 0),"
        " (is_callable('missing_callable') ? 1 : 0),"
        " (is_callable([CallableTarget::class, 'run']) ? 1 : 0),"
        " (is_callable([CallableTarget::class, 'hidden']) ? 1 : 0),"
        " (CallableTarget::checksHidden() ? 1 : 0);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("255:1.5:1:0:1111111101:10101", output.bytes);
}

TEST(math_builtins_cover_integer_float_and_collection_forms) {
    const char *source =
        "echo floor(2.9), ':', ceil(-2.1), ':', round(1.236, 2), ':',"
        " sqrt(81), ':', pow(2, 5), ':', intdiv(7, 2), ':',"
        " fmod(7, 4), ':', max([2, 9, 4]), ':', min(3, -2, 8), ':',"
        " log(8, 2), ':', (pi() > 3 ? 1 : 0);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("2:-2:1.24:9:32:3:3:9:-2:3:1", output.bytes);
    ASSERT_EQ(PPHP_OK,
              execute("try { intdiv(1, 0); } catch (DivisionByZeroError) { echo 'caught'; }",
                      &output, NULL, 0U));
    ASSERT_STR("caught", output.bytes);
}

TEST(string_builtins_cover_search_transform_split_and_join) {
    const char *source =
        "$parts = explode(',', 'a,b,c'); $chunks = str_split('abcd', 2);"
        "echo substr('abcdef', -4, 2), ':', strpos('banana', 'na'), ':',"
        " strrpos('banana', 'na'), ':',"
        " (str_contains('pico php', 'php') ? 1 : 0),"
        " (str_starts_with('pico', 'pi') ? 1 : 0),"
        " (str_ends_with('pico', 'co') ? 1 : 0), ':',"
        " strtoupper('abc'), ':', strtolower('ABC'), ':',"
        " ucfirst('hello'), ':', lcfirst('Hello'), ':', trim(\"  x \\n\"), ':',"
        " str_repeat('ab', 2), ':', strrev('abc'), ':',"
        " (strcmp('a', 'b') < 0 ? 1 : 0),"
        " (strcasecmp('A', 'a') === 0 ? 1 : 0),"
        " (strncmp('abc', 'abd', 2) === 0 ? 1 : 0), ':',"
        " bin2hex('AB'), ':', hex2bin('4142'), ':', chr(65), ':', ord('Z'), ':',"
        " str_replace('a', 'A', 'banana'), ':', implode('-', $parts), ':',"
        " count($parts), ':', $parts[1], ':', $chunks[1], ':',"
        " str_pad('x', 5, '-', 2), ':', dechex(255), ':', hexdec('ff'), ':',"
        " decbin(10), ':', bindec('1010'), ':', decoct(8), ':', octdec('10');";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("cd:2:4:111:ABC:abc:Hello:hello:x:abab:cba:111:4142:AB:A:90:bAnAnA:a-b-c:3:b:cd:--x--:ff:255:1010:10:10:8",
               output.bytes);
}

TEST(array_builtins_preserve_php_key_and_order_rules) {
    const char *source =
        "$a = ['x' => 1, 4 => 2, 3];"
        "$keys = array_keys($a); $values = array_values($a);"
        "$slice = array_slice($a, 1, 2);"
        "$merged = array_merge(['a' => 1, 8 => 2], ['a' => 3, 9 => 4]);"
        "$reversed = array_reverse($a);"
        "$filled = array_fill(-2, 4, 'z');"
        "$fillKeys = array_fill_keys(['name', 3], 7);"
        "$flipped = array_flip(['first' => 'a', 'second' => 2, 'third' => 'a']);"
        "$combined = array_combine(['left', 'right'], [9, 8]);"
        "$range = range(3, 1);"
        "echo (in_array(2, $a, true) ? 1 : 0), ':', array_search(3, $a, true), ':',"
        " (array_key_exists('x', $a) ? 1 : 0), ':',"
        " $keys[0], ',', $keys[1], ',', $keys[2], ':',"
        " $values[0], ',', $values[1], ',', $values[2], ':',"
        " $slice[0], ',', $slice[1], ':',"
        " $merged['a'], ',', $merged[0], ',', $merged[1], ':',"
        " $reversed[0], ',', $reversed[1], ',', $reversed['x'], ':',"
        " array_sum($a), ',', array_product([2, 3, 4]), ':',"
        " $filled[-2], ',', $filled[-1], ',', $filled[0], ',', $filled[1], ':',"
        " $fillKeys['name'], ',', $fillKeys[3], ':',"
        " $flipped['a'], ',', $flipped[2], ':',"
        " $combined['left'], ',', $combined['right'], ':',"
        " $range[0], ',', $range[1], ',', $range[2], ':',"
        " (array_is_list([1, 2]) ? 1 : 0),"
        " (array_is_list([1 => 2]) ? 1 : 0);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("1:5:1:x,4,5:1,2,3:2,3:3,2,4:3,2,1:6,24:z,z,z,z:7,7:third,second:9,8:3,2,1:10",
               output.bytes);
}

TEST(default_and_variadic_parameters_work_for_all_callable_forms) {
    const char *source =
        "function describe($first = 2, $options = ['n' => 3], ...$rest) {"
        " return $first + $options['n'] + count($rest);"
        "}"
        "$named = 'describe';"
        "class Defaults {"
        " public function total($value = 7, ...$rest) {"
        "  return $value + count($rest);"
        " }"
        "}"
        "$offset = 4;"
        "$closure = function ($value = 2, ...$rest) use ($offset) {"
        " return $value + $offset + count($rest);"
        "};"
        "$object = new Defaults();"
        "echo describe(), ':', describe(4, ['n' => 5], 6, 7), ':',"
        " $named(), ':', $object->total(), ':', $object->total(1, 2, 3), ':',"
        " $closure(), ':', $closure(1, 2, 3);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("5:11:5:7:3:6:7", output.bytes);
}

TEST(array_and_argument_unpacking_preserve_evaluation_order) {
    const char *source =
        "function sum3($a, $b, $c) { return $a + $b + $c; }"
        "class SpreadBox {"
        " public $total;"
        " public function __construct($a, $b) { $this->total = $a + $b; }"
        " public function sum($a, $b) { return $a + $b; }"
        "}"
        "$tail = [2, 3]; $named = 'sum3';"
        "$box = new SpreadBox(...[7, 8]);"
        "$closure = fn($a, $b) => $a + $b;"
        "$array = [0, ...[5, 6], 'x' => 1, ...['x' => 2, 9 => 7]];"
        "echo sum3(1, ...$tail), ':', $named(...[4, 5, 6]), ':',"
        " $box->total, ':', $box->sum(...[1, 2]), ':',"
        " $closure(...[3, 4]), ':',"
        " $array[0], ',', $array[1], ',', $array[2], ',',"
        " $array['x'], ',', $array[3];";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("6:15:15:3:7:0,5,6,2,7", output.bytes);
}

TEST(global_static_and_named_constants_use_persistent_storage) {
    const char *source =
        "const BASE = 3, TOTAL = BASE + 2;"
        "$counter = 10;"
        "function change($amount) {"
        " global $counter; $counter += $amount; return $counter;"
        "}"
        "function tick() { static $value = 1; return $value++; }"
        "function conditionalGlobal() {"
        " $value = 9; if (false) { global $value; } return $value;"
        "}"
        "function defaultConstant($value = TOTAL) { return $value; }"
        "echo change(2), ':', $counter, ':', tick(), ',', tick(), ':',"
        " conditionalGlobal(), ':', defaultConstant(), ':',"
        " (PHP_INT_MAX > 0 ? 1 : 0), ':', PHP_INT_SIZE;";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("12:12:1,2:9:5:1:4", output.bytes);
}

TEST(repl_chunks_retain_globals_and_constants_by_name) {
    const char *first = "$value = 40; const STEP = 2;";
    const char *second = "echo $value + STEP;";
    pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
    output_buffer output;
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source_mode(state, first, strlen(first), "repl-1", 1));
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source_mode(state, second, strlen(second), "repl-2", 1));
    ASSERT_STR("42", output.bytes);
    pphp_close(state);
}

TEST(repl_chunks_retain_functions_classes_and_closures) {
    const char *first =
        "function plus($a, $b) { return $a + $b; }"
        "class ReplBox {"
        " private $value;"
        " public function __construct($value) { $this->value = $value; }"
        " public function get() { return $this->value; }"
        "}"
        "$offset = 5; $adder = fn($value) => $value + $offset;";
    const char *second =
        "$box = new ReplBox(7);"
        "echo plus(2, 3), ':', $box->get(), ':', $adder(4), ':',"
        " (function_exists('plus') ? 1 : 0), ':',"
        " (class_exists('ReplBox') ? 1 : 0);";
    pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
    output_buffer output;
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source_mode(state, first, strlen(first), "repl-1", 1));
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source_mode(state, second, strlen(second), "repl-2", 1));
    ASSERT_STR("5:7:9:1:1", output.bytes);
    pphp_close(state);
}

TEST(constant_and_reflection_builtins_share_runtime_registries) {
    const char *source =
        "function reflectedFunction() { return 1; }"
        "class ReflectedClass { public function work() { return 2; } }"
        "$object = new ReflectedClass();"
        "echo (define('DYNAMIC_VALUE', 7) ? 1 : 0), ':',"
        " (defined('DYNAMIC_VALUE') ? 1 : 0), ':', constant('DYNAMIC_VALUE'), ':',"
        " (function_exists('strlen') ? 1 : 0),"
        " (function_exists('reflectedFunction') ? 1 : 0), ':',"
        " (class_exists('ReflectedClass') ? 1 : 0), ':',"
        " (method_exists($object, 'work') ? 1 : 0),"
        " (method_exists('ReflectedClass', 'missing') ? 1 : 0), ':',"
        " get_class($object);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("1:1:7:11:1:10:ReflectedClass", output.bytes);
}

TEST(formatting_builtins_cover_width_precision_bases_and_print_r) {
    const char *source =
        "$formatted = sprintf("
        " '%05d|%-4s|%.2f|%x|%X|%o|%b|%c|%e|%g|%%|%u',"
        " -12, 'xy', 1.236, 255, 255, 8, 10, 65, 12.5, 12.5, -1);"
        "echo $formatted, ':';"
        "$length = printf('[%04d]', 7); echo ':', $length, ':';"
        "$printed = print_r(['x' => 1, 2], true);"
        "echo str_replace(\"\\n\", '|', $printed), ':',"
        " sprintf('%b|%x', -1, -1);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("-0012|xy  |1.24|ff|FF|10|1010|A|1.250000e+1|12.5|%|4294967295:[0007]:6:Array|(|    [x] => 1|    [0] => 2|)|:11111111111111111111111111111111|ffffffff",
               output.bytes);
}

TEST(json_builtins_round_trip_ordered_arrays_and_pretty_output) {
    const char *source =
        "$json = json_encode(["
        " 'name' => 'pico', 'values' => [1, true, null], 'slash' => 'a/b'"
        "]);"
        "$decoded = json_decode($json);"
        "$numbers = json_decode('[-12,1.25,2e3,2147483648,1e999999999999999999]');"
        "$pretty = json_encode(['a' => 1, 'b' => [true, null]],"
        " JSON_PRETTY_PRINT);"
        "echo $json, ':', $decoded['name'], ':', $decoded['values'][1], ':',"
        " str_replace(\"\\n\", '|', $pretty), ':',"
        " (json_decode('{bad') === null ? 1 : 0), ':',"
        " $numbers[0], ':', $numbers[1], ':', $numbers[2], ':',"
        " (is_float($numbers[3]) ? 1 : 0), ':',"
        " (is_float($numbers[4]) ? 1 : 0);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("{\"name\":\"pico\",\"values\":[1,true,null],\"slash\":\"a\\/b\"}:pico:1:{|    \"a\": 1,|    \"b\": [|        true,|        null|    ]|}:1:-12:1.25:2000:1:1",
               output.bytes);
}

TEST(time_random_and_system_builtins_use_portable_runtime_services) {
    const char *source =
        "srand(1234); $a = rand(10, 20); srand(1234); $b = mt_rand(10, 20);"
        "echo ($a === $b ? 1 : 0), ':';"
        "echo date('Y-m-d H:i:s D N w', 0), ':';"
        "$hr = hrtime(); echo count($hr), ':', (microtime() > 0 ? 1 : 0), ':';"
        "echo gc_collect_cycles(), ':'; exit('done'); echo 'unreachable';";
    output_buffer output;
    pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK, pphp_exec_source_mode(state, source, strlen(source),
                                            "test", 1));
    ASSERT_STR("1:1970-01-01 00:00:00 Thu 4 4:2:1:0:done", output.bytes);
    ASSERT_TRUE(pphp_exit_requested(state));
    ASSERT_EQ(0, pphp_exit_status(state));
    pphp_close(state);
}

TEST(array_mutators_callbacks_and_sorts_preserve_cow_semantics) {
    const char *source =
        "$original = [3, 1, 2]; $work = $original; sort($work);"
        "echo implode(',', $original), ':', implode(',', $work), ':';"
        "echo array_push($work, 4, 5), ':', array_pop($work), ':',"
        " array_shift($work), ':', array_unshift($work, 9), ':',"
        " implode(',', $work), ':';"
        "$mapped = array_map(fn($v) => $v * 2, $work);"
        "$filtered = array_filter($mapped, fn($v) => $v % 4 === 0);"
        "echo implode(',', $mapped), ':', implode(',', $filtered), ':';"
        "function total($carry, $value) { return $carry + $value; }"
        "echo array_reduce($work, 'TOTAL', 0), ':';"
        "$descending = [1, 3, 2];"
        "usort($descending, fn($left, $right) => $right <=> $left);"
        "echo implode(',', $descending);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("3,1,2:1,2,3:5:5:1:4:9,2,3,4:18,4,6,8:4,8:18:3,2,1",
               output.bytes);
}

TEST(str_replace_accepts_array_search_replacement_and_subject_values) {
    const char *source =
        "$result = str_replace(['red', 'green', 'blue'],"
        " ['R', 'G'], ['first' => 'red green blue', 'blue-red']);"
        "echo $result['first'], ':', $result[0], ':',"
        " str_replace(['a', 'b'], '-', 'abc');";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("R G :-R:--c", output.bytes);
}

TEST(static_class_members_promotion_and_magic_methods_execute) {
    const char *source =
        "class BaseStatic {"
        " const VALUE = 4; public static $count = 1;"
        " public static function add($value) {"
        "  self::$count = self::$count + $value; return self::$count;"
        " }"
        " public function value() { return self::VALUE; }"
        "}"
        "class ChildStatic extends BaseStatic {"
        " public static function name() { return static::class; }"
        " public function inherited() { return parent::value(); }"
        "}"
        "class PromotedMagic {"
        " public function __construct(private $x, public readonly int $y = 2) {}"
        " public function __get($name) { return 'get:' . $name; }"
        " public function __set($name, $value) { $this->x = $value; }"
        " public function __toString() { return 'value=' . $this->x; }"
        " public function __destruct() { echo ':destroyed'; }"
        "}"
        "echo BaseStatic::VALUE, ':', BaseStatic::$count, ':',"
        " BaseStatic::add(2), ':', ChildStatic::add(3), ':',"
        " ChildStatic::name(), ':', (new ChildStatic())->inherited(), ':';"
        "$magic = new PromotedMagic(5); echo $magic->missing, ':', $magic->y, ':';"
        "$magic->dynamic = 8; echo $magic, ':', ('x=' . $magic); unset($magic);";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("4:1:3:6:ChildStatic:4:get:missing:2:value=8:x=value=8:destroyed",
               output.bytes);
}

TEST(interfaces_and_abstract_final_contracts_are_enforced) {
    const char *valid =
        "interface Runnable { public function run(); }"
        "interface Named extends Runnable { public function name(); }"
        "abstract class BaseTask implements Named {"
        " public function name() { return 'task'; }"
        "}"
        "final class Task extends BaseTask {"
        " public function run() { return 9; }"
        "}"
        "$task = new Task(); echo $task->name(), ':', $task->run(), ':',"
        " ($task instanceof Runnable), ':', ($task instanceof Named), ':',"
        " ($task instanceof BaseTask);";
    output_buffer output;
    char error[256];
    ASSERT_EQ(PPHP_OK, execute(valid, &output, error, sizeof(error)));
    ASSERT_STR("task:9:1:1:1", output.bytes);
    ASSERT_EQ(PPHP_E_RUNTIME,
              execute("interface I { function required(); } class Bad implements I {}",
                      &output, error, sizeof(error)));
    ASSERT_TRUE(strstr(error, "must implement method required") != NULL);
    ASSERT_EQ(PPHP_E_RUNTIME,
              execute("class A { final function f() {} } class B extends A { function f() {} }",
                      &output, error, sizeof(error)));
    ASSERT_TRUE(strstr(error, "cannot define method") != NULL);
}

TEST(static_property_visibility_is_enforced) {
    const char *source =
        "class StaticBase {"
        " protected static $value = 3; private static $secret = 4;"
        " public static function secret() { return self::$secret; }"
        "}"
        "class StaticChild extends StaticBase {"
        " public static function read() { return self::$value; }"
        "}"
        "echo StaticBase::secret(), ':', StaticChild::read(), ':';"
        "try { echo StaticBase::$secret; } catch (Error) { echo 'private'; }";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("4:3:private", output.bytes);

    source =
        "class StaticCase {"
        " public static $value = 1; public static $Value = 2;"
        "}"
        "echo StaticCase::$value, StaticCase::$Value;";
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("12", output.bytes);
}

TEST(cycle_collector_reclaims_self_and_mutual_object_cycles) {
    const char *source =
        "class Node { public $next; }"
        "$self = new Node(); $self->next = $self; unset($self);"
        "echo gc_collect_cycles(), ':';"
        "$left = new Node(); $right = new Node();"
        "$left->next = $right; $right->next = $left;"
        "unset($left); unset($right); echo gc_collect_cycles(), ':',"
        " gc_collect_cycles();";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("1:2:0", output.bytes);
}

TEST(cycle_collector_traverses_closure_captures) {
    const char *source =
        "class Holder { public $callback; }"
        "$holder = new Holder();"
        "$holder->callback = function () use ($holder) { return $holder; };"
        "unset($holder); echo gc_collect_cycles(), ':', gc_collect_cycles();";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("2:0", output.bytes);
}

TEST(file_builtins_cover_whole_file_stream_and_path_operations) {
    const char *source =
        "$path = 'build/host/php_pico_file_test.txt';"
        "$moved = 'build/host/php_pico_file_moved.txt';"
        "$dir = 'build/host/php_pico_dir_test';"
        "echo file_put_contents($path, 'abc'), ':',"
        " file_put_contents($path, 'd', FILE_APPEND), ':',"
        " filesize($path), ':', file_get_contents($path), ':';"
        "$file = fopen($path, 'r');"
        "echo fread($file, 2), ':', ftell($file), ':',"
        " fseek($file, 0, SEEK_SET), ':', fgets($file), ':',"
        " fread($file, 1), ':', feof($file), ':', fclose($file), ':';"
        "echo rename($path, $moved), ':', file_exists($moved), ':',"
        " unlink($moved), ':', mkdir($dir), ':',"
        " implode(',', scandir($dir)), ':', rmdir($dir), ':',"
        " function_exists('file_get_contents');";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("3:1:4:abcd:ab:2:0:abcd::1:1:1:1:1:1:.,..:1:1",
               output.bytes);
}

TEST(include_once_loads_definitions_and_require_reports_missing_files) {
    const char *path = "build/host/php_pico_include_test.php";
    const char *searched_path = "build/host/php_pico_include_search.php";
    const char *included =
        "<?php function included_twice($value) { return $value * 2; }"
        "class IncludedValue { public function get() { return 7; } }"
        "$included_global = 5; echo 'I';";
    const char *searched_source = "<?php echo 'S';";
    const char *source =
        "include_once 'build/host/php_pico_include_test.php';"
        "include_once './build/host/php_pico_include_test.php';"
        "include 'build/host/php_pico_include_search';"
        "$object = new IncludedValue();"
        "echo ':', included_twice($included_global), ':', $object->get();";
    FILE *file = fopen(path, "wb");
    FILE *searched;
    output_buffer output;
    char error[256];
    ASSERT_TRUE(file != NULL);
    ASSERT_EQ((long)strlen(included),
              (long)fwrite(included, 1U, strlen(included), file));
    ASSERT_EQ(0, fclose(file));
    searched = fopen(searched_path, "wb");
    ASSERT_TRUE(searched != NULL);
    ASSERT_EQ((long)strlen(searched_source),
              (long)fwrite(searched_source, 1U, strlen(searched_source),
                           searched));
    ASSERT_EQ(0, fclose(searched));
    ASSERT_EQ(PPHP_OK, execute(source, &output, error, sizeof(error)));
    ASSERT_STR("IS:10:7", output.bytes);
    ASSERT_EQ(0, remove(path));
    ASSERT_EQ(0, remove(searched_path));
    ASSERT_EQ(PPHP_E_RUNTIME,
              execute("require 'build/host/does_not_exist.php';", &output,
                      error, sizeof(error)));
    ASSERT_TRUE(strstr(error, "required file") != NULL);
}

TEST(public_c_api_registers_functions_classes_methods_and_native_data) {
    const char *source =
        "<?php $box = native_make(); echo native_add(2, 3), ':',"
        " $box->increment(2), ':', NativeBox::label(), ':',"
        " NativeBox::VALUE, ':', function_exists('native_add'), ':';"
        "try { $box->fail(); } catch (RuntimeException $error) {"
        " echo $error->getMessage(); } unset($box);";
    pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
    output_buffer output;
    int result;
    ASSERT_TRUE(state != NULL);
    native_finalized = 0;
    native_test_class = pphp_def_class(state, "NativeBox", NULL);
    ASSERT_TRUE(native_test_class != NULL);
    pphp_def_func(state, "native_add", native_add, 2, 2);
    pphp_def_func(state, "native_make", native_make, 0, 0);
    pphp_def_method(native_test_class, "increment", native_increment,
                    PPHP_PUBLIC);
    pphp_def_method(native_test_class, "label", native_label,
                    PPHP_PUBLIC | PPHP_STATIC);
    pphp_def_method(native_test_class, "fail", native_fail, PPHP_PUBLIC);
    pphp_def_cconst_int(native_test_class, "VALUE", 7);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    result = pphp_exec_source(state, source, strlen(source), "native-api");
    ASSERT_EQ(PPHP_OK, result);
    ASSERT_STR("5:42:native:7:1:native failure", output.bytes);
    ASSERT_EQ(1, native_finalized);
    pphp_close(state);
}

TEST(peripheral_gems_are_registered_and_route_operations_through_hal) {
    const char *source =
        "echo class_exists('GPIO'), class_exists('ADC'), class_exists('PWM'),"
        " class_exists('I2C'), class_exists('SPI'), class_exists('UART'),"
        " class_exists('Machine'), class_exists('Watchdog'), class_exists('RTC'),"
        " ':', GPIO::OUT, ':', method_exists('I2C', 'scan'), ':',"
        " strlen(Machine::unique_id()), ':';"
        "RTC::set(100); echo RTC::now(), ':'; Machine::sleep_ms(0);"
        "$gpio = new GPIO(25, GPIO::OUT); $gpio->high();"
        "echo $gpio->read(), ':', (new ADC(1))->read_u16(), ':',"
        " (new SPI(0, 2, 3, 4))->transfer('xy'), ':';"
        "try { (new I2C(0, 4, 5))->read(64, 1); }"
        "catch (RuntimeException $error) { echo 'hal'; }";
    output_buffer output;
    ASSERT_EQ(PPHP_OK, execute(source, &output, NULL, 0U));
    ASSERT_STR("111111111:2:1:16:100:1:257:xy:hal", output.bytes);
}

TEST(gpio_events_dispatch_callbacks_only_at_vm_safe_points) {
    pphp_state *state = pphp_open(vm_pool, sizeof(vm_pool));
    output_buffer output;
    ASSERT_TRUE(state != NULL);
    memset(&output, 0, sizeof(output));
    pphp_set_output(state, capture_output, &output);
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source_mode(
                  state,
                  "$events = 0; $pin = new GPIO(6, GPIO::IN);"
                  "$pin->irq(function ($value) { global $events;"
                  " $events = $events + $value; }, GPIO::EDGE_RISE);",
                  strlen("$events = 0; $pin = new GPIO(6, GPIO::IN);"
                         "$pin->irq(function ($value) { global $events;"
                         " $events = $events + $value; }, GPIO::EDGE_RISE);"),
                  "gpio-event-setup", 1));
    ASSERT_EQ(PPHP_HAL_OK, hal_event_push(1U, 6U, 3U));
    pphp_tick(state);
    memset(&output, 0, sizeof(output));
    ASSERT_EQ(PPHP_OK,
              pphp_exec_source_mode(state, "echo $events;", 13U,
                                    "gpio-event-check", 1));
    ASSERT_STR("3", output.bytes);
    pphp_close(state);
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
        {"PBC string XIP", pbc_string_constants_are_zero_copy_image_views},
        {"PBC binary strings", pbc_string_records_are_binary_safe_and_nul_terminated},
        {"PBC string consumers", pbc_strings_work_across_runtime_consumers},
        {"PBC writer bounds", pbc_writer_rejects_oversized_record_counts},
        {"PBC retained lifetimes", pbc_modules_keep_classes_closures_and_owned_backings_alive},
        {"PBC sequential states", pbc_xip_images_can_be_used_by_sequential_states},
        {"source retained lifetimes", source_modules_outlive_objects_kept_in_globals},
        {"shutdown class lifetimes", shutdown_destructors_run_before_class_metadata_is_released},
        {"transient PBC lifetimes", transient_pbc_modules_are_released_after_success_and_failure},
        {"closure frame owners", closure_frames_retain_last_callable_owners},
        {"closure frame scope", closure_frames_retain_called_scope},
        {"class graph reachability", class_reachability_sweeps_dead_graphs_beside_live_graphs},
        {"PBC malformed bounds", pbc_loader_rejects_truncated_and_wrapping_sections},
#if PPHP_TYPECHECK
        {"PBC declared type corruption", pbc_loader_rejects_corrupt_declared_type_metadata},
        {"PBC type context OOM", pbc_type_context_reports_allocation_failure},
        {"failed definition rollback", failed_forward_variance_rolls_back_definitions},
#endif
        {"array COW runtime", arrays_use_copy_on_write_and_normalized_keys},
        {"language conformance", language_conformance_covers_casts_lvalues_nullsafe_and_interpolation},
        {"member lvalues", member_lvalues_and_dynamic_names_preserve_cow_and_evaluation_order},
        {"conditional functions", conditional_functions_register_only_when_their_declaration_executes},
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
        {"object clone", clone_copies_property_slots_and_invokes_clone_hook},
        {"type builtins", type_conversion_and_predicate_builtins_follow_php_values},
        {"math builtins", math_builtins_cover_integer_float_and_collection_forms},
        {"string builtins", string_builtins_cover_search_transform_split_and_join},
        {"array builtins", array_builtins_preserve_php_key_and_order_rules},
        {"default and variadic parameters", default_and_variadic_parameters_work_for_all_callable_forms},
        {"array and argument unpacking", array_and_argument_unpacking_preserve_evaluation_order},
        {"persistent language bindings", global_static_and_named_constants_use_persistent_storage},
        {"REPL global persistence", repl_chunks_retain_globals_and_constants_by_name},
        {"REPL definition persistence", repl_chunks_retain_functions_classes_and_closures},
        {"constant and reflection builtins", constant_and_reflection_builtins_share_runtime_registries},
        {"formatting builtins", formatting_builtins_cover_width_precision_bases_and_print_r},
        {"JSON builtins", json_builtins_round_trip_ordered_arrays_and_pretty_output},
        {"time, random, and system builtins", time_random_and_system_builtins_use_portable_runtime_services},
        {"array mutation, callbacks, and sorts", array_mutators_callbacks_and_sorts_preserve_cow_semantics},
        {"array string replacement", str_replace_accepts_array_search_replacement_and_subject_values},
        {"static and magic class features", static_class_members_promotion_and_magic_methods_execute},
        {"interface, abstract, and final contracts", interfaces_and_abstract_final_contracts_are_enforced},
        {"static property visibility", static_property_visibility_is_enforced},
        {"object cycle collection", cycle_collector_reclaims_self_and_mutual_object_cycles},
        {"mixed container cycle collection", cycle_collector_traverses_closure_captures},
        {"file builtins", file_builtins_cover_whole_file_stream_and_path_operations},
        {"include and require", include_once_loads_definitions_and_require_reports_missing_files},
        {"public native extension API", public_c_api_registers_functions_classes_methods_and_native_data},
        {"peripheral gems", peripheral_gems_are_registered_and_route_operations_through_hal},
        {"GPIO event dispatch", gpio_events_dispatch_callbacks_only_at_vm_safe_points}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
