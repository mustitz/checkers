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
static const uint64_t CHOOSE_OVERFLOW = ~0ull;

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
        *data++ = is_overflow ? CHOOSE_OVERFLOW : value;
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

uint64_t position_to_index(
    const struct position * const position,
    const struct position_code_info * const info)
{
    return 1;
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

#endif
