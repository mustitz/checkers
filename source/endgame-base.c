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

static const int reverse_map[32] = {
    [A1] = H8, [B2] = G7, [C3] = F6, [D4] = E5, [E5] = D4, [F6] = C3, [G7] = B2, [H8] = A1,
    [C1] = F8, [D2] = E7, [E3] = D6, [F4] = C5, [G5] = B4, [H6] = A3,
    [A3] = H6, [B4] = G5, [C5] = F4, [D6] = E3, [E7] = D2, [F8] = C1,
    [E1] = D8, [F2] = C7, [G3] = B6, [H4] = A5,
    [A5] = H4, [B6] = G3, [C7] = F2, [D8] = E1,
    [G1] = B8, [H2] = A7,
    [A7] = H2, [B8] = G1
};

static bitboard_t reverse_table[4][256];

void init_reverse_table(void)
{
    for (int byte_num = 0; byte_num < 4; ++byte_num)
    for (int byte_val = 0; byte_val < 256; ++byte_val) {
        bitboard_t result = 0;
        for (int i=0; i<8; ++i) {
            const int is_set = (MASK(i) & byte_val) != 0;
            if (!is_set) {
                continue;
            }
            const int sq = 8*byte_num + i;
            result |= MASK(reverse_map[sq]);
        }
        reverse_table[byte_num][byte_val] = result;
    }
}

