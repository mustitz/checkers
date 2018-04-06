#ifndef MU__PARSER__H__
#define MU__PARSER__H__

#define PARSER_WARNING__OVERFLOW        +1
#define PARSER_ERROR__END_OF_LINE       -1
#define PARSER_ERROR__NO_DIGITS         -2

#include "mu-data-struct.h"

/*
 * Keyword tracker
 */

#define KW_TRACKER__IGNORE_CASE           1

struct keyword_desc
{
    const char * text;
    int id;
};

struct keyword_tracker_step
{
    uint64_t possible[256];
    struct keyword_tracker_step * next;
};


struct keyword_tracker
{
    const struct keyword_desc * keyword_list;
    const struct keyword_tracker_step * first_step;
};

struct keyword_tracker * build_keyword_tracker(const struct keyword_desc * keyword_list, struct mempool * restrict pool, int flags);



/*
 * Line Parser
 */

struct line_parser
{
    const unsigned char * line;
    const unsigned char * current;
    const unsigned char * lexem_start;
};

static inline void parser_skip_spaces(struct line_parser * restrict me)
{
    while (is_space_char(*me->current)) {
        me->current++;
    }
    me->lexem_start = me->current;
}

void parser_set_line(struct line_parser * restrict me, const char * line);
int parser_check_eol(struct line_parser * restrict me);
int parser_is_text(struct line_parser * restrict me, const char * text);
int parser_try_int(struct line_parser * restrict me, int * value);
int parser_read_keyword(struct line_parser * restrict me, const struct keyword_tracker * tracker);
int parser_read_id(struct line_parser * restrict me);

#endif
