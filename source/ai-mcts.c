#include "checkers.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// #define TRACE_MCTS

#define PARAM_USE_ETB       1
#define PARAM_MAX_MOVES     2
#define PARAM_C             3
#define PARAM_SIMUL_COUNT   4
#define PARAM_EXPLAIN       5

#define ITEM(name) { #name, PARAM_##name }
struct keyword_desc mcts_params[] = {
    ITEM(USE_ETB),
    ITEM(MAX_MOVES),
    ITEM(C),
    ITEM(SIMUL_COUNT),
    ITEM(EXPLAIN),
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
    int qsimulations;
    int explain;
    int qmoves_in_explain;
};

static inline struct mcts_ai * get_mcts_ai(struct ai * const me)
{
    return move_ptr(me, -offsetof(struct mcts_ai, base));
}

static inline const struct mcts_ai * cget_mcts_ai(const struct ai * const me)
{
    return move_cptr(me, -offsetof(struct mcts_ai, base));
}

static inline void print_move(
    FILE * const f,
    const struct position * const prev,
    const struct position * const next)
{
    const int active = prev->active;
    const int index = IDX_ALL ^ active;
    const bitboard_t old = prev->bitboards[index];
    const bitboard_t new = next->bitboards[index];
    const bitboard_t delta = old ^ new;
    const square_t from = get_first_square(delta & old);
    const square_t to = get_first_square(delta & new);

    const bitboard_t enemy_old = prev->bitboards[index ^ 1];
    const bitboard_t enemy_new = next->bitboards[index ^ 1];
    const char * const inner_symbol = enemy_old == enemy_new ? "-" : ":";

    fprintf(f, "%s%s%s", lower_square_str[from], inner_symbol, lower_square_str[to]);
}

struct node
{
    const struct position * position;
    struct position * answers;
    struct node * * nodes;

    int64_t result_sum;
    int64_t qgames;
    int qanswers;
    int in_mcts_tree;
    int reserved;
};

static inline int result_from_estimation(
    const int8_t estimation,
    const int active)
{
    if (estimation == 0) return 0;
    const int is_white = active == WHITE;
    const int is_win = estimation > 0;
    return (is_white ^ is_win) ? -1 : +1;
}

static inline struct node * terminal_node(
    struct node * restrict const node,
    int result)
{
    node->result_sum = result;
    node->qgames = 1;
    node->answers = NULL;
    node->nodes = NULL;
    return node;
}

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

    int8_t etb_estimation = etb_lookup(position, me->use_etb);
    if (etb_estimation != ETB_NA) {
        const int result = result_from_estimation(etb_estimation, position->active);
        return terminal_node(node, result);
    }


    struct move_ctx * restrict const ctx = me->move_ctx;
    ctx->position = position;
    gen_moves(ctx);

    const int answer_count = node->qanswers = ctx->answer_count;
    if (answer_count == 0) {
        const int result = result_from_estimation(-1, position->active);
        return terminal_node(node, result);
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

static inline float ubc_estimation(
    const struct node * const node,
    const float C,
    const float total)
{
    const float sign = node->position->active == WHITE ? -1.0 : +1.0;
    const float ev = (float)node->result_sum / (float)node->qgames;
    return sign * ev + C * sqrt(log(total)/node->qgames);
}

static inline int select_move(
    struct mcts_ai * restrict const me,
    struct node * restrict node)
{
    #ifdef TRACE_MCTS
        printf("Select move!\n");
    #endif

    const int64_t total = node->qgames;
    int best_i = 0;
    float best_estimation = ubc_estimation(node->nodes[0], me->C, total);

    #ifdef TRACE_MCTS
        printf("  ");
        print_move(stdout, node->position, node->nodes[0]->position);
        printf(" %.6f\n", best_estimation);
    #endif

    for (int i=1; i<node->qanswers; ++i) {
        const float estimation = ubc_estimation(node->nodes[i], me->C, total);

        #ifdef TRACE_MCTS
            printf("  ");
            print_move(stdout, node->position, node->nodes[i]->position);
            printf(" %.6f\n", estimation);
        #endif

        if (estimation > best_estimation) {
            best_estimation = estimation;
            best_i = i;
        }
    }

    return best_i;
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
            ++node->qgames;

            #ifdef TRACE_MCTS
                switch (result) {
                    case -1:
                        printf("Simulate over, 0-1.\n");
                        break;
                    case 0:
                        printf("Simulate over, ½-½.\n");
                        break;
                    case +1:
                        printf("Simulate over, 1-0.\n");
                        break;
                    default:
                        printf("Unexpected result %ld.\n", result);
                        break;
                }
            #endif

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

        #ifdef TRACE_MCTS
            if (!mcts_expanded) {
                print_move(stdout, node->position, node->nodes[answer_index]->position);
                printf("\n");
            }
        #endif

        node = node->nodes[answer_index];

        if (node->in_mcts_tree == 0 && mcts_expanded == 0) {
            node->in_mcts_tree = 1;
            mcts_expanded = 1;
        }
    }
}

static int etb_move(
    const struct mcts_ai * const me,
    const struct move_ctx * const move_ctx)
{
    const struct position * const position = move_ctx->position;
    int8_t estimation = etb_lookup(position, me->use_etb);
    if (estimation == ETB_NA) {
        return -1;
    }

    const int answer_count = move_ctx->answer_count;
    int indexes[answer_count];
    int qindexes = 0;
    int best_grade = INT_MIN;

    for (int i=0; i<answer_count; ++i) {
        const int new_grade = etb_grade(move_ctx->answers + i, 10000);
        if (new_grade < best_grade) {
            continue;
        }
        if (new_grade == best_grade) {
            indexes[qindexes++] = i;
            continue;
        }
        best_grade = new_grade;
        indexes[0] = i;
        qindexes = 1;
    }

    return indexes[rand() % qindexes];
}

