#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

#define PARAM_USE_ETB     1

#define ITEM(name) { #name, PARAM_##name }
struct keyword_desc random_params[] = {
    ITEM(USE_ETB),
    { NULL, 0 }
};
#undef ITEM

struct random_ai
{
    struct ai base;
    struct mempool * pool;
    struct keyword_tracker * params;
    int use_etb;
};

struct random_ai * get_random_ai(struct ai * me)
{
    return move_ptr(me, -offsetof(struct random_ai, base));
}

const struct random_ai * cget_random_ai(const struct ai * const me)
{
    return move_cptr(me, -offsetof(struct random_ai, base));
}

void random_ai_set_position(
    struct ai * restrict const me,
    const struct position * const position)
{
}

int random_ai_do_move(
    struct ai * restrict const me,
    struct move_ctx * restrict const move_ctx)
{
    if (move_ctx->answer_count == 0) {
        printf("Internal error: call random_ai_do_move with move_ctx->answer_count = 0.\n");
        return -1;
    }

    return rand() % move_ctx->answer_count;
}

const struct keyword_tracker * random_ai_get_supported_param(
    const struct ai * const ai)
{
    const struct random_ai * const me = cget_random_ai(ai);
    return me->params;
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
    struct random_ai * restrict const me,
    struct line_parser * restrict const lp);

static void set_use_etb(
    struct random_ai * restrict const me,
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

static inline void random_ai_set_param(
    struct ai * restrict const ai,
    const int param_id,
    struct line_parser * restrict const lp)
{
    struct random_ai * restrict const me = get_random_ai(ai);
    static const int handler_len = sizeof(set_param_handlers) / sizeof(set_param_handlers[0]);
    if (param_id <= 0 || param_id >= handler_len) {
        fprintf(stderr, "Error, param with id = %d is not supported. The valid range is from 1 to %d.\n", param_id, handler_len - 1);
        return;
    }

    set_param_handlers[param_id](me, lp);
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

    me->params = build_keyword_tracker(random_params, pool, KW_TRACKER__IGNORE_CASE);
    if (me->params == NULL) {
        printf("Error: build_keyword_tracker(...) failed.\n");
        free_mempool(pool);
        return NULL;
    }

    me->pool = pool;
    me->base.set_position = random_ai_set_position;
    me->base.do_move = random_ai_do_move;
    me->base.get_supported_param = random_ai_get_supported_param;
    me->base.set_param = random_ai_set_param;
    me->base.free = random_ai_free;

    me->use_etb = 0;

    return &me->base;
}
