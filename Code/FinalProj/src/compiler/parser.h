#ifndef _PARSER_H
#define _PARSER_H

#include "lexer.h"
#include "hashtable.h"

#include <stdbool.h>

typedef enum symbol_kind {
    SYMBOL_TYPEDEF,
    SYMBOL_OBJECT,
    SYMBOL_TAG,
} symbol_kind;

typedef struct scope_t {
    hash_table_t *symbols;
    struct scope_t *parentScope;
} scope_t;

typedef struct parser_t {
    token_t *tokens;
    u32 count;
    u32 pos;

    scope_t *scope;
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

void PushScope(parser_t *p);
void PopScope(parser_t *p);

void InsertSymbol(parser_t *p, slice_t *name, symbol_kind kind);

#endif