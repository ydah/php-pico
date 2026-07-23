#include "alloc.h"

#include <stdint.h>
#include <string.h>

#define ALIGNMENT 8U
#define ALIGN_MASK (ALIGNMENT - 1U)
#define BLOCK_USED 1U
#if PPHP_RC_DEBUG
/* Allocation sizes are 8-byte aligned, leaving bit 1 available without
 * increasing the per-block header used by release firmware. */
#define BLOCK_TRACKED 2U
#endif
#define BLOCK_SIZE_MASK (~(uint32_t)ALIGN_MASK)
#define FL_COUNT 24U
#define SL_COUNT 4U

typedef struct block_header {
    uint32_t size_flags;
    uint32_t prev_size;
} block_header;

typedef struct free_links {
    block_header *next;
    block_header *prev;
} free_links;

typedef struct pool_control {
    uint8_t *begin;
    uint8_t *end;
    size_t total;
    block_header *bins[FL_COUNT][SL_COUNT];
    int initialized;
} pool_control;

static pool_control pool;

static size_t align_up(size_t value) {
    return (value + ALIGN_MASK) & ~((size_t)ALIGN_MASK);
}

static size_t align_down(size_t value) {
    return value & ~((size_t)ALIGN_MASK);
}

static size_t block_size(const block_header *block) {
    return (size_t)(block->size_flags & BLOCK_SIZE_MASK);
}

static int block_is_used(const block_header *block) {
    return (block->size_flags & BLOCK_USED) != 0U;
}

static void block_set(block_header *block, size_t size, int used) {
    block->size_flags = (uint32_t)size | (used ? BLOCK_USED : 0U);
}

static free_links *links_of(block_header *block) {
    return (free_links *)((uint8_t *)block + sizeof(*block));
}

static block_header *next_block(block_header *block) {
    return (block_header *)((uint8_t *)block + block_size(block));
}

static unsigned floor_log2(size_t value) {
    unsigned result = 0U;
    while (value > 1U) {
        value >>= 1U;
        result++;
    }
    return result;
}

static void mapping(size_t size, unsigned *fl, unsigned *sl) {
    unsigned first = floor_log2(size);
    size_t base;
    size_t step;

    if (first >= FL_COUNT) {
        first = FL_COUNT - 1U;
    }
    base = (size_t)1U << first;
    step = base / SL_COUNT;
    if (step == 0U) {
        step = 1U;
    }
    *fl = first;
    *sl = (unsigned)((size > base ? size - base : 0U) / step);
    if (*sl >= SL_COUNT) {
        *sl = SL_COUNT - 1U;
    }
}

static void remove_free(block_header *block) {
    unsigned fl;
    unsigned sl;
    free_links *links = links_of(block);

    mapping(block_size(block), &fl, &sl);
    if (links->prev != NULL) {
        links_of(links->prev)->next = links->next;
    } else {
        pool.bins[fl][sl] = links->next;
    }
    if (links->next != NULL) {
        links_of(links->next)->prev = links->prev;
    }
    links->next = NULL;
    links->prev = NULL;
}

static void insert_free(block_header *block) {
    unsigned fl;
    unsigned sl;
    free_links *links = links_of(block);

    mapping(block_size(block), &fl, &sl);
    links->prev = NULL;
    links->next = pool.bins[fl][sl];
    if (links->next != NULL) {
        links_of(links->next)->prev = block;
    }
    pool.bins[fl][sl] = block;
}

static block_header *find_free(size_t needed) {
    unsigned fl;
    unsigned sl;
    unsigned i;
    unsigned j;

    mapping(needed, &fl, &sl);
    for (i = fl; i < FL_COUNT; i++) {
        unsigned start = i == fl ? sl : 0U;
        for (j = start; j < SL_COUNT; j++) {
            block_header *block = pool.bins[i][j];
            while (block != NULL) {
                if (block_size(block) >= needed) {
                    return block;
                }
                block = links_of(block)->next;
            }
        }
    }
    return NULL;
}

static size_t minimum_block_size(void) {
    return align_up(sizeof(block_header) + sizeof(free_links));
}

void pphp_pool_init(void *buffer, size_t size) {
    uintptr_t raw = (uintptr_t)buffer;
    uintptr_t aligned = (raw + ALIGN_MASK) & ~((uintptr_t)ALIGN_MASK);
    size_t skipped = (size_t)(aligned - raw);
    size_t available;
    block_header *first;
    block_header *sentinel;

    memset(&pool, 0, sizeof(pool));
    if (buffer == NULL || size <= skipped + minimum_block_size() + sizeof(block_header)) {
        return;
    }
    available = align_down(size - skipped);
    first = (block_header *)aligned;
    block_set(first, available - sizeof(block_header), 0);
    first->prev_size = 0U;
    sentinel = next_block(first);
    block_set(sentinel, 0U, 1);
    sentinel->prev_size = (uint32_t)block_size(first);
    pool.begin = (uint8_t *)first;
    pool.end = (uint8_t *)sentinel + sizeof(*sentinel);
    pool.total = block_size(first) - sizeof(block_header);
    pool.initialized = 1;
    insert_free(first);
}

