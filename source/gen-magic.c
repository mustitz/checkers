#include "checkers.h"

#include <stdio.h>
#include <stdlib.h>

struct tables
{
    int square_to_index[32];
    square_t index_to_square[64];
    char upper_square_str[32][4];
    char lower_square_str[32][4];
    struct square_magic square_magic[32];
    struct mam_take_magic mam_take_magic_1[32][256];
    struct mam_take_magic mam_take_magic_7[32][256];
    uint64_t choose[33][33];
    bitboard_t reverse_table[4][256];
    struct position_code_info position_code_infos[SIDE_BITBOARD_CODE_COUNT][SIDE_BITBOARD_CODE_COUNT];
};

struct tables tables;



int permutations[1*2*3*4*5*6*7*8][8];

static void save_permutation(const int * values, int n)
{
    static size_t counter = 0;
    memcpy(permutations[counter++], values, 8*sizeof(int));
}

static void recurcive_gen_permutations(int * restrict base, int base_n, const int * variable, int variable_n)
{
    if (variable_n == 0) {
        return save_permutation(base, base_n);
    }

    int tmp[variable_n];
    for (int i=0; i<variable_n; ++i) {
        base[base_n] = variable[i];

        int k = 0;
        for (int j=0; j<variable_n; ++j) {
            if (i != j) {
                tmp[k++] = variable[j];
            }
        }

        recurcive_gen_permutations(base, base_n+1, tmp, variable_n-1);
    }
}

static void gen_permutations(const int * digits, int n)
{
    int base[n];
    recurcive_gen_permutations(base, 0, digits, n);
}



static void init_square_to_index(void);
static void init_index_to_square(void);
static void init_strings(void);
static void init_magic(void);
static void init_choose(void);
static void init_reverse_table(void);
static void init_position_code_infos(void);
static void print_file();

int main()
{
    int digits[] = { 0, 1, 2, 3, 4, 5, 6, 7};
    gen_permutations(digits, 8);

    init_square_to_index();
    init_index_to_square();
    init_strings();
    init_magic();
    init_choose();
    init_reverse_table();
    init_position_code_infos();
    print_file();
    return 0;
}

static void init_square_to_index(void)
{
    tables.square_to_index[A1] = 000;
    tables.square_to_index[A3] = 020;
    tables.square_to_index[A5] = 040;
    tables.square_to_index[A7] = 060;
    tables.square_to_index[B2] = 011;
    tables.square_to_index[B4] = 031;
    tables.square_to_index[B6] = 051;
    tables.square_to_index[B8] = 071;

    tables.square_to_index[C1] = 002;
    tables.square_to_index[C3] = 022;
    tables.square_to_index[C5] = 042;
    tables.square_to_index[C7] = 062;
    tables.square_to_index[D2] = 013;
    tables.square_to_index[D4] = 033;
    tables.square_to_index[D6] = 053;
    tables.square_to_index[D8] = 073;

    tables.square_to_index[E1] = 004;
    tables.square_to_index[E3] = 024;
    tables.square_to_index[E5] = 044;
    tables.square_to_index[E7] = 064;
    tables.square_to_index[F2] = 015;
    tables.square_to_index[F4] = 035;
    tables.square_to_index[F6] = 055;
    tables.square_to_index[F8] = 075;

    tables.square_to_index[G1] = 006;
    tables.square_to_index[G3] = 026;
    tables.square_to_index[G5] = 046;
    tables.square_to_index[G7] = 066;
    tables.square_to_index[H2] = 017;
    tables.square_to_index[H4] = 037;
    tables.square_to_index[H6] = 057;
    tables.square_to_index[H8] = 077;
}

static void init_index_to_square(void)
{
    for (int i=0; i<64; ++i) {
        tables.index_to_square[i] = I9;
    }

    for (int i=0; i<32; ++i) {
        tables.index_to_square[tables.square_to_index[i]] = i;
    }
}