struct node_stats {
    int64_t result_sum;
    int64_t qgames;
    float ev;
    int index;
};

int node_stats_cmp(
    const struct node_stats * const a,
    const struct node_stats * const b)
{
    if (a->qgames < b->qgames) return +1;
    if (a->qgames > b->qgames) return -1;
    if (a->result_sum < b->result_sum) return +1;
    if (a->result_sum > b->result_sum) return -1;
    return 0;
}

int cmp_node_stats(const void * const a, const void * const b)
{
    return node_stats_cmp(a, b);
}

static void explain_node(
    const struct mcts_ai * const me,
    const struct node * const node,
    const int depth)
{
    if (depth >= me->qmoves_in_explain) {
        return;
    }

    const int qanswers = node->qanswers;
    struct node_stats node_stats[qanswers];

    for (int i=0; i<qanswers; ++i) {
        node_stats[i].index = i;

        const struct node * const answer = node->nodes[i];
        if (answer != NULL && answer->qgames > 0) {
            node_stats[i].result_sum = answer->result_sum;
            node_stats[i].qgames = answer->qgames;
            node_stats[i].ev = (float)answer->result_sum / (float)answer->qgames;
        } else {
            node_stats[i].result_sum = 0;
            node_stats[i].qgames = 0;
            node_stats[i].ev = 0.0;
        }
    }

    qsort(node_stats, qanswers, sizeof(struct node_stats), cmp_node_stats);

    for (int i=0; i<qanswers; ++i) {
        const struct node_stats * const stats = node_stats + i;
        const int index = stats->index;
        const struct node * const answer = node->nodes[index];
        printf("Explain: %*s", depth*4, "");
        print_move(stdout, node->position, node->answers + index);
        printf("%*s", 8, "");
        if (stats->qgames > 0) {
            printf("%10ld   %7.4f\n", stats->qgames, stats->ev);
        } else {
            printf("%*s-\n", 9, "");
        }

        if (i == 0) {
            explain_node(me, answer, depth+1);
        }
    }
}

static int do_move(
    struct mcts_ai * restrict const me,
    struct move_ctx * restrict const move_ctx)
{
    const int answer_count = move_ctx->answer_count;

    if (answer_count == 0) {
        printf("Internal error: call robust_ai_do_move with move_ctx->answer_count = 0.\n");
        return -1;
    }

    if (answer_count == 1) {
        return 0;
    }

    if (me->use_etb > 0) {
        int result = etb_move(me, move_ctx);
        if (result >= 0) {
            return result;
        }
    }

    me->think_pool = create_mempool(128*1024*1024);

    const struct position * const position = move_ctx->position;
    struct node * root = alloc_node(me, position);
    root->in_mcts_tree = 1;

    for (int i=0; i<me->qsimulations; ++i) {
        simulate(me, root);

        #ifdef TRACE_MCTS
            for (int j=0; j<answer_count; ++j) {
                printf("Simulate ");
                print_move(stdout, position, root->answers + j);
                if (root->nodes[j] != NULL) {
                    printf("  %ld from %ld\n", root->nodes[j]->result_sum, root->nodes[j]->qgames);
                } else {
                    printf("-\n");
                }
            }
        #endif
    }

    if (me->explain) {
        explain_node(me, root, 0);
    }

    int answers[root->qanswers];
    int qanswers = 0;
    int64_t best = 0;

    for (int i=0; i<root->qanswers; ++i) {

        const struct node * const answer = root->nodes[i];
        if (answer == NULL) {
            continue;
        }

        const int64_t qgames = answer->qgames;
        if (best > qgames) {
            continue;
        }
        if (best == qgames) {
            answers[qanswers++] = i;
            continue;
        }
        best = qgames;
        answers[0] = i;
        qanswers = 1;
    }

    struct mempool * restrict const tmp = me->think_pool;
    me->think_pool = NULL;
    free_mempool(tmp);

    const int index = qanswers == 1 ? 0 : rand() % qanswers;
    return answers[index];
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

static void set_simul_count(
    struct mcts_ai * restrict const me,
    struct line_parser * restrict const lp)
{
    int simul_count;
    int status = parser_read_last_int(lp, &simul_count);

    if (status != 0) {
        return ai_param_fail(lp, status, "AI SET SIMUL_COUNT");
    }

    if (simul_count <= 0) {
        printf("Wrong SIMUL COUNT value %d. It should be positive nonzero integer.\n", simul_count);
        return;
    }

    me->qsimulations = simul_count;
}

static void set_explain(
    struct mcts_ai * restrict const me,
    struct line_parser * restrict const lp)
{
    int explain;
    int status = parser_read_last_int(lp, &explain);

    if (status != 0) {
        return ai_param_fail(lp, status, "AI SET EXPLAIN");
    }

    if (explain < 0 || explain > 1) {
        printf("Wrong EXPLAIN value %d. It should be 0 or 1.\n", explain);
        return;
    }

    me->explain = explain;
}

static const set_param_func set_param_handlers[] = {
    [PARAM_USE_ETB] = set_use_etb,
    [PARAM_MAX_MOVES] = set_max_moves,
    [PARAM_C] = set_C,
    [PARAM_SIMUL_COUNT] = set_simul_count,
    [PARAM_EXPLAIN] = set_explain,
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
    printf("-s%d", me->qsimulations);
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
    printf("%*s %d\n", len, "simul_count", me->qsimulations);
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
    me->qsimulations = 1000;
    me->qmoves_in_explain = 3;

    return &me->base;
}
