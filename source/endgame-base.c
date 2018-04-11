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

static void init_endgame_entry(
    char * restrict const filepath,
    const size_t filepath_len,
    const int wsim,
    const int wmam,
    const int bsim,
    const int bmam)
{
    const int wall = wsim + wmam;
    const int ball = bsim + bmam;

    const int wcode = calc_code_and_check(wall, wsim, "wcode");
    const int bcode = calc_code_and_check(ball, bsim, "bcode");

    struct position_code_info * restrict const info = &position_codes[wcode][bcode];

    const int is_reversed = wcode > bcode;
    info->is_reversed = is_reversed;
    const int lo_code = is_reversed ? bcode : wcode;
    const int hi_code = is_reversed ? wcode : bcode;

    {
        const int status = snprintf(info->filename, 15, "%02d%02d.ceitb", lo_code, hi_code);
        if (status < 0) {
            static int error_count = 0;
            if (error_count++ == 0) {
                fprintf(stderr, "sprintf error in %s:%d\n", __FILE__, __LINE__);
                return;
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
                return;
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
        for (int bmam = 0; bmam <= 12 - bsim; ++bsim) {

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

int position_to_code(const struct position * const position)
{
    int all_0 = pop_count(position->bitboards[IDX_ALL_0]);
    int sim_0 = pop_count(position->bitboards[IDX_SIM_0]);
    int all_1 = pop_count(position->bitboards[IDX_ALL_1]);
    int sim_1 = pop_count(position->bitboards[IDX_SIM_1]);

    if (all_0 <= 0 || all_0 > 12 || all_1 <= 0 || all_1 > 12) {
        return -1;
    }

    int idx_0 = bitboard_stat_to_code(all_0, sim_0);
    int idx_1 = bitboard_stat_to_code(all_1, sim_1);
    return idx_0 + SIDE_BITBOARD_CODE_COUNT * idx_1;
}

uint64_t position_to_index(
    const struct position * const position,
    const int code)
{
    return 1;
}
