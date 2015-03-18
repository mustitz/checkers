#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_DEPTH    20
#define BUF_SIZE       1024

struct robust_ai
{
    struct ai base;
    struct mempool * pool;
    int depth;
    struct position * buf;
};

struct robust_ai * get_robust_ai(struct ai * me)
{
    return move_ptr(me, -offsetof(struct robust_ai, base)); 
}

void robust_ai_set_position(struct ai * restrict me, const struct position * position)
{
}

static int recursive_estimate(struct robust_ai * restrict me, const struct position * position)
{
    return 0;
}

int robust_ai_do_move(struct ai * restrict ai, struct move_ctx * restrict move_ctx)
{
    struct robust_ai * restrict me = get_robust_ai(ai);

    if (move_ctx->answer_count == 0) {
        printf("Internal error: call random_ai_do_move with move_ctx->answer_count = 0.\n");
        return -1;
    }

    if (move_ctx->answer_count == 1) {
        return 0;
    }

    void * saved = move_ctx->answers;
    const struct position * answers = move_ctx->answers;
    move_ctx->answers = me->buf;

    int best_score = -0x40000000;
    int best_move = 0;
    for (size_t i=0; i<move_ctx->answer_count; ++i) {
        int score = recursive_estimate(me, answers + i);
        if (score > best_score) {
            best_move = i;
        }
    }

    move_ctx->answers = saved;
    return best_move;
}

void robust_ai_free(struct ai * restrict ai)
{
    struct robust_ai * restrict me = get_robust_ai(ai);
    free_mempool(me->pool);
}

struct ai * create_robust_ai()
{
    size_t size;

    struct mempool * pool = create_mempool(1024);
    if (pool == NULL) {
        printf("Error: create_mempool(1024) failed.\n");
        return NULL;
    }

    size = sizeof(struct robust_ai);
    struct robust_ai * restrict me = mempool_alloc(pool, size);
    if (me == NULL) {
        printf("Error: mempool_alloc(pool, %lu) failed.\n", size);
        free_mempool(pool);
        return NULL;
    }

    size = BUF_SIZE * sizeof(struct position);
    struct position * restrict buf = mempool_alloc(pool, size);
    if (buf == NULL) {
        printf("Error: mempool_alloc(pool, %lu) failed.\n", size);
        free_mempool(pool);
        return NULL;
    }

    me->pool = pool;
    me->base.set_position = robust_ai_set_position;
    me->base.do_move = robust_ai_do_move;
    me->base.free = robust_ai_free;

    me->depth = DEFAULT_DEPTH;

    return &me->base;
}
