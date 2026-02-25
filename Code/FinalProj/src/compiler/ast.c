#include "ast.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "darray.h"
#include "parser.h"

ast_node_t *GetNewNode(ast_node_type type) {
    ast_node_t *node = (ast_node_t *)malloc(sizeof(ast_node_t));
    node->type = type;

    return node;
}

// Expression Parsing
static ast_node_t *ParseExpression(parser_t *p);
static ast_node_t *ParseAssignmentExpression(parser_t *p);

typedef ast_node_t *(*parse_fn_t)(parser_t *);
typedef bool (*punct_to_op_fn_t)(token_punctuation_type, ast_binary_op *);

static ast_node_t *ParseBinaryLevel(parser_t *p, parse_fn_t parseSubExpr, punct_to_op_fn_t mapOp) {
    ast_node_t *left = parseSubExpr(p);

    while (true) {
        token_t *t = Peek(p);

        if (t->type != TOKEN_PUNCTUATION)
            break;

        ast_binary_op op;
        if (!mapOp(t->puncType, &op))
            break;

        Advance(p);

        ast_node_t *new = GetNewNode(AST_BINARY_EXPR);
        new->binary_op.op = op;
        new->binary_op.left = left;
        new->binary_op.right = parseSubExpr(p);

        left = new;
    }

    return left;
}

ast_node_t *ParsePrimaryExpression(parser_t *p) {
    token_t *t = Peek(p);
    
    switch (t->type) {
        case TOKEN_IDENTIFIER: {
            ast_node_t *new = GetNewNode(AST_IDENTIFIER);
            new->identifier.name = t->lexeme;

            Advance(p);

            return new;
        }

        case TOKEN_LITERAL: {
            token_literal_type litType = t->litType;
            ast_node_t *new = GetNewNode(litType == LITERAL_INT ? AST_LITERAL_INT : AST_LITERAL_STRING);
            
            if (litType == LITERAL_INT) new->int_literal.literal = t->intLiteral;
            if (litType == LITERAL_STRING) new->string_literal.literal = t->strLiteral;

            Advance(p);

            return new;
        }

        case TOKEN_PUNCTUATION: {
            ExpectPunctuation(p, PUNCTUATION_OPEN_PAREN, "expect open parenthese around expression");
            ast_node_t *expr = ParseExpression(p);
            ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expect close parenthese around expression");

            return expr;
        }

        default: return NULL;
    }
}

ast_node_t *ParsePostfixExpression(parser_t *p) {
    ast_node_t *primaryExpr = ParsePrimaryExpression(p);

    while (true) {
        token_t *t = Peek(p);

        if (t->type != TOKEN_PUNCTUATION)
            break;

        switch (t->puncType) {
            case PUNCTUATION_OPEN_BRACK: {
                Advance(p);

                ast_node_t *inExpr = ParseExpression(p);
                ExpectPunctuation(p, PUNCTUATION_CLOSE_BRACK, "expects close brackets after open brackets");

                ast_node_t *new = GetNewNode(AST_INDEX);
                new->index.array = primaryExpr;
                new->index.index = inExpr;

                primaryExpr = new;
            } break;

            case PUNCTUATION_OPEN_PAREN: {
                Advance(p);

                ast_node_t *new = GetNewNode(AST_FUNC_CALL);
                new->func_call.fun = primaryExpr;
                new->func_call.params = DArrayCreate(ast_node_t *);
                while (!MatchPunctuation(p, PUNCTUATION_CLOSE_PAREN)) {
                    ast_node_t *param = ParseAssignmentExpression(p);
                    DArrayPush(new->func_call.params, param);
                }

                primaryExpr = new;
            } break;

            case PUNCTUATION_PERIOD: {
                Advance(p);

                token_t *member = Expect(p, TOKEN_IDENTIFIER, "expects an identifier as member");

                ast_node_t *new = GetNewNode(AST_MEMBER);
                new->member.parent = primaryExpr;
                new->member.member = member->lexeme;

                primaryExpr = new;
            } break;

            case PUNCTUATION_INCREMENT:
            case PUNCTUATION_DECREMENT: {
                Advance(p);

                ast_node_t *new = GetNewNode(AST_UNARY_EXPR);
                new->unary_op.op = t->puncType == PUNCTUATION_INCREMENT ? UNARY_OP_INCREMENT : UNARY_OP_DECREMENT;
                new->unary_op.expr = primaryExpr;

                primaryExpr = new;
            } break;

            default: return primaryExpr;
        }
    }

    return primaryExpr;
}

