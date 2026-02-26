#ifndef _LEXER_H
#define _LEXER_H

#include "common.h"

typedef enum token_type {
    TOKEN_IDENTIFIER,
    TOKEN_LITERAL,
    TOKEN_PUNCTUATION,
    TOKEN_KEYWORD,
    TOKEN_EOF,
} token_type;

typedef enum token_literal_type {
    LITERAL_INT,
    LITERAL_REAL,
    LITERAL_STRING,
} token_literal_type;

typedef enum token_punctuation_type {
    PUNCTUATION_COMMA,
    PUNCTUATION_OPEN_PAREN,
    PUNCTUATION_CLOSE_PAREN,
    PUNCTUATION_OPEN_BRACK,
    PUNCTUATION_CLOSE_BRACK,
    PUNCTUATION_OPEN_CURLY,
    PUNCTUATION_CLOSE_CURLY,
    PUNCTUATION_SEMICOLON,
    PUNCTUATION_COLON,
    PUNCTUATION_QUESTION_MARK,
    PUNCTUATION_PERIOD,
    PUNCTUATION_STAR,

    PUNCTUATION_PLUS,
    PUNCTUATION_MINUS,
    PUNCTUATION_DIV,
    PUNCTUATION_MOD,

    PUNCTUATION_EQUALS,
    PUNCTUATION_PLUS_EQUALS,
    PUNCTUATION_MINUS_EQUALS,
    PUNCTUATION_MULT_EQUALS,
    PUNCTUATION_DIV_EQUALS,

    PUNCTUATION_LT,
    PUNCTUATION_GT,
    PUNCTUATION_LT_EQ,
    PUNCTUATION_GT_EQ,
    PUNCTUATION_EQUIV,
    PUNCTUATION_NOT_EQUIV,
    PUNCTUATION_LOGIC_OR,
    PUNCTUATION_LOGIC_AND,
    PUNCTUATION_LOGIC_NOT,

    PUNCTUATION_OR,
    PUNCTUATION_AND,
    PUNCTUATION_CARROT,
    PUNCTUATION_NOT,

    PUNCTUATION_INCREMENT,
    PUNCTUATION_DECREMENT,

    PUNCTUATION_SHL,
    PUNCTUATION_SHR,

    PUNCTUATION_COMMENT,

    PUNCTUATION_COUNT,
} token_punctuation_type;

typedef enum token_keyword_type {
    KEYWORD_AUTO,
    KEYWORD_EXTERN,
    KEYWORD_REGISTER,
    KEYWORD_STATIC,
    KEYWORD_TYPEDEF,

    KEYWORD_VOID,
    KEYWORD_CHAR,
    KEYWORD_SHORT,
    KEYWORD_INT,
    KEYWORD_LONG,
    KEYWORD_FLOAT,
    KEYWORD_DOUBLE,
    KEYWORD_SIGNED,
    KEYWORD_UNSIGNED,

    KEYWORD_STRUCT,
    KEYWORD_UNION,
    KEYWORD_ENUM,

    KEYWORD_CONST,
    KEYWORD_RESTRICT,
    KEYWORD_VOLATILE,

    KEYWORD_INLINE,

    KEYWORD_GOTO,
    KEYWORD_CONTINUE,
    KEYWORD_BREAK,
    KEYWORD_RETURN,
    KEYWORD_WHILE,
    KEYWORD_DO,
    KEYWORD_FOR,
    KEYWORD_IF,
    KEYWORD_ELSE,
    KEYWORD_SWITCH,
    KEYWORD_CASE,
    KEYWORD_DEFAULT,

    KEYWORD_COUNT,
} token_keyword_type;

typedef enum pre_type {
    PRE_IF,
    PRE_IFDEF,
    PRE_IFNDEF,
    PRE_ELIF,
    PRE_ELSE,
    PRE_ENDIF,
    PRE_DEFINE,
    PRE_INCLUDE,
    PRE_COUNT,
} pre_type;

typedef struct trie_node_t {
    token_punctuation_type type;
    struct trie_node_t *children[128];
} trie_node_t;

typedef struct token_t {
    token_type type;
    union {
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