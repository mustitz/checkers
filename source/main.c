#include "checkers.h"
#include "mu-parser.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define KW_QUIT          1
#define KW_MOVE          2
#define KW_LIST          3

#define ITEM(name) { #name, KW_##name }
struct keyword_desc root_level_keywords[] = {
    { "exit", KW_QUIT },
    ITEM(QUIT),
    ITEM(MOVE),
    ITEM(LIST),
    { NULL, 0 }
};
#undef ITEM

struct cmd_parser
{
    struct line_parser line_parser;
    struct keyword_tracker * root_level;
};

static void init_parser(struct cmd_parser * restrict me, struct mempool * pool)
{
    me->root_level = build_keyword_tracker(root_level_keywords, pool, KW_TRACKER__IGNORE_CASE);
    if (me->root_level == NULL) {
        panic("Can not create root_level keyword tracker.");
    }
}

static void error(struct cmd_parser * restrict me, const char * fmt, ...) __attribute__ ((format (printf, 2, 3))); 

static void error(struct cmd_parser * restrict me, const char * fmt, ...)
{
    const struct line_parser * lp = &me->line_parser;

    va_list args;
    va_start(args, fmt);
    printf("Parsing error: ");
    vprintf(fmt, args);
    va_end(args);

    int offset = lp->lexem_start - lp->line;
    printf("\n> %s> %*s^\n", lp->line, offset, "");
}



int process_quit(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (!parser_check_eol(lp)) {
        error(me, "End of line expected (section declaration is over), but someting was found.");
        return 0;
    }

    return 1;
}

int process_cmd(struct cmd_parser * restrict me, const char * cmd)
{
    struct line_parser * restrict lp = &me->line_parser;
    parser_set_line(lp, cmd);

    if (parser_check_eol(lp)) {
        return 0;
    }

    int keyword = parser_read_keyword(lp, me->root_level);

    if (keyword == -1) {
        error(me, "Invalid lexem at the begginning of the line.");
        return 0;
    }

    if (keyword == 0) {
        error(me, "Invalid keyword at the begginning of the line.");
        return 0;
    }

    switch (keyword) {
        case KW_QUIT:
            return process_quit(me);  
    }

    error(me, "Unexpected keyword at the begginning of the line.");
    return 0;
}

int main()
{
    srand(time(0));

    char * line = 0;
    size_t len = 0;

    struct mempool * restrict pool = create_mempool(16000);
    if (pool == NULL) {
        panic("Can not create_mempool(16000)\n");
        return 1;
    }

    struct cmd_parser parser;
    init_parser(&parser, pool);

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

    free_mempool(pool);

    if (line) {
        free(line);
    }

    return 0;
}
