#ifndef MU__DATA_STRUCT__H__
#define MU__DATA_STRUCT__H__

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>





/*
 * Testing support.
 */

extern const char * test_name;
void vpanic(const char * fmt, va_list args);
void panic(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));
void fail(void);
void test_fail(const char * fmt, ...) __attribute__ ((format (printf, 1, 2)));





/*
 * Here is utility for working with data structures.
 * CLEAR_DLIST - init empry double list structure.
 */

#define CLEAR_DLIST(a) { (a)->next = (a); (a)->prev = (a); }
#define INSERT_AFTER_IN_DLIST(base, item) \
    { (item)->next = (base)->next; (item)->prev = (base); (base)->next->prev = (item); (base)->next = (item); }
#define INSERT_BEFORE_IN_DLIST(base, item) \
    { (item)->next = (base); (item)->prev = (base)->prev; (base)->prev->next = (item); (base)->prev = (item); }





/*
 * Here is working with pointers and memory pool for grouping allocation operations.
 * The main idea of mempool is many allocs one free operation.
 * mempool is a good choice for many small object allocations.
 */

static inline void * move_ptr(void * ptr, ptrdiff_t delta)
{
    char * tmp = ptr;
    return tmp + delta;
}

static inline const void * move_cptr(const void * const ptr, ptrdiff_t delta)
{
    const char * const tmp = ptr;
    return tmp + delta;
}

static inline ptrdiff_t sub_ptr(const void * a, const void * b)
{
    const char * tmp_a = a;
    const char * tmp_b = b;
    return tmp_a - tmp_b;
}

struct mempool_item
{
    void * base;
    void * current;
    size_t avail;
    struct mempool_item * restrict next;
};

struct mempool
{
    struct mempool_item * restrict last;
    size_t allocated;
    size_t used;
    size_t overhead;
    size_t avail;
    size_t gap;
};

struct mempool * create_mempool(size_t initial_size);
void free_mempool(struct mempool * restrict me);
void * mempool_alloc(struct mempool * restrict me, size_t size);
void mempool_align(struct mempool * restrict me, size_t bound);
void mempool_print_stats(const struct mempool * me);





/*
 * Character sets (256 bits)
 */

struct char_set
{
    uint64_t bits[4];
};

static inline int is_in_char_set(unsigned char ch, const struct char_set * set)
{
    int offset = ch % 64;
    size_t index = ch / 64;
    uint64_t mask = 1; mask <<= offset;
    return (set->bits[index] & mask) != 0;
}

static const struct char_set space_set = { { 0x00000001FFFFFFFE, 0, 0, 0 } };
static const struct char_set id_first_char_set =  { { 0, 0x07FFFFFE87FFFFFE, 0, 0 } };
static const struct char_set id_char_set =  { { 0x03FF000000000000, 0x07FFFFFE87FFFFFE, 0, 0 } };

static inline int is_space_char(unsigned char ch)
{
    return is_in_char_set(ch, &space_set);
}

static inline int is_first_id_char(unsigned char ch)
{
    return is_in_char_set(ch, &id_first_char_set);
}

static inline int is_id_char(unsigned char ch)
{
    return is_in_char_set(ch, &id_char_set);
}





/*
 * String hash table
 */

struct strhash_item_dlist
{
    struct strhash_item_dlist * next;
    struct strhash_item_dlist * prev;
};

union strhash_value
{
    void * as_ptr;
    const void * as_cptr;
    uint64_t as_uint;
    int64_t as_int;
    double as_double;
};

struct strhash_item
{
    const char * name;
    union strhash_value value;
    struct strhash_item_dlist dlist;
};

static inline struct strhash_item * get_strhash_item_from_dlist(struct strhash_item_dlist * dlist)
{
    return move_ptr(dlist, -offsetof(struct strhash_item, dlist));
}

struct strhash
{
    int mod;
    struct mempool * pool;
    struct strhash_item_dlist items[1];
};

struct strhash * create_strhash(struct mempool * restrict pool, int mod);
void free_strhash(struct strhash * restrict me);
void strhash_set(struct strhash * restrict me, const char * name, union strhash_value value);
union strhash_value * strhash_get(struct strhash * restrict me, const char * name);

static inline void strhash_set_ptr(struct strhash * restrict me, const char * name, void * value)
{
    union strhash_value tmp;
    tmp.as_ptr = value;
    strhash_set(me, name, tmp);
}

static inline void strhash_set_cptr(struct strhash * restrict me, const char * name, const void * value)
{
    union strhash_value tmp;
    tmp.as_cptr = value;
    strhash_set(me, name, tmp);
}

static inline void strhash_set_uint(struct strhash * restrict me, const char * name, uint64_t value)
{
    union strhash_value tmp;
    tmp.as_uint = value;
    strhash_set(me, name, tmp);
}

static inline void strhash_set_int(struct strhash * restrict me, const char * name, int64_t value)
{
    union strhash_value tmp;
    tmp.as_int = value;
    strhash_set(me, name, tmp);
}

static inline void strhash_set_double(struct strhash * restrict me, const char * name, double value)
{
    union strhash_value tmp;
    tmp.as_double = value;
    strhash_set(me, name, tmp);
}

static inline void * strhash_get_ptr(struct strhash * restrict me, const char * name)
{
    const union strhash_value * value = strhash_get(me, name);
    return value != NULL ? value->as_ptr : NULL;
}

static inline const void * strhash_get_cptr(struct strhash * restrict me, const char * name)
{
    const union strhash_value * value = strhash_get(me, name);
    return value != NULL ? value->as_cptr : NULL;
}

static inline uint64_t strhash_get_uint(struct strhash * restrict me, const char * name)
{
    const union strhash_value * value = strhash_get(me, name);
    return value != NULL ? value->as_uint : 0;
}

static inline int64_t strhash_get_int(struct strhash * restrict me, const char * name)
{
    const union strhash_value * value = strhash_get(me, name);
    return value != NULL ? value->as_int : 0;
}

static inline double strhash_get_double(struct strhash * restrict me, const char * name)
{
    const union strhash_value * value = strhash_get(me, name);
    return value != NULL ? value->as_double : 0.0;
}

#endif