ast_node_t *ParseUnaryExpression(parser_t *p) {
    token_t *t = Peek(p);
    if (t->type == TOKEN_PUNCTUATION) {
        ast_unary_op op;
        if (PunctuationToUnary(t->puncType, &op)) {
            Advance(p);
            
            ast_node_t *new = GetNewNode(AST_UNARY_EXPR);
            new->unary_op.op = op;
            new->unary_op.expr = ParseUnaryExpression(p);

            return new;
        }
    }

    ast_node_t *postfixExpr = ParsePostfixExpression(p);

    return postfixExpr;
}

ast_node_t *ParseMultExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseUnaryExpression, PunctuationToMult);
}

ast_node_t *ParseAddExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseMultExpression, PunctuationToAdd);
}

ast_node_t *ParseShiftExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseAddExpression, PunctuationToShift);
}

ast_node_t *ParseRelationalExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseShiftExpression, PunctuationToRelation);
}

ast_node_t *ParseEqualityExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseRelationalExpression, PunctuationToEquality);
}

ast_node_t *ParseAndExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseEqualityExpression, PunctuationToAnd);
}

ast_node_t *ParseExclusiveOrExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseAndExpression, PunctuationToXOr);
}

ast_node_t *ParseInclusiveOrExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseExclusiveOrExpression, PunctuationToOr);
}

ast_node_t *ParseLogicAndExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseInclusiveOrExpression, PunctuationToLogicAnd);
}

ast_node_t *ParseLogicOrExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseLogicAndExpression, PunctuationToLogicOr);
}

ast_node_t *ParseConditionalExpression(parser_t *p) {
    ast_node_t *orExpr = ParseLogicOrExpression(p);

    if (MatchPunctuation(p, PUNCTUATION_QUESTION_MARK)) {
        ast_node_t *expr = ParseExpression(p);

        ExpectPunctuation(p, PUNCTUATION_COLON, "expected a colon in ternary expression");
        ast_node_t *condExpr = ParseConditionalExpression(p);

        ast_node_t *ternExpr = GetNewNode(AST_TERNARY_EXPR);
        ternExpr->ternary_expr.condition = orExpr;
        ternExpr->ternary_expr.then_expr = expr;
        ternExpr->ternary_expr.else_expr = condExpr;

        return ternExpr;
    }

    return orExpr;
}

ast_node_t *ParseConstantExpression(parser_t *p) {
    return ParseConditionalExpression(p);
}

bool HasAssignmentOperator(parser_t *p, ast_assignment_op *op) {
    token_t *t = Peek(p);

    if (t->type != TOKEN_PUNCTUATION) {
        return false;
    }

    switch (t->puncType) {
        case PUNCTUATION_EQUALS: *op = ASSIGN; break;
        case PUNCTUATION_PLUS_EQUALS: *op = ASSIGN_ADD; break;
        case PUNCTUATION_MINUS_EQUALS: *op = ASSIGN_SUB; break;
        case PUNCTUATION_MULT_EQUALS: *op = ASSIGN_MUL; break;
        case PUNCTUATION_DIV_EQUALS: *op = ASSIGN_DIV; break;

        default: return false;
    }

    Advance(p);

    return true;
}

ast_node_t *ParseAssignmentExpression(parser_t *p) {
    ast_node_t *condExpr = ParseConditionalExpression(p);

    token_t *t = Peek(p);
    if (t->type == TOKEN_PUNCTUATION) {
        ast_assignment_op op;
        if (PunctuationToAssignment(t->puncType, &op)) {
            Advance(p);

            ast_node_t *newExpr = GetNewNode(AST_ASSIGN_EXPR);
            newExpr->assign_op.op = op;
            newExpr->assign_op.left = condExpr;
            newExpr->assign_op.right = ParseAssignmentExpression(p);
        }
    }

    return condExpr;
}

ast_node_t *ParseExpression(parser_t *p) {
    ast_node_t *assnExpr = ParseAssignmentExpression(p);

    return assnExpr;
}

