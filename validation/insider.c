#include "checkers.h"

#define WITH_TESTS
#include "../source/mu-data-struct.c"
#include "../source/mu-parser.c"
#include "../source/endgame-base.c"

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int test_empty()
{
    return 0;
}

/*
 * Here is bitboard tests.
 */

int test_diag(int delta, const square_t * squares)
{
    const square_t * current = squares;
    bitboard_t diag = 0;
    int len = 0;

    for (;;) {
        diag |= MASK(*current);
        ++len;
        square_t sq = *current + delta;
        if (sq > 32) {
            sq -= 32;
        }

        ++current;
        if (*current == I9) {
            return 0;
        }

        if (sq != *current) {
            fprintf(stderr, "Invalid diagonal pair %d/%d:", sq, *current);
            for (current = squares; *current != I9; ++current) {
                fprintf(stderr, " %d", *current);
            }
            fprintf(stderr, ".");
            fail();
        }
    }

    if (pop_count(diag) != len) {
        test_fail("pop_count mismatch (diag).");
    }

    square_t sq = get_first_square(diag);
    int isSet = (MASK(sq) & diag) != 0;
    if (!isSet) {
        test_fail("get_first_square unexpected.");
    }

    if (delta != 7) {
        return 0;
    }

    bitboard_t tmp_d7 = rotate_d7(diag) | MASK(*squares);
    if (tmp_d7 != diag) {
        test_fail("rotate_d7 fails.");
    }

    --current;
    bitboard_t tmp_u7 = rotate_u7(diag) | MASK(*current);
    if (tmp_u7 != diag) {
        test_fail("rotate_u7 fails.");
    }
}

int test_bitboard()
{
    bitboard_t all_1 = RANK_1 | RANK_2 | RANK_3 | RANK_4 | RANK_5 | RANK_6 | RANK_7 | RANK_8;
    bitboard_t all_2 = FILE_A | FILE_B | FILE_C | FILE_D | FILE_E | FILE_F | FILE_G | FILE_H;
    bitboard_t all_3 = ~(bitboard_t)0;

    if (all_1 != BOARD) {
        test_fail("Invalid all_1.");
    }

    if (all_2 != BOARD) {
        test_fail("Invalid all_2.");
    }

    if (all_3 != BOARD) {
        test_fail("Invalid all_3.\n");
    }

    if (pop_count(all_1) != 32) {
        test_fail("pop_count mismatch (all).");
    }

    bitboard_t tmp;
    tmp = MASK(A1) | MASK(G5) | MASK(A7);
    if (pop_count(tmp) != 3) {
        test_fail("pop_count mismatch (1).");
    }

    tmp |= MASK(B8) | MASK(E1) | MASK(H8);
    if (pop_count(tmp) != 6) {
        test_fail("pop_count mismatch (2).");
    }

    square_t a7b8[] = { A7, B8,  I9 };
    square_t a5d8[] = { A5, B6, C7, D8,  I9 };
    square_t a3f8[] = { A3, B4, C5, D6, E7, F8,  I9 };
    square_t a1h8[] = { A1, B2, C3, D4, E5, F6, G7, H8,  I9 };
    square_t c1h6[] = { C1, D2, E3, F4, G5, H6,  I9 };
    square_t e1h4[] = { E1, F2, G3, H4,  I9 };
    square_t g1h2[] = { G1, H2,  I9 };

    square_t c1a3[] = { C1, B2, A3,  I9 };
    square_t e1a5[] = { E1, D2, C3, B4, A5,  I9 };
    square_t g1a7[] = { G1, F2, E3, D4, C5, B6, A7,  I9 };
    square_t h2b8[] = { H2, G3, F4, E5, D6, C7, B8,  I9 };
    square_t h4d8[] = { H4, G5, F6, E7, D8,  I9 };
    square_t h6f8[] = { H6, G7, F8,  I9 };

    test_diag(7, a7b8);
    test_diag(7, a5d8);
    test_diag(7, a3f8);
    test_diag(7, a1h8);
    test_diag(7, c1h6);
    test_diag(7, e1h4);
    test_diag(7, g1h2);

    test_diag(1, c1a3);
    test_diag(1, e1a5);
    test_diag(1, g1a7);
    test_diag(1, h2b8);
    test_diag(1, h4d8);
    test_diag(1, h6f8);

    return 0;
}

typedef int (* test_function)();

struct test_item
{
    const char * name;
    test_function function;
};

struct test_item tests[] = {
    { "empty", &test_empty },
    { "bitboard", &test_bitboard },
    { "mempool", &test_mempool },
    { "str_map", &test_str_map },
    { "sets", &test_sets },
    { "parser", &test_parser },
    { "choose", &test_choose },
    { "fr_offsets", &test_fr_offsets },
    { NULL, NULL }
};

void print_tests()
{
    struct test_item * current = tests;
    for (; current->name != NULL; ++current) {
        printf("%s\n", current->name);
    }
}

void run_test_item(const struct test_item * item)
{
    test_name = item->name;
    printf("Run test `%s'\n", item->name);
    int test_exit_code = (*item->function)();
    if (test_exit_code) {
        exit(test_exit_code);
    }
}

void run_all_tests()
{
    struct test_item * current = tests;
    for (; current->name != NULL; ++current) {
        run_test_item(current);
    }
}

void run_test(const char * name)
{
    if (strcmp(name, "all") == 0) {
        return run_all_tests();
    }

    struct test_item * current = tests;
    for (; current->name != NULL; ++current) {
        if (strcmp(name, current->name) == 0) {
            return run_test_item(current);
        }
    }

    fprintf(stderr, "Test `%s' is not found.", name);
    fail();
}

int main(int argc, char * argv[])
{
    if (argc == 1) {
        print_tests();
        return 0;
    }

    for (size_t i=1; i<argc; ++i) {
        run_test(argv[i]);
    }

    return 0;
}
