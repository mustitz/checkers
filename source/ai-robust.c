#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

struct robust_ai
{
    struct ai base;
    struct mempool * pool;
};

struct robust_ai * get_robust_ai(struct ai * me)
{
    return move_ptr(me, -offsetof(struct robust_ai, base)); 
}

void robust_ai_set_position(struct ai * restrict me, const struct position * position)
{
}

int robust_ai_do_move(struct ai * restrict me, const struct move_ctx * move_ctx)
{
    if (move_ctx->answer_count == 0) {
        printf("Internal error: call random_ai_do_move with move_ctx->answer_count = 0.\n");
        return -1;
    }

    return 0;
}

void robust_ai_free(struct ai * restrict ai)
{
    struct robust_ai * restrict me = get_robust_ai(ai);
    free_mempool(me->pool);
}

struct ai * create_robust_ai()
{
    struct mempool * pool = create_mempool(1024);
    if (pool == NULL) {
        printf("Error: create_mempool(1024) failed.\n");
        return NULL;
    }

    struct robust_ai * restrict me = mempool_alloc(pool, sizeof(struct robust_ai));
    if (me == NULL) {
        printf("Error: mempool_alloc(pool, %lu) failed.\n", sizeof(struct robust_ai));
        free_mempool(pool);
        return NULL;
    }

    me->pool = pool;
    me->base.set_position = robust_ai_set_position;
    me->base.do_move = robust_ai_do_move;
    me->base.free = robust_ai_free;

    return &me->base;
}