ast_node_t *ParseVariableDeclaration(parser_t *p) {
    token_t *name = Expect(p, TOKEN_IDENTIFIER, "expected an identifier for variable name");

    ast_node_t *init = NULL;

    if (MatchPunctuation(p, PUNCTUATION_EQUALS)) {
        init = ParseConstantExpression(p);
    }

    ast_node_t *node = GetNewNode(AST_DECL);
    // node->decl.name = name->lexeme;
    // node->decl.value = init;

    return node;
}

ast_node_t *ParseFunction(parser_t *p) {
    return NULL;
}

static bool MergeBuiltinType(builtin_type_t *dstType, builtin_type_t *srcType) {
    if (srcType->base != BUILTIN_NONE) {
        if (dstType->base != BUILTIN_NONE) {
            printf("type specifier has too many builtin types\n");
            return false;
        }

        dstType->base = srcType->base;
    }

    if (srcType->width != WIDTH_DEFAULT) {
        if (dstType->width != WIDTH_DEFAULT) {
            if (srcType->width == WIDTH_LONG && dstType->width == WIDTH_LONG) {
                dstType->width = WIDTH_LONGLONG;
            } else {
                printf("type specifier has too many width specifiers\n");
                return false;
            }
        } else {
            dstType->width = srcType->width;
        }
    }

    if (srcType->sign != SIGN_NONE) {
        if (dstType->sign != SIGN_NONE) {
            printf("type specifier has too many sign specifiers\n");
            return false;
        }

        dstType->sign = srcType->sign;
    }

    return true;
}

void ParseTypeSpecifier(parser_t *p, decl_specifiers_t *spec) {
    token_t *t = Peek(p);

    if (!spec->typeSpecifier) {
        spec->typeSpecifier = (type_specifier_t *)malloc(sizeof(type_specifier_t));
        memset(spec->typeSpecifier, 0, sizeof(type_specifier_t));
    }

    builtin_type_t type = {0};
    if (KeywordToBuiltinType(t->keywordType, &type)) {
        spec->typeSpecifier->kind = SPECIFIER_BUILTIN;

        if (!MergeBuiltinType(&spec->typeSpecifier->builtin.type, &type)) {
            return;
        }
    }

    // TODO Implement struct/union and enum type specifiers
}

decl_specifiers_t *ParseDeclarationSpecifiers(parser_t *p) {
    decl_specifiers_t *spec = (decl_specifiers_t *)malloc(sizeof(decl_specifiers_t));
    memset(spec, 0, sizeof(decl_specifiers_t));

    storage_class_specifier storageSpec;
    type_qualifier qualifier;    
    function_specifier funcSpec;

    bool advancedOnce = false;
    
    while (true) {
        token_t *t = Peek(p);
        if (t->type == TOKEN_KEYWORD) {
            if (KeywordToStorageClass(t->keywordType, &storageSpec)) {
                if (spec->storageClass != STORAGE_SPEC_NONE) {
                    printf("expects only one storage class specifier\n");
                    return NULL;
                }

                spec->storageClass = storageSpec;
            } else if (KeywordToTypeQualifier(t->keywordType, &qualifier)) {
                spec->typeQualifier |= qualifier;
            } else if (KeywordToFunctionSpecifier(t->keywordType, &funcSpec)) {
                spec->functionSpecifier |= funcSpec;
            } else {
                ParseTypeSpecifier(p, spec);
            }
        } else if (t->type == TOKEN_IDENTIFIER) {
            if (HashContains(p->scope->symbols, t->lexeme)) {
                if (spec->typeSpecifier) {
                    printf("declaration specifiers already has type specifier\n");
                    return NULL;
                }

                spec->typeSpecifier = (type_specifier_t *)malloc(sizeof(type_specifier_t));
                spec->typeSpecifier->kind = SPECIFIER_TYPEDEF_NAME;
                spec->typeSpecifier->typedef_type.name = t->lexeme;
            } else if (!advancedOnce) {
                return NULL;
            } else {
                return spec;
            }
        } else {
            return spec;
        }

        Advance(p);
        advancedOnce = true;
    }
}

static ast_declarator_t *ParseDeclarator(parser_t *p, bool isAbstract);

