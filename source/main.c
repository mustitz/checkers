#include "checkers.h"
#include "mu-parser.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define KW_QUIT          1
#define KW_MOVE          2
#define KW_LIST          3
#define KW_FEN           4

#define ITEM(name) { #name, KW_##name }
struct keyword_desc root_level_keywords[] = {
    { "exit", KW_QUIT },
    ITEM(QUIT),
    ITEM(MOVE),
    ITEM(LIST),
    ITEM(FEN),
    { NULL, 0 }
};
#undef ITEM

struct cmd_parser
{
    struct line_parser line_parser;
    struct keyword_tracker * keyword_tracker;
    struct game * game;
};

static struct cmd_parser * create_parser(struct mempool * pool, struct game * game)
{
    struct cmd_parser * restrict me = mempool_alloc(pool, sizeof(struct cmd_parser));
    if (me == NULL) {
        panic("Can not create cmd_parser.");
    }

    me->keyword_tracker = build_keyword_tracker(root_level_keywords, pool, KW_TRACKER__IGNORE_CASE);
    if (me->keyword_tracker == NULL) {
        panic("Can not create keyword_traker.");
    }

    me->game = game;
    return me;
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



static int read_keyword(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    parser_skip_spaces(lp);
    return parser_read_keyword(lp, me->keyword_tracker); 
}


static int process_quit(struct cmd_parser * restrict me);
static void process_move(struct cmd_parser * restrict me);
static void process_fen(struct cmd_parser * restrict me);

static int process_cmd(struct cmd_parser * restrict me, const char * cmd)
{
    struct line_parser * restrict lp = &me->line_parser;
    parser_set_line(lp, cmd);

    if (parser_check_eol(lp)) {
        return 0;
    }

    int keyword = read_keyword(me);

    if (keyword == -1) {
        error(me, "Invalid lexem at the begginning of the line.");
        return 0;
    }

    if (keyword == 0) {
        error(me, "Invalid keyword at the begginning of the line.");
        return 0;
    }

    if (keyword == KW_QUIT) {
        return process_quit(me);
    }

    switch (keyword) {
        case KW_MOVE:
            process_move(me);
            break;
        case KW_FEN:
            process_fen(me);
            break;
        default:
            error(me, "Unexpected keyword at the begginning of the line.");
            break;
    }

    return 0;
}

static int process_quit(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (!parser_check_eol(lp)) {
        error(me, "End of line expected (QUIT command is parsed), but someting was found.");
        return 0;
    }

    return 1;
}

static void process_move_list(struct cmd_parser * restrict me);

static void process_move(struct cmd_parser * restrict me)
{
    int keyword = read_keyword(me);

    if (keyword == -1) {
        return error(me, "Invalid lexem in MOVE command.");
    }

    if (keyword == 0) {
        return error(me, "Invalid keyword in MOVE command.");
    }

    switch (keyword) {
        case KW_LIST:
            return process_move_list(me);
    }

    error(me, "Unexpected keyword in MOVE command.");
}

static void process_move_list(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (parser_check_eol(lp)) {
        game_move_list(me->game);
    } else {
        error(me, "End of line expected (MOVE LIST command is parsed), but something was found.");
    }
}

static void process_fen(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (parser_check_eol(lp)) {
        game_print_fen(me->game);
    } else {
        error(me, "End of line expected (FEN command is parsed), but something was found.");
    }
}

int main()
{
    srand(time(0));

    char * line = 0;
    size_t len = 0;

    struct mempool * restrict pool = create_mempool(64*1024);
    if (pool == NULL) {
        panic("Can not create_mempool(64k)\n");
        return 1;
    }

    struct game * game = create_game(pool);
    struct cmd_parser * parser = create_parser(pool, game);

    for (;; ) {
        ssize_t hasRead = getline(&line, &len, stdin);
        if (hasRead == -1) {
            break;
        }

        int isQuit = process_cmd(parser, line);
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
