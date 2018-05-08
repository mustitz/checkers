#include "checkers.h"

#include <stdio.h>

struct mcts_ai
{
    struct ai base;
    struct mempool * pool;
};

struct mcts_ai * get_mcts_ai(struct ai * const me)
{
    return move_ptr(me, -offsetof(struct mcts_ai, base));
}

void mcts_ai_set_position(struct ai * restrict me, const struct position * position)
{
}

int  mcts_ai_do_move(struct ai * restrict me, struct move_ctx * restrict move_ctx)
{
    /* TODO: Here is a stub */
    return 0;
}

void mcts_ai_free(struct ai * restrict ai)
{
    struct mcts_ai * restrict const me = get_mcts_ai(ai);
    free_mempool(me->pool);
}

struct ai * create_mcts_ai(void)
{
    struct mempool * const pool = create_mempool(1024);
    if (pool == NULL) {
        printf("Error: create_mempool(1024) failed.\n");
        return NULL;
    }

    const size_t sz = sizeof(struct mcts_ai);
    struct mcts_ai * restrict me = mempool_alloc(pool, sz);
    if (me == NULL) {
        printf("Error: mempool_alloc(pool, %lu) failed.\n", sz);
        free_mempool(pool);
        return NULL;
    }

    me->pool = pool;
    me->base.set_position = mcts_ai_set_position;
    me->base.do_move = mcts_ai_do_move;
    me->base.free = mcts_ai_free;

    return &me->base;
}