ast_parameter_t *ParseParameterDeclaration(parser_t *p) {
    decl_specifiers_t *specifiers = ParseDeclarationSpecifiers(p);

    ast_parameter_t *param = (ast_parameter_t *)malloc(sizeof(ast_parameter_t));
    param->specifiers = *specifiers;

    token_t *t = Peek(p);
    if (t->type == TOKEN_PUNCTUATION && t->puncType == PUNCTUATION_COMMA) {
        return param;
    }

    param->declarator = ParseDeclarator(p, true);

    return param;
}

ast_parameter_t **ParseParameterList(parser_t *p) {
    ast_parameter_t **params = DArrayCreate(ast_parameter_t *);
    
    do {
        ast_parameter_t *param = ParseParameterDeclaration(p);
        DArrayPush(params, param);
    } while (MatchPunctuation(p, PUNCTUATION_COMMA));

    return params;
}

ast_declarator_t *ParseDirectDeclarator(parser_t *p, bool isAbstract) {
    ast_declarator_t *inner = NULL;

    if (MatchPunctuation(p, PUNCTUATION_OPEN_PAREN)) {
        inner = ParseDeclarator(p, isAbstract);
        ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expects close parentheses in direct declarator");
    } else if (Match(p, TOKEN_IDENTIFIER)) {
        inner = (ast_declarator_t *)malloc(sizeof(ast_declarator_t));
        inner->kind = DECL_IDENTIFIER;
        inner->identifier.name = Previous(p)->lexeme;
    } else if (!isAbstract) {
        printf("expect open parenthese or identifier in direct declarator\n");
        return NULL;
    }

    while (true) {
        if (MatchPunctuation(p, PUNCTUATION_OPEN_BRACK)) {
            ast_declarator_t *decl = (ast_declarator_t *)malloc(sizeof(ast_declarator_t));
            decl->kind = DECL_ARRAY;
            decl->array.inner = inner;

            if (!MatchPunctuation(p, PUNCTUATION_CLOSE_BRACK)) {
                decl->array.size = ParseExpression(p);
                ExpectPunctuation(p, PUNCTUATION_CLOSE_BRACK, "expects close bracket in array declarator");
            } else {
                decl->array.size = NULL;
            }

            inner = decl;
        } else if (MatchPunctuation(p, PUNCTUATION_OPEN_PAREN)) {
            ast_declarator_t *decl = (ast_declarator_t *)malloc(sizeof(ast_declarator_t));
            decl->kind = DECL_FUNCTION;
            decl->function.inner = inner;

            decl->function.parameters = ParseParameterList(p);

            ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expects close parenthese after function declarator");

            inner = decl;
        } else {
            break;
        }
    }

    return inner;
}

ast_declarator_t *ParseDeclarator(parser_t *p, bool isAbstract) {
    ast_declarator_t *pointerChain = NULL;

    while (MatchPunctuation(p, PUNCTUATION_STAR)) {
        ast_declarator_t *ptr = (ast_declarator_t *)malloc(sizeof(ast_declarator_t));
        ptr->kind = DECL_POINTER;
        ptr->pointer.inner = pointerChain;
        ptr->pointer.qualifiers = 0;
        
        while (true) {
            token_t *t = Peek(p);
            if (t->type == TOKEN_KEYWORD) {
                type_qualifier qual;
                if (KeywordToTypeQualifier(t->keywordType, &qual)) {
                    Advance(p);
                    ptr->pointer.qualifiers |= qual;
                } else {
                    printf("expects type qualifiers inside pointer declarator\n");
                    return NULL;
                }
            } else {
                break;
            }
        }

        pointerChain = ptr;
    }

    ast_declarator_t *direct = ParseDirectDeclarator(p, isAbstract);

    if (!pointerChain) {
        return direct;
    }

    if (direct == NULL && isAbstract) {
        return pointerChain;
    }

    ast_declarator_t *bottom = pointerChain;
    while (bottom->pointer.inner) {
        bottom = bottom->pointer.inner;
    }
    bottom->pointer.inner = direct;
    
    return pointerChain;
}

