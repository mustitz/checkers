#ifndef MU__CHECKERS__H__
#define MU__CHECKERS__H__

#include <limits.h>
#include <memory.h>
#include <stdint.h>

#include "mu-data-struct.h"
#include "mu-parser.h"





/*
 * Math
 */

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define U64_OVERFLOW (~(uint64_t)0)

static inline uint64_t safe_mul(const uint64_t a, const uint64_t b)
{
    if (a == U64_OVERFLOW) {
        return U64_OVERFLOW;
    }

    if (b == U64_OVERFLOW) {
        return U64_OVERFLOW;
    }

    if (a == 0) {
        return 0;
    }

    const uint64_t result = a * b;
    if (result / a != b) {
        return U64_OVERFLOW;
    }

    return result;
}

extern const uint64_t choose[33][33];





/*
 * Buffer sizes
 */

#define BUF_SIZE__TAKING          64
#define BUF_SIZE__ANSWERS        128
#define BUF_SIZE__VERBOSE_MOVE   256





/*
 *   BOARD GEOMETRY
 */

typedef int square_t;
typedef uint32_t bitboard_t;

enum square_t {
    /* Illegal */ I9 = -1,
    /*     0   */ A1,
    /*     1   */ H4, G5, F6, E7, D8,
    /*     6   */ C1, B2, A3,
    /*     9   */ H6, G7, F8,
    /*    12   */ E1, D2, C3, B4, A5,
    /*    17   */ H8,
    /*    18   */ G1, F2, E3, D4, C5, B6, A7,
    /*    25   */ H2, G3, F4, E5, D6, C7, B8
};

#define MASK(field) ((bitboard_t)(1) << field)

#define RANK_1 (MASK(A1) | MASK(C1) | MASK(E1) | MASK(G1))
#define RANK_2 (MASK(B2) | MASK(D2) | MASK(F2) | MASK(H2))
#define RANK_3 (MASK(A3) | MASK(C3) | MASK(E3) | MASK(G3))
#define RANK_4 (MASK(B4) | MASK(D4) | MASK(F4) | MASK(H4))
#define RANK_5 (MASK(A5) | MASK(C5) | MASK(E5) | MASK(G5))
#define RANK_6 (MASK(B6) | MASK(D6) | MASK(F6) | MASK(H6))
#define RANK_7 (MASK(A7) | MASK(C7) | MASK(E7) | MASK(G7))
#define RANK_8 (MASK(B8) | MASK(D8) | MASK(F8) | MASK(H8))

#define FILE_A (MASK(A1) | MASK(A3) | MASK(A5) | MASK(A7))
#define FILE_B (MASK(B2) | MASK(B4) | MASK(B6) | MASK(B8))
#define FILE_C (MASK(C1) | MASK(C3) | MASK(C5) | MASK(C7))
#define FILE_D (MASK(D2) | MASK(D4) | MASK(D6) | MASK(D8))
#define FILE_E (MASK(E1) | MASK(E3) | MASK(E5) | MASK(E7))
#define FILE_F (MASK(F2) | MASK(F4) | MASK(F6) | MASK(F8))
#define FILE_G (MASK(G1) | MASK(G3) | MASK(G5) | MASK(G7))
#define FILE_H (MASK(H2) | MASK(H4) | MASK(H6) | MASK(H8))

#define BOARD (RANK_1 | RANK_2 | RANK_3 | RANK_4 | RANK_5 | RANK_6 | RANK_7 | RANK_8)

static inline square_t get_first_square(bitboard_t bb)
{
    return __builtin_ctz(bb);
}

static inline square_t extract_first_square(bitboard_t * bb)
{
    square_t result = get_first_square(*bb);
    *bb &= *bb - 1;
    return result;
}

static inline int pop_count(bitboard_t bb)
{
    return __builtin_popcount(bb);
}

static inline bitboard_t rotate_u7(bitboard_t bb)
{
    return (bb << 7) | (bb >> 25);
}

