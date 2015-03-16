#include "checkers.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct game * create_game(struct mempool * restrict pool)
{
    struct game * me = mempool_alloc(pool, sizeof(struct game));
    if (me == NULL) {
        panic("Can not allocate me in create_game.");
    }

    me->ai = NULL;
    me->verbose_move_count = -1;

    me->position = mempool_alloc(pool, sizeof(struct position));
    if (me->position == NULL) {
        panic("Can not allocate position in create_game.");
    }

    me->position->active = WHITE;
    me->position->bitboards[IDX_ALL_0] = RANK_1 | RANK_2 | RANK_3;
    me->position->bitboards[IDX_SIM_0] = me->position->bitboards[IDX_ALL_0];
    me->position->bitboards[IDX_ALL_1] = RANK_6 | RANK_7 | RANK_8;
    me->position->bitboards[IDX_SIM_1] = me->position->bitboards[IDX_ALL_1];

    me->move_ctx = mempool_alloc(pool, sizeof(struct move_ctx));
    if (me->move_ctx == NULL) {
        panic("Can not allocate move_ctx in create_game.");
    }

    me->verbose_moves = mempool_alloc(pool, BUF_SIZE__VERBOSE_MOVE * sizeof(struct verbose_move));
    if (me->verbose_moves == NULL) {
        panic("Can not allocate verbose_moves in create_game.");
    }

    me->move_ctx->position = me->position;

    me->move_ctx->sim_taking = mempool_alloc(pool, BUF_SIZE__TAKING * sizeof(struct taking)); 
    if (me->move_ctx->sim_taking == NULL) {
        panic("Can not allocate move_ctx->sim_taking in create_game.");
    }

    me->move_ctx->mam_taking = mempool_alloc(pool, BUF_SIZE__TAKING * sizeof(struct taking));
    if (me->move_ctx->mam_taking == NULL) {
        panic("Can not allocate move_ctx->mam_taking in create_game.");
    }

    me->move_ctx->sim_move_inner = mempool_alloc(pool, BUF_SIZE__TAKING * sizeof(struct move_inner));
    if (me->move_ctx->sim_move_inner == NULL) {
        panic("Can not allocate move_ctx->sim_move_inner in create_game.");
    }

    me->move_ctx->mam_move_inner = mempool_alloc(pool, BUF_SIZE__TAKING * sizeof(struct move_inner));
    if (me->move_ctx->mam_move_inner == NULL) {
        panic("Can not allocate move_ctx->mam_move_inner in create_game.");
    }

    me->move_ctx->answers = mempool_alloc(pool, BUF_SIZE__ANSWERS * sizeof(struct position));
    if (me->move_ctx->answers == NULL) {
        panic("Can not allocate move_ctx->answers in create_game.");
    }

    me->move_ctx->moves = mempool_alloc(pool, BUF_SIZE__ANSWERS * sizeof(struct move));
    if (me->move_ctx->moves == NULL) {
        panic("Can not allocate move_ctx->moves in create_game.");
    }

    return me;
}



static void game_gen_verbose_moves(struct game * restrict me);

static void game_gen_moves(struct game * restrict me)
{
    if (me->verbose_move_count == -1) {
        user_friendly_gen_moves(me->move_ctx);
        game_gen_verbose_moves(me);
    }
}

static int cmp_verbose_moves(const void * a, const void * b)
{
    const struct verbose_move * verbose_move_a = a;
    const struct verbose_move * verbose_move_b = b;
    const int * squares_a = verbose_move_a->squares;
    const int * squares_b = verbose_move_b->squares;
    
    for (int i=0; i<12; ++i) {
        if (squares_a[i] == squares_b[i]) {
            continue;
        }

        if (squares_a[i] == -1) return -1;
        if (squares_b[i] == -1) return +1;
        int tmp_a = squares_a[i] | ((squares_a[i] & 7) << 10);
        int tmp_b = squares_b[i] | ((squares_b[i] & 7) << 10);
        if (tmp_a < tmp_b) return -1;
        if (tmp_a > tmp_b) return +1;
    }

    return memcmp(a, b, sizeof(struct verbose_move));
}

static void game_gen_verbose_moves(struct game * restrict me)
{
    int answer_count = me->move_ctx->answer_count;
    const struct move * moves = me->move_ctx->moves;

    bitboard_t bitboards[14];
    int indexes[14];
    int max_indexes[14];

    me->verbose_move_count = 0;
    for (int i=0; i<answer_count; ++i) {
        const struct move * move = moves + i;

        int bitboard_count = 0;
        bitboards[bitboard_count++] = move->from;
        for (int j=0; j<move->inner.count; ++j) {
            bitboards[bitboard_count++] = move->inner.values[j];
        }
        bitboards[bitboard_count++] = move->to;

        for (int j=0; j<bitboard_count; ++j) {
            indexes[j] = 0;
            max_indexes[j] = pop_count(bitboards[j]);
        }

        bitboard_t current[14];
        memcpy(current, bitboards, 14 * sizeof(bitboard_t));

        int k;
        do {

            struct verbose_move * restrict verbose_move = me->verbose_moves + me->verbose_move_count;
            for (int j=0; j<12; ++j) {
                if (j < bitboard_count) {
                    square_t sq = get_first_square(current[j]);
                    verbose_move->squares[j] = square_to_index[sq]; 
                } else {
                    verbose_move->squares[j] = -1;
                }
            }

            verbose_move->len = bitboard_count;
            verbose_move->is_mam = move->is_mam;
            verbose_move->is_taking = !! move->killed; 
            verbose_move->index = i;
            ++me->verbose_move_count;

            k = 0;
            for (;;) {
                if (k >= bitboard_count) {
                    break;
                }
                ++indexes[k];
                if (indexes[k] < max_indexes[k]) {
                    current[k] &= current[k] - 1;
                    break;
                }

                indexes[k] = 0;
                current[k] = bitboards[k];
                k++;
            }
        } while (k < bitboard_count);
    }

    qsort(me->verbose_moves, me->verbose_move_count, sizeof(struct verbose_move), cmp_verbose_moves);
}