ast_designator_t *ParseDesignator(parser_t *p) {
    ast_designator_t *des = NULL;

    if (MatchPunctuation(p, PUNCTUATION_OPEN_BRACK)) {
        des = (ast_designator_t *)malloc(sizeof(ast_designator_t));
        des->kind = DESIGNATOR_INDEX;
        des->index = ParseConstantExpression(p);

        ExpectPunctuation(p, PUNCTUATION_CLOSE_BRACK, "expects close bracket in index designator");
    } else if (MatchPunctuation(p, PUNCTUATION_PERIOD)) {
        des = (ast_designator_t *)malloc(sizeof(ast_designator_t));
        des->kind = DESIGNATOR_FIELD;

        token_t *field = Expect(p, TOKEN_IDENTIFIER, "expects identifier in field designator");
        
        des->field = field->lexeme;
    }

    return des;
}

ast_initializer_t *ParseInitializer(parser_t *p) {
    ast_initializer_t *init = (ast_initializer_t *)malloc(sizeof(ast_initializer_t));
    
    if (MatchPunctuation(p, PUNCTUATION_OPEN_CURLY)) {
        init->kind = INITIALIZER_LIST;
        init->list = DArrayCreate(ast_initializer_list_t *);
        
        do {
            ast_initializer_list_t *item = (ast_initializer_list_t *)malloc(sizeof(ast_initializer_list_t));
            memset(item, 0, sizeof(ast_initializer_list_t));

            while (true) {
                ast_designator_t *designator = ParseDesignator(p);
                if (!designator) {
                    break;
                }

                if (!item->designation) {
                    item->designation = DArrayCreate(ast_designator_t *);
                }

                DArrayPush(item->designation, designator);
            }

            if (item->designation) {
                ExpectPunctuation(p, PUNCTUATION_EQUALS, "expects equals with designation");
            }

            item->initializer = ParseInitializer(p);
            DArrayPush(init->list, item);

            if (!MatchPunctuation(p, PUNCTUATION_COMMA))
                break;

            token_t *t = Peek(p);
            if (t->type == TOKEN_PUNCTUATION && t->puncType == PUNCTUATION_CLOSE_CURLY)
                break;
        } while (1);

        ExpectPunctuation(p, PUNCTUATION_CLOSE_CURLY, "expects closed curly brackets after initializer");
    } else {
        init->kind = INITIALIZER_EXPR;
        init->expr = ParseExpression(p);
    }
    
    return init;
}

ast_init_declarator_t *ParseInitDeclarator(parser_t *p) {
    ast_init_declarator_t *node = (ast_init_declarator_t *)malloc(sizeof(ast_init_declarator_t));

    node->declarator = ParseDeclarator(p, false);
    node->initializer = NULL;

    if (MatchPunctuation(p, PUNCTUATION_EQUALS)) {
        node->initializer = ParseInitializer(p);
    }

    return node;
}

static bool IsDeclaratorStart(parser_t *p) {
    token_t *t = Peek(p);

    if (t->type == TOKEN_PUNCTUATION) {
        switch (t->puncType) {
            case PUNCTUATION_STAR: return true;
            case PUNCTUATION_OPEN_PAREN: return true;

            default: return false;
        }
    } else if (t->type == TOKEN_IDENTIFIER) {
        return true;
    }

    return false;
}

ast_node_t *ParseDeclaration(parser_t *p) {
    ast_node_t *new = GetNewNode(AST_DECL);

    decl_specifiers_t *declSpecifiers = ParseDeclarationSpecifiers(p);
    if (declSpecifiers == NULL) {
        printf("expect known type, given %s\n", Peek(p)->lexeme);
        return NULL;
    }

    new->decl.specifiers = *declSpecifiers;

    new->decl.initDeclList = DArrayCreate(ast_declarator_t *);

    if (IsDeclaratorStart(p)) {
        do {
            ast_init_declarator_t *declarator = ParseInitDeclarator(p);
            DArrayPush(new->decl.initDeclList, declarator);
        } while (MatchPunctuation(p, PUNCTUATION_COMMA));
    }

    ExpectPunctuation(p, PUNCTUATION_SEMICOLON, "expects semicolon after declaration");

    return new;
}