static void init_strings(void)
{
    for (int i=0; i<32; ++i) {
        int index = tables.square_to_index[i];
        static const char * upper_file_str = "ABCDEFGH";
        static const char * lower_file_str = "abcdefgh";
        static const char * rank_str = "12345678";
        tables.upper_square_str[i][0] = upper_file_str[index % 8];
        tables.upper_square_str[i][1] = rank_str[index / 8];
        tables.lower_square_str[i][0] = lower_file_str[index % 8];
        tables.lower_square_str[i][1] = rank_str[index / 8];
    }
}

static void handle_way_1(int n, const int * squares);
static void handle_way_7(int n, const int * squares);

static void init_magic(void)
{
    int a1a1[] = { A1 };
    int c1a3[] = { C1, B2, A3 };
    int e1a5[] = { E1, D2, C3, B4, A5 };
    int g1a7[] = { G1, F2, E3, D4, C5, B6, A7 };
    int h2b8[] = { H2, G3, F4, E5, D6, C7, B8 };
    int h4d8[] = { H4, G5, F6, E7, D8 };
    int h6f8[] = { H6, G7, F8 };
    int h8h8[] = { H8 };

    handle_way_1(1, a1a1);
    handle_way_1(3, c1a3);
    handle_way_1(5, e1a5);
    handle_way_1(7, g1a7);
    handle_way_1(7, h2b8);
    handle_way_1(5, h4d8);
    handle_way_1(3, h6f8);
    handle_way_1(1, h8h8);

    int a1h8[] = { A1, B2, C3, D4, E5, F6, G7, H8 };
    int a3f8[] = { A3, B4, C5, D6, E7, F8 };
    int a5d8[] = { A5, B6, C7, D8 };
    int a7b8[] = { A7, B8 };
    int c1h6[] = { C1, D2, E3, F4, G5, H6 };
    int e1h4[] = { E1, F2, G3, H4 };
    int g1h2[] = { G1, H2 };

    handle_way_7(8, a1h8);
    handle_way_7(6, a3f8);
    handle_way_7(4, a5d8);
    handle_way_7(2, a7b8);
    handle_way_7(6, c1h6);
    handle_way_7(4, e1h4);
    handle_way_7(2, g1h2);
}

enum bitboard_dead_next_index
{
    IDX_DEAD_0,
    IDX_NEXT_0,
    IDX_DEAD_1,
    IDX_NEXT_1
};

static void process_mask(uint32_t mask, int n, const int * squares, int i, bitboard_t * restrict data);

static void handle_way_1(int n, const int * squares)
{
    for (int i=0; i<n; ++i) {
        tables.square_magic[squares[i]].shift1 = squares[0];
    }

    for (int i=0; i<n; ++i) {
        for (uint32_t mask = 0; mask < 256; ++mask) {

            bitboard_t data[4];
            process_mask(mask, n, squares, i, data);

            if (data[IDX_NEXT_0]) {
                tables.mam_take_magic_1[squares[i]][mask].dead[0] = data[IDX_DEAD_0];
                tables.mam_take_magic_1[squares[i]][mask].next[0] = data[IDX_NEXT_0];
            }

            if (data[IDX_NEXT_1]) {
                tables.mam_take_magic_1[squares[i]][mask].dead[1] = data[IDX_DEAD_1];
                tables.mam_take_magic_1[squares[i]][mask].next[1] = data[IDX_NEXT_1];
            }
        }
    }
}

struct permutation_ctx
{
    int n;
    const int * squares;

    uint32_t magic;
    const int * permutation;
    int offsets[8];
};

static void find_permutation(struct permutation_ctx * restrict ctx);

