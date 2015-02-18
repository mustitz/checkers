#include "mu-data-struct.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_POOL_SIZE  64000



/*
 * Memory pool
 */

struct mempool * create_mempool(size_t initial_size)
{
    if (initial_size == 0) {
        initial_size = DEFAULT_POOL_SIZE;
    }

    size_t overhead = sizeof(struct mempool) + sizeof(struct mempool_item);
    size_t min_size = 8 * overhead;
    if (initial_size < min_size) {
        initial_size = min_size;
    }

    struct mempool * me = malloc(initial_size);
    if (me == NULL) {
        return NULL;
    }

    me->allocated = initial_size;
    me->used = 0;
    me->overhead = overhead;
    me->avail = initial_size - overhead;
    me->gap = 0;

    me->last = move_ptr(me, sizeof(struct mempool));
    me->last->base = me;
    me->last->current = move_ptr(me, overhead);
    me->last->avail = initial_size - overhead;
    me->last->next = NULL;

    return me;
};

static void recursive_free(struct mempool_item * restrict item)
{
    if (item->next) {
        recursive_free(item->next);
    }
    free(item->base);
}

void free_mempool(struct mempool * restrict me)
{
    recursive_free(me->last);
}

void * mempool_alloc(struct mempool * restrict me, size_t size)
{
    static size_t bounds[9] = { 1, 1, 2, 4, 4, 8, 8, 8, 8 };
    if (size > 8) {
        mempool_align(me, 16);
    } else {
        mempool_align(me, bounds[size]);
    }

    for (;;) {
        if (me->last->avail >= size) {
            struct mempool_item * last = me->last;
            void * result = last->current;
            last->current = move_ptr(last->current, size);
            last->avail -= size;
            me->used += size;
            me->avail = last->avail;
            return result;
        }

        me->gap += me->last->avail;
        
        size_t item_size = me->allocated;
        size_t min_size = 16 * sizeof(struct mempool_item);
        if (item_size < min_size) {
            item_size = min_size;
        }

        struct mempool_item * new_item = malloc(item_size);
        if (new_item == NULL) {
            return NULL;
        }
        me->allocated += item_size;

        new_item->base = new_item;
        new_item->avail = item_size - sizeof(struct mempool_item);
        new_item->current = move_ptr(new_item, sizeof(struct mempool_item));
        new_item->next = me->last;

        me->last = new_item;
        me->overhead += sub_ptr(new_item->current, new_item->base);
        me->avail = new_item->avail;
    }
}

void mempool_align(struct mempool * restrict me, size_t bound)
{
    size_t address = (size_t)me->last->current;
    size_t mod = address % bound;
    if (mod == 0) {
        return;
    }

    size_t filler = bound - mod;
    if (me->last->avail < filler) {
        me->gap += me->last->avail;
        me->last->avail = 0;
        return;
    }

    me->gap += filler;
    me->last->avail -= filler;
    me->last->current = move_ptr(me->last->current, filler);
}

void mempool_print_stats(const struct mempool * me)
{
    printf("  allocated = %lu\n", me->allocated);
    printf("  used = %lu\n", me->used);
    printf("  overhead = %lu\n", me->overhead);
    printf("  avail = %lu\n", me->avail);
    printf("  gap = %lu\n", me->gap);
}




/*
 * Hash table for strings
 */

struct strhash * create_strhash(struct mempool * pool, int mod)
{
    if (pool == NULL) {
        pool = create_mempool(0);
        if (pool == NULL) {
            return NULL;
        }
    }

    if (mod == 0) {
        mod = 1009;
    }

    size_t size = offsetof(struct strhash, items);
    size += sizeof(struct strhash_item_dlist) * mod;

    struct strhash * result = mempool_alloc(pool, size);
    if (result == NULL) {
        return NULL;
    }
    void * last = move_ptr(result, size);

    result->mod = mod;
    result->pool = pool;
    struct strhash_item_dlist * current = result->items;
    for (int i=0; i<mod; ++i) {
        void * ptr = current;
        if (ptr > last) {
            test_fail("Write after allocated region for struct strhash.");
        }
        CLEAR_DLIST(current);
        ++current;
    }

    return result;
}

void free_strhash(struct strhash * restrict me)
{
    free_mempool(me->pool);
}

struct strhash__bs
{
    struct strhash_item_dlist * base;
    size_t size;
};

static inline struct strhash__bs strhash__get_bs(struct strhash * restrict me, const char * name)
{
    int acc = 0;
    const char * ptr = name;
    for (; *ptr; ++ptr) {
        acc <<= 1;
        acc ^= (int)*ptr;
    }

    int hash = acc % me->mod;

    struct strhash__bs result;
    result.base = me->items + hash;
    result.size = sub_ptr(ptr, name);
    return result;
}