ast_node_t *AstFromTokens(token_t *tokens) {
    ast_node_t *program = (ast_node_t *)malloc(sizeof(ast_node_t));
    program->type = AST_PROGRAM;
    program->program.decls = DArrayCreate(ast_node_t *);

    parser_t p = {0};
    p.tokens = tokens;
    p.count = DArrayLength(tokens);
    p.pos = 0;
    p.scope = NULL;
    
    PushScope(&p);
    
    while (!Match(&p, TOKEN_EOF)) {
        ast_node_t *decl = ParseDeclaration(&p);
        if (!decl) {
            return NULL;
        }

        DArrayPush(program->program.decls, decl);
    }

    return program;
}

void PrintDeclSpecifiers(decl_specifiers_t *specifiers) {
    bool needSpace = false;

    if (specifiers->storageClass != STORAGE_SPEC_NONE) {
        printf("%s", StorageClassToStr(specifiers->storageClass));
        needSpace = true;
    }

    for (int i = 0; i < TYPE_QUALIFIER_COUNT; i++) {
        if (specifiers->typeQualifier & (1 << i)) {
            if (needSpace) {
                printf(" ");
            }
            printf("%s", TypeQualifierToStr((type_qualifier)(1 << i)));
            needSpace = true;
        }
    }

    for (int i = 0; i < FUNCTION_SPECIFIER_COUNT; i++) {
        if (specifiers->functionSpecifier & (1 << i)) {
            if (needSpace) {
                printf(" ");
            }
            printf("%s", FunctionSpecifierToStr((function_specifier)(1 << i)));
            needSpace = true;
        }
    }

    type_specifier_t *spec = specifiers->typeSpecifier;
    switch (spec->kind) {
        case SPECIFIER_BUILTIN: {
            builtin_type_t bt = spec->builtin.type;
            
            if (bt.sign != SIGN_NONE) {
                if (needSpace) {
                    printf(" ");
                }
                printf("%s", SignToStr(bt.sign));
                needSpace = true;
            }

            if (bt.width != WIDTH_DEFAULT) {
                if (needSpace) {
                    printf(" ");
                }
                printf("%s", WidthToStr(bt.sign));
                needSpace = true;
            }
            
            if (needSpace) {
                printf(" ");
            }
            if (bt.base != BUILTIN_NONE) {
                printf("%s", BaseToStr(bt.base));
                needSpace = true;
            } else
                printf("int");
        } break;

        case SPECIFIER_STRUCT_UNION: {
            printf("%s ", spec->struct_union.isUnion ? "union" : "struct");
            if (spec->struct_union.name) {
                printf("%s", spec->struct_union.name);
            }

            // TODO Implement struct/union printing
        } break;

        case SPECIFIER_ENUM: {
            // TODO Implement enum printing
        } break;

        case SPECIFIER_TYPEDEF_NAME: {
            printf("%s", spec->typedef_type.name);
        } break;

        default: break;
    }
}

void PrintAstDeclarator(ast_declarator_t *decl) {
    switch (decl->kind) {
        case DECL_IDENTIFIER: {
            printf("%s", decl->identifier.name);
        } break;

        case DECL_POINTER: {
            printf("*");
            for (int i = 0; i < TYPE_QUALIFIER_COUNT; i++) {
                if (decl->pointer.qualifiers & (1 << i)) {
                    printf(" %s", TypeQualifierToStr((type_qualifier)(1 << i)));
                }
            }
            PrintAstDeclarator(decl->pointer.inner);
        } break;

        case DECL_ARRAY: {
            PrintAstDeclarator(decl->array.inner);
            printf("[");
            PrintAst(decl->array.size, 0);
            printf("]");
        } break;

        case DECL_FUNCTION: {
            PrintAstDeclarator(decl->function.inner);
            printf("(");
            int paramCount = DArrayLength(decl->function.parameters);
            for (int i = 0; i < paramCount; i++) {
                ast_parameter_t *param = decl->function.parameters[i];
                PrintDeclSpecifiers(&param->specifiers);
                printf(" ");
                PrintAstDeclarator(param->declarator);
                
                if (i < paramCount - 1) {
                    printf(", ");
                }
            }
            printf(")");
        } break;
        
        default: break;
    }
}

