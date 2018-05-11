#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

struct random_ai
{
    struct ai base;
    struct mempool * pool;
};

struct random_ai * get_random_ai(struct ai * me)
{
    return move_ptr(me, -offsetof(struct random_ai, base));
}

const struct random_ai * cget_random_ai(const struct ai * const me)
{
    return move_cptr(me, -offsetof(struct random_ai, base));
}

void random_ai_set_position(struct ai * restrict me, const struct position * position)
{
}

int random_ai_do_move(struct ai * restrict me, struct move_ctx * restrict move_ctx)
{
    if (move_ctx->answer_count == 0) {
        printf("Internal error: call random_ai_do_move with move_ctx->answer_count = 0.\n");
        return -1;
    }

    return rand() % move_ctx->answer_count;
}

const struct keyword_tracker * random_ai_get_supported_param(const struct ai * const ai)
{
    return NULL;
}

void random_ai_free(struct ai * restrict ai)
{
    struct random_ai * restrict me = get_random_ai(ai);
    free_mempool(me->pool);
}

struct ai * create_random_ai(void)
{
    struct mempool * pool = create_mempool(1024);
    if (pool == NULL) {
        printf("Error: create_mempool(1024) failed.\n");
        return NULL;
    }

    struct random_ai * restrict me = mempool_alloc(pool, sizeof(struct random_ai));
    if (me == NULL) {
        printf("Error: mempool_alloc(pool, %lu) failed.\n", sizeof(struct random_ai));
        free_mempool(pool);
        return NULL;
    }

    me->pool = pool;
    me->base.set_position = random_ai_set_position;
    me->base.do_move = random_ai_do_move;
    me->base.get_supported_param = random_ai_get_supported_param;
    me->base.free = random_ai_free;

    return &me->base;
}