static void handle_way_7(int n, const int * squares)
{
    struct permutation_ctx ctx;
    ctx.squares = squares;
    ctx.n = n;
    find_permutation(&ctx);
    if (ctx.permutation == 0) {
        fprintf(stderr, "Fail to find permutations for handle_way_7.\n");
        exit(-1);
    }

    bitboard_t mask = 0;
    for (int i=0; i<n; ++i) {
        mask |= MASK(squares[i]);
    }

    for (int i=0; i<n; ++i) {
        tables.square_magic[squares[i]].factor7 = ctx.magic;
        tables.square_magic[squares[i]].mask7 = mask;
    }

    for (int i=0; i<n; ++i) {
        for (uint32_t magic_mask = 0; magic_mask < 256; ++magic_mask) {

            uint32_t mask = 0;
            uint32_t bit = 1;
            for (int j=0; j<n; ++j) {
                int bit_index = ctx.permutation[j] + 8 - n;
                if (magic_mask & (1<<bit_index)) {
                    mask |= bit;
                }

                bit <<= 1;
            }

            bitboard_t data[4];
            process_mask(mask, n, squares, i, data);

            if (data[IDX_NEXT_0]) {
                tables.mam_take_magic_7[squares[i]][magic_mask].dead[0] = data[IDX_DEAD_0];
                tables.mam_take_magic_7[squares[i]][magic_mask].next[0] = data[IDX_NEXT_0];
            }

            if (data[IDX_NEXT_1]) {
                tables.mam_take_magic_7[squares[i]][magic_mask].dead[1] = data[IDX_DEAD_1];
                tables.mam_take_magic_7[squares[i]][magic_mask].next[1] = data[IDX_NEXT_1];
            }
        }
    }
}

static int try_permutation(struct permutation_ctx * restrict ctx);

static void find_permutation(struct permutation_ctx * restrict ctx)
{
    if (ctx->n == 0) {
        ctx->permutation = 0;
        return;
    }

    size_t count = 1;
    for (int i=2; i<ctx->n; ++i) {
        count *= i;
    }

    for (ssize_t i=0; i<count; ++i) {
        ctx->permutation = permutations[i];
        if (try_permutation(ctx)) {
            return;
        }
    }

    ctx->permutation = 0;
}

static int calc_offsets(struct permutation_ctx * restrict ctx);
static void calc_magic(struct permutation_ctx * restrict ctx);
static int verify(struct permutation_ctx * restrict ctx);

static int try_permutation(struct permutation_ctx * restrict ctx)
{
    int is_ok = calc_offsets(ctx);
    if (!is_ok) {
        return 0;
    }

    calc_magic(ctx);

    if (!verify(ctx)) {
        return 0;
    }

    return 1;
}

static int calc_offsets(struct permutation_ctx * restrict ctx)
{
    for (int i=0; i<ctx->n; ++i) {
        int expected = 32 - ctx->n + ctx->permutation[i];
        ctx->offsets[i] = expected - ctx->squares[i];
        if (ctx->offsets[i] < 0) {
            return 0;
        }
    }

    return 1;
}

static void calc_magic(struct permutation_ctx * restrict ctx)
{
    ctx->magic = 0;
    for (int i=0; i<ctx->n; ++i) {
        ctx->magic |= 1 << ctx->offsets[i];
    }
}

static int verify(struct permutation_ctx * restrict ctx)
{
    uint32_t mask = 1 << ctx->n;
    while (mask --> 0) {
        uint32_t bb = 0;
        for (int i=0; i<ctx->n; ++i) {
            int bit = get_bit(mask, i);
            bb |= bit << ctx->squares[i];
        }

        uint32_t tmp = (bb * ctx->magic) >> (32-ctx->n);
        for (int i=0; i<ctx->n; ++i) {
            int b1 = get_bit(mask, i);
            int b2 = get_bit(tmp, ctx->permutation[i]);
            if (b1 != b2) {
                return 0;
            }
        }
    }

    return 1;
}

