#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

struct position_code_info position_codes[SIDE_BITBOARD_CODE_COUNT][SIDE_BITBOARD_CODE_COUNT];

const char * endgame_dir = ".";

void read_endgame_tablebase(struct position_code_info * restrict const info, FILE * restrict const f)
{
    fprintf(stderr, "Warning: read_endgame_tablebase is not implemeted.\n");
}

static inline int calc_code_and_check(const int all, const int sim, const char * const name)
{
    const int code = bitboard_stat_to_code(all, sim);

    if (code >= 0 && code < SIDE_BITBOARD_CODE_COUNT) {
        return code;
    }

    fprintf(stderr, "Fatal: %s = %d out of range [0 ... %d)\n", name, code, SIDE_BITBOARD_CODE_COUNT);
    fprintf(stderr, " all = %d, sim = %d.\n", all, sim);
    exit(1);
}

static uint64_t choose[33][33];
static const uint64_t U64_OVERFLOW = ~0ull;

static void init_choose(void)
{
	uint64_t * restrict data = &choose[0][0];

    for (int n=0; n<33; ++n)
    for (int k=0; k<33; ++k) {
        if (k > n) {
            *data++ = 0;
            continue;
        }
        if (k == n || k == 0) {
            *data++ = 1;
            continue;
        }

        const uint64_t prev1 = data[-34];
        const uint64_t prev2 = data[-33];
        const uint64_t value = prev1 + prev2;
        const int is_overflow = value < prev1 || value < prev2;
        *data++ = is_overflow ? U64_OVERFLOW : value;
    }
}

uint64_t safe_mul(const uint64_t a, const uint64_t b)
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



static void init_position_info_offset(
    struct position_code_info * restrict const info)
{
    const int wsim = info->wsim;
    const int bsim = info->bsim;

    uint64_t * restrict offsets = info->fr_offsets;
    const uint64_t * const begin = offsets + 1;
    const uint64_t * const end = offsets + 6;

    *offsets++ = 0;
    for (; offsets != end; ++offsets) {
        const int wfsim = offsets - begin;
        const int wosim = wsim - wfsim;
        if (wosim < 0) {
            offsets[0] = offsets[-1];
            continue;
        }

        const uint64_t m1 = choose[4][wfsim];
        const uint64_t m2 = choose[24][wosim];
        const uint64_t m3 = safe_mul(m1, m2);
        const uint64_t m4 = choose[28-wosim][bsim];
        const uint64_t m5 = safe_mul(m3, m4);

        if (m5 == U64_OVERFLOW) {
            fprintf(stderr, "U64 multiplicatation oveflow: %lu * %lu * %lu.\n", m1, m2, m4);
            exit(1);
        }

        offsets[0] = offsets[-1] + m5;
    }
}



static void init_endgame_entry(
    char * restrict const filepath,
    const size_t filepath_len,
    const int wsim,
    const int wmam,
    const int bsim,
    const int bmam)
{
    init_choose();

    const int wall = wsim + wmam;
    const int ball = bsim + bmam;

    const int wcode = calc_code_and_check(wall, wsim, "wcode");
    const int bcode = calc_code_and_check(ball, bsim, "bcode");

    struct position_code_info * restrict const info = &position_codes[wcode][bcode];

    const int is_reversed = wcode < bcode;

    info->is_reversed = is_reversed;
    if (!is_reversed) {
        info->wall = wall;
        info->wsim = wsim;
        info->wmam = wmam;
        info->ball = ball;
        info->bsim = bsim;
        info->bmam = bmam;
    } else {
        info->wall = ball;
        info->wsim = bsim;
        info->wmam = bmam;
        info->ball = wall;
        info->bsim = wsim;
        info->bmam = wmam;
    }

    init_position_info_offset(info);

    const int lo_code = is_reversed ? bcode : wcode;
    const int hi_code = is_reversed ? wcode : bcode;

    {
        const int status = snprintf(info->filename, 15, "%02d%02d.ceitb", lo_code, hi_code);
        if (status < 0) {
            static int error_count = 0;
            if (error_count++ == 0) {
                fprintf(stderr, "sprintf error in %s:%d\n", __FILE__, __LINE__);
                exit(1);
            }
        }
        info->filename[status] = '\0';
    }

    {
        const int status = snprintf(filepath, filepath_len-1, "%s/%s", endgame_dir, info->filename);
        if (status < 0) {
            static int error_count = 0;
            if (error_count++ == 0) {
                fprintf(stderr, "sprintf error in %s:%d\n", __FILE__, __LINE__);
                exit(1);
            }
        }
        filepath[status] = '\0';
    }

    FILE * f = fopen(filepath, "rb");
    if (f != NULL) {
        read_endgame_tablebase(info, f);
        fclose(f);
    }
}

