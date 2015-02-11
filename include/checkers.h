#ifndef MU__CHECKERS__H__
#define MU__CHECKERS__H__

#include <stdint.h>

typedef uint32_t square_t;
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

static inline enum square_t get_first_square(const bitboard_t bitboard)
{
    return __builtin_ctz(bitboard);
}

static inline int pop_count(const bitboard_t bitboard)
{
    return __builtin_popcount(bitboard);
}

static inline bitboard_t rotate_l7(const bitboard_t bitboard)
{
    return (bitboard << 7) | (bitboard >> 25);
}

static inline bitboard_t rotate_r7(const bitboard_t bitboard)
{
    return (bitboard >> 7) | (bitboard << 25);
}

extern int square_to_index[32];
extern square_t index_to_square[64];
extern const char * const upper_square_str[32];
extern const char * const lower_square_str[32];

#endif
