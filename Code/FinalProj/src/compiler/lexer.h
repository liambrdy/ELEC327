#ifndef _LEXER_H
#define _LEXER_H

#include "common.h"

typedef enum token_type {
    TOKEN_IDENTIFIER,
    TOKEN_LITERAL,
    TOKEN_OPERATOR,
    TOKEN_PUNCTUATION,
    TOKEN_KEYWORD,
    TOKEN_EOF,
} token_type;

typedef enum token_literal_type {
    LITERAL_INT,
    LITERAL_REAL,
    LITERAL_STRING,
} token_literal_type;

typedef enum token_operator_type {
    OPERATOR_PLUS,
    OPERATOR_MINUS,
    OPERATOR_MULT,
    OPERATOR_DIV,
    OPERATOR_EQUALS,
    OPERATOR_LESS_THAN,
    OPERATOR_GREATER_THAN,
    OPERATOR_EQUIV,
    OPERATOR_NOT_EQUIV,
    OPERATOR_INCREMENT,
    OPERATOR_DECREMENT,
    OPERATOR_COUNT,
} token_operator_type;

typedef enum token_punctuation_type {
    PUNCTUATION_COMMA,
    PUNCTAUTION_OPEN_PAREN,
    PUNCTUATION_CLOSE_PAREN,
    PUNCTUATION_OPEN_BRACK,
    PUNCTUATION_CLOSE_BRACK,
    PUNCTUATION_OPEN_CURLY,
    PUNCTUATION_CLOSE_CURLY,
    PUNCTIATION_SEMICOLON,
    PUNCTUATION_COUNT,
} token_punctuation_type;

typedef enum token_keyword_type {
    KEYWORD_FUNC,
    KEYWORD_LET,
    KEYWORD_COUNT,
} token_keyword_type;

typedef struct token_t {
    token_type type;
    union {
        token_operator_type opType;
        token_punctuation_type puncType;
        token_keyword_type keywordType;
        struct {
            token_literal_type litType;
            union {
                int intLiteral;
                float realLiteral;
                u8 *strLiteral;
            };
        };

        int typeType;
    };

    u8 *lexeme;
    int line;
} token_t;

token_t *tokenize(u8 *buffer, int bufferSize);

void PrintTokens(token_t *tokens);

#endif