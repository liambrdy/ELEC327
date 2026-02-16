#include "lexer.h"

#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "darray.h"

typedef struct lookup_result_t {
    union {
        token_literal_type litType;
        token_punctuation_type puncType;
        token_keyword_type keywordType;

        int type;
    };

    bool found;
} lookup_result_t;

typedef struct lookup_map_t {
    u8 *str;
    union {
        token_literal_type litType;
        token_punctuation_type puncType;
        token_keyword_type keywordType;

        int type;
    };
} lookup_map_t;

lookup_map_t keywords[] = {
    {"func", KEYWORD_FUNC},
    {"let", KEYWORD_LET},
};

lookup_map_t punctuations[] = {
    {",", PUNCTUATION_COMMA},
    {"(", PUNCTAUTION_OPEN_PAREN},
    {")", PUNCTUATION_CLOSE_PAREN},
    {"[", PUNCTUATION_OPEN_BRACK},
    {"]", PUNCTUATION_CLOSE_BRACK},
    {"{", PUNCTUATION_OPEN_CURLY},
    {"}", PUNCTUATION_CLOSE_CURLY},
    {";", PUNCTUATION_SEMICOLON},
    {":", PUNCTUATION_COLON},
    {"?", PUNCTUATION_QUESTION_MARK},

    {"+", PUNCTUATION_PLUS},
    {"-", PUNCTUATION_MINUS},
    {"*", PUNCTUATION_MULT},
    {"/", PUNCTUATION_DIV},

    {"=", PUNCTUATION_EQUALS},
    {"+=", PUNCTUATION_PLUS_EQUALS},
    {"-=", PUNCTUATION_MINUS_EQUALS},
    {"*=", PUNCTUATION_TIMES_EQUALS},
    {"/=", PUNCTUATION_DIV_EQUALS},

    {"<", PUNCTUATION_LT},
    {">", PUNCTUATION_GT},
    {"<=", PUNCTUATION_LT_EQ},
    {">=", PUNCTUATION_GT_EQ},
    {"==", PUNCTUATION_EQUIV},
    {"!=", PUNCTUATION_NOT_EQUIV},
    {"||", PUNCTUATION_LOGIC_OR},
    {"&&", PUNCTUATION_LOGIC_AND},
    {"|", PUNCTUATION_OR},
    {"&", PUNCTUATION_AND},
    {"^", PUNCTUATION_CARROT},

    {"++", PUNCTUATION_INCREMENT},
    {"--", PUNCTUATION_DECREMENT},
};

lookup_result_t LookupStr(u8* str, int typeCount, lookup_map_t *lookupMap) {
    lookup_result_t res = {0};

    for (int i = 0; i < typeCount; i++) {
        if (strcmp(str, lookupMap[i].str) == 0) {
            res.type = lookupMap[i].type;
            res.found = true;

            return res;
        }
    }

    return res;
}

u8 *substring(u8 *string, int start, int end) {
    int length = end - start + 1;
    u8 *substr = (u8 *)malloc((length + 1) * sizeof(u8));
    memcpy(substr, string + start, length);
    substr[length] = '\0';

    return substr;
}

void InsertNode(trie_node_t *node, u8 *str, token_punctuation_type type) {
    trie_node_t *current = node;
    
    for (int i = 0; i < strlen(str); i++) {
        if (!current->children[str[i]]) {
            trie_node_t *newNode = (trie_node_t *)malloc(sizeof(trie_node_t));
            newNode->type = PUNCTUATION_COUNT;

            current->children[str[i]] = newNode;
        }

        current = current->children[str[i]];
    }

    current->type = type;
}

