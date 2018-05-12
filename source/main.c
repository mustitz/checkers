#include "checkers.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define KW_QUIT             1
#define KW_MOVE             2
#define KW_LIST             3
#define KW_SELECT           4
#define KW_FEN              5
#define KW_AI               6
#define KW_SET              7
#define KW_ETB              8
#define KW_INFO             9
#define KW_GEN             10
#define KW_ETB_DIR         11
#define KW_LOAD            12
#define KW_INDEX           13

#define ITEM(name) { #name, KW_##name }
struct keyword_desc root_level_keywords[] = {
    { "exit", KW_QUIT },
    ITEM(QUIT),
    ITEM(MOVE),
    ITEM(LIST),
    ITEM(SELECT),
    ITEM(FEN),
    ITEM(AI),
    ITEM(SET),
    ITEM(ETB),
    ITEM(INFO),
    ITEM(GEN),
    ITEM(ETB_DIR),
    ITEM(LOAD),
    ITEM(INDEX),
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



struct str_with_len
{
    const char * s;
    int len;
};

struct str_with_len read_set_value(struct cmd_parser * restrict me, const int is_eq_supported)
{
    struct line_parser * restrict lp = &me->line_parser;
    parser_skip_spaces(lp);
    if (is_eq_supported && *lp->current == '=') {
        ++lp->current;
        parser_skip_spaces(lp);
    }

    const unsigned char * begin = lp->current;
    const unsigned char * end = lp->current;

    for (; *lp->current != '\0'; ++lp->current) {
        if (*lp->current > ' ') {
            end = lp->current + 1;
        }
    }

