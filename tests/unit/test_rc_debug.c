#include "pphp/pphp.h"

#include "codegen.h"
#include "parray.h"
#include "parser.h"
#include "pstring.h"
#include "resource.h"
#include "state.h"

#include <stdio.h>
#include <string.h>

static uint8_t memory_pool[PPHP_HEAP_SIZE];

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__,          \
                    __LINE__, #condition);                                     \
            return 0;                                                          \
        }                                                                      \
    } while (0)

static int rc_ok(pphp_state *state, size_t *checked) {
    pphp_rc_check_result result;
    CHECK(pphp_rc_check(state, &result));
    CHECK(result.status == PPHP_RC_CHECK_OK);
    CHECK(result.target == NULL);
    if (checked != NULL) *checked = result.checked;
    return 1;
}

static int set_global(pphp_state *state, const char *name, pvalue value) {
    pstring *key = ps_new_cstr(name);
    int ok;
    if (key == NULL) return 0;
    ok = pa_set(state->globals, pv_heap(PT_STRING, &key->header), value);
    pv_release(pv_heap(PT_STRING, &key->header));
    return ok;
}

static int install_cycle_and_iterator(pphp_state *state) {
    parray *cycle = pa_new(1U);
    parray *iterated = pa_new(2U);
    parray_iterator *iterator;
    pvalue cycle_value;
    pvalue iterated_value;
    pvalue iterator_value;
    CHECK(cycle != NULL && iterated != NULL);
    cycle_value = pv_heap(PT_ARRAY, &cycle->header);
    CHECK(pa_push(cycle, cycle_value));
    CHECK(set_global(state, "cycle", cycle_value));
    pv_release(cycle_value);

    iterated_value = pv_heap(PT_ARRAY, &iterated->header);
    CHECK(pa_push(iterated, pv_int(1)));
    CHECK(pa_push(iterated, pv_int(2)));
    iterator = pa_iterator_new(iterated);
    CHECK(iterator != NULL);
    iterator_value = pv_heap(PT_RESOURCE, &iterator->resource.header);
    CHECK(set_global(state, "iterator", iterator_value));
    pv_release(iterator_value);
    pv_release(iterated_value);
    return 1;
}

static int compile_pbc_image(const char *path, uint8_t **image,
                             size_t *image_length) {
    static const char source[] =
        "function from_pbc(string $value='xip'): string { return $value; }"
        " $pbc_value=from_pbc();";
    pc_arena arena;
    pc_parser parser;
    pc_codegen_error error;
    pc_ast *program;
    pmodule module;
    FILE *file;
    long length;
    size_t read_count;
    int status;
    pc_arena_init(&arena, 4096U);
    pc_parser_init(&parser, &arena, source, sizeof(source) - 1U, 0);
    program = pc_parse_program(&parser);
    CHECK(program != NULL);
    CHECK(pc_codegen_program(program, &module, &error));
    status = pphp_pbc_write_file(&module, path);
    pmodule_destroy(&module);
    pc_arena_destroy(&arena);
    CHECK(status == PPHP_OK);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    CHECK(fseek(file, 0L, SEEK_END) == 0);
    length = ftell(file);
    CHECK(length > 0L);
    CHECK(fseek(file, 0L, SEEK_SET) == 0);
    *image = pphp_alloc((size_t)length);
    CHECK(*image != NULL);
    read_count = fread(*image, 1U, (size_t)length, file);
    CHECK(fclose(file) == 0);
    CHECK(read_count == (size_t)length);
    *image_length = (size_t)length;
    return 1;
}

static int run_checks(void) {
    static const char source[] =
        "$base=['a'=>[1,2,3]];"
        "class RcBox { public string $value='v'; public static $shared=[4];"
        " public const LABEL='label';"
        " public function closure(){ return fn()=> $this->value; } }"
        "$box=new RcBox(); $closure=$box->closure();"
        "$file=fopen('/tmp/php-pico-rc-debug-resource.tmp','w+');"
        "for($i=0;$i<10000;$i++){ $copy=$base; $copy[]=$i; }";
    const char pbc_path[] = "/tmp/php-pico-rc-debug-test.pbc";
    pphp_state *state;
    pstring *probe;
    pvalue probe_value;
    pphp_rc_check_result mismatch;
    uint8_t *image = NULL;
    size_t image_length = 0U;
    size_t first_checked;
    size_t repeated_checked;
    size_t i;
    void *fillers[512];
    size_t filler_count = 0U;

    state = pphp_open(memory_pool, sizeof(memory_pool));
    CHECK(state != NULL);
    CHECK(rc_ok(state, &first_checked));
    CHECK(first_checked != 0U);
    CHECK(pphp_exec_source_mode(state, source, sizeof(source) - 1U,
                                "rc-debug", 1) == PPHP_OK);
    CHECK(install_cycle_and_iterator(state));

    probe = ps_new_cstr("probe");
    CHECK(probe != NULL);
    probe_value = pv_heap(PT_STRING, &probe->header);
    CHECK(set_global(state, "probe", probe_value));
    pv_release(probe_value);
    for (i = 0U; i < 10000U; i++) {
        pv_retain(probe_value);
        pv_release(probe_value);
    }
    CHECK(rc_ok(state, &repeated_checked));
    CHECK(repeated_checked > first_checked);
    CHECK(rc_ok(state, &first_checked));
    CHECK(first_checked == repeated_checked);

    probe->header.refcnt++;
    CHECK(!pphp_rc_check(state, &mismatch));
    CHECK(mismatch.status == PPHP_RC_CHECK_MISMATCH);
    CHECK(mismatch.target == &probe->header);
    CHECK((size_t)mismatch.actual == mismatch.expected + 1U);
    probe->header.refcnt--;
    CHECK(rc_ok(state, NULL));

    CHECK(compile_pbc_image(pbc_path, &image, &image_length));
    CHECK(pphp_exec_pbc(state, image, image_length) == PPHP_OK);
    CHECK(rc_ok(state, NULL));

    while (filler_count < sizeof(fillers) / sizeof(fillers[0])) {
        void *filler = pphp_alloc(256U);
        if (filler == NULL) break;
        fillers[filler_count++] = filler;
    }
    CHECK(filler_count != 0U);
    CHECK(!pphp_rc_check(state, &mismatch));
    CHECK(mismatch.status == PPHP_RC_CHECK_NOMEM);
    while (filler_count != 0U) pphp_free(fillers[--filler_count]);
    CHECK(rc_ok(state, NULL));

    pphp_close(state);
    pphp_free(image);
    (void)remove(pbc_path);
    (void)remove("/tmp/php-pico-rc-debug-resource.tmp");
    return 1;
}

int main(void) {
    if (!run_checks()) return 1;
    puts("rc debug tests passed");
    return 0;
}
