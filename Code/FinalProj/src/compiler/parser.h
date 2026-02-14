#ifndef _PARSER_H
#define _PARSER_H

#include "lexer.h"

#include <stdbool.h>

typedef struct parser_t {
    token_t *tokens;
    u32 count;
    u32 pos;
} parser_t;

token_t *Peek(parser_t *p);
token_t *PeekNext(parser_t *p);
token_t *Previous(parser_t *p);
token_t *Advance(parser_t *p);
bool AtEnd(parser_t *p);

bool Match(parser_t *p, token_type type);
bool MatchKeyword(parser_t *p, token_keyword_type type);
bool MatchOperator(parser_t *p, token_operator_type type);
bool MatchPunctuation(parser_t *p, token_punctuation_type type);

token_t *ExpectKeyword(parser_t *p, token_keyword_type type, const char *error_msg);
token_t *ExpectOperator(parser_t *p, token_operator_type type, const char *error_msg);
token_t *ExpectPunctuation(parser_t *p, token_punctuation_type type, const char *error_msg);


#endif