    const char * s = (const char *)begin;
    const int len = end - begin;
    return (struct str_with_len){s, len};
}



static int process_quit(struct cmd_parser * restrict me);
static void process_move(struct cmd_parser * restrict me);
static void process_fen(struct cmd_parser * restrict me);
static void process_ai(struct cmd_parser * restrict me);
static void process_set(struct cmd_parser * restrict me);
static void process_etb(struct cmd_parser * restrict me);

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
        case KW_AI:
            process_ai(me);
            break;
        case KW_SET:
            process_set(me);
            break;
        case KW_ETB:
            process_etb(me);
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
static void process_move_select(struct cmd_parser * restrict me);

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
        case KW_SELECT:
            return process_move_select(me);
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

static void process_move_select(struct cmd_parser * restrict me)
{
    int num;
    struct line_parser * restrict lp = &me->line_parser;
    parser_skip_spaces(lp);

    int status = parser_read_last_int(lp, &num);
    if (status != 0) {
        if (status != PARSER_ERROR__NO_EOL) {
            return error(me, "Integer constant expected.");
        } else {
            return error(me, "End of line expected (MOVE SELECT n is parser), but something was found.");
        }
    }

    game_move_select(me->game, num);
}

static int parse_fen(struct cmd_parser * restrict me, struct position * restrict position);

static void process_fen(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (parser_check_eol(lp)) {
        position_print_fen(me->game->position);
    } else {
        struct position new_position;
        if (parse_fen(me, &new_position) == 0) {
            game_set_position(me->game, &new_position);
        }
    }
}

static int parse_fen(struct cmd_parser * restrict me, struct position * restrict position)
{
    memset(position, 0, sizeof(struct position));

    struct line_parser * restrict lp = &me->line_parser;

    parser_skip_spaces(lp);

    position->active = -1;
    unsigned char activeSideCh = *lp->current++;
    if (activeSideCh == 'W' || activeSideCh == 'w') {
        position->active = WHITE;
    }
    if (activeSideCh == 'B' || activeSideCh == 'b') {
        position->active = BLACK;
    }
    if (position->active < 0) {
        error(me, "Invalid active side char in fen, only 'W' and 'B' is allowed.");
        return 1;
    }

    parser_skip_spaces(lp);
    if (*lp->current++ != ':') {
        error(me, "Colon synbol ':' expected in fen.");
        return 2;
    }

    for (;;) {
        parser_skip_spaces(lp);
        if (*lp->current == '\0') {
            break;
        }

        int idx_all = -1;
        unsigned char sideCh = *lp->current++;
        if (sideCh == 'W' || sideCh == 'w') {
            idx_all = IDX_ALL_0;
        }
        if (sideCh == 'B' || sideCh == 'b') {
            idx_all = IDX_ALL_1;
        }
        if (idx_all < 0) {
            error(me, "Invalid side char in fen, only 'W' and 'B' is allowed.");
            return 3;
        }

        for (;;) {
            parser_skip_spaces(lp);
            int is_king = 0;
            if (*lp->current == 'K' || *lp->current == 'k') {
                is_king = 1;
                ++lp->current;
                parser_skip_spaces(lp);
            }

            if (*lp->current < 'a' || *lp->current > 'h') {
                error(me, "File character (a letter from 'a' to 'h') expected.");
                return 4;
            }

            int file = *lp->current++ - 'a';

            if (*lp->current < '1' || *lp->current > '8') {
                error(me, "Rank character (a digit from '1' from '8') expected.");
                return 5;
            }

            int rank = *lp->current++ - '1';

            int index = file + 8*rank;
            square_t sq = index_to_square[index];
            position->bitboards[idx_all] |= MASK(sq);
            if (!is_king) {
                position->bitboards[idx_all + IDX_SIM] |= MASK(sq);
            }

            parser_skip_spaces(lp);
            unsigned char delimeter = *lp->current;
            if (delimeter == ',') {
                ++lp->current;
                continue;
            }
            if (delimeter == ':') {
                ++lp->current;
                break;
            }
            if (delimeter == '\0') {
                break;
            }

            error(me, "Invalid delimeter, ',' or ':' or EOL expected.");
            return 6;
        }
    }

    if (position->bitboards[IDX_ALL_0] == 0) {
        error(me, "No white checkers on the board.");
        return 7;
    }

    if (position->bitboards[IDX_ALL_1] == 0) {
        error(me, "No black checkers on the board.");
        return 7;
    }

    return 0;
}

static void process_ai_select(struct cmd_parser * restrict me);
static void process_ai_list(struct cmd_parser * restrict me);
static void process_ai_set(struct cmd_parser * restrict me);

static void process_ai(struct cmd_parser * restrict me)
{
    int keyword = read_keyword(me);

    if (keyword == -1) {
        return error(me, "Invalid lexem in AI command.");
    }

    if (keyword == 0) {
        return error(me, "Invalid keyword in AI command.");
    }

    switch (keyword) {
        case KW_SELECT:
            return process_ai_select(me);
        case KW_LIST:
            return process_ai_list(me);
        case KW_SET:
            return process_ai_set(me);
    }

    error(me, "Unexpected keyword in AI command.");
}

static void process_ai_select(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (parser_check_eol(lp)) {
        game_ai_select(me->game);
    } else {
        error(me, "End of line expected (AI SELECT command is parsed), but something was found.");
    }
}

static void process_ai_list(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (parser_check_eol(lp)) {
        game_ai_list(me->game);
    } else {
        error(me, "End of line expected (AI LIST command is parsed), but something was found.");
    }
}

static int read_ai_param(
    struct line_parser * restrict lp,
    const struct keyword_tracker * const params)
{
    if (params == NULL) {
        unsigned char ch = *lp->current;
        return is_first_id_char(ch) ? 0 : -1;
    }

    return parser_read_keyword(lp, params);
}

static void process_ai_set(struct cmd_parser * restrict me)
{
    struct game * restrict const game = me->game;
    struct ai * const ai = get_game_ai(game);
    if (ai == NULL) {
        return error(me, "No AI selected.");
    }

    const struct keyword_tracker * const params = ai_get_supported_param(ai);
    struct line_parser * restrict lp = &me->line_parser;
    if (parser_check_eol(lp)) {
        return error(me, "AI SET parameter expected, but EOL found.");
    }

    const int param_id = read_ai_param(lp, params);
    switch (param_id) {
        case -1:
            return error(me, "AI SET Invalid lexem.");
        case 0:
            return error(me, "AI SET Invalid param.");
        default:
            break;
    }

    parser_skip_spaces(lp);
    ai_set_param(ai, param_id, lp);
}

static void process_set_ai(struct cmd_parser * restrict me);
static void process_set_etb_dir(struct cmd_parser * restrict me);

static void process_set(struct cmd_parser * restrict me)
{
    int keyword = read_keyword(me);

    if (keyword == -1) {
        return error(me, "Invalid lexem in SET command.");
    }

    if (keyword == 0) {
        return error(me, "Invalid keyword in SET command.");
    }

    switch (keyword) {
        case KW_AI:
            return process_set_ai(me);
        case KW_ETB_DIR:
            return process_set_etb_dir(me);
    }

    error(me, "Unexpected keyword in SET command.");
}

static void process_set_ai(struct cmd_parser * restrict me)
{
    struct str_with_len str = read_set_value(me, 1);
    game_set_ai(me->game, str.s, str.len);
}

static void process_set_etb_dir(struct cmd_parser * restrict me)
{
    struct str_with_len str = read_set_value(me, 1);
    etb_set_dir(str.s, str.len);
}

static void process_etb_info(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (!parser_check_eol(lp)) {
        return error(me, "End of line expected (ETB INFO command is parsed), but someting was found.");
    }

    game_etb_info(me->game);
}

static void process_etb_gen(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (parser_check_eol(lp)) {
        return error(me, "ETB GEN expected endgame abbrevation, for example WB, wwB, etc.");
    }

    int wmam = 0;
    int wsim = 0;
    int bmam = 0;
    int bsim = 0;

    for (;; ++lp->current) {
        const int ch = *lp->current;
        if (ch >= '\0' && ch <= ' ') {
            break;
        }
        switch (ch) {
            case 'W':
                ++wmam;
                break;
            case 'w':
                ++wsim;
                break;
            case 'B':
                ++bmam;
                break;
            case 'b':
                ++bsim;
                break;
            default:
                return error(me, "iWrong endgame abbrevation, only WBwb characters are supported.");
        }
    }

    if (!parser_check_eol(lp)) {
        error(me, "End of line expected (ETB GEN abbrevation command is parsed), but something was found.");
    }

    gen_etb(wmam+wsim, wsim, bmam+bsim, bsim);
}

static void process_etb_load(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (!parser_check_eol(lp)) {
        struct str_with_len str = read_set_value(me, 1);
        etb_set_dir(str.s, str.len);
    }

    return etb_load_all();
}

static void process_etb_index(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (!parser_check_eol(lp)) {
        return error(me, "ETB INDEX: End of line expected, but something was found..");
    }

    etb_index(me->game->position);
}

static void process_etb(struct cmd_parser * restrict me)
{
    struct line_parser * restrict lp = &me->line_parser;
    if (parser_check_eol(lp)) {
        return etb_status();
    }

    int keyword = read_keyword(me);

    if (keyword == -1) {
        return error(me, "Invalid lexem in ETB command.");
    }

    if (keyword == 0) {
        return error(me, "Invalid keyword in ETB command.");
    }

    switch (keyword) {
        case KW_INFO:
            return process_etb_info(me);
        case KW_GEN:
            return process_etb_gen(me);
        case KW_LOAD:
            return process_etb_load(me);
        case KW_INDEX:
            return process_etb_index(me);
    }

    error(me, "Unexpected keyword in ETB command.");
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

    etb_free();
    game_ai_free(game);
    free_mempool(pool);

    if (line) {
        free(line);
    }

    return 0;
}
