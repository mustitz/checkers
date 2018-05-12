#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_DEPTH                   32
#define BUF_SIZE                      1024
#define MAX_ESTIMATION            10000000
#define MIN_ESTIMATION     -MAX_ESTIMATION

#define PARAM_DEPTH       1
#define PARAM_USE_ETB     2

#define ITEM(name) { #name, PARAM_##name }
struct keyword_desc robust_params[] = {
    ITEM(DEPTH),
    ITEM(USE_ETB),
    { NULL, 0 }
};
#undef ITEM

static unsigned long int next = 1;

static int random10(void)
{
  next = next * 1103515245 + 12345;
  unsigned int result = (next/65536) % 10;
  return result;
}

static void randomize(void)
{
   next = time(NULL);
}

struct robust_ai
{
    struct ai base;
    struct mempool * pool;
    struct position * buf;
    struct keyword_tracker * params;

    int depth;
    int use_etb;
};

struct robust_ai * get_robust_ai(struct ai * me)
{
    return move_ptr(me, -offsetof(struct robust_ai, base));
}

const struct robust_ai * cget_robust_ai(const struct ai * const me)
{
    return move_cptr(me, -offsetof(struct robust_ai, base));
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
    struct robust_ai * restrict const me,
    struct line_parser * restrict const lp);

static void set_depth(
    struct robust_ai * restrict const me,
    struct line_parser * restrict const lp)
{
    int depth;
    int status = parser_read_last_int(lp, &depth);

    if (status != 0) {
        return ai_param_fail(lp, status, "AI SET DEPTH");
    }

    if (depth <= 0) {
        printf("Wrong DEPTH value %d. It should be greater then zero.\n", depth);
        return;
    }

    me->depth = depth;
}

static void set_use_etb(
    struct robust_ai * restrict const me,
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
    [PARAM_DEPTH] = set_depth,
    [PARAM_USE_ETB] = set_use_etb,
    [0] = NULL
};

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

static inline int lookup_etb(
    struct robust_ai * restrict const me,
    const struct position * const position,
    const int current_depth,
    int * restrict estimation)
{
    const bitboard_t * const bitboards = position->bitboards;
    const bitboard_t all = bitboards[IDX_ALL_0] | bitboards[IDX_ALL_1];

    if (pop_count(all) > me->use_etb) {
        return 1;
    }

    int8_t etb_estimation = etb_estimate(position);
    if (etb_estimation == ETB_NA) {
        return 1;
    }

    if (etb_estimation == 0) {
        *estimation = 0;
        return 0;
    }

    const int etb_delta = 1000*etb_estimation;
    const int depth_delta = 10*current_depth;
    if (etb_estimation > 0) {
        *estimation = MAX_ESTIMATION - etb_delta - depth_delta;
    } else {
        *estimation = MIN_ESTIMATION - etb_delta + depth_delta;
    }
    return 0;
}

static int estimate(
    struct robust_ai * restrict const me,
    int current_depth,
    const struct position * position)
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
    result += 35 * (my_mam_count - op_mam_count);
    result += 10 * (my_sim_count - op_sim_count);
    return result;
/*
    int result = 0;
    result += 128 * (my_sim_count - op_sim_count);
    result += 384 * (my_mam_count - op_mam_count);
    result += 64; // Bonus for move

    result *= game_stage_factor[my_all_count + op_all_count];

    int my_tmp = my_mam_count <= 3 ? my_mam_count : 3;
    int op_tmp = op_mam_count <= 3 ? op_mam_count : 3;
    result *= game_mam_factor[my_tmp][op_tmp];

    return result;
*/
}

static int recursive_estimate(struct robust_ai * restrict me, struct move_ctx * restrict move_ctx, const struct position * position)
{
    int current_depth = move_ctx->answers - me->buf;

    int etb_estimation;
    if (lookup_etb(me, position, current_depth, &etb_estimation) == 0) {
        return etb_estimation;
    }

    if (current_depth > me->depth) {
        return estimate(me, current_depth, position);
    }

    move_ctx->position = position;
    gen_moves(move_ctx);
    int answer_count = move_ctx->answer_count;
    if (answer_count == 0) {
        return MIN_ESTIMATION + 10 * current_depth;
    }

    const struct position * answers = move_ctx->answers;
    move_ctx->answers += answer_count;

    int result = MAX_ESTIMATION;
    for (int i=0; i<answer_count; ++i) {
        int estimation = recursive_estimate(me, move_ctx, answers + i);
        // printf("  %*s E %d\n", current_depth, "", estimation);
        if (estimation < result) {
            result = estimation;
        }
    }

    move_ctx->answers -= answer_count;
    return -result;
}

int robust_ai_do_move(struct ai * restrict ai, struct move_ctx * restrict move_ctx)
{
    struct robust_ai * restrict me = get_robust_ai(ai);
    int answer_count = move_ctx->answer_count;
    const struct position * answers = move_ctx->answers;

    if (answer_count == 0) {
        printf("Internal error: call robust_ai_do_move with move_ctx->answer_count = 0.\n");
        return -1;
    }

    if (answer_count == 1) {
        return 0;
    }

    move_ctx->answers = me->buf;

    int best_estimation = MIN_ESTIMATION;
    int best_move = 0;
    for (size_t i=0; i<answer_count; ++i) {
        int estimation = -recursive_estimate(me, move_ctx, answers + i);
        // printf(">>> %d\n", estimation);
        estimation += random10();
        if (estimation > best_estimation) {
            best_move = i;
            best_estimation = estimation;
        }
    }

    // printf("Estimation: %d\n", best_estimation);
    return best_move;
}

const struct keyword_tracker * robust_ai_get_supported_param(const struct ai * const ai)
{
    const struct robust_ai * const me = cget_robust_ai(ai);
    return me->params;
}

static inline void robust_ai_set_param(
    struct ai * restrict const ai,
    const int param_id,
    struct line_parser * restrict const lp)
{
    struct robust_ai * restrict const me = get_robust_ai(ai);
    static const int handler_len = sizeof(set_param_handlers) / sizeof(set_param_handlers[0]);
    if (param_id <= 0 || param_id >= handler_len) {
        fprintf(stderr, "Error, param with id = %d is not supported. The valid range is from 1 to %d.\n", param_id, handler_len - 1);
        return;
    }

    set_param_handlers[param_id](me, lp);
}

void robust_ai_free(struct ai * restrict ai)
{
    struct robust_ai * restrict me = get_robust_ai(ai);
    free_mempool(me->pool);
}

struct ai * create_robust_ai()
{
    randomize();

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

    me->params = build_keyword_tracker(robust_params, pool, KW_TRACKER__IGNORE_CASE);
    if (me->params == NULL) {
        printf("Error: build_keyword_tracker(...) failed.\n");
        free_mempool(pool);
        return NULL;
    }

    me->pool = pool;
    me->buf = buf;
    me->base.set_position = robust_ai_set_position;
    me->base.do_move = robust_ai_do_move;
    me->base.get_supported_param = robust_ai_get_supported_param;
    me->base.set_param = robust_ai_set_param;
    me->base.free = robust_ai_free;

    me->depth = DEFAULT_DEPTH;
    me->use_etb = 0;

    return &me->base;
}