static void process_mask(uint32_t mask, int n, const int * squares, int i, bitboard_t * restrict data)
{
    data[IDX_NEXT_0] = 0;
    data[IDX_NEXT_1] = 0;

    int f = i;
    uint32_t bit = 1 << i;

    for (;;) {
        ++f;
        bit <<= 1;

        if (f >= n || (bit & mask)) {
            break;
        }
    }

    if (f < n) {
        data[IDX_DEAD_0] = MASK(squares[f]);

        for (;;) {
            ++f;
            bit <<= 1;

            if (f>=n || (bit & mask)) {
                break;
            }

            data[IDX_NEXT_0] |= MASK(squares[f]);
        }
    }

    f = i;
    bit = 1 << f;

    for (;;) {

        --f;
        bit >>= 1;

        if (f < 0 || (bit & mask)) {
            break;
        }
    }

    if (f >= 0) {

        data[IDX_DEAD_1] = MASK(squares[f]);

        for (;;) {
            --f;
            bit >>= 1;

            if (f < 0 || (bit & mask)) {
                break;
            }

            data[IDX_NEXT_1] |= MASK(squares[f]);
        }
    }
}



static void init_choose(void)
{
    uint64_t * restrict data = &tables.choose[0][0];

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



static const int reverse_map[32] = {
    [A1] = H8, [B2] = G7, [C3] = F6, [D4] = E5, [E5] = D4, [F6] = C3, [G7] = B2, [H8] = A1,
    [C1] = F8, [D2] = E7, [E3] = D6, [F4] = C5, [G5] = B4, [H6] = A3,
    [A3] = H6, [B4] = G5, [C5] = F4, [D6] = E3, [E7] = D2, [F8] = C1,
    [E1] = D8, [F2] = C7, [G3] = B6, [H4] = A5,
    [A5] = H4, [B6] = G3, [C7] = F2, [D8] = E1,
    [G1] = B8, [H2] = A7,
    [A7] = H2, [B8] = G1
};

static void init_reverse_table(void)
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
        tables.reverse_table[byte_num][byte_val] = result;
    }
}



static uint64_t init_position_code_info_offset(
    struct position_code_info * restrict const info)
{
    const int is_reversed = info->wcode < info->bcode;

    const int wsim = !is_reversed ? info->wsim : info->bsim;
    const int bsim = !is_reversed ? info->bsim : info->wsim;
    const int wmam = !is_reversed ? info->wmam : info->bmam;
    const int bmam = !is_reversed ? info->bmam : info->wmam;

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

        const uint64_t m1 = tables.choose[4][wfsim];
        const uint64_t m2 = tables.choose[24][wosim];
        const uint64_t m3 = safe_mul(m1, m2);
        const uint64_t m4 = tables.choose[28-wosim][bsim];
        const uint64_t m5 = safe_mul(m3, m4);

        if (m5 == U64_OVERFLOW) {
            fprintf(stderr, "wsim %d, bsim %d, wmam %d, bmam %d:\n", wsim, bsim, wmam, bmam);
            fprintf(stderr, "U64 multiplicatation oveflow (sim): %lu * %lu * %lu.\n", m1, m2, m4);
            fprintf(stderr, "Location %s:%d\n", __FILE__, __LINE__);
            exit(1);
        }

        offsets[0] = offsets[-1] + m5;
    }

    const uint64_t qsim = offsets[-1];
    const uint64_t qwmam = tables.choose[32-wsim-bsim][wmam];
    const uint64_t qbmam = tables.choose[32-wsim-bsim-wmam][bmam];

    const uint64_t m1 = safe_mul(qsim, qwmam);
    const uint64_t m2 = safe_mul(m1, qbmam);
    const uint64_t m3 = safe_mul(2, m2);
    if (m3 == U64_OVERFLOW) {
        fprintf(stderr, "wsim %d, bsim %d, wmam %d, bmam %d:\n", wsim, bsim, wmam, bmam);
        fprintf(stderr, "  U64 multiplicatation oveflow (total): %lu * %lu * %lu * 2.\n", qsim, qwmam, qbmam);
        fprintf(stderr, "Location %s:%d\n", __FILE__, __LINE__);
        exit(1);
    }

    return m3;
}

