#include "checkers.h"

#include <stdio.h>

struct tables
{
    int square_to_index[32];
    square_t index_to_square[64];
    char upper_square_str[32][4];
    char lower_square_str[32][4];
};

struct tables tables;

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
}

int main()
{
    init_square_to_index();
    init_index_to_square();
    init_strings();

    print_file();
    return 0;
}
