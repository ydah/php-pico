#include "test.h"

#include "alloc.h"
#include "pstring.h"
#include "symbol.h"
#include "value.h"

#include <stdint.h>

static uint8_t memory_pool[256U * 1024U];

TEST(allocator_allocates_aligned_memory) {
    void *a;
    void *b;
    pphp_pool_init(memory_pool, sizeof(memory_pool));
    a = pphp_alloc(1U);
    b = pphp_alloc(31U);
    ASSERT_TRUE(a != NULL);
    ASSERT_TRUE(b != NULL);
    ASSERT_EQ(0, (uintptr_t)a & 7U);
    ASSERT_EQ(0, (uintptr_t)b & 7U);
    ASSERT_TRUE(pphp_pool_check());
    pphp_free(a);
    pphp_free(b);
    ASSERT_EQ(1, pphp_pool_get_stats().fragments);
}

TEST(allocator_survives_random_operations) {
    enum { SLOT_COUNT = 256, ITERATIONS = 100000 };
    void *slots[SLOT_COUNT] = {0};
    size_t sizes[SLOT_COUNT] = {0U};
    uint32_t random = UINT32_C(0x12345678);
    int i;
    pphp_pool_init(memory_pool, sizeof(memory_pool));
    for (i = 0; i < ITERATIONS; i++) {
        size_t slot;
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        slot = (random >> 16U) % SLOT_COUNT;
        if (slots[slot] != NULL) {
            pphp_free(slots[slot]);
            slots[slot] = NULL;
            sizes[slot] = 0U;
        } else {
            size_t size = (random & 511U) + 1U;
            slots[slot] = pphp_alloc(size);
            if (slots[slot] != NULL) {
                sizes[slot] = size;
                ((uint8_t *)slots[slot])[0] = (uint8_t)slot;
                ((uint8_t *)slots[slot])[size - 1U] = (uint8_t)(slot ^ 0xffU);
            }
        }
        if ((i % 1000) == 0) {
            ASSERT_TRUE(pphp_pool_check());
        }
    }
    for (i = 0; i < SLOT_COUNT; i++) {
        if (slots[i] != NULL) {
            ASSERT_EQ((uint8_t)i, ((uint8_t *)slots[i])[0]);
            ASSERT_EQ((uint8_t)((unsigned)i ^ 0xffU),
                      ((uint8_t *)slots[i])[sizes[i] - 1U]);
            pphp_free(slots[i]);
        }
    }
    ASSERT_TRUE(pphp_pool_check());
    ASSERT_EQ(1, pphp_pool_get_stats().fragments);
    ASSERT_EQ(0, pphp_pool_get_stats().used);
}

TEST(values_follow_php_truthiness) {
    pstring *empty;
    pstring *zero;
    pstring *zero_float;
    pphp_pool_init(memory_pool, sizeof(memory_pool));
    empty = ps_new_cstr("");
    zero = ps_new_cstr("0");
    zero_float = ps_new_cstr("0.0");
    ASSERT_TRUE(!pv_is_truthy(pv_null()));
    ASSERT_TRUE(!pv_is_truthy(pv_bool(0)));
    ASSERT_TRUE(!pv_is_truthy(pv_int(0)));
    ASSERT_TRUE(!pv_is_truthy(pv_heap(PT_STRING, &empty->header)));
    ASSERT_TRUE(!pv_is_truthy(pv_heap(PT_STRING, &zero->header)));
    ASSERT_TRUE(pv_is_truthy(pv_heap(PT_STRING, &zero_float->header)));
    ps_destroy(empty);
    ps_destroy(zero);
    ps_destroy(zero_float);
}

TEST(strings_hash_and_compare_by_content) {
    pstring *left;
    pstring *right;
    pstring *different;
    pphp_pool_init(memory_pool, sizeof(memory_pool));
    left = ps_new_cstr("php-pico");
    right = ps_new_cstr("php-pico");
    different = ps_new_cstr("PHP-PICO");
    ASSERT_TRUE(left != NULL && right != NULL && different != NULL);
    ASSERT_TRUE(ps_equal(left, right));
    ASSERT_TRUE(!ps_equal(left, different));
    ASSERT_EQ(left->hash, right->hash);
    ps_destroy(left);
    ps_destroy(right);
    ps_destroy(different);
}

TEST(symbols_are_interned_and_table_grows) {
    psymbol_table table;
    pstring *first;
    char name[24];
    int i;
    pphp_pool_init(memory_pool, sizeof(memory_pool));
    ASSERT_TRUE(psymbol_init(&table, 8U));
    first = psymbol_intern(&table, "Example", 7U);
    ASSERT_TRUE(first != NULL);
    ASSERT_TRUE(first == psymbol_intern(&table, "Example", 7U));
    for (i = 0; i < 200; i++) {
        int length = snprintf(name, sizeof(name), "symbol_%d", i);
        ASSERT_TRUE(length > 0);
        ASSERT_TRUE(psymbol_intern(&table, name, (size_t)length) != NULL);
    }
    ASSERT_EQ(201, table.count);
    ASSERT_TRUE(psymbol_find(&table, "Example", 7U) == first);
    psymbol_destroy(&table);
    ASSERT_EQ(1, pphp_pool_get_stats().fragments);
}

int main(void) {
    static const test_case tests[] = {
        {"allocator aligns and coalesces", allocator_allocates_aligned_memory},
        {"allocator random operations", allocator_survives_random_operations},
        {"PHP truthiness", values_follow_php_truthiness},
        {"string hashing", strings_hash_and_compare_by_content},
        {"symbol interning", symbols_are_interned_and_table_grows}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
