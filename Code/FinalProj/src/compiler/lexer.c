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

lookup_result_t LookupStr(slice_t *slice, int typeCount, lookup_map_t *lookupMap) {
    lookup_result_t res = {0};

    for (int i = 0; i < typeCount; i++) {
        if (CompareSliceToStr(slice, lookupMap[i].str, strlen(lookupMap[i].str))) {
            res.type = lookupMap[i].type;
            res.found = true;

            return res;
        }
    }

    return res;
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

void ExpandOrEmit(preprocessor_t *pre, token_t *t) {
    if (t->type == TOKEN_NEW_LINE) {
        pre->atLineStart = true;
        return;
    }

    macro_t *macro = NULL;

    if (t->type == TOKEN_IDENTIFIER && HashContainsRet(pre->macroTable, &t->lexeme, (void **)&macro)) {
        int len = DArrayLength(macro->replacement);
        for (int i = 0; i < len; i++) {
            DArrayPush(pre->output, macro->replacement[i]);
        }
    } else {
        DArrayPush(pre->output, *t);
    }

    pre->atLineStart = false;
}

bool PopFile(preprocessor_t *pre) {
    if (DArrayLength(pre->includeStack) == 0) {
        return false;
    }

    file_context_t ctx = {};
    DArrayPop(pre->includeStack, &ctx);

    pre->lex = ctx.lex;
    pre->currentFile = ctx.filename;
    pre->lookAheadCount = 0;

    return true;
}

static void PushEOF(preprocessor_t *pre) {
    token_t eof = {
        .type = TOKEN_EOF
    };

    DArrayPush(pre->output, eof);
}

static bool LexerAtEOF(lexer_t *lex) {
    return lex->buffer[lex->cursor] == '\0';
}

static void SkipWhitespaceComments(lexer_t *lex) {
    while (true) {
        u8 c = lex->buffer[lex->cursor];
        u8 n = lex->buffer[lex->cursor + 1];

        if (c != '\n' && isspace(c)) {
            lex->cursor++;
            continue;
        }

        if (c == '/' && n == '/') {
            lex->cursor += 2;
            while (!LexerAtEOF(lex) && lex->buffer[lex->cursor++] != '\n');

            continue;
        }

        if (c == '/' && n == '*') {
            lex->cursor += 2;

            while (!LexerAtEOF(lex)) {
                if (lex->buffer[lex->cursor] == '*' && lex->buffer[lex->cursor + 1] == '/') {
                    lex->cursor += 2;
                    break;
                }

                lex->cursor++;
            }

            continue;
        }

        break;
    }
}

static slice_t MakeSlice(lexer_t *lex, u32 start, u32 end) {
    slice_t s = {
        .str = lex->buffer + start,
        .len = end - start,
    };

    return s;
}

static token_t LexString(lexer_t *lex) {
    i32 strStart = lex->cursor++;

    while (!LexerAtEOF(lex)) {
        u8 c = lex->buffer[lex->cursor++];

        if (c == '\\') {
            lex->cursor++;
            continue;
        }

        if (c == '"')
            break;

        if (c == '\n') {
            printf("error: new line in string literal\n");
        }
    }

    token_t str = {
        .type = TOKEN_LITERAL,
        .litType = LITERAL_STRING,
        .strLiteral = MakeSlice(lex, strStart + 1, lex->cursor - 1),
        .lexeme = MakeSlice(lex, strStart, lex->cursor),
    };

    return str;
}

static token_t LexChar(lexer_t *lex) {
    i32 charStart = lex->cursor++;

    int val = 0;

    while (!LexerAtEOF(lex)) {
        u8 c = lex->buffer[lex->cursor++];

        if (c == '\\') {
            lex->cursor++;
            continue;
        }

        if (c == '\'')
            break;

        if (c == '\n') {
            printf("error: new line in char literal\n");
        }

        val = (val << 8) | c;
    }

    token_t chr = {
        .type = TOKEN_LITERAL,
        .litType = LITERAL_CHAR,
        .intLiteral = val,
        .lexeme = MakeSlice(lex, charStart, lex->cursor),
    };

    return chr;
}

static token_t LexNumber(lexer_t *lex) {
    int start = lex->cursor;

    while (!LexerAtEOF(lex)) {
        u8 c = lex->buffer[lex->cursor];

        if (!isdigit(c))
            break;

        lex->cursor++;
    }

    slice_t lexeme = MakeSlice(lex, start, lex->cursor);
    token_t intLiteral = {
        .type = TOKEN_LITERAL,
        .litType = LITERAL_INT,
        .intLiteral = SliceToInt(&lexeme),
        .lexeme = lexeme,
    };

    return intLiteral;
}

static token_t LexIdentifierKeyword(lexer_t *lex) {
    u32 cursorStart = lex->cursor;
    
    while (!LexerAtEOF(lex)) {
        u8 c = lex->buffer[lex->cursor];

        if (!isalnum(c) && c != '_') {
            break;
        }

        lex->cursor++;
    }

    slice_t lexeme = MakeSlice(lex, cursorStart, lex->cursor);

    lookup_result_t res = LookupStr(&lexeme, KEYWORD_COUNT, keywords);
    if (res.found) {
        token_t newKeyword = {
            .type = TOKEN_KEYWORD,
            .keywordType = res.keywordType,
            .lexeme = lexeme,
        };

        return newKeyword;
    } else {
        token_t newIdentifier = {
            .type = TOKEN_IDENTIFIER,
            .lexeme = lexeme,
        };

        return newIdentifier;
    }
}

static token_t LexPunctuation(lexer_t *lex) {
    trie_node_t *node = puncTree;
    int pos = lex->cursor;
    
    int lastPos = pos;
    token_punctuation_type lastType = PUNCTUATION_COUNT;

    while (!LexerAtEOF(lex)) {
        u8 c = lex->buffer[lex->cursor];
        trie_node_t *next = node->children[c];

        if (!next) break;

        node = next;
        lex->cursor++;

        if (node->type != PUNCTUATION_COUNT) {
            lastPos = lex->cursor;
            lastType = node->type;
        }
    }
    
    if (lastType != PUNCTUATION_COUNT) {
        token_t punctToken = {0};
        punctToken.type = TOKEN_PUNCTUATION;
        punctToken.puncType = lastType;
        punctToken.lexeme = MakeSlice(lex, pos, lex->cursor);

        return punctToken;
    }

    printf("unknown punctuation: %.*s\n", lex->cursor - pos, lex->buffer);
    return (token_t){0};
}

static token_t RawNextToken(lexer_t *lex) {
    SkipWhitespaceComments(lex);

    u8 c = lex->buffer[lex->cursor];

    if (c == '"')
        return LexString(lex);

    if (c == '\'')
        return LexChar(lex);

    if (isalpha(c) || c == '_')
        return LexIdentifierKeyword(lex);

    if (isdigit(c))
        return LexNumber(lex);

    if (c == '#') {
        lex->cursor++;
        return (token_t) {
            .type = TOKEN_HASH
        };
    }

    if (c == '\n') {
        lex->cursor++;
        return (token_t) {
            .type = TOKEN_NEW_LINE
        };
    }

    if (c == '\0') {
        return (token_t) {
            .type = TOKEN_EOF,
        };
    }

    return LexPunctuation(lex);
}

static token_t PrePeek(preprocessor_t *pre, int n) {
    while (pre->lookAheadCount <= n - 1) {
        pre->lookAhead[pre->lookAheadCount++] = RawNextToken(&pre->lex);
    }

    return pre->lookAhead[n - 1];
}

static token_t PreConsume(preprocessor_t *pre) {
    if (pre->lookAheadCount == 0) {
        return RawNextToken(&pre->lex);
    }

    token_t t = pre->lookAhead[0];
    for (int i = 1; i < pre->lookAheadCount; i++) 
        pre->lookAhead[i - 1] = pre->lookAhead[i];

    pre->lookAheadCount--;

    return t;
}

static token_t PreExpect(preprocessor_t *pre, token_type type, u8 *msg) {
    token_t t = PreConsume(pre);
    if (t.type != type) {
        printf("%s\n", msg);
    }

    return t;
}

static bool PreMatch(preprocessor_t *pre, token_type type) {
    token_t t = PrePeek(pre, 1);
    if (t.type == type) {
        PreConsume(pre);
        return true;
    }

    return false;
}

void ParseDirective(preprocessor_t *pre) {
    token_t name = PreConsume(pre);

    if (name.type == TOKEN_KEYWORD && name.keywordType != KEYWORD_ELSE) {
        printf("unknown preprocessor: " SLICE_STR "\n", SLICE_ARGS(name.lexeme));
        return;
    }

    lookup_result_t res = LookupStr(&name.lexeme, PRE_COUNT, preLookup);
    if (!res.found) {
        printf("unknown preprocessor: " SLICE_STR "\n", SLICE_ARGS(name.lexeme));
        return;
    }

    if (!IsActive(pre) && !CanSurviveUnactive(res.preType)) {
        return;
    }

    switch (res.preType) {
        case PRE_DEFINE: {
            token_t defName = PreExpect(pre, TOKEN_IDENTIFIER, "expects identifier for #define");

            if (HashContains(pre->macroTable, &defName.lexeme)) {
                printf(SLICE_STR " is already defined\n", SLICE_ARGS(defName.lexeme));
                return;
            }

            macro_t m = {};
            m.replacement = DArrayCreate(token_t);
            while (!PreMatch(pre, TOKEN_NEW_LINE)) {
                token_t t = PrePeek(pre, 1);
                if (t.type == TOKEN_PUNCTUATION && t.puncType == PUNCTUATION_BACKSLASH) {
                    PreConsume(pre);
                    PreExpect(pre, TOKEN_NEW_LINE, "expects new line after extended line define");
                } else {
                    token_t t = PreConsume(pre);
                    DArrayPush(m.replacement, t);
                }
            }

            pre->atLineStart = true;
            HashInsert(pre->macroTable, &defName.lexeme, &m);
        } break;

        case PRE_IFDEF: {
            token_t defName = PreExpect(pre, TOKEN_IDENTIFIER, "expect identifier after #ifdef");
            bool defined = HashContains(pre->macroTable, &defName.lexeme);
            bool parent = IsActive(pre);

            conditional_level_t level = {
                .parentActive = parent,
                .thisBranchTaken = defined,
                .currentlyActive = parent && defined,
            };

            DArrayPush(pre->condStack, level);
            PreExpect(pre, TOKEN_NEW_LINE, "expects new line after #ifdef");
        } break;

        case PRE_IFNDEF: {
            token_t defName = PreExpect(pre, TOKEN_IDENTIFIER, "expect identifier after #ifndef");
            bool defined = !HashContains(pre->macroTable, &defName.lexeme);
            bool parent = IsActive(pre);

            conditional_level_t level = {
                .parentActive = parent,
                .thisBranchTaken = defined,
                .currentlyActive = parent && defined,
            };

            DArrayPush(pre->condStack, level);
            PreExpect(pre, TOKEN_NEW_LINE, "expects new line after #ifndef");
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
            PreExpect(pre, TOKEN_NEW_LINE, "expects new line after #else");
        } break;

        case PRE_ENDIF: {
            if (DArrayLength(pre->condStack) == 0) {
                printf("#endif with empty conditional stack\n");
                return;
            }

            DArrayPop(pre->condStack, NULL);
            PreMatch(pre, TOKEN_NEW_LINE);
        } break;

        case PRE_INCLUDE: {
            token_t t = PrePeek(pre, 1);
            if (PreMatch(pre, TOKEN_LITERAL)) {
                if (t.litType != LITERAL_STRING) {
                    printf("expects string literal for include\n");
                    return;
                }

                bool found = false;

                int searchDirLen = DArrayLength(pre->incDirs);
                for (int i = 0; i < searchDirLen; i++) {
                    u8 *dir = pre->incDirs[i];

                    u32 newStrLen = strlen(dir) + 1 + t.strLiteral.len + 1; // +1 for '/'
                    u8 *path = (u8 *)malloc(newStrLen);

                    snprintf(path, newStrLen, "%s/" SLICE_STR, dir, SLICE_ARGS(t.strLiteral));
                    loaded_file_t f = LoadFile(path);
                    if (!f.success) {
                        free(path);
                        continue;
                    }

                    file_context_t ctx = {
                        .lex = pre->lex,
                        .filename = pre->currentFile,
                    };

                    DArrayPush(pre->includeStack, ctx);

                    pre->lex = (lexer_t){
                        .buffer = f.buffer,
                        .bufferLen = f.bufferLen,
                        .cursor = 0,
                    };
                    pre->currentFile = SliceToStr(&t.strLiteral);

                    found = true;
                    break;
                }

                if (!found) {
                    printf("include file not found: " SLICE_STR "\n", SLICE_ARGS(t.lexeme));
                    return;
                }
            }
        } break;

        default: break;
    }
}

token_t *tokenize(u8 *buffer, u32 bufferLen, u8 *filename, u8 **incDirs) {
    preprocessor_t pre = {
        .output = DArrayCreate(token_t),
        .condStack = DArrayCreate(conditional_level_t),
        .includeStack = DArrayCreate(file_context_t),
        .macroTable = CreateHashTable(sizeof(macro_t)),
        .atLineStart = true,
        .currentFile = filename,
        .incDirs = incDirs,
        .lookAhead = {0},
        .lookAheadCount = 0,
        .lex = {
            .buffer = buffer,
            .bufferLen = bufferLen,
            .cursor = 0,
        },
    };

    if (!pre.incDirs) {
        pre.incDirs = DArrayCreate(u8 *);
    }

    u8 *currentDir = GetFileDir(pre.currentFile);
    DArrayPush(pre.incDirs, currentDir);

    if (!puncTree) {
        puncTree = PushStruct(globalArena, trie_node_t);
        puncTree->type = PUNCTUATION_COUNT;

        for (int i = 0; i < sizeof(punctuations) / sizeof(punctuations[0]); i++) {
            InsertNode(puncTree, punctuations[i].str, punctuations[i].puncType);
        }
    }

    while (true) {
        if (LexerAtEOF(&pre.lex)) {
            if (!PopFile(&pre)) {
                PushEOF(&pre);

                break;
            }

            continue;
        }

        token_t tok = RawNextToken(&pre.lex);

        if (tok.type == TOKEN_HASH && pre.atLineStart) {
            ParseDirective(&pre);
            continue;
        }

        if (!IsActive(&pre)) {
            continue;
        }

        ExpandOrEmit(&pre, &tok);
    }

    if (DArrayLength(pre.condStack) != 0) {
        printf("must have #endif corresponding to each #if\n");
        return NULL;
    }

    return pre.output;   
}

void PrintTokens(token_t *tokens) {
    printf("[");
    for (u32 i = 0; i < DArrayLength(tokens); i++) {
        token_t token = tokens[i];

        switch (token.type) {
            case TOKEN_IDENTIFIER: {
                printf("Identifier(" SLICE_STR ")", SLICE_ARGS(token.lexeme));
            } break;

            case TOKEN_LITERAL: {
                switch (token.litType) {
                    case LITERAL_INT: printf("IntLiteral(%d)", token.intLiteral); break;
                    case LITERAL_REAL: printf("RealLiteral(%f)", token.realLiteral); break;
                    case LITERAL_STRING: printf("StrLiteral(" SLICE_STR ")", SLICE_ARGS(token.strLiteral)); break;
                }
            } break;

            case TOKEN_PUNCTUATION: {
                printf("Punctuation(" SLICE_STR ")", SLICE_ARGS(token.lexeme));
            } break;

            case TOKEN_KEYWORD: {
                printf("Keyword(" SLICE_STR ")", SLICE_ARGS(token.lexeme));
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