#include "parser.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

token_t *Peek(parser_t *p) {
    return &p->tokens[p->pos];
}

token_t *PeekNext(parser_t *p) {
    if (p->pos < p->count) {
        return &p->tokens[p->pos + 1];
    }

    return NULL;
}

token_t *Previous(parser_t *p) {
    if (p->pos > 0) {
        return &p->tokens[p->pos - 1];
    }

    return NULL;
}

token_t *Advance(parser_t *p) {
    return &p->tokens[p->pos++];
}

bool AtEnd(parser_t *p) {
    return p->pos == p->count;
}

bool _Match(parser_t *p, token_type type, int typeType) {
    token_t *now = Peek(p);
    if (now->type == type && now->typeType == typeType) {
        p->pos++;
        return true;
    }

    return false;
}

bool Match(parser_t *p, token_type type) {
    token_t *now = Peek(p);
    if (now->type == type) {
        p->pos++;
        return true;
    }

    return false;
}

bool MatchKeyword(parser_t *p, token_keyword_type type) {
    return _Match(p, TOKEN_KEYWORD, type);
}

bool MatchPunctuation(parser_t *p, token_punctuation_type type) {
    return _Match(p, TOKEN_PUNCTUATION, type);
}

token_t *Expect(parser_t *p, token_type type, const char *error_msg) {
    token_t *now = Peek(p);
    if (now->type == type) {
        p->pos++;
        return now;
    }

    printf("%s\n", error_msg);
    return NULL;
}

token_t *_Expect(parser_t *p, token_type type, int typeType, const char *error_msg) {
    token_t *now = Peek(p);
    if (now->type == type && now->typeType == typeType) {
        p->pos++;
        return now;
    }

    printf("%s\n", error_msg);
    return NULL;
}

token_t *ExpectKeyword(parser_t *p, token_keyword_type type, const char *error_msg) {
    return _Expect(p, TOKEN_KEYWORD, type, error_msg);
}

token_t *ExpectPunctuation(parser_t *p, token_punctuation_type type, const char *error_msg) {
    return _Expect(p, TOKEN_PUNCTUATION, type, error_msg);
}

void PushScope(parser_t *p) {
    scope_t *scope = (scope_t *)malloc(sizeof(scope_t));
    scope->symbols = CreateHashTable(sizeof(symbol_kind));
    scope->parentScope = p->scope;
    p->scope = scope;
}

void PopScope(parser_t *p) {
    scope_t *old = p->scope;
    p->scope = old->parentScope;
    free(old);
}

void InsertSymbol(parser_t *p, u8 *name, symbol_kind kind) {
    HashInsert(p->scope->symbols, name, &kind);
}
