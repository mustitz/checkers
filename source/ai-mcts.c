#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

#define PARAM_USE_ETB     1
#define PARAM_MAX_MOVES   2
#define PARAM_C           3

#define ITEM(name) { #name, PARAM_##name }
struct keyword_desc mcts_params[] = {
    ITEM(USE_ETB),
    ITEM(MAX_MOVES),
    ITEM(C),
    { NULL, 0 }
};
#undef ITEM

#define DEFAULT_MAX_MOVES   100
#define DEFAULT_C           0.5

struct mcts_ai
{
    struct ai base;
    struct mempool * pool;
    struct mempool * think_pool;
    struct keyword_tracker * params;
    struct move_ctx * move_ctx;
    struct node * * history;
    int use_etb;
    int max_moves;
    float C;
};

static inline struct mcts_ai * get_mcts_ai(struct ai * const me)
{
    return move_ptr(me, -offsetof(struct mcts_ai, base));
}

static inline const struct mcts_ai * cget_mcts_ai(const struct ai * const me)
{
    return move_cptr(me, -offsetof(struct mcts_ai, base));
}

struct node
{
    const struct position * position;
    struct position * answers;
    struct node * * nodes;

    int64_t result_sum;
    int qgames;
    int qanswers;
    int in_mcts_tree;
    int reserved;
};

static struct node * alloc_node(
    const struct mcts_ai * const me,
    const struct position * const position)
{
    mempool_align(me->think_pool, 32);
    struct node * restrict node = mempool_alloc(me->think_pool, sizeof(struct node));
    if (node == NULL) {
        return NULL;
    }

    node->position = position;
    node->qgames = 0;
    node->in_mcts_tree = 0;

    /* TODO: ETB */

    struct move_ctx * restrict const ctx = me->move_ctx;
    ctx->position = position;
    gen_moves(ctx);

    const int answer_count = node->qanswers = ctx->answer_count;
    if (answer_count == 0) {
        node->result_sum = position->active == BLACK ? +1 : -1;
        node->answers = NULL;
        node->nodes = NULL;
        return node;
    }

    node->result_sum = 0;

    const size_t answers_sz = answer_count * sizeof(struct position);
    mempool_align(me->think_pool, 32);
    node->answers = mempool_alloc(me->think_pool, answers_sz);
    if (node->answers == NULL) {
        return NULL;
    }
    memcpy(node->answers, ctx->answers, answers_sz);

    const size_t nodes_sz = answer_count * sizeof(struct node *);
    mempool_align(me->think_pool, 32);
    node->nodes = mempool_alloc(me->think_pool, nodes_sz);
    if (node->nodes == NULL) {
        return NULL;
    }
    memset(node->nodes, 0, nodes_sz);

    return node;
}

static inline int rollout_move(
    struct mcts_ai * restrict const me,
    struct node * restrict node)
{
    return rand() % node->qanswers;
}

static inline int select_move(
    struct mcts_ai * restrict const me,
    struct node * restrict node)
{
    return 0;
}

static void simulate(
    struct mcts_ai * restrict const me,
    struct node * restrict node)
{
    struct node * * history = me->history;
    int qhistory = 0;

    int mcts_expanded = 0;

    for (int i=0;; ++i) {

        if (i >= me->max_moves) {
            node->answers = NULL;
            node->result_sum = 0;
        }

        /* Game over? */
        if (node->answers == NULL) {
            const int64_t result = node->result_sum;
            for (int j=0; j<qhistory; ++j) {
                history[j]->result_sum += result;
                ++history[j]->qgames;
            }
            return;
        }

        history[qhistory++] = node;

        int j=0;
        for (; j<node->qanswers; ++j) {
            if (node->nodes[j] == NULL) {
                break;
            }
            if (!node->nodes[j]->in_mcts_tree) {
                break;
            }
        }

        const int answer_index = j == node->qanswers ? select_move(me, node) : rollout_move(me, node);

        if (node->nodes[answer_index] == NULL) {
            node->nodes[answer_index] = alloc_node(me, node->answers + answer_index);
        }
        node = node->nodes[answer_index];

        if (node->in_mcts_tree == 0 && mcts_expanded == 0) {
            node->in_mcts_tree = 1;
            mcts_expanded = 1;
        }
    }
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

static void set_max_moves(
    struct mcts_ai * restrict const me,
    struct line_parser * restrict const lp)
{
    int max_moves;
    int status = parser_read_last_int(lp, &max_moves);

    if (status != 0) {
        return ai_param_fail(lp, status, "AI SET MAX_MOVES");
    }

    if (max_moves <= 0) {
        printf("Wrong MAX MOVES value %d. It should be positive nonzero integer.\n", max_moves);
        return;
    }

    const size_t history_sz = max_moves * sizeof(struct node *);
    struct node * * new_history = malloc(history_sz);
    if (new_history == NULL) {
        printf("Cannot malloc(%lu) for new history.\n", history_sz);
        return;
    }

    free(me->history);
    me->max_moves = max_moves;
    me->history = new_history;
}

static void set_C(
    struct mcts_ai * restrict const me,
    struct line_parser * restrict const lp)
{
    float C;
    int status = parser_read_last_float(lp, &C);

    if (status != 0) {
        return ai_param_fail(lp, status, "AI SET C");
    }

    if (C <= 0.0) {
        printf("Wrong C value %f.3. It should be positive nonzero float.\n", C);
        return;
    }

    me->C = C;
}

static const set_param_func set_param_handlers[] = {
    [PARAM_USE_ETB] = set_use_etb,
    [PARAM_MAX_MOVES] = set_max_moves,
    [PARAM_C] = set_C,
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
    printf("-C%.3f", me->C);
    if (me->use_etb != 0) {
        printf("-etb%d", me->use_etb);
    }
    if (me->max_moves != DEFAULT_MAX_MOVES) {
        printf("-mm%d", me->max_moves);
    }
    printf("\n");

    printf("%*s %d\n", len, "use_etb", me->use_etb);
    printf("%*s %d\n", len, "max_moves", me->max_moves);
    printf("%*s %.6f\n", len, "C", me->C);
    printf("%*s %s\n", len, "hash", AI_MCTS_HASH);
}



/* API */

static void mcts_ai_set_position(struct ai * restrict me, const struct position * position)
{
}

static int  mcts_ai_do_move(struct ai * restrict me, struct move_ctx * restrict move_ctx)
{
    return do_move(get_mcts_ai(me), move_ctx);
}

static const struct keyword_tracker * mcts_ai_get_supported_param(
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

static void mcts_ai_info(const struct ai * const ai)
{
    const struct mcts_ai * const me = cget_mcts_ai(ai);
    info(me);
}

static void mcts_ai_free(struct ai * restrict ai)
{
    struct mcts_ai * restrict const me = get_mcts_ai(ai);
    if (me->history != NULL) {
        free(me->history);
    }
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
    me->think_pool = NULL;
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

    me->max_moves = DEFAULT_MAX_MOVES;
    const size_t history_sz = me->max_moves * sizeof(struct node *);
    me->history = malloc(history_sz);
    if (me->history == NULL) {
        printf("Error: malloc(%lu) is failed (history).\n", history_sz);
        free_mempool(pool);
        return NULL;
    }

    me->C = DEFAULT_C;

    return &me->base;
}

void use_func() /* Dummy function for killing static unused warnings */
{
    printf("%p", &simulate);
}
