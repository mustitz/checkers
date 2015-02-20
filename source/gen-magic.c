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
};

struct tables tables;



int permutations[1*2*3*4*5*6*7*8][8];

void save_permutation(const int * values, int n)
{
    static size_t counter = 0;
    memcpy(permutations[counter++], values, 8*sizeof(int));
}

void recurcive_gen_permutations(int * restrict base, int base_n, const int * variable, int variable_n)
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

void gen_permutations(const int * digits, int n)
{
    int base[n];
    recurcive_gen_permutations(base, 0, digits, n);
}



static void init_square_to_index();
static void init_index_to_square();
static void init_strings();
static void init_magic();
static void print_file();

int main()
{
    int digits[] = { 0, 1, 2, 3, 4, 5, 6, 7};
    gen_permutations(digits, 8);

    init_square_to_index();
    init_index_to_square();
    init_strings();
    init_magic();
    print_file();
    return 0;
}

static void init_square_to_index()
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

static void init_index_to_square()
{
    for (int i=0; i<64; ++i) {
        tables.index_to_square[i] = I9;
    }

    for (int i=0; i<32; ++i) {
        tables.index_to_square[tables.square_to_index[i]] = i;
    }
}

static void init_strings()
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

static void init_magic()
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



static void print_file()
{
    printf("#include \"checkers.h\"\n\n");

    printf("int square_to_index[32] = {\n");
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
    printf("square_t index_to_square[64] = {\n");
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

    printf("struct square_magic square_magic[32] = {\n");
    for (int i=0; i<32; ++i) {
        printf("    { 0x%08X, 0x%08X, %2d}%s\n", 
            tables.square_magic[i].mask7, 
            tables.square_magic[i].factor7, 
            tables.square_magic[i].shift1,
            (i != 31 ? "," : "")
        );
    }
    printf("};\n\n");

    printf("struct mam_take_magic mam_take_magic_1[32][256] = {\n");
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

    printf("struct mam_take_magic mam_take_magic_7[32][256] = {\n");
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
    printf("};\n");
}