void PrintAstInitializer(ast_initializer_t *initializer) {
    switch (initializer->kind) {
        case INITIALIZER_EXPR: {
            PrintAst(initializer->expr, 0);
        } break;

        case INITIALIZER_LIST: {
            int listLen = DArrayLength(initializer->list);
            for (int i = 0; i < listLen; i++) {
                ast_initializer_list_t *init = initializer->list[i];
                
                if (init->designation) {
                    int designatorLen = DArrayLength(init->designation);
                    for (int j = 0; j < designatorLen; j++) {
                        switch (init->designation[j]->kind) {
                            case DESIGNATOR_FIELD: {
                                printf(".%s", init->designation[j]->field);
                            } break;

                            case DESIGNATOR_INDEX: {
                                printf("[");
                                PrintAst(init->designation[j]->index, 0);
                                printf("]");
                            } break;
                        }
                    }

                    printf(" = ");
                }

                PrintAstInitializer(init->initializer);
            }
        } break;
    }
}

void PrintAst(ast_node_t *parent, int depth) {
    u8 *tabs = NULL;
    if (depth > 0) {
        tabs = (u8 *)malloc(depth);
        for (int i = 0; i < depth; i++)
            tabs[i] = '\t';
    }

    switch (parent->type) {
        case AST_PROGRAM: {
            printf("[\n");
            for (int i = 0; i < DArrayLength(parent->program.decls); i++) {
                printf("\t");
                PrintAst(parent->program.decls[i], depth + 1);

                if (i < DArrayLength(parent->program.decls) - 1) {
                    printf(", ");
                }
                printf("\n");
            }
            printf("]\n");
        } break;

        case AST_BLOCK: {

        } break;

        case AST_DECL: {
            printf("Declaration(");
            PrintDeclSpecifiers(&parent->decl.specifiers);
            printf("\n%s\t", tabs);

            int declCount = DArrayLength(parent->decl.initDeclList);
            for (int i = 0; i < declCount; i++) {
                ast_init_declarator_t *decl = parent->decl.initDeclList[i];
                PrintAstDeclarator(decl->declarator);

                if (decl->initializer) {
                    printf(" = { ");
                    PrintAstInitializer(decl->initializer);
                    printf(" }");
                }

                if (i < declCount - 1) {
                    printf(",\n%s\t", tabs);
                }
            }
            printf(")");
        } break;

        case AST_FUNC_CALL: {
            printf("FunCall(");
            PrintAst(parent->func_call.fun, depth + 1);
            printf("\tparams = [");

            i32 paramCount = DArrayLength(parent->func_call.params);
            for (int i = 0; i < paramCount; i++) {
                PrintAst(parent->func_call.params[i], depth + 1);
                if (i < paramCount - 1) {
                    printf(", ");
                }
            }
            printf("])");
        } break;

        case AST_RETURN: {

        } break;

        case AST_IF: {

        } break;

        case AST_WHILE: {

        } break;

        case AST_TERNARY_EXPR: {
            printf("TernExpr(");
            PrintAst(parent->ternary_expr.condition, depth + 1);
            printf(" ? ");
            PrintAst(parent->ternary_expr.then_expr, depth + 1);
            printf(" : ");
            PrintAst(parent->ternary_expr.else_expr, depth + 1);
            printf(")");
        } break;

        case AST_BINARY_EXPR: {
            printf("BinOp(");
            PrintAst(parent->binary_op.left, depth + 1);
            printf(" %s ", BinaryToStr(parent->binary_op.op));
            PrintAst(parent->binary_op.right, depth + 1);
            printf(")");
        } break;

        case AST_UNARY_EXPR: {
            printf("UnOp(%s", UnaryToStr(parent->unary_op.op));
            PrintAst(parent->unary_op.expr, depth + 1);
            printf(")");
        } break;

        case AST_ASSIGN_EXPR: {

        } break;

        case AST_INDEX: {
            printf("Index(");
            PrintAst(parent->index.array, depth + 1);
            printf("[");
            PrintAst(parent->index.index, depth + 1);
            printf("])");
        } break;

        case AST_MEMBER: {
            printf("Member(");
            PrintAst(parent->member.parent, depth + 1);
            printf(".%s)", parent->member.member);
        } break;

        case AST_LITERAL_INT: {
            printf("%d", parent->int_literal.literal);
        } break;

        case AST_LITERAL_STRING: {

        } break;

        case AST_IDENTIFIER: {
            printf("Ident(%s)", parent->identifier.name);
        } break;

        default: break;
    }
}