#ifndef _LEXER_H
#define _LEXER_H

#include "common.h"
#include "darray.h"
#include "hashtable.h"
#include "str.h"

typedef enum token_type {
    TOKEN_IDENTIFIER,
    TOKEN_LITERAL,
    TOKEN_PUNCTUATION,
    TOKEN_KEYWORD,

    TOKEN_HASH,
    TOKEN_NEW_LINE,
    TOKEN_EOF,
} token_type;

typedef enum token_literal_type {
    LITERAL_INT,
    LITERAL_REAL,
    LITERAL_STRING,
    LITERAL_CHAR,
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
    PUNCTUATION_RIGHT_ARROW,
    PUNCTUATION_LEFT_ARROW,

    PUNCTUATION_PLUS,
    PUNCTUATION_MINUS,
    PUNCTUATION_DIV,
    PUNCTUATION_MOD,

    PUNCTUATION_EQUALS,
    PUNCTUATION_PLUS_EQUALS,
    PUNCTUATION_MINUS_EQUALS,
    PUNCTUATION_MULT_EQUALS,
    PUNCTUATION_DIV_EQUALS,
    PUNCTUATION_SHL_EQUALS,
    PUNCTUATION_SHR_EQUALS,
    PUNCTUATION_MOD_EQUALS,
    PUNCTUATION_AND_EQUALS,
    PUNCTUATION_OR_EQUALS,
    PUNCTUATION_XOR_EQUALS,

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
    PUNCTUATION_BACKSLASH,

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
    PRE_DEFINE,
    PRE_IFDEF,
    PRE_IFNDEF,
    PRE_ELSE,
    PRE_ENDIF,
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
                slice_t strLiteral;
            };
        };

        int typeType;
    };

    slice_t lexeme;
    int line;
} token_t;

typedef struct macro_t {
    token_t *replacement;
} macro_t;

typedef struct conditional_level_t {
    bool parentActive;
    bool thisBranchTaken;
    bool currentlyActive;
} conditional_level_t;

typedef struct lexer_t {
    u8 *buffer;
    u32 bufferLen;
    u32 cursor;
} lexer_t;

typedef struct file_context_t {
    lexer_t lex;
    u8 *filename;
} file_context_t;

typedef struct preprocessor_t {
    lexer_t lex;

    bool atLineStart;

    hash_table_t *macroTable;
    conditional_level_t *condStack;
    file_context_t *includeStack;

    u8 *currentFile;
    u8 **incDirs;

    token_t lookAhead[2];
    u32 lookAheadCount;

    token_t *output;
} preprocessor_t;

token_t *tokenize(u8 *buffer, u32 bufferSize, u8 *filename, u8 **incDirs);

void PrintTokens(token_t *tokens);

#endif