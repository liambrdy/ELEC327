#ifndef _PARSER_H
#define _PARSER_H

#include "lexer.h"
#include "hashtable.h"

#include <stdbool.h>

typedef struct typedef_scope_t {
    hash_table_t *typedefs;
    struct scope_t *parentScope;
} typedef_scope_t;

typedef struct parser_t {
    token_t *tokens;
    u32 count;
    u32 pos;

    typedef_scope_t *scope;
} parser_t;

token_t *Peek(parser_t *p);
token_t *PeekNext(parser_t *p);
token_t *Previous(parser_t *p);
token_t *Advance(parser_t *p);
void Reverse(parser_t *p);
void GoTo(parser_t *p, u32 pos);
bool AtEnd(parser_t *p);

bool Match(parser_t *p, token_type type);
bool MatchKeyword(parser_t *p, token_keyword_type type);
bool MatchPunctuation(parser_t *p, token_punctuation_type type);

token_t *Expect(parser_t *p, token_type type, const char *error_msg);
token_t *ExpectKeyword(parser_t *p, token_keyword_type type, const char *error_msg);
token_t *ExpectPunctuation(parser_t *p, token_punctuation_type type, const char *error_msg);

void PushTypedefScope(parser_t *p);
void PopTypedefScope(parser_t *p);

void InsertTypedef(parser_t *p, slice_t *name);

#endif