static void init_position_code_info_entry(
    const int wsim,
    const int wmam,
    const int bsim,
    const int bmam)
{
    const int wall = wsim + wmam;
    const int ball = bsim + bmam;

    const int wcode = bitboard_stat_to_code(wall, wsim);
    const int bcode = bitboard_stat_to_code(ball, bsim);
    if (wcode < 0 || wcode >= SIDE_BITBOARD_CODE_COUNT || bcode < 0 || bcode >= SIDE_BITBOARD_CODE_COUNT) {
        fprintf(stderr, "Fatal: wrong wcode, bcode pair %d,%d in %s:%d.\n", wcode, bcode, __FILE__, __LINE__);
        exit(1);
    }

    struct position_code_info * restrict const info = &tables.position_code_infos[wcode][bcode];

    const int is_reversed = wcode < bcode;

    info->wall = wall;
    info->wsim = wsim;
    info->wmam = wmam;
    info->ball = ball;
    info->bsim = bsim;
    info->bmam = bmam;
    info->wcode = wcode;
    info->bcode = bcode;

    info->total = init_position_code_info_offset(info);

    const int lo_code = is_reversed ? bcode : wcode;
    const int hi_code = is_reversed ? wcode : bcode;

    const int status = snprintf(info->filename, 15, "%02d%02d.ceitb", lo_code, hi_code);
    if (status < 0) {
        fprintf(stderr, "sprintf error in %s:%d\n", __FILE__, __LINE__);
        exit(1);
    }
    info->filename[status] = '\0';
}

static void init_position_code_infos(void)
{
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

            init_position_code_info_entry(wsim, wmam, bsim, bmam);
        }
    }
}



