#include "checkers.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

struct line_parser
{
};

int process_cmd(struct line_parser * restrict me, const char * cmd)
{
    return 1;
}

int main()
{
    srand(time(0));

    char * line = 0;
    size_t len = 0;

    struct line_parser parser;

    for (;; ) {
        ssize_t hasRead = getline(&line, &len, stdin);
        if (hasRead == -1) {
            break;
        }

        int isQuit = process_cmd(&parser, line);
        if (isQuit) {
            break;
        }
    }

    if (line) {
        free(line);
    }

    return 0;
}
