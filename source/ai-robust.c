#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_DEPTH    10
#define BUF_SIZE       1024

struct robust_ai
{
    struct ai base;
    struct mempool * pool;
    int depth;
    struct position * buf;
    const struct position * current;
};

struct robust_ai * get_robust_ai(struct ai * me)
{
    return move_ptr(me, -offsetof(struct robust_ai, base)); 
}

void robust_ai_set_position(struct ai * restrict me, const struct position * position)
{
}

int game_stage_factor[32] = {
    99, 99, 99, 98, 97, 96, 95, 94, 93, 93, 92, 92,
    91, 91, 90, 90, 89, 89, 88, 88, 87, 86, 85, 84,
    80, 80, 80, 80, 80, 80, 80, 80
};

int game_mam_factor[4][4] = {
    { 99, 88, 88, 88 },
    { 88, 66, 66, 66 },
    { 88, 66, 66, 66 },
    { 88, 66, 66, 33 }
};

static int estimate(const struct position * position)
{
    bitboard_t my_all = position->bitboards[IDX_ALL_0 ^ position->active];
    bitboard_t op_all = position->bitboards[IDX_ALL_1 ^ position->active];
    bitboard_t my_sim = position->bitboards[IDX_SIM_0 ^ position->active];
    bitboard_t op_sim = position->bitboards[IDX_SIM_1 ^ position->active];

    int my_all_count = pop_count(my_all);
    int op_all_count = pop_count(op_all);
    int my_sim_count = pop_count(my_sim);
    int op_sim_count = pop_count(op_sim);
    int my_mam_count = my_all_count - my_sim_count;
    int op_mam_count = op_all_count - op_sim_count;

    int result = 0;
    result += 128 * (my_sim_count - op_sim_count);
    result += 384 * (my_mam_count - op_mam_count);
    result += 64; // Bonus for move
    
    result *= game_stage_factor[my_all_count + op_all_count];

    int my_tmp = my_mam_count <= 3 ? my_mam_count : 3;
    int op_tmp = op_mam_count <= 3 ? op_mam_count : 3;
    result *= game_mam_factor[my_tmp][op_tmp];

    return result;
}

static int recursive_estimate(struct robust_ai * restrict me, struct move_ctx * restrict move_ctx, const struct position * position)
{
    int current_depth = move_ctx->answers - me->buf;
    if (current_depth > me->depth) {
        return estimate(position);
    }

    move_ctx->position = position;
    gen_moves(move_ctx);
    int answer_count = move_ctx->answer_count;
    const struct position * answers = move_ctx->answers;

    move_ctx->answers += answer_count;

    int result = -0x40000000;
    for (int i=0; i<answer_count; ++i) {
        int estimation = recursive_estimate(me, move_ctx, answers + i);
        if (estimation > result) {
            result = estimation;
        }
    }

    move_ctx->answers -= answer_count; 
    return result;
}

int robust_ai_do_move(struct ai * restrict ai, struct move_ctx * restrict move_ctx)
{
    struct robust_ai * restrict me = get_robust_ai(ai);
    int answer_count = move_ctx->answer_count;
    const struct position * answers = move_ctx->answers;

    if (answer_count == 0) {
        printf("Internal error: call random_ai_do_move with move_ctx->answer_count = 0.\n");
        return -1;
    }

    if (answer_count == 1) {
        return 0;
    }

    move_ctx->answers = me->buf;

    int best_score = -0x40000000;
    int best_move = 0;
    for (size_t i=0; i<answer_count; ++i) {
        me->current = answers + i;
        int score = recursive_estimate(me, move_ctx, answers + i);
        if (score > best_score) {
            best_move = i;
        }
    }

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
    me->buf = buf;
    me->base.set_position = robust_ai_set_position;
    me->base.do_move = robust_ai_do_move;
    me->base.free = robust_ai_free;

    me->depth = DEFAULT_DEPTH;

    return &me->base;
}