static inline bitboard_t rotate_d7(bitboard_t bb)
{
    return (bb >> 7) | (bb << 25);
}

static inline int get_bit(bitboard_t bb, int n)
{
    return ((1 << n) & bb) != 0;
}

struct square_magic
{
    bitboard_t mask7;
    uint32_t factor7;
    int shift1;
};

struct mam_take_magic
{
    bitboard_t dead[2];
    bitboard_t next[2];
};

extern const int square_to_index[32];
extern const square_t index_to_square[64];
extern const char * const upper_square_str[32];
extern const char * const lower_square_str[32];
extern const struct square_magic square_magic[32];
extern const struct mam_take_magic mam_take_magic_1[32][256];
extern const struct mam_take_magic mam_take_magic_7[32][256];

static inline const char * upper_index_str(int index)
{
    return upper_square_str[index_to_square[index]];
}

static inline const char * lower_index_str(int index)
{
    return lower_square_str[index_to_square[index]];
}





/*
 *   MOVE GENERATION
 */

#define NEXT_TAKING(index) (index = (index + 1) % BUF_SIZE__TAKING)

typedef int side_t;
typedef int piece_t;

enum side_t { WHITE, BLACK };
enum piece_t { ALL, SINGLE };
enum position_bitboard_index_t {
    IDX_ALL = 0,
    IDX_ALL_0 = 0,
    IDX_ALL_1 = 1,
    IDX_SIM = 2,
    IDX_SIM_0 = 2,
    IDX_SIM_1 = 3
};

struct position
{
    bitboard_t bitboards[4];
    side_t active;
};

struct way
{
    unsigned int is_7 : 1;
    unsigned int is_down : 1;
};

struct taking
{
    bitboard_t from;
    bitboard_t killed;
    bitboard_t current;
    struct way way;
};

struct move_inner
{
    int count;
    bitboard_t values[11];
};

static inline void copy_inner(struct move_inner * restrict dest, const struct move_inner * src)
{
    dest->count = src->count;
    memcpy(dest->values, src->values, src->count * sizeof(bitboard_t));
}

struct move
{
    bitboard_t from;
    bitboard_t to;
    bitboard_t killed;
    int is_mam;
    struct move_inner inner;
};

struct move_ctx
{
    bitboard_t all;
    bitboard_t enemy;
    const struct position * position;

    int sim_taking_last;
    int mam_taking_last;
    struct taking * restrict sim_taking;
    struct taking * restrict mam_taking;
    struct move_inner * restrict sim_move_inner;
    struct move_inner * restrict mam_move_inner;

    int answer_count;
    struct position * restrict answers;
    struct move * restrict moves;
};

void gen_moves(struct move_ctx * restrict ctx);
void user_friendly_gen_moves(struct move_ctx * restrict ctx);
void position_print_fen(const struct position * position);





/*
 * AI Object
 */

struct ai
{
    void (*set_position)(
        struct ai * restrict const me,
        const struct position * const position);
    int (*do_move)(
        struct ai * restrict const me,
        struct move_ctx * restrict const move_ctx);
    const struct keyword_tracker * (*get_supported_param)(
        const struct ai * const ai);
    void (*set_param)(
        struct ai * restrict const me,
        const int param_id,
        struct line_parser * restrict const lp);
    void (*info)(const struct ai * const me);
    void (*free)(struct ai * restrict me);
};

struct ai * create_random_ai(void);
struct ai * create_robust_ai(void);
struct ai * create_mcts_ai(void);

static inline void ai_set_position(
    struct ai * restrict const me,
    const struct position * const position)
{
    return me->set_position(me, position);
}

static inline int ai_do_move(
    struct ai * restrict me,
    struct move_ctx * restrict const move_ctx)
{
    return me->do_move(me, move_ctx);
}

static inline const struct keyword_tracker * ai_get_supported_param(
    const struct ai * const me)
{
    return me->get_supported_param(me);
}

