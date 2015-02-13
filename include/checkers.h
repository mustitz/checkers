#ifndef MU__CHECKERS__H__
#define MU__CHECKERS__H__

#include <memory.h>
#include <stdint.h>





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

static inline square_t get_first_square(const bitboard_t bitboard)
{
    return __builtin_ctz(bitboard);
}

static inline int pop_count(const bitboard_t bitboard)
{
    return __builtin_popcount(bitboard);
}

static inline bitboard_t rotate_u7(const bitboard_t bitboard)
{
    return (bitboard << 7) | (bitboard >> 25);
}

static inline bitboard_t rotate_d7(const bitboard_t bitboard)
{
    return (bitboard >> 7) | (bitboard << 25);
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

extern int square_to_index[32];
extern square_t index_to_square[64];
extern const char * const upper_square_str[32];
extern const char * const lower_square_str[32];
extern struct square_magic square_magic[32];
extern struct mam_take_magic mam_take_magic_1[32][256];
extern struct mam_take_magic mam_take_magic_7[32][256];





/*
 *   MOVE GENERATION
 */

#define TAKING_SIZE   64
#define NEXT_TAKING(index) (index = (index + 1) % TAKING_SIZE)

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
    struct position position;

    int mam_taking_last;
    struct taking mam_taking[TAKING_SIZE];
    struct move_inner mam_move_inner[TAKING_SIZE];

    int sim_taking_last;
    struct taking sim_taking[TAKING_SIZE];
    struct move_inner sim_move_inner[TAKING_SIZE];

    int answer_count;
    struct position * restrict answers;
    struct move * restrict moves;
};

#endif
