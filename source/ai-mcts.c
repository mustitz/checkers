#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

#define PARAM_USE_ETB     1

#define ITEM(name) { #name, PARAM_##name }
struct keyword_desc mcts_params[] = {
    ITEM(USE_ETB),
    { NULL, 0 }
};
#undef ITEM

struct mcts_ai
{
    struct ai base;
    struct mempool * pool;
    struct keyword_tracker * params;
    struct move_ctx * move_ctx;
    int use_etb;
};

struct mcts_ai * get_mcts_ai(struct ai * const me)
{
    return move_ptr(me, -offsetof(struct mcts_ai, base));
}

const struct mcts_ai * cget_mcts_ai(const struct ai * const me)
{
    return move_cptr(me, -offsetof(struct mcts_ai, base));
}

static int rollout(struct mcts_ai * restrict const me, const struct position * const position)
{
    const int active = position->active;

    struct move_ctx * restrict const ctx = me->move_ctx;
    ctx->position = position;

    for (;;) {
        const bitboard_t * const bitboards = position->bitboards;
        const bitboard_t all = bitboards[IDX_ALL_0] | bitboards[IDX_ALL_1];

        if (pop_count(all) <= me->use_etb) {
            const int8_t etb_estimation = etb_estimate(position);
            if (etb_estimation != ETB_NA) {
                if (etb_estimation == 0) {
                    return 0;
                }
                const int is_win = (etb_estimation < 0) ^ (ctx->position->active == active);
                return is_win ? +1 : -1;
            }
        }

        gen_moves(ctx);

        const int answer_count = ctx->answer_count;
        if (answer_count == 0) {
            const int is_win = ctx->position->active != active;
            return is_win ? +1 : -1;
        }

        const int i = rand() % answer_count;
        const struct position position_storage = ctx->answers[i];
        ctx->position = &position_storage;
    }
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

static void ai_param_fail(
    struct line_parser * restrict const lp,
    const int status,
    const char * const what)
{
    if (status == PARSER_ERROR__NO_EOL) {
        printf("End of lne expected in “%s n”, but something is found.\n", what);
    } else {
        printf("Integer value is expected in “%s”.\n", what);
    }

    int offset = lp->lexem_start - lp->line;
    printf("> %s> %*s^\n", lp->line, offset, "");
}

typedef void (* set_param_func)(
    struct mcts_ai * restrict const me,
    struct line_parser * restrict const lp);

static void set_use_etb(
    struct mcts_ai * restrict const me,
    struct line_parser * restrict const lp)
{
    int use_etb;
    int status = parser_read_last_int(lp, &use_etb);

    if (status != 0) {
        return ai_param_fail(lp, status, "AI SET USE_ETB");
    }

    if (use_etb < 0 || use_etb > 12) {
        printf("Wrong USE_ETB value %d. It should be in range from 0 to 12.\n", use_etb);
        return;
    }

    me->use_etb = use_etb;
}

static const set_param_func set_param_handlers[] = {
    [PARAM_USE_ETB] = set_use_etb,
    [0] = NULL
};

static void set_param(
    struct mcts_ai * restrict const me,
    const int param_id,
    struct line_parser * restrict const lp)
{
    static const int handler_len = sizeof(set_param_handlers) / sizeof(set_param_handlers[0]);
    if (param_id <= 0 || param_id >= handler_len) {
        fprintf(stderr, "Error, param with id = %d is not supported. The valid range is from 1 to %d.\n", param_id, handler_len - 1);
        return;
    }

    set_param_handlers[param_id](me, lp);
}

extern const char * const AI_MCTS_HASH;
static void info(const struct mcts_ai * const me)
{
    static const int len = 10;

    printf("%*s mcts-%*.*s", len, "id", 8, 8, AI_MCTS_HASH);
    if (me->use_etb != 0) {
        printf("-etb%d", me->use_etb);
    }
    printf("\n");

    printf("%*s %d\n", len, "use_etb", me->use_etb);
    printf("%*s %s\n", len, "hash", AI_MCTS_HASH);
}



/* API */

void mcts_ai_set_position(struct ai * restrict me, const struct position * position)
{
}

int  mcts_ai_do_move(struct ai * restrict me, struct move_ctx * restrict move_ctx)
{
    return do_move(get_mcts_ai(me), move_ctx);
}

const struct keyword_tracker * mcts_ai_get_supported_param(
    const struct ai * const ai)
{
    const struct mcts_ai * const me = cget_mcts_ai(ai);
    return me->params;
}

static void mcts_ai_set_param(
    struct ai * restrict const ai,
    const int param_id,
    struct line_parser * restrict const lp)
{
    struct mcts_ai * restrict const me = get_mcts_ai(ai);
    set_param(me, param_id, lp);
}

void mcts_ai_info(const struct ai * const ai)
{
    const struct mcts_ai * const me = cget_mcts_ai(ai);
    info(me);
}

void mcts_ai_free(struct ai * restrict ai)
{
    struct mcts_ai * restrict const me = get_mcts_ai(ai);
    free_mempool(me->pool);
}

static inline void * alloc(
    struct mcts_ai * restrict const me,
    const size_t sz,
    const char * const title)
{
    struct mempool * restrict const mempool = me->pool;
    mempool_align(mempool, 256);
    void * restrict const result = mempool_alloc(mempool, sz);
    if (result == NULL) {
        fprintf(stderr, "MCTS AI: Fail to allocate %s from mempool, %lu bytes are required.\n", title, sz);
    }
    return result;
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
    me->base.get_supported_param = mcts_ai_get_supported_param;
    me->base.set_param = mcts_ai_set_param;
    me->base.info = mcts_ai_info;
    me->base.free = mcts_ai_free;

    const size_t move_ctx_sz = sizeof(struct move_ctx);
    me->move_ctx = alloc(me, move_ctx_sz, "move context");
    if (me->move_ctx == NULL) {
        free_mempool(pool);
        return NULL;
    }

    const size_t buf_taking_sz = BUF_SIZE__TAKING * sizeof(struct taking);

    me->move_ctx->sim_taking = alloc(me, buf_taking_sz, "taking buffer for simples");
    if (me->move_ctx->sim_taking == NULL) {
        free_mempool(pool);
        return NULL;
    }

    me->move_ctx->mam_taking = alloc(me, buf_taking_sz, "taking buffer for mams");
    if (me->move_ctx->mam_taking == NULL) {
        free_mempool(pool);
        return NULL;
    }

    const size_t answers_sz = BUF_SIZE__ANSWERS * sizeof(struct position);
    me->move_ctx->answers = alloc(me, answers_sz, "answers array");
    if (me->move_ctx->answers == NULL) {
        free_mempool(pool);
        return NULL;
    }

    me->params = build_keyword_tracker(mcts_params, pool, KW_TRACKER__IGNORE_CASE);
    if (me->params == NULL) {
        printf("Error: build_keyword_tracker(...) failed.\n");
        free_mempool(pool);
        return NULL;
    }

    me->use_etb = 0;
    return &me->base;
}
