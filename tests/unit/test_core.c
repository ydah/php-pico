#include "test.h"

#include "alloc.h"
#include "pstring.h"
#include "parray.h"
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

TEST(arrays_preserve_order_and_normalize_keys) {
    parray *array;
    pstring *numeric;
    pstring *leading_zero;
    pvalue value;
    pvalue key;
    size_t position = 0U;
    size_t next;
    pphp_pool_init(memory_pool, sizeof(memory_pool));
    array = pa_new(0U);
    numeric = ps_new_cstr("1");
    leading_zero = ps_new_cstr("01");
    ASSERT_TRUE(array != NULL && numeric != NULL && leading_zero != NULL);
    ASSERT_TRUE(pa_push(array, pv_int(10)));
    ASSERT_TRUE(pa_set(array, pv_heap(PT_STRING, &numeric->header), pv_int(20)));
    ASSERT_TRUE(pa_set(array, pv_heap(PT_STRING, &leading_zero->header), pv_int(30)));
    ASSERT_EQ(3, pa_count(array));
    ASSERT_TRUE(pa_get(array, pv_int(1), &value));
    ASSERT_EQ(20, value.as.i);
    pv_release(value);
    ASSERT_TRUE(pa_get(array, pv_heap(PT_STRING, &leading_zero->header), &value));
    ASSERT_EQ(30, value.as.i);
    pv_release(value);
    ASSERT_TRUE(pa_entry_at(array, position, &key, &value, &next));
    ASSERT_EQ(0, key.as.i);
    ASSERT_EQ(10, value.as.i);
    position = next;
    pv_release(key);
    pv_release(value);
    ASSERT_TRUE(pa_entry_at(array, position, &key, &value, &next));
    ASSERT_EQ(1, key.as.i);
    ASSERT_EQ(20, value.as.i);
    pv_release(key);
    pv_release(value);
    ps_destroy(numeric);
    ps_destroy(leading_zero);
    pv_release(pv_heap(PT_ARRAY, &array->header));
}

TEST(array_clone_supports_copy_on_write) {
    parray *original;
    parray *copy;
    pvalue value;
    pphp_pool_init(memory_pool, sizeof(memory_pool));
    original = pa_new(0U);
    ASSERT_TRUE(original != NULL);
    ASSERT_TRUE(pa_push(original, pv_int(1)));
    ASSERT_TRUE(pa_push(original, pv_int(2)));
    copy = pa_clone(original);
    ASSERT_TRUE(copy != NULL);
    ASSERT_TRUE(pa_set(copy, pv_int(0), pv_int(99)));
    ASSERT_TRUE(pa_push(copy, pv_int(3)));
    ASSERT_TRUE(pa_get(original, pv_int(0), &value));
    ASSERT_EQ(1, value.as.i);
    pv_release(value);
    ASSERT_EQ(2, pa_count(original));
    ASSERT_EQ(3, pa_count(copy));
    pv_release(pv_heap(PT_ARRAY, &original->header));
    pv_release(pv_heap(PT_ARRAY, &copy->header));
    ASSERT_EQ(0, pphp_pool_get_stats().used);
}

TEST(array_random_updates_remain_consistent) {
    parray *array;
    int present[101] = {0};
    int values[101] = {0};
    uint32_t random = UINT32_C(0xa341316c);
    int i;
    pphp_pool_init(memory_pool, sizeof(memory_pool));
    array = pa_new(0U);
    ASSERT_TRUE(array != NULL);
    for (i = 0; i < 10000; i++) {
        int key;
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        key = (int)((random >> 16U) % 101U);
        if ((random & 3U) == 0U) {
            (void)pa_remove(array, pv_int((pphp_int)(key - 50)));
            present[key] = 0;
        } else {
            int stored = (int)(random & 0x7fffU);
            ASSERT_TRUE(pa_set(array, pv_int((pphp_int)(key - 50)), pv_int(stored)));
            present[key] = 1;
            values[key] = stored;
        }
        if ((i % 100) == 0) {
            int probe;
            for (probe = 0; probe < 101; probe++) {
                pvalue found;
                int exists = pa_get(array, pv_int((pphp_int)(probe - 50)), &found);
                ASSERT_EQ(present[probe], exists);
                if (exists) {
                    ASSERT_EQ(values[probe], found.as.i);
                    pv_release(found);
                }
            }
        }
    }
    pv_release(pv_heap(PT_ARRAY, &array->header));
    ASSERT_EQ(0, pphp_pool_get_stats().used);
}

int main(void) {
    static const test_case tests[] = {
        {"allocator aligns and coalesces", allocator_allocates_aligned_memory},
        {"allocator random operations", allocator_survives_random_operations},
        {"PHP truthiness", values_follow_php_truthiness},
        {"string hashing", strings_hash_and_compare_by_content},
        {"symbol interning", symbols_are_interned_and_table_grows},
        {"array keys and order", arrays_preserve_order_and_normalize_keys},
        {"array COW clone", array_clone_supports_copy_on_write},
        {"array random updates", array_random_updates_remain_consistent}
    };
    return run_tests(tests, sizeof(tests) / sizeof(tests[0]));
}
