#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

struct mcts_ai
{
    struct ai base;
    struct mempool * pool;
};

struct mcts_ai * get_mcts_ai(struct ai * const me)
{
    return move_ptr(me, -offsetof(struct mcts_ai, base));
}

static int rollout(struct mcts_ai * restrict const me, const struct position * const position)
{
    /* TODO: Not implemented */
    return 0;
}

static int do_move(struct mcts_ai * restrict const me, struct move_ctx * restrict const move_ctx)
{
    const struct position * const answers = move_ctx->answers;
    const int answer_count = move_ctx->answer_count;
    if (answer_count == 1) {
        return 0;
    }

    int best[answer_count];
    int best_result = -1;
    int qbest = 0;

    for (int i=0; i<answer_count; ++i) {
        const int result = -rollout(me, answers + i);
        if (result < best_result) {
            continue;
        }
        if (result == best_result) {
            best[qbest++] = i;;
            continue;
        }

        qbest = 1;
        best[0] = i;
        best_result = result;
    }

    if (qbest == 0) {
        printf("Internal error: qbest == 0.\n");
        return -1;
    }

    const int num = rand() % qbest;
    return best[num];
}



/* API */

void mcts_ai_set_position(struct ai * restrict me, const struct position * position)
{
}

int  mcts_ai_do_move(struct ai * restrict me, struct move_ctx * restrict move_ctx)
{
    return do_move(get_mcts_ai(me), move_ctx);
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