static void game_print_verbose_moves(struct game * restrict me);

void game_move_list(struct game * restrict me)
{
    game_gen_moves(me);
    game_print_verbose_moves(me);
}



static void print_verbose_move(const struct verbose_move * verbose_move);

static void game_print_verbose_moves(struct game * restrict me)
{
    for (int i=0; i<me->verbose_move_count; ++i) {
        printf("%d\t", i+1);
        print_verbose_move(me->verbose_moves + i);
    }
}

static void print_verbose_move(const struct verbose_move * verbose_move)
{
    const char * delimeter = verbose_move->is_taking ? ":" : "-";
    printf("%s", lower_index_str(verbose_move->squares[0]));
    for (int i=1; i<verbose_move->len; ++i) {
        printf("%s%s", delimeter, lower_index_str(verbose_move->squares[i]));
    }
    printf("\n");
}

void game_move_select(struct game * restrict me, int num)
{
    if (num <= 0) {
        printf("Error: move number should pe positive.\n");
        return;
    }

    game_gen_moves(me);
    if (me->verbose_move_count == 0) {
        printf("Error: no moves possible.\n");
        return;
    }

    if (num > me->verbose_move_count) {
        printf("Error: invalid move no, should be in range from 1 to %d.\n", me->verbose_move_count);
        return;
    }

    int answer_index = me->verbose_moves[num-1].index;
    game_set_position(me, me->move_ctx->answers + answer_index);
}

void print_side(const char * side, bitboard_t sim, bitboard_t mam);

void game_print_fen(const struct game * me)
{
    const struct position * position = me->position;
    
    if (position->active == WHITE) {
        printf("W:");
    }
    if (position->active == BLACK) {
        printf("B:");
    }
    bitboard_t ws = position->bitboards[IDX_SIM_0];
    bitboard_t wm = position->bitboards[IDX_ALL_0] ^ ws;
    bitboard_t bs = position->bitboards[IDX_SIM_1];
    bitboard_t bm = position->bitboards[IDX_ALL_1] ^ bs;

    if (ws | wm) {
        print_side("W", ws, wm);
        if (bs | bm) {
            printf(":");
        }
    }

    if (bs | bm) {
        print_side("B", bs, bm);
    }

    printf("\n");
}

void print_bitboard(const char * prefix, bitboard_t bb);

void print_side(const char * side, bitboard_t sim, bitboard_t mam)
{
    if (sim | mam) {
        printf("%s", side);
    }

    if (sim) {
        print_bitboard("", sim);
        if (mam) {
            printf(",");
        }
    }

    if (mam) {
        print_bitboard("K", mam);
    }
}

static int cmp_str(const void * a, const void * b)
{
    const char * const * ptr_a = a;
    const char * const * ptr_b = b;
    return strcmp(*ptr_a, *ptr_b);
}

void print_bitboard(const char * prefix, bitboard_t bb)
{
    int n = pop_count(bb);
    const char * lower_str[n];

    int i = 0;
    while (bb) {
        enum square_t sq = get_first_square(bb);
        bb &= bb - 1;
        lower_str[i++] = lower_square_str[sq]; 
    }

    assert(i == n);

    qsort(lower_str, n, sizeof(const char *), cmp_str);

    if (n > 0) {
        printf("%s%s", prefix, lower_str[0]);
    }

    for (i=1; i<n; ++i) {
        printf(",%s%s", prefix, lower_str[i]);
    }
}

static inline const unsigned char * skip_spaces(const unsigned char * fen)
{
    for (;;) {
        if (*fen == '\0') return fen;
        if (*fen > ' ') return fen;
        ++fen;
    }

}

void game_set_position(struct game * restrict me, const struct position * position)
{
    *me->position = *position;
    me->verbose_move_count = -1;
}

void game_ai_select(struct game * restrict me)
{
    if (me->ai == NULL) {
        me->ai = create_ai();
        if (me->ai == NULL) {
            printf("Error: can not create AI object, create_ai returns NULL.\n");
            return;
        }
    }

    game_gen_moves(me);
    const struct move_ctx * move_ctx = me->move_ctx;
    if (move_ctx->answer_count == 0) {
        printf("Error: no moves possible.\n");
        return;
    }

    int num = ai_do_move(me->ai, move_ctx);
    if (num >= move_ctx->answer_count) {
        printf("Engine error: invalid move no, should be in range from 0 to %d.\n", move_ctx->answer_count-1);
        return;
    }

    for (int i=0; i<me->verbose_move_count; ++i) {
        const struct verbose_move * verbose_move = me->verbose_moves + i;
        if (verbose_move->index == num) {
            game_set_position(me, me->move_ctx->answers + num);
            print_verbose_move(verbose_move);
            return;
        }
    }

    printf("Internal error: verbose move is not found.\n");
}

void game_ai_free(struct game * restrict me)
{
    struct ai * restrict ai = me->ai;
    me->ai = NULL;
    if (ai != NULL) {
        ai_free(ai);
    }
}