void init_endgame_base(void)
{
    const size_t filepath_len = strlen(endgame_dir) + 17;
    char * restrict const filepath = malloc(filepath_len);

    for (int wsim = 0; wsim <= 12; ++wsim)
    for (int wmam = 0; wmam <= 12 - wsim; ++wmam) {

        if (wsim == 0 && wmam == 0) {
            continue;
        }

        for (int bsim = 0; bsim <= 12; ++bsim)
        for (int bmam = 0; bmam <= 12 - bsim; ++bmam) {

            if (bsim == 0 && bmam == 0) {
                continue;
            }

            init_endgame_entry(filepath, filepath_len, wsim, wmam, bsim, bmam);
        }
    }

    free(filepath);
}

void debug_position_code(void)
{
    static const int precision = 4;

    for (int sim = 0; sim <= 12; ++sim) {
        for (int mam = 0; mam <= 12; ++mam) {

            const int all = sim + mam;

            if (all == 0) {
                printf("%*s", precision, "-");
                continue;
            }

            if (all > 12) {
                break;
            }

            const int code = bitboard_stat_to_code(all, sim);
            printf("%*d", precision, code);
        }

        printf("\n");
    }
}

const struct position_code_info * get_position_info(const struct position * const position)
{
    int all_0 = pop_count(position->bitboards[IDX_ALL_0]);
    int sim_0 = pop_count(position->bitboards[IDX_SIM_0]);
    int all_1 = pop_count(position->bitboards[IDX_ALL_1]);
    int sim_1 = pop_count(position->bitboards[IDX_SIM_1]);

    if (all_0 <= 0 || all_0 > 12 || all_1 <= 0 || all_1 > 12) {
        return NULL;
    }

    int idx_0 = bitboard_stat_to_code(all_0, sim_0);
    int idx_1 = bitboard_stat_to_code(all_1, sim_1);
    return &position_codes[idx_0][idx_1];
}

static inline uint64_t cindex(const int len, const int * indexes)
{
    uint64_t result = 0;
    for (int i=1; i<=len; ++i) {
        result += choose[*indexes++][i];
    }

    return result;
}

static void calc_indexes(int * restrict indexes, bitboard_t bb, const bitboard_t used)
{
    while (bb != 0) {
        const square_t sq = get_first_square(bb);
        const bitboard_t sq_mask = bb & -bb;
        const bitboard_t used_before_sq = sq_mask - 1;
        int skip = pop_count(used_before_sq & used);
        *indexes++ = sq - skip;
        bb ^= sq_mask;
    }
}

uint64_t position_to_index(
    const struct position * const position,
    const struct position_code_info * const info)
{
    int indexes[12];

    const int wdelta = info->is_reversed;
    const int bdelta = !wdelta;

    const bitboard_t wall_bb = position->bitboards[IDX_ALL + wdelta];
    const bitboard_t ball_bb = position->bitboards[IDX_ALL + bdelta];
    const bitboard_t wsim_bb = position->bitboards[IDX_SIM + wdelta];
    const bitboard_t bsim_bb = position->bitboards[IDX_SIM + bdelta];
    const bitboard_t wmam_bb = wall_bb ^ wsim_bb;
    const bitboard_t bmam_bb = ball_bb ^ bsim_bb;

    const bitboard_t wfsim_bb = wsim_bb & RANK_1;
    const bitboard_t wosim_bb = wsim_bb & (BOARD ^ RANK_1);

    const int wfsim = pop_count(wfsim_bb);
    const int wosim = pop_count(wosim_bb);

    calc_indexes(indexes, wfsim_bb, BOARD ^ RANK_1);
    const uint64_t idx_wfsim = cindex(wfsim, indexes);

    calc_indexes(indexes, wosim_bb, RANK_1 | RANK_8);
    const uint64_t idx_wosim = cindex(wosim, indexes);

    calc_indexes(indexes, bsim_bb, RANK_1 | wosim_bb);
    const uint64_t idx_bsim = cindex(info->bsim, indexes);

    calc_indexes(indexes, wmam_bb, bsim_bb | wsim_bb);
    const uint64_t idx_wmam = cindex(info->wmam, indexes);

    calc_indexes(indexes, bmam_bb, wall_bb | bsim_bb);
    const uint64_t idx_bmam = cindex(info->bmam, indexes);

    const uint64_t wfsim_total = choose[4][wfsim];
    const uint64_t wosim_total = choose[24][wosim];

    uint64_t sim_result = idx_bsim;
    sim_result = sim_result * wosim_total + idx_wosim;
    sim_result = sim_result * wfsim_total + idx_wfsim;
    sim_result += info->fr_offsets[wfsim];

    const int free_squares = 32 - info->wsim - info->bsim;
    const uint64_t wmam_total = choose[free_squares][info->wmam];

    uint64_t mam_result = idx_bmam;
    mam_result = mam_result * wmam_total + idx_wmam;

    const uint64_t result = sim_result + mam_result * info->fr_offsets[5];
    return 2 * result + (position->active ^ wdelta);
}

