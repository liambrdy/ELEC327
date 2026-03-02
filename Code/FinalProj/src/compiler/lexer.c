#include "lexer.h"

#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "darray.h"
#include "arena.h"
#include "file.h"

typedef struct lookup_result_t {
    union {
        token_literal_type litType;
        token_punctuation_type puncType;
        token_keyword_type keywordType;
        pre_type preType;

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
        pre_type preType;

        int type;
    };
} lookup_map_t;

lookup_map_t keywords[] = {
    {"auto", KEYWORD_AUTO},
    {"extern", KEYWORD_EXTERN},
    {"register", KEYWORD_REGISTER},
    {"static", KEYWORD_STATIC},
    {"typedef", KEYWORD_TYPEDEF},
    {"void", KEYWORD_VOID},
    {"char", KEYWORD_CHAR},
    {"short", KEYWORD_SHORT},
    {"int", KEYWORD_INT},
    {"long", KEYWORD_LONG},
    {"float", KEYWORD_FLOAT},
    {"double", KEYWORD_DOUBLE},
    {"signed", KEYWORD_SIGNED},
    {"unsigned", KEYWORD_UNSIGNED},
    {"struct", KEYWORD_STRUCT},
    {"union", KEYWORD_UNION},
    {"enum", KEYWORD_ENUM},
    {"const", KEYWORD_CONST},
    {"restrict", KEYWORD_RESTRICT},
    {"volatile", KEYWORD_VOLATILE},
    {"inline", KEYWORD_INLINE},
    {"goto", KEYWORD_GOTO},
    {"continue", KEYWORD_CONTINUE},
    {"break", KEYWORD_BREAK},
    {"return", KEYWORD_RETURN},
    {"while", KEYWORD_WHILE},
    {"do", KEYWORD_DO},
    {"for", KEYWORD_FOR},
    {"if", KEYWORD_IF},
    {"else", KEYWORD_ELSE},
    {"switch", KEYWORD_SWITCH},
    {"case", KEYWORD_CASE},
    {"default", KEYWORD_DEFAULT},
};

static trie_node_t *puncTree = NULL;

lookup_map_t punctuations[] = {
    {",", PUNCTUATION_COMMA},
    {"(", PUNCTUATION_OPEN_PAREN},
    {")", PUNCTUATION_CLOSE_PAREN},
    {"[", PUNCTUATION_OPEN_BRACK},
    {"]", PUNCTUATION_CLOSE_BRACK},
    {"{", PUNCTUATION_OPEN_CURLY},
    {"}", PUNCTUATION_CLOSE_CURLY},
    {";", PUNCTUATION_SEMICOLON},
    {":", PUNCTUATION_COLON},
    {"?", PUNCTUATION_QUESTION_MARK},
    {".", PUNCTUATION_PERIOD},
    {"*", PUNCTUATION_STAR},
    {"->", PUNCTUATION_RIGHT_ARROW},
    {"<-", PUNCTUATION_LEFT_ARROW},

    {"+", PUNCTUATION_PLUS},
    {"-", PUNCTUATION_MINUS},
    {"/", PUNCTUATION_DIV},
    {"%", PUNCTUATION_MOD},

    {"=", PUNCTUATION_EQUALS},
    {"+=", PUNCTUATION_PLUS_EQUALS},
    {"-=", PUNCTUATION_MINUS_EQUALS},
    {"*=", PUNCTUATION_MULT_EQUALS},
    {"/=", PUNCTUATION_DIV_EQUALS},
    {"<<=", PUNCTUATION_SHL_EQUALS},
    {">>=", PUNCTUATION_SHR_EQUALS},
    {"^=", PUNCTUATION_MOD_EQUALS},
    {"&=", PUNCTUATION_AND_EQUALS},
    {"|=", PUNCTUATION_OR_EQUALS},
    {"^=", PUNCTUATION_XOR_EQUALS},

    {"<", PUNCTUATION_LT},
    {">", PUNCTUATION_GT},
    {"<=", PUNCTUATION_LT_EQ},
    {">=", PUNCTUATION_GT_EQ},
    {"==", PUNCTUATION_EQUIV},
    {"!=", PUNCTUATION_NOT_EQUIV},
    {"||", PUNCTUATION_LOGIC_OR},
    {"&&", PUNCTUATION_LOGIC_AND},
    {"!", PUNCTUATION_LOGIC_NOT},
    {"|", PUNCTUATION_OR},
    {"&", PUNCTUATION_AND},
    {"^", PUNCTUATION_CARROT},
    {"~", PUNCTUATION_NOT},
    {"<<", PUNCTUATION_SHL},
    {">>", PUNCTUATION_SHR},

    {"//", PUNCTUATION_COMMENT},
    {"\\", PUNCTUATION_BACKSLASH},

    {"++", PUNCTUATION_INCREMENT},
    {"--", PUNCTUATION_DECREMENT},
};

