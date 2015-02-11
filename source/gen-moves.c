#include "checkers.h"

#include <string.h>

static const bitboard_t promotion[2] = { RANK_8, RANK_1 };

static const bitboard_t possible_u1 = BOARD & ~RANK_7 & ~RANK_8 & ~FILE_A & ~FILE_B;
static const bitboard_t possible_d1 = BOARD & ~RANK_1 & ~RANK_2 & ~FILE_G & ~FILE_H;
static const bitboard_t possible_u7 = BOARD & ~RANK_7 & ~RANK_8 & ~FILE_G & ~FILE_H;
static const bitboard_t possible_d7 = BOARD & ~RANK_1 & ~RANK_2 & ~FILE_A & ~FILE_B;

static const bitboard_t can_move_u1 = BOARD & ~RANK_8 & ~FILE_A;
static const bitboard_t can_move_u7 = BOARD & ~RANK_8 & ~FILE_H;
static const bitboard_t can_move_d1 = BOARD & ~RANK_1 & ~FILE_H;
static const bitboard_t can_move_d7 = BOARD & ~RANK_1 & ~FILE_A;

#ifdef USER_FRIENDLY
    #undef USER_FRIENDLY
#endif
#define USER_FRIENDLY 0    

#include "inc.gen-moves.c"



#undef USER_FRIENDLY
#define USER_FRIENDLY 1
#define gen_moves        user_friendly_gen_moves
#define add_mam_takes    user_friendly_add_mam_takes 
#define gen_mam_takes    user_friendly_gen_mam_takes
#define add_sim_takes    user_friendly_add_sim_takes
#define gen_sim_takes    user_friendly_gen_sim_takes
#define gen_mam_moves    user_friendly_gen_mam_moves
#define gen_sim_moves    user_friendly_gen_sim_moves

#include "inc.gen-moves.c"



#undef USER_FRIENDLY
#undef gen_moves
#undef add_mam_takes 
#undef gen_mam_takes 
#undef add_sim_takes
#undef gen_sim_takes
#undef gen_mam_moves
#undef gen_sim_moves