void cdeindex(
    int len,
    int * const indexes,
    uint64_t index)
{
    if (len == 0) {
        return;
    }

    int * restrict ptr = indexes + len;
    uint64_t dec = 0;

    int c = len;
    for (;; ++c) {
        const uint64_t next_dec = choose[c][len];
        if (next_dec > index) {
            break;
        }
        dec = next_dec;
    }

    for (;;) {
        *--ptr = --c;
        if (ptr == indexes) {
            return;
        }

        --len;
        index -= dec;

        for (;; --c) {
            dec = choose[c-1][len];
            if (dec <= index) {
                break;
            }
        }
    }
}


/*
 * Tests
 */

#ifdef WITH_TESTS

static void check_choose(const int n, const int k, const uint64_t value)
{
    const uint64_t result = choose[n][k];
    if (result != value) {
        test_fail("Wrong choose C(%d, %d) = %lu, expected %lu.\n", n, k, result, value);
    }
}

static int test_choose()
{
    init_choose();
    check_choose(20,  2,       190);
    check_choose(32,  5,    201376);
    check_choose(28, 10,  13123110);
    check_choose(32, 16, 601080390);
    check_choose(31, 29,       465);
    check_choose(31, 32,         0);
    return 0;
}

static void check_offset(
    const struct position_code_info * const info,
    const int index,
    const uint64_t value)
{
    const uint64_t result = info->fr_offsets[index];
    if (result != value) {
        test_fail("Wrong offset %lu, expected %lu for index %d in position info struct.\n", result, value, index);
    }
}

static int test_fr_offsets()
{
    init_endgame_base();

    const int wcode = calc_code_and_check(3, 2, "wcode");
    const int bcode = calc_code_and_check(2, 1, "bcode");
    const struct position_code_info * const info = &position_codes[wcode][bcode];

    check_offset(info, 0, 0);
    check_offset(info, 1, 7176);
    check_offset(info, 2, 9768);
    check_offset(info, 3, 9936);
    check_offset(info, 4, 9936);
    check_offset(info, 5, 9936);
    return 0;
}

static int test_index_deindex()
{
    init_choose();

    static const int N = 4;
    int indexes[N];
    for (uint64_t index=0; index<5000; ++index) {
        cdeindex(N, indexes, index);
        uint64_t calculated = cindex(N, indexes);
        if (index != calculated) {
            test_fail("Wring index/deindex for %lu (%d, %d, %d, %d), calculated %lu.\n",
                index, indexes[0], indexes[1], indexes[2], indexes[3], calculated);
        }
    }

    return 0;
}

static int test_position_to_index()
{
    static const struct position position = {
        .active = WHITE,
        .bitboards = {
            [IDX_ALL_0] = MASK(G1) | MASK(B2) | MASK(G3),
            [IDX_ALL_1] = MASK(A5) | MASK(E7),
            [IDX_SIM_0] = MASK(G1) | MASK(B2),
            [IDX_SIM_1] = MASK(A5)
        }
    };

    init_endgame_base();
    const struct position_code_info * const info = get_position_info(&position);
    uint64_t index = position_to_index(&position, info);
    if (index != 2778902) {
        test_fail("Wrong position index, expected 2778902, returned %lu.\n", index);
    }

    return 0;
}

#endif