static void print_file(void)
{
    printf("#include \"checkers.h\"\n\n");

    printf("const int square_to_index[32] = {\n");
    for (int i=0; i<32; ++i) {
        if ((i % 8) == 0) {
            printf("    ");
        }

        printf("0%d%d%s",
            tables.square_to_index[i] / 8,
            tables.square_to_index[i] % 8,
            (i != 31 ? ", " : "")
        );

        if ((i % 8) == 7) {
            printf("\n");
        }
    }
    printf("};\n\n");

    printf("#define __ I9\n");
    printf("const square_t index_to_square[64] = {\n");
    for (int index = 0; index < 64; ++index) {

        if ((index % 8) == 0) {
            printf("    ");
        }

        enum square_t s = tables.index_to_square[index];
        if (s >= 0) {
            printf("%s%s",
                tables.upper_square_str[s],
                (index != 63 ? "," : "")
            );
        } else {
            printf("__%s", (index != 63 ? "," : ""));
        }

        if ((index % 8) == 7) {
            printf("\n");
        }
    }
    printf("};\n");
    printf("#undef __\n\n");

    printf("const char * const upper_square_str[32] = {\n");
    for (int i=0; i<32; ++i) {
        if ((i % 8) == 0) {
            printf("    ");
        }

        printf("\"%s\"%s",
            tables.upper_square_str[i],
            (i != 31 ? "," : "")
        );

        if ((i % 8) == 7) {
            printf("\n");
        }

    };
    printf("};\n\n");

    printf("const char * const lower_square_str[32] = {\n");
    for (int i=0; i<32; ++i) {
        if ((i % 8) == 0) {
            printf("    ");
        }

        printf("\"%s\"%s",
            tables.lower_square_str[i],
            (i != 31 ? "," : "")
        );

        if ((i % 8) == 7) {
            printf("\n");
        }

    };
    printf("};\n\n");

    printf("const struct square_magic square_magic[32] = {\n");
    for (int i=0; i<32; ++i) {
        printf("    { 0x%08X, 0x%08X, %2d}%s\n",
            tables.square_magic[i].mask7,
            tables.square_magic[i].factor7,
            tables.square_magic[i].shift1,
            (i != 31 ? "," : "")
        );
    }
    printf("};\n\n");

    printf("const struct mam_take_magic mam_take_magic_1[32][256] = {\n");
    for (int i=0; i<32; ++i) {
        printf("    {\n");
        for (int j=0; j<256; ++j) {
            printf("        { { 0x%08X, 0x%08X }, { 0x%08X, 0x%08X } }%s\n",
                tables.mam_take_magic_1[i][j].dead[0],
                tables.mam_take_magic_1[i][j].dead[1],
                tables.mam_take_magic_1[i][j].next[0],
                tables.mam_take_magic_1[i][j].next[1],
                (j != 255 ? "," : "")
             );
        }
        printf("    }%s\n", (i != 31 ? "," : ""));
    }
    printf("};\n\n");

    printf("const struct mam_take_magic mam_take_magic_7[32][256] = {\n");
    for (int i=0; i<32; ++i) {
        printf("    {\n");
        for (int j=0; j<256; ++j) {
            printf("        { { 0x%08X, 0x%08X }, { 0x%08X, 0x%08X } }%s\n",
                tables.mam_take_magic_7[i][j].dead[0],
                tables.mam_take_magic_7[i][j].dead[1],
                tables.mam_take_magic_7[i][j].next[0],
                tables.mam_take_magic_7[i][j].next[1],
                (j != 255 ? "," : "")
             );
        }
        printf("    }%s\n", (i != 31 ? "," : ""));
    }
    printf("};\n\n");

    printf("const uint64_t choose[33][33] = {\n");
    for (int i=0; i<33; ++i) {
        printf("    {");
        for (int j=0; j<33; ++j) {
            printf(" %*lu%s", 10, tables.choose[i][j], (j != 32 ? "," : ""));
        }
        printf(" }%s\n", (i != 32 ? "," : ""));
    }
    printf("};\n\n");

    printf("const bitboard_t reverse_table[4][256] = {\n");
    for (int i=0; i<4; ++i) {
        printf("    {\n");
        for (int j=0; j<256; ++j) {
            if (j % 8 == 0) {
                printf("        ");
            }
            printf("0x%08X%s", tables.reverse_table[i][j], (j != 255 ? "," : ""));

            if ((j % 8) == 7) {
                printf("\n");
            }
        }
        printf("    }%s\n", (i != 3 ? "," : ""));
    }
    printf("};\n\n");

    printf("const struct position_code_info position_code_infos[SIDE_BITBOARD_CODE_COUNT][SIDE_BITBOARD_CODE_COUNT] = {\n");
    for (int i=0; i<SIDE_BITBOARD_CODE_COUNT; ++i) {
        printf("    {\n");
        for (int j=0; j<SIDE_BITBOARD_CODE_COUNT; ++j) {
            const struct position_code_info * const info = &tables.position_code_infos[i][j];
            printf("        { &position_code_infos[%2d][%2d], %2d, %2d, %2d, %2d, %2d, %2d, %2d, %2d, \"%s\", { %*lu, %*lu, %*lu, %*lu, %*lu, %*lu }, %*lu }%s\n",
                MAX(info->wcode, info->bcode),
                MIN(info->wcode, info->bcode),
                info->wcode, info->wall, info->wsim, info->wmam,
                info->bcode, info->ball, info->bsim, info->bmam,
                info->filename,
                1, info->fr_offsets[0],
                13, info->fr_offsets[1],
                13, info->fr_offsets[2],
                13, info->fr_offsets[3],
                13, info->fr_offsets[4],
                13, info->fr_offsets[5],
                20, info->total,
                (j != SIDE_BITBOARD_CODE_COUNT-1 ? "," : ""));
        }
        printf("    }%s\n", (i != SIDE_BITBOARD_CODE_COUNT-1 ? "," : ""));
    }
    printf("};\n");
}
