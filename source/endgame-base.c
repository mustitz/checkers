#include "checkers.h"

#include <stdio.h>

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
    return -1;
}

uint64_t position_to_index(
    const struct position * const position,
    const int code)
{
    return 1;
}