static inline bitboard_t reverse(const bitboard_t bb)
{
    const unsigned char * const indexes =(const unsigned char *) &bb;
    return 0
        | reverse_table[0][indexes[0]]
        | reverse_table[1][indexes[1]]
        | reverse_table[2][indexes[2]]
        | reverse_table[3][indexes[3]]
    ;
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
    bitboard_t bb_storage[4] ;

    const int is_reversed = info->is_reversed;
    const int active = is_reversed ^ position->active;

    if (is_reversed) {
        bb_storage[0] = reverse(position->bitboards[IDX_ALL_1]);
        bb_storage[1] = reverse(position->bitboards[IDX_ALL_0]);
        bb_storage[2] = reverse(position->bitboards[IDX_SIM_1]);
        bb_storage[3] = reverse(position->bitboards[IDX_SIM_0]);
    }

    const bitboard_t * const bbs = is_reversed ? bb_storage : position->bitboards;

    const bitboard_t wall_bb = bbs[IDX_ALL_0];
    const bitboard_t ball_bb = bbs[IDX_ALL_1];
    const bitboard_t wsim_bb = bbs[IDX_SIM_0];
    const bitboard_t bsim_bb = bbs[IDX_SIM_1];
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
    return 2 * result + active;
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

/* TODO: Very slow implementation, optimization is needed */
static bitboard_t indexes_to_bitboard(
    const int len,
    const int * indexes,
    const bitboard_t used)
{
    if (len == 0) {
        return 0;
    }

    const void * const end = indexes + len;

    bitboard_t result = 0;
    int index = 0;
    for (int i=0; i<32; ++i) {
        bitboard_t mask = MASK(i);
        if (used & mask) {
            continue;
        }
        if (index++ != *indexes) {
            continue;
        }

        result |= MASK(i);

        if (++indexes == end) {
            break;
        }
    }

    return result;
}

int index_to_position(
    struct position * restrict const position,
    const struct position_code_info * const info,
    uint64_t index)
{
    if (index == U64_OVERFLOW) {
        return -1;
    }

    const int is_reversed = info->is_reversed;
    const int active = (index % 2);
    index /= 2;

    uint64_t sim_index = index % info->fr_offsets[5];
    uint64_t mam_index = index / info->fr_offsets[5];

    const int wfsim = 0
        + (sim_index >= info->fr_offsets[1])
        + (sim_index >= info->fr_offsets[2])
        + (sim_index >= info->fr_offsets[3])
        + (sim_index >= info->fr_offsets[4])
    ;

    const int wosim = info->wsim - wfsim;
    sim_index -= info->fr_offsets[wfsim];

    const uint64_t wfsim_total = choose[4][wfsim];
    const uint64_t wosim_total = choose[24][wosim];

    const uint64_t idx_wfsim = sim_index % wfsim_total;
    sim_index /= wfsim_total;

    const uint64_t idx_wosim = sim_index % wosim_total;
    const uint64_t idx_bsim = sim_index / wosim_total;

    const int free_squares = 32 - info->wsim - info->bsim;
    const uint64_t wmam_total = choose[free_squares][info->wmam];

    const uint64_t idx_wmam = mam_index % wmam_total;
    const uint64_t idx_bmam = mam_index / wmam_total;

    int wfsim_indexes[wfsim];
    int wosim_indexes[wosim];
    int bsim_indexes[info->bsim];
    int wmam_indexes[info->wmam];
    int bmam_indexes[info->bmam];

    cdeindex(wfsim, wfsim_indexes, idx_wfsim);
    cdeindex(wosim, wosim_indexes, idx_wosim);
    cdeindex(info->bsim, bsim_indexes, idx_bsim);
    cdeindex(info->wmam, wmam_indexes, idx_wmam);
    cdeindex(info->bmam, bmam_indexes, idx_bmam);

    const bitboard_t wfsim_bb = indexes_to_bitboard(wfsim, wfsim_indexes, BOARD ^ RANK_1);
    const bitboard_t wosim_bb = indexes_to_bitboard(wosim, wosim_indexes, RANK_1 | RANK_7);
    const bitboard_t bsim_bb = indexes_to_bitboard(info->bsim, bsim_indexes, wosim_bb | RANK_1);

    const bitboard_t sim_bb = wfsim_bb | wosim_bb | bsim_bb;
    const bitboard_t wmam_bb = indexes_to_bitboard(info->wmam, wmam_indexes, sim_bb);
    const bitboard_t bmam_bb = indexes_to_bitboard(info->bmam, bmam_indexes, sim_bb | wmam_bb);

    const bitboard_t wsim_bb = wfsim_bb | wosim_bb;
    const bitboard_t wall_bb = wsim_bb | wmam_bb;
    const bitboard_t ball_bb = bsim_bb | bmam_bb;

    if (!is_reversed) {
        position->active = active;
        position->bitboards[IDX_ALL_0] = wall_bb;
        position->bitboards[IDX_ALL_1] = ball_bb;
        position->bitboards[IDX_SIM_0] = wsim_bb;
        position->bitboards[IDX_SIM_1] = bsim_bb;
    } else {
        position->active = !active;
        position->bitboards[IDX_ALL_0] = reverse(ball_bb);
        position->bitboards[IDX_ALL_1] = reverse(wall_bb);
        position->bitboards[IDX_SIM_0] = reverse(bsim_bb);
        position->bitboards[IDX_SIM_1] = reverse(wsim_bb);
    }

    return 0;
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

struct position_with_index
{
    struct position position;
    struct position rposition;
    uint64_t index;
};

struct position_with_index position_index_data[] = {
    {
        .position = {
            .active = WHITE,
            .bitboards = {
                [IDX_ALL_0] = MASK(G1) | MASK(B2) | MASK(G3),
                [IDX_ALL_1] = MASK(A5) | MASK(E7),
                [IDX_SIM_0] = MASK(G1) | MASK(B2),
                [IDX_SIM_1] = MASK(A5)
            }
        },
        .rposition = {
            .active = BLACK,
            .bitboards = {
                [IDX_ALL_0] = MASK(H4) | MASK(D2),
                [IDX_ALL_1] = MASK(B8) | MASK(G7) | MASK(B6),
                [IDX_SIM_0] = MASK(H4),
                [IDX_SIM_1] = MASK(B8) | MASK(G7),
            },
        },
        .index = 2778902
    }
};

static void test_one_position_to_index(
    const char * const title,
    const struct position * const position,
    const uint64_t index)
{
    const struct position_code_info * const info = get_position_info(position);
    const uint64_t calculated = position_to_index(position, info);
    if (calculated != index) {
        test_fail("Wrong %s position index, expected %lu, calculated %lu.\n", title, index, calculated);
    }
}

static int test_position_to_index()
{
    init_endgame_base();
    init_reverse_table();

    const int qtest_positions = sizeof(position_index_data) / sizeof(position_index_data[0]);
    const struct position_with_index * const end = position_index_data + qtest_positions;
    const struct position_with_index * ptr = position_index_data;

    for (; ptr != end; ++ptr) {
        test_one_position_to_index("normal", &ptr->position, ptr->index);
        test_one_position_to_index("reversed", &ptr->rposition, ptr->index);
    }

    return 0;
}

static void diff_positions(const struct position * const a, const struct position * const b)
{
    if (a->active != b->active) {
        fprintf(stderr, "Active a->%d, expected %d.\n", a->active, b->active);
    }
    if (a->bitboards[IDX_ALL_0] != b->bitboards[IDX_ALL_0]) {
        fprintf(stderr, "WALL a->0x%08X, expected 0x%08X.\n", a->bitboards[IDX_ALL_0], b->bitboards[IDX_ALL_0]);
    }
    if (a->bitboards[IDX_ALL_1] != b->bitboards[IDX_ALL_1]) {
        fprintf(stderr, "BALL a->0x%08X, expected 0x%08X.\n", a->bitboards[IDX_ALL_1], b->bitboards[IDX_ALL_1]);
    }
    if (a->bitboards[IDX_SIM_0] != b->bitboards[IDX_SIM_0]) {
        fprintf(stderr, "WSIM a->0x%08X, expected 0x%08X.\n", a->bitboards[IDX_SIM_0], b->bitboards[IDX_SIM_0]);
    }
    if (a->bitboards[IDX_SIM_1] != b->bitboards[IDX_SIM_1]) {
        fprintf(stderr, "BSIM a->0x%08X, expected 0x%08X.\n", a->bitboards[IDX_SIM_1], b->bitboards[IDX_SIM_1]);
    }
}

static inline int cmp_positions(const struct position * const a, const struct position * const b)
{
    if (memcmp(a, b, sizeof(struct position)) == 0) {
        return 0;
    }

    diff_positions(a, b);
    return 1;
}

static void test_one_index_to_position(
    const char * const title,
    const struct position * const position,
    const uint64_t index)
{
    struct position calculated;
    const struct position_code_info * const info = get_position_info(position);
    const int status = index_to_position(&calculated, info, index);

    if (status != 0) {
        test_fail("Wrong status %d for %s position.\n", status, title);
    }

    if (cmp_positions(&calculated, position) != 0) {
        test_fail("Position mismatch!\n");
    }
}

static int test_index_to_position()
{
    init_endgame_base();

    const int qtest_positions = sizeof(position_index_data) / sizeof(position_index_data[0]);
    const struct position_with_index * const end = position_index_data + qtest_positions;
    const struct position_with_index * ptr = position_index_data;

    for (; ptr != end; ++ptr) {
        test_one_index_to_position("normal", &ptr->position, ptr->index);
        // test_one_index_to_position("reversed", &ptr->rposition, ptr->index);
    }

    return 0;
}

static void check_reverse(const bitboard_t a, const bitboard_t b)
{
    const bitboard_t c = reverse(a);
    const bitboard_t d = reverse(b);

    if (b != c) {
        test_fail("Wrong reverse for bitboards 0x%08X (0x%08X) and 0x%08X (0x%08X)\n", a, c, b, d);
    }

    if (a != d) {
        test_fail("Wrong reverse for bitboards 0x%08X (0x%08X) and 0x%08X (0x%08X)\n", b, d, a, c);
    }
}

static int test_reverse()
{
    init_reverse_table();

    check_reverse(BOARD, BOARD);
    check_reverse(RANK_1, RANK_8);
    check_reverse(RANK_2, RANK_7);
    check_reverse(RANK_3, RANK_6);
    check_reverse(RANK_4, RANK_5);
    check_reverse(MASK(A1), MASK(H8));
    check_reverse(MASK(E3), MASK(D6));
    check_reverse(MASK(F4), MASK(C5));
    check_reverse(MASK(A5) | MASK(E7), MASK(H4) | MASK(D2));

    return 0;
}

#endif