token_t *tokenize(u8 *buffer, int bufferSize) {
    token_t *tokens = DArrayCreate(token_t);

    trie_node_t parent = {0};
    parent.type = PUNCTUATION_COUNT;

    for (int i = 0; i < sizeof(punctuations) / sizeof(punctuations[0]); i++) {
        InsertNode(&parent, punctuations[i].str, punctuations[i].puncType);
    }

    int line = 0;
    int cursor = 0;
    while (cursor < bufferSize) {
        if (isalpha(buffer[cursor])) {
            int start = cursor;
            while (isalnum(buffer[cursor + 1])) cursor++;

            u8 *lexeme = substring(buffer, start, cursor);

            lookup_result_t res = LookupStr(lexeme, KEYWORD_COUNT, keywords);
            if (res.found) {
                token_t newKeyword = {
                    .type = TOKEN_KEYWORD,
                    .keywordType = res.keywordType,
                    .lexeme = lexeme,
                    .line = line,
                };

                DArrayPush(tokens, newKeyword);
            } else {
                token_t newIdentifier = {
                    .type = TOKEN_IDENTIFIER,
                    .lexeme = lexeme,
                    .line = line
                };

                DArrayPush(tokens, newIdentifier);
            }

            cursor++;
        } else if (isdigit(buffer[cursor])) {
            int start = cursor;
            while (isdigit(buffer[cursor + 1])) cursor++;

            u8 *lexeme = substring(buffer, start, cursor);
            token_t intLiteral = {
                .type = TOKEN_LITERAL,
                .litType = LITERAL_INT,
                .intLiteral = atoi(lexeme),
                .lexeme = lexeme,
                .line = line,
            };

            DArrayPush(tokens, intLiteral);

            cursor++;
        } else {
            trie_node_t *node = &parent;
            int pos = cursor;
            
            int lastPos = pos;
            token_punctuation_type lastType = PUNCTUATION_COUNT;

            while (pos < bufferSize) {
                u8 c = buffer[pos];
                trie_node_t *next = node->children[c];

                if (!next) break;

                node = next;
                pos++;

                if (node->type != PUNCTUATION_COUNT) {
                    lastPos = pos;
                    lastType = node->type;
                }
            }
            
            if (lastType != PUNCTUATION_COUNT) {
                token_t punctToken = {0};
                punctToken.type = TOKEN_PUNCTUATION;
                punctToken.puncType = lastType;
                punctToken.lexeme = substring(buffer, cursor, lastPos - 1);
                punctToken.line = line;

                DArrayPush(tokens, punctToken);

                cursor = lastPos;
            } else {
                switch (buffer[cursor]) {
                    case '"': {
                        int start = cursor;
                        while (buffer[cursor + 1] != '"') cursor++;

                        token_t strLiteral = {
                            .type = TOKEN_LITERAL,
                            .litType = LITERAL_STRING,
                            .strLiteral = substring(buffer, start + 1, cursor),
                            .lexeme = substring(buffer, start, cursor + 1),
                            .line = line,
                        };

                        DArrayPush(tokens, strLiteral);

                        cursor += 2;
                    } break;

                    case ' ': {
                        while (buffer[cursor] == ' ') cursor++;
                    } break;

                    case '\n': {
                        cursor++;
                        line++;
                    } break;

                    case '\0': {
                        token_t eof = {
                            .type = TOKEN_EOF,
                            .line = line,
                        };

                        DArrayPush(tokens, eof);
                        return tokens;
                    } break;

                    default: {
                        printf("Unknown character: %c\n", buffer[cursor]);
                        return NULL;
                    }
                }
            }
        }
    }

    return 0;
}

void PrintTokens(token_t *tokens) {
    printf("[");
    for (u32 i = 0; i < DArrayLength(tokens); i++) {
        token_t token = tokens[i];

        switch (token.type) {
            case TOKEN_IDENTIFIER: {
                printf("Identifier(%s)", token.lexeme);
            } break;

            case TOKEN_LITERAL: {
                switch (token.litType) {
                    case LITERAL_INT: printf("IntLiteral(%d)", token.intLiteral); break;
                    case LITERAL_REAL: printf("RealLiteral(%f)", token.realLiteral); break;
                    case LITERAL_STRING: printf("StrLiteral(%s)", token.strLiteral); break;
                }
            } break;

            case TOKEN_PUNCTUATION: {
                printf("Punctuation(%s)", token.lexeme);
            } break;

            case TOKEN_KEYWORD: {
                printf("Keyword(%s)", token.lexeme);
            } break;

            case TOKEN_EOF: {
                printf("EOF()");
            } break;
        }

        if (i < DArrayLength(tokens) - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}