void strhash_set(struct strhash * restrict me, const char * name, union strhash_value value)
{
    struct strhash__bs bs = strhash__get_bs(me, name);

    struct strhash_item_dlist * current = bs.base->next;
    for (; current != bs.base; current = current->next) {
        struct strhash_item * item = get_strhash_item_from_dlist(current);
        if (strcmp(item->name, name) == 0) {
            item->value = value;
            return;
        }
    }

    char * restrict allocated_name = mempool_alloc(me->pool, bs.size+1);
    strcpy(allocated_name, name);

    struct strhash_item * item = mempool_alloc(me->pool, sizeof(struct strhash_item));
    item->name = allocated_name;
    item->value = value;
    INSERT_AFTER_IN_DLIST(bs.base, &item->dlist);
}

union strhash_value * strhash_get(struct strhash * restrict me, const char * name)
{
    struct strhash__bs bs = strhash__get_bs(me, name);
    
    struct strhash_item_dlist * current = bs.base->next;
    for (; current != bs.base; current = current->next) {
        struct strhash_item * item = get_strhash_item_from_dlist(current);
        if (strcmp(item->name, name) == 0) {
            return &item->value;
        }
    }

    return NULL;
}



/*
 * Test support
 */

const char * test_name = "";

void vpanic(const char * fmt, va_list args)
{
    fprintf(stderr, "Internal error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    exit(1);
}

void panic(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vpanic(fmt, args);
    va_end(args);
}

void fail()
{
    fprintf(stderr, "\n");
    exit(1);
}

void test_fail(const char * fmt, ...)
{
    fprintf(stderr, "Test `%s' fails: ", test_name);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fail();
}





/*
 * Tests
 */

#ifdef WITH_TESTS

static void check_mempool_stats(struct mempool * restrict me, size_t used)
{
    if (me->used != used) {
        mempool_print_stats(me);
        test_fail("Invalid stats (used).");
    }

    size_t allocated = me->used + me->overhead + me->avail + me->gap;
    if (me->allocated != allocated) {
        mempool_print_stats(me);
        test_fail("Invalid stats (allocated != used + overhead + avail + gap).");
    }
}

#define NTRY  10000
static int test_mempool()
{
    static char * pointers[NTRY];
    static size_t sizes[NTRY];
    size_t used = 0;

    struct mempool * restrict pool = create_mempool(4000);
    check_mempool_stats(pool, 0);

    for (int i=0; i<NTRY; ++i) {
        size_t size = 8 + rand() % 24;
        char * restrict ptr = mempool_alloc(pool, size);
        used += size;
        check_mempool_stats(pool, used);

        sizes[i] = size;
        pointers[i] = ptr;

        char filler = ' ' + i % 90;
        for (size_t j=0; j<size; ++j) {
            ptr[j] = filler;
        }
    }

    for (int i=0; i<NTRY; ++i) {
        size_t size = sizes[i];
        const char * ptr = pointers[i];

        char filler = ' ' + i % 90;
        for (size_t j=0; j<size; ++j) {
            if (ptr[j] != filler) {
                test_fail("Memory corrupted.");
            }
        }
    }

    free_mempool(pool);
    return 0;
}
#undef NTRY



#define NTRY 1000
static int test_str_map()
{
    char name[100];
    struct strhash * hash = create_strhash(NULL, 51);

    for (int i=1; i<NTRY; ++i) {
        snprintf(name, 100, "item # %d", i);
        strhash_set_int(hash, name, i);
    }

    for (int i=1; i<NTRY; ++i) {
        snprintf(name, 100, "item # %d", i);
        int value = strhash_get_int(hash, name);
        if (value != i) {
            test_fail("Invalid str hash value A[%s] = %d != %d.", name, value, i);
        }
    }

    if (strhash_get(hash, "xxx") != NULL) {
        test_fail("strhash_get for unknown key should return NULL.");
    }

    for (int i=1; i<NTRY; ++i) {
        snprintf(name, 100, "item # %d", i);
        strhash_set_int(hash, name, i+1);
    }

    for (int i=1; i<NTRY; ++i) {
        snprintf(name, 100, "item # %d", i);
        int value = strhash_get_int(hash, name);
        if (value != i+1) {
            test_fail("Invalid str hash value A[%s] = %d != %d.", name, value, i);
        }
    }

    free_strhash(hash);
    return 0;
}
#undef NTRY


static int test_sets()
{
    if (is_space_char(0)) test_fail("is_space(0) is true.");

    for (unsigned char ch = 1; ch <= 32; ++ch) {
        if (!is_space_char(ch)) test_fail("is_space_char(%d) is false.", (int)ch);
    }

    for (unsigned char ch = 33; ch > 32; ++ch) {
        if (is_space_char(ch)) test_fail("is_space_char(%d) is true.", (int)ch);
    }

    return 0;
}

#endif
