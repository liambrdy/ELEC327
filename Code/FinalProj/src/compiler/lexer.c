#include "lexer.h"

#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "darray.h"

typedef struct lookup_result_t {
    union {
        token_operator_type opType;
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
        token_operator_type opType;
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

lookup_map_t twoOperators[] = {
    {"<=", OPERATOR_LESS_THAN},
    {">=", OPERATOR_GREATER_THAN},
    {"==", OPERATOR_EQUIV},
    {"!=", OPERATOR_NOT_EQUIV},
    {"++", OPERATOR_INCREMENT},
    {"--", OPERATOR_DECREMENT},
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

void PushPunctuation(token_t **tokens, u8 *buffer, int cursor, int line, token_punctuation_type punctType) {
    token_t punctuation = {
        .type = TOKEN_PUNCTUATION,
        .puncType = punctType,
        .lexeme = substring(buffer, cursor, cursor),
        .line = line,
    };

    DArrayPush(*tokens, punctuation);
}

void PushOperator(token_t **tokens, u8 *buffer, int cursor, int line, token_operator_type opType) {
    token_t operator = {
        .type = TOKEN_OPERATOR,
        .opType = opType,
        .lexeme = substring(buffer, cursor, cursor),
        .line = line,
    };

    DArrayPush(*tokens, operator);
}

token_t *tokenize(u8 *buffer, int bufferSize) {
    token_t *tokens = DArrayCreate(token_t);

    int line = 0;
    int cursor = 0;
    while (cursor < bufferSize) {
        lookup_result_t twoOpRes = {0};
        u8 *twoOpLexeme = 0;
        bool twoOpLexemeUsed = false;
        
        if (bufferSize - cursor > 1) {
            twoOpLexeme = substring(buffer, cursor, cursor+1);
            twoOpRes = LookupStr(twoOpLexeme, sizeof(twoOperators) / sizeof(twoOperators[0]), twoOperators);
        }

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
        } else if (twoOpRes.found) {
            token_t twoOp = {
                .type = TOKEN_OPERATOR,
                .opType = twoOpRes.opType,
                .lexeme = twoOpLexeme,
                .line = line
            };

            DArrayPush(tokens, twoOp);

            cursor += 2;
            twoOpLexemeUsed = true;
        } else {
            switch (buffer[cursor]) {
                case '+': PushOperator(&tokens, buffer, cursor, line, OPERATOR_PLUS); cursor++; break;
                case '-': PushOperator(&tokens, buffer, cursor, line, OPERATOR_MINUS); cursor++; break;
                case '*': PushOperator(&tokens, buffer, cursor, line, OPERATOR_MULT); cursor++; break;
                case '/': PushOperator(&tokens, buffer, cursor, line, OPERATOR_DIV); cursor++; break;
                case '=': PushOperator(&tokens, buffer, cursor, line, OPERATOR_EQUALS); cursor++; break;

                case ',': PushPunctuation(&tokens, buffer, cursor, line, PUNCTUATION_COMMA); cursor++; break;
                case '(': PushPunctuation(&tokens, buffer, cursor, line, PUNCTAUTION_OPEN_PAREN); cursor++; break;
                case ')': PushPunctuation(&tokens, buffer, cursor, line, PUNCTUATION_CLOSE_PAREN); cursor++; break;
                case '[': PushPunctuation(&tokens, buffer, cursor, line, PUNCTUATION_OPEN_BRACK); cursor++; break;
                case ']': PushPunctuation(&tokens, buffer, cursor, line, PUNCTUATION_CLOSE_BRACK); cursor++; break;
                case '{': PushPunctuation(&tokens, buffer, cursor, line, PUNCTUATION_OPEN_CURLY); cursor++; break;
                case '}': PushPunctuation(&tokens, buffer, cursor, line, PUNCTUATION_CLOSE_CURLY); cursor++; break;
                case ';': PushPunctuation(&tokens, buffer, cursor, line, PUNCTIATION_SEMICOLON); cursor++; break;

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

        if (twoOpLexeme && !twoOpLexemeUsed) {
            free(twoOpLexeme);
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

            case TOKEN_OPERATOR: {
                printf("Operator(%s)", token.lexeme);
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