void *pphp_alloc(size_t size) {
    size_t needed;
    size_t remainder;
    block_header *block;
    block_header *following;

    if (!pool.initialized || size == 0U || size > UINT32_MAX - sizeof(block_header)) {
        return NULL;
    }
    needed = align_up(size + sizeof(block_header));
    if (needed < minimum_block_size()) {
        needed = minimum_block_size();
    }
    block = find_free(needed);
    if (block == NULL) {
        return NULL;
    }
    remove_free(block);
    remainder = block_size(block) - needed;
    if (remainder >= minimum_block_size()) {
        block_header *split = (block_header *)((uint8_t *)block + needed);
        block_set(split, remainder, 0);
        split->prev_size = (uint32_t)needed;
        block_set(block, needed, 1);
        following = next_block(split);
        following->prev_size = (uint32_t)remainder;
        insert_free(split);
    } else {
        block_set(block, block_size(block), 1);
        following = next_block(block);
        following->prev_size = (uint32_t)block_size(block);
    }
    return (uint8_t *)block + sizeof(*block);
}

void pphp_free(void *ptr) {
    block_header *block;
    block_header *following;

    if (ptr == NULL || !pool.initialized) {
        return;
    }
    block = (block_header *)((uint8_t *)ptr - sizeof(block_header));
    if ((uint8_t *)block < pool.begin || (uint8_t *)block >= pool.end || !block_is_used(block)) {
        return;
    }
    block_set(block, block_size(block), 0);
    following = next_block(block);
    if (block_size(following) != 0U && !block_is_used(following)) {
        size_t merged;
        remove_free(following);
        merged = block_size(block) + block_size(following);
        block_set(block, merged, 0);
        next_block(block)->prev_size = (uint32_t)merged;
    }
    if (block->prev_size != 0U) {
        block_header *previous = (block_header *)((uint8_t *)block - block->prev_size);
        if (!block_is_used(previous)) {
            size_t merged;
            remove_free(previous);
            merged = block_size(previous) + block_size(block);
            block_set(previous, merged, 0);
            next_block(previous)->prev_size = (uint32_t)merged;
            block = previous;
        }
    }
    insert_free(block);
}

void *pphp_realloc(void *ptr, size_t size) {
    block_header *block;
    size_t old_size;
    void *replacement;
#if PPHP_RC_DEBUG
    int tracked;
#endif

    if (ptr == NULL) {
        return pphp_alloc(size);
    }
    if (size == 0U) {
        pphp_free(ptr);
        return NULL;
    }
    block = (block_header *)((uint8_t *)ptr - sizeof(block_header));
    old_size = block_size(block) - sizeof(*block);
    if (size <= old_size) {
        return ptr;
    }
#if PPHP_RC_DEBUG
    tracked = (block->size_flags & BLOCK_TRACKED) != 0U;
#endif
    replacement = pphp_alloc(size);
    if (replacement == NULL) {
        return NULL;
    }
    memcpy(replacement, ptr, old_size);
#if PPHP_RC_DEBUG
    if (tracked) pphp_alloc_track(replacement);
#endif
    pphp_free(ptr);
    return replacement;
}

#if PPHP_RC_DEBUG
void pphp_alloc_track(void *ptr) {
    block_header *block;
    if (ptr == NULL || !pool.initialized) return;
    block = (block_header *)((uint8_t *)ptr - sizeof(*block));
    if ((uint8_t *)block < pool.begin || (uint8_t *)block >= pool.end ||
        !block_is_used(block) || block_size(block) == 0U) return;
    block->size_flags |= BLOCK_TRACKED;
}

int pphp_alloc_visit_tracked(pphp_tracked_visit_fn visit, void *context) {
    block_header *block;
    if (!pool.initialized || visit == NULL) return 0;
    block = (block_header *)pool.begin;
    while (block_size(block) != 0U) {
        if (block_is_used(block) &&
            (block->size_flags & BLOCK_TRACKED) != 0U &&
            !visit((pheader *)((uint8_t *)block + sizeof(*block)), context)) {
            return 0;
        }
        block = next_block(block);
    }
    return 1;
}
#endif

pphp_pool_stats pphp_pool_get_stats(void) {
    pphp_pool_stats stats = {0U, 0U, 0U, 0U, 0U};
    block_header *block;

    if (!pool.initialized) {
        return stats;
    }
    stats.total = pool.total;
    block = (block_header *)pool.begin;
    while (block_size(block) != 0U) {
        size_t payload = block_size(block) - sizeof(*block);
        if (block_is_used(block)) {
            stats.used += payload;
        } else {
            stats.free += payload;
            stats.fragments++;
            if (payload > stats.largest_free) {
                stats.largest_free = payload;
            }
        }
        block = next_block(block);
    }
    return stats;
}

int pphp_pool_check(void) {
    block_header *block;
    uint32_t previous_size = 0U;

    if (!pool.initialized) {
        return 0;
    }
    block = (block_header *)pool.begin;
    while ((uint8_t *)block < pool.end) {
        if (block->prev_size != previous_size) {
            return 0;
        }
        if (block_size(block) == 0U) {
            return block_is_used(block);
        }
        if ((block_size(block) & ALIGN_MASK) != 0U || block_size(block) < minimum_block_size()) {
            return 0;
        }
        previous_size = (uint32_t)block_size(block);
        block = next_block(block);
    }
    return 0;
}