lookup_map_t preLookup[] = {
    {"define", PRE_DEFINE},
    {"ifdef", PRE_IFDEF},
    {"ifndef", PRE_IFNDEF},
    {"else", PRE_ELSE},
    {"endif", PRE_ENDIF},
    {"include", PRE_INCLUDE},
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
    u8 *substr = PushArray(globalArena, u8, length + 1);
    memcpy(substr, string + start, length);
    substr[length] = '\0';

    return substr;
}

void InsertNode(trie_node_t *node, u8 *str, token_punctuation_type type) {
    trie_node_t *current = node;
    
    for (int i = 0; i < strlen(str); i++) {
        if (!current->children[str[i]]) {
            trie_node_t *newNode = PushStruct(globalArena, trie_node_t);

            newNode->type = PUNCTUATION_COUNT;

            current->children[str[i]] = newNode;
        }

        current = current->children[str[i]];
    }

    current->type = type;
}

u8 *GetWord(u8 *buffer, int *cursor) {
    int start = *cursor;
    while (isalnum(buffer[(*cursor) + 1]) || buffer[(*cursor) + 1] == '_') (*cursor)++;

    return substring(buffer, start, *cursor);
}

token_t *tokenize(u8 *buffer, int bufferSize) {
    token_t *tokens = DArrayCreate(token_t);

    if (!puncTree) {
        puncTree = PushStruct(globalArena, trie_node_t);
        puncTree->type = PUNCTUATION_COUNT;


        for (int i = 0; i < sizeof(punctuations) / sizeof(punctuations[0]); i++) {
            InsertNode(puncTree, punctuations[i].str, punctuations[i].puncType);
        }
    }

    int line = 0;
    int cursor = 0;
    while (cursor < bufferSize) {
        if (isalpha(buffer[cursor]) || buffer[cursor] == '_') {
            u8 *lexeme = GetWord(buffer, &cursor);

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
            trie_node_t *node = puncTree;
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
            
            if (lastType == PUNCTUATION_COMMENT) {
                cursor = lastPos;
                while (buffer[cursor++] != '\n') {
                    if (buffer[cursor] == '\0')
                        break;
                }
            } else if (lastType != PUNCTUATION_COUNT) {
                token_t punctToken = {0};
                punctToken.type = TOKEN_PUNCTUATION;
                punctToken.puncType = lastType;
                punctToken.lexeme = substring(buffer, cursor, lastPos - 1);
                punctToken.line = line;

                DArrayPush(tokens, punctToken);

                cursor = lastPos;
            } else {
                switch (buffer[cursor]) {
                    case '#': {
                        token_t hash = {
                            .type = TOKEN_HASH,
                            .line = line,
                        };
                        DArrayPush(tokens, hash);

                        cursor++;
                    } break;

                    case '"': {
                        u8 *str = DArrayCreate(u8);

                        int start = cursor;
                        while (buffer[cursor + 1] != '"') {
                            u8 c = buffer[cursor + 1];
                            if (buffer[cursor + 1] == '\\') {
                                u8 esc = buffer[cursor + 2];
                                DArrayPush(str, esc);
                                cursor += 2;
                            } else {
                                DArrayPush(str, c);
                                cursor++;
                            }
                        }

                        u8 end = '\0';
                        DArrayPush(str, end);

                        token_t strLiteral = {
                            .type = TOKEN_LITERAL,
                            .litType = LITERAL_STRING,
                            .strLiteral = str,
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
                        token_t nl = {
                            .type = TOKEN_NEW_LINE,
                            .line = line,
                        };
                        DArrayPush(tokens, nl);

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
                        printf("Unknown character on line %d: %c\n", line + 1, buffer[cursor]);
                        return NULL;
                    }
                }
            }
        }
    }

    printf("Unrechable\n");
    return 0;
}

static token_t *Peek(preprocessor_t *pre) {
    return &pre->input[pre->cursor];
}

static bool Match(preprocessor_t *pre, token_type type) {
    if (Peek(pre)->type == type) {
        pre->cursor++;

        if (type == TOKEN_NEW_LINE) {
            pre->atLineStart = false;
        }

        return true;
    }

    return false;
}

static token_t *Consume(preprocessor_t *pre, token_type type) {
    token_t *t = Peek(pre);
    if (t->type != type) {
        printf("Next token is not of type %d\n", type);
        return NULL;
    }
    
    pre->cursor++;
    return t;
}

static void Expect(preprocessor_t *pre, token_type type, u8 *msg) {
    if (pre->input[pre->cursor].type != type) {
        printf("%s\n", msg);
        return;
    }

    pre->cursor++;

    if (type == TOKEN_NEW_LINE)
        pre->atLineStart = true;
}

conditional_level_t *CondTop(preprocessor_t *pre) {
    int len = DArrayLength(pre->condStack);
    return &pre->condStack[len - 1];
}

bool IsActive(preprocessor_t *pre) {
    int len = DArrayLength(pre->condStack);
    return len == 0 || pre->condStack[len - 1].currentlyActive;
}

bool CanSurviveUnactive(pre_type type) {
    switch (type) {
        case PRE_DEFINE: return false;
        case PRE_IFDEF: return true;
        case PRE_IFNDEF: return true;
        case PRE_ELSE: return true;
        case PRE_ENDIF: return true;
        case PRE_INCLUDE: return false;

        default: return false;
    }
}

void ParseDirective(preprocessor_t *pre) {
    token_t *name = Consume(pre, TOKEN_IDENTIFIER);

    lookup_result_t res = LookupStr(name->lexeme, PRE_COUNT, preLookup);
    if (!res.found) {
        printf("unknown preprocessor: %s\n", name->lexeme);
        return;
    }

    if (!IsActive(pre) && !CanSurviveUnactive(res.preType)) {
        return;
    }

    switch (res.preType) {
        case PRE_DEFINE: {
            token_t *defName = Consume(pre, TOKEN_IDENTIFIER);

            if (HashContains(pre->macroTable, defName->lexeme)) {
                printf("%s is already defined\n", defName->lexeme);
                return;
            }

            macro_t m = {};
            m.replacement = DArrayCreate(token_t);
            while (!Match(pre, TOKEN_NEW_LINE)) {
                token_t *t = Peek(pre);
                if (t->type = TOKEN_PUNCTUATION && t->puncType == PUNCTUATION_BACKSLASH) {
                    pre->cursor++;
                    Expect(pre, TOKEN_NEW_LINE, "expects new line after extended line define");
                } else {
                    DArrayPush(m.replacement, pre->input[pre->cursor++]);
                }
            }

            pre->atLineStart = true;
            HashInsert(pre->macroTable, defName->lexeme, &m);
        } break;

        case PRE_IFDEF: {
            token_t *defName = Consume(pre, TOKEN_IDENTIFIER);
            bool defined = HashContains(pre->macroTable, defName->lexeme);
            bool parent = IsActive(pre);

            conditional_level_t level = {
                .parentActive = parent,
                .thisBranchTaken = defined,
                .currentlyActive = parent && defined,
            };

            DArrayPush(pre->condStack, level);
            Expect(pre, TOKEN_NEW_LINE, "expects new line after #ifdef");
        } break;

        case PRE_IFNDEF: {
            token_t *defName = Consume(pre, TOKEN_IDENTIFIER);
            bool ndefined = !HashContains(pre->macroTable, defName->lexeme);
            bool parent = IsActive(pre);

            conditional_level_t level = {
                .parentActive = parent,
                .thisBranchTaken = ndefined,
                .currentlyActive = parent && ndefined,
            };

            DArrayPush(pre->condStack, level);
            Expect(pre, TOKEN_NEW_LINE, "expects new line after #ifndef");
        } break;

        case PRE_ELSE: {
            if (DArrayLength(pre->condStack) == 0) {
                printf("#else with empty conditional stack\n");
                return;
            }

            conditional_level_t *level = CondTop(pre);
            bool newActive = level->parentActive && !level->thisBranchTaken;

            level->currentlyActive = newActive;
            level->thisBranchTaken = true;
            Expect(pre, TOKEN_NEW_LINE, "expects new line after #else");
        } break;

        case PRE_ENDIF: {
            if (DArrayLength(pre->condStack) == 0) {
                printf("#endif with empty conditional stack\n");
                return;
            }

            DArrayPop(pre->condStack, NULL);
            Match(pre, TOKEN_NEW_LINE);
        } break;

        case PRE_INCLUDE: {
            token_t *t = Peek(pre);
            if (Match(pre, TOKEN_LITERAL)) {
                if (t->litType =! LITERAL_STRING) {
                    printf("expects string literal for include\n");
                    return;
                }

                int currentFileLen = strlen(pre->currentFile);
                u8 *currentDirPath = (u8 *)malloc(currentFileLen);

                memset(currentDirPath, 0, currentFileLen);
                int lastSlashPath = 0;
                for (int i = 0; i < currentFileLen; i++) {
                    if (pre->currentFile[i] == '/') lastSlashPath = i;
                }

                strncpy(currentDirPath, pre->currentFile, lastSlashPath);
                currentDirPath[lastSlashPath] = '\0';

                u32 newStrLen = strlen(pre->currentFile) + strlen(t->strLiteral) + 2;
                u8 *path = (u8 *)malloc(newStrLen);

                snprintf(path, newStrLen, "%s/%s", currentDirPath, t->strLiteral);

                loaded_file_t f = LoadFile(path);
                if (!f.success) {
                    printf("must give path to include file: %s not found\n", t->strLiteral);
                    return;
                }

                free(currentDirPath);
                free(path);

                token_t *tokens = tokenize(f.buffer, f.bufferLen);

                file_context_t ctx = {
                    .tokens = pre->input,
                    .cursor = pre->cursor,
                    .filename = pre->currentFile,
                };
                DArrayPush(pre->includeStack, ctx);

                pre->input = tokens;
                pre->cursor = 0;
                pre->currentFile = t->lexeme;
            }
        } break;

        default: break;
    }
}

void ExpandOrEmit(preprocessor_t *pre) {
    token_t *t = Peek(pre);

    if (t->type == TOKEN_NEW_LINE) {
        pre->atLineStart = true;
        pre->cursor++;
        return;
    }

    macro_t *macro = NULL;

    if (t->type == TOKEN_IDENTIFIER && HashContainsRet(pre->macroTable, t->lexeme, (void **)&macro)) {
        pre->cursor++;

        int len = DArrayLength(macro->replacement);
        for (int i = 0; i < len; i++) {
            DArrayPush(pre->output, macro->replacement[i]);
        }
    } else {
        DArrayPush(pre->output, pre->input[pre->cursor]);
        pre->cursor++;
    }

    pre->atLineStart = false;
}

token_t *preprocess(token_t *ppTokens, u8 *filename) {
    preprocessor_t pre = {0};
    pre.input = ppTokens;
    pre.output = DArrayCreate(token_t);
    pre.condStack = DArrayCreate(conditional_level_t);
    pre.includeStack = DArrayCreate(file_context_t);
    pre.macroTable = CreateHashTable(sizeof(macro_t));
    pre.atLineStart = true;
    pre.currentFile = filename;

    bool parent = true;

    do {
        if (!parent) {
            file_context_t ctx = {};
            DArrayPop(pre.includeStack, &ctx);

            pre.input = ctx.tokens;
            pre.cursor = ctx.cursor;
            pre.currentFile = ctx.filename;
        }

        parent = false;

        while (!Match(&pre, TOKEN_EOF)) {
            if (pre.atLineStart && Match(&pre, TOKEN_HASH))
                ParseDirective(&pre);
            else if (IsActive(&pre))
                ExpandOrEmit(&pre);
            else
                pre.cursor++;
        }
    } while (DArrayLength(pre.includeStack) != 0);

    return pre.output;
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

            case TOKEN_NEW_LINE: {
                printf("NewLine()");
            } break;

            case TOKEN_HASH: {
                printf("#");
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