static inline void ai_set_param(
    struct ai * restrict const me,
    const int param_id,
    struct line_parser * restrict const lp)
{
    me->set_param(me, param_id, lp);
}

static inline void ai_info(const struct ai * const me)
{
    return me->info(me);
}

static inline void ai_free(struct ai * restrict me)
{
    return me->free(me);
}

#define create_ai create_robust_ai





/*
 * Game object
 */

struct verbose_move
{
    square_t squares[12];
    int len;
    int is_mam;
    int is_taking;
    int index;
};

struct game
{
    struct position * restrict position;
    struct move_ctx * restrict move_ctx;
    struct verbose_move * restrict verbose_moves;
    struct ai * restrict ai;
    int verbose_move_count;
    int current_ai;
};

struct game * create_game(struct mempool * restrict pool);
void game_move_list(struct game * restrict me);
void game_move_select(struct game * restrict me, int num);
void game_print_fen(const struct game * me);
void game_set_position(struct game * restrict me, const struct position * position);
struct ai * get_game_ai(struct game * restrict const me);
void game_ai_select(struct game * restrict me, const int is_hint);
void game_ai_free(struct game * restrict me);
void game_ai_list(struct game * restrict me);
void game_set_ai(struct game * restrict me, const char * name, int name_len);
void game_etb_info(struct game * restrict const me);





/*
 * Endgame tablebase API
 */

#define SIDE_BITBOARD_CODE_COUNT   90
#define ETB_NA                   -128

struct position_code_info
{
    const struct position_code_info * base;
    int wcode, wall, wsim, wmam;
    int bcode, ball, bsim, bmam;
    char filename[16];
    uint64_t fr_offsets[6];
    uint64_t total;
};

extern const bitboard_t reverse_table[4][256];
extern const struct position_code_info position_code_infos[SIDE_BITBOARD_CODE_COUNT][SIDE_BITBOARD_CODE_COUNT];
extern void * position_code_data[SIDE_BITBOARD_CODE_COUNT][SIDE_BITBOARD_CODE_COUNT];

static inline int bitboard_stat_to_code(const int all_0, const int sim_0)
{
    return ( all_0 * (all_0+3) - 2 * (sim_0+1) ) >> 1;
}

uint64_t position_to_index(
    const struct position * const position,
    const struct position_code_info * const info);

void gen_etb(const int wall, const int wsim, const int ball, const int bsim);
void etb_set_dir(const char * dir, const int len);
void etb_status(void);
void etb_free(void);
void etb_load_all(void);
int8_t etb_estimate(const struct position * const position);
void etb_index(const struct position * const position);

static inline int8_t etb_lookup(
    const struct position * const position,
    const int use_etb)
{
    const bitboard_t * const bitboards = position->bitboards;
    const bitboard_t all = bitboards[IDX_ALL_0] | bitboards[IDX_ALL_1];

    if (pop_count(all) > use_etb) {
        return ETB_NA;
    }

    return etb_estimate(position);
}

static inline int etb_grade(
    const struct position * const position,
    const int big_offset)
{
    int8_t estimation = etb_estimate(position);
    if (estimation == ETB_NA) return INT_MIN;

    const bitboard_t bb_sim = position->bitboards[IDX_SIM_0] | position->bitboards[IDX_SIM_1];
    const bitboard_t bb_all = position->bitboards[IDX_ALL_0] | position->bitboards[IDX_ALL_1];
    const bitboard_t bb_mam = bb_all ^ bb_sim;

    const int qfree = 32 - pop_count(bb_all);
    const int qmam = pop_count(bb_mam);
    const int grade = 32 * qfree + qmam;
    const int offset = big_offset + 100 * grade;

    if (estimation < 0) return +offset + estimation;
    if (estimation > 0) return -offset + estimation;
    return 0;
}

void etb_shm_create(void);
void etb_shm_destroy(void);
void etb_shm_use(void);

#endif
