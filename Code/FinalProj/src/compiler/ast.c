#include "ast.h"
#include "semantics.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "darray.h"
#include "parser.h"
#include "arena.h"

ast_node_t *GetNewNode(ast_node_type type) {
    ast_node_t *node = PushStruct(globalArena, ast_node_t);
    node->type = type;

    return node;
}

// Expression Parsing
static ast_node_t *ParseExpression(parser_t *p);
static ast_node_t *ParseAssignmentExpression(parser_t *p);
static ast_node_t *ParseCastExpression(parser_t *p);
static ast_declarator_t *ParseDeclarator(parser_t *p, bool isAbstract);
static void ParseTypeSpecifier(parser_t *p, decl_specifiers_t *spec);
static bool IsTypeSpecifier(parser_t *p);
static void ParseTypeSpecifier(parser_t *p, decl_specifiers_t *spec);
static bool IsDeclaratorStart(parser_t *p);

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
            ast_node_t *new = GetNewNode((litType == LITERAL_INT || litType == LITERAL_CHAR) ? AST_LITERAL_INT : AST_LITERAL_STRING);
            
            if (litType == LITERAL_INT || litType == LITERAL_CHAR) new->int_literal.literal = t->intLiteral;
            if (litType == LITERAL_STRING) new->string_literal.str = t->strLiteral;

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
                if (!MatchPunctuation(p, PUNCTUATION_CLOSE_PAREN)) {
                    while (true) {
                        ast_node_t *param = ParseAssignmentExpression(p);
                        DArrayPush(new->func_call.params, param);
                        
                        if (MatchPunctuation(p, PUNCTUATION_CLOSE_PAREN))
                            break;

                        ExpectPunctuation(p, PUNCTUATION_COMMA, "expects comma in function parameter list");

                    }
                }


                primaryExpr = new;
            } break;

            case PUNCTUATION_RIGHT_ARROW:
            case PUNCTUATION_PERIOD: {
                Advance(p);

                token_t *member = Expect(p, TOKEN_IDENTIFIER, "expects an identifier as member");

                ast_node_t *new = GetNewNode(AST_MEMBER);
                new->member.parent = primaryExpr;
                new->member.member = member->lexeme;
                new->member.isPointer = t->puncType == PUNCTUATION_RIGHT_ARROW;

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
            if (op == UNARY_OP_INCREMENT || op == UNARY_OP_DECREMENT)
                new->unary_op.expr = ParseUnaryExpression(p);
            else
                new->unary_op.expr = ParseCastExpression(p);

            return new;
        }
    }

    ast_node_t *postfixExpr = ParsePostfixExpression(p);

    return postfixExpr;
}

ast_node_t *ParseCastExpression(parser_t *p) {
    if (MatchPunctuation(p, PUNCTUATION_OPEN_PAREN)) {
        token_t *t = Peek(p);
        type_qualifier qual;
        
        if ((t->type == TOKEN_KEYWORD && KeywordToTypeQualifier(t->puncType, &qual)) || IsTypeSpecifier(p)) {
            ast_node_t *new = GetNewNode(AST_CAST_EXPR);
            
            do {
                t = Peek(p);
                if (KeywordToTypeQualifier(t->keywordType, &qual)) {
                    new->cast_expr.qualifiers |= qual;
                } else {
                    decl_specifiers_t spec = {0};
                    ParseTypeSpecifier(p, &spec);

                    if (new->cast_expr.type) {
                        printf("cast already has type specifier\n");
                        return NULL;
                    }

                    new->cast_expr.type = spec.typeSpecifier;
                }
                Advance(p);
            } while (!MatchPunctuation(p, PUNCTUATION_CLOSE_PAREN) && !IsDeclaratorStart(p));

            if (IsDeclaratorStart(p)) {
                new->cast_expr.declarator = ParseDeclarator(p, true);
                ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expects closed parenthese after cast declarator");
            }

            new->cast_expr.expr = ParseCastExpression(p);

            return new;
        } else {
            Reverse(p);
        }
    }

    return ParseUnaryExpression(p);
}

ast_node_t *ParseMultExpression(parser_t *p) {
    return ParseBinaryLevel(p, ParseCastExpression, PunctuationToMult);
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

            return newExpr;
        }
    }

    return condExpr;
}

ast_node_t *ParseExpression(parser_t *p) {
    ast_node_t *assnExpr = ParseAssignmentExpression(p);

    return assnExpr;
}

// Declaration Parsing
static ast_node_t *ParseDeclaration(parser_t *p);
static ast_declarator_t *ParseDeclarator(parser_t *p, bool isAbstract);

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
        spec->typeSpecifier = PushStruct(globalArena, type_specifier_t);
        memset(spec->typeSpecifier, 0, sizeof(type_specifier_t));
    }

    builtin_type_t type = {0};

    if (KeywordToBuiltinType(t->keywordType, &type)) {
        spec->typeSpecifier->kind = SPECIFIER_BUILTIN;

        if (!MergeBuiltinType(&spec->typeSpecifier->builtin.type, &type)) {
            return;
        }
    } else if (MatchKeyword(p, KEYWORD_STRUCT) || MatchKeyword(p, KEYWORD_UNION)) {
        spec->typeSpecifier->kind = SPECIFIER_STRUCT_UNION;
        spec->typeSpecifier->struct_union.isUnion = Previous(p)->keywordType == KEYWORD_UNION;
        
        if (Match(p, TOKEN_IDENTIFIER)) {
            spec->typeSpecifier->struct_union.name = Previous(p)->lexeme;
        }

        if (MatchPunctuation(p, PUNCTUATION_OPEN_CURLY)) {
            spec->typeSpecifier->struct_union.fields = DArrayCreate(ast_decl_t);
            do {
                ast_node_t *decl = ParseDeclaration(p);
                DArrayPush(spec->typeSpecifier->struct_union.fields, decl->decl);

                t = Peek(p);
                if (t->type == TOKEN_PUNCTUATION && t->puncType == PUNCTUATION_CLOSE_CURLY) {
                    break;
                }
            } while (1);
        } else if (!spec->typeSpecifier->struct_union.name.str) {
            printf("expects identifier if no struct declaration");
            return;
        }
    } else if (MatchKeyword(p, KEYWORD_ENUM)) {
        spec->typeSpecifier->kind = SPECIFIER_ENUM;

        if (Match(p, TOKEN_IDENTIFIER)) {
            spec->typeSpecifier->enum_type.name = Previous(p)->lexeme;
        }

        if (MatchPunctuation(p, PUNCTUATION_OPEN_CURLY)) {
            spec->typeSpecifier->enum_type.enumerators = DArrayCreate(ast_enum_item_t);
            do {
                ast_enum_item_t item = {0};

                token_t *name = Expect(p, TOKEN_IDENTIFIER, "expects identifier for enum element name");
                item.name = name->lexeme;

                if (MatchPunctuation(p, PUNCTUATION_EQUALS)) {
                    item.value = ParseConstantExpression(p);
                }

                DArrayPush(spec->typeSpecifier->enum_type.enumerators, item);

                if (!MatchPunctuation(p, PUNCTUATION_COMMA))
                    break;

                t = Peek(p);
                if (t->type == TOKEN_PUNCTUATION && t->puncType == PUNCTUATION_CLOSE_CURLY)
                    break;

            } while (1);
        } else if (!spec->typeSpecifier->enum_type.name.str) {
            printf("expects identifier if no enum declaration");
            return;
        }
    }
}

decl_specifiers_t *ParseDeclarationSpecifiers(parser_t *p) {
    decl_specifiers_t *spec = PushStruct(globalArena, decl_specifiers_t);
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
            if (HashContains(p->scope->typedefs, &t->lexeme)) {
                if (spec->typeSpecifier) {
                    printf("declaration specifiers already has type specifier\n");
                    return NULL;
                }

                spec->typeSpecifier = PushStruct(globalArena, type_specifier_t);
                spec->typeSpecifier->kind = SPECIFIER_TYPEDEF_NAME;
                spec->typeSpecifier->typedef_type.name = t->lexeme;
            } else if (!advancedOnce) {
                printf("unknown type on line %d: " SLICE_STR "\n", t->line + 1, SLICE_ARGS(t->lexeme));
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

ast_parameter_t *ParseParameterDeclaration(parser_t *p) {
    decl_specifiers_t *specifiers = ParseDeclarationSpecifiers(p);

    ast_parameter_t *param = PushStruct(globalArena, ast_parameter_t);
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
    
    token_t *t = Peek(p);
    if (t->type == TOKEN_PUNCTUATION && t->puncType == PUNCTUATION_CLOSE_PAREN) {
        return NULL;
    }

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
        inner = PushStruct(globalArena, ast_declarator_t);
        inner->kind = DECL_IDENTIFIER;
        inner->identifier.name = Previous(p)->lexeme;
    } else if (!isAbstract) {
        printf("expect open parenthese or identifier in direct declarator\n");
        return NULL;
    }

    while (true) {
        if (MatchPunctuation(p, PUNCTUATION_OPEN_BRACK)) {
            ast_declarator_t *decl = PushStruct(globalArena, ast_declarator_t);
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
            ast_declarator_t *decl = PushStruct(globalArena, ast_declarator_t);
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
        ast_declarator_t *ptr = PushStruct(globalArena, ast_declarator_t);
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
        des = PushStruct(globalArena, ast_designator_t);
        des->kind = DESIGNATOR_INDEX;
        des->index = ParseConstantExpression(p);

        ExpectPunctuation(p, PUNCTUATION_CLOSE_BRACK, "expects close bracket in index designator");
    } else if (MatchPunctuation(p, PUNCTUATION_PERIOD)) {
        des = PushStruct(globalArena, ast_designator_t);
        des->kind = DESIGNATOR_FIELD;

        token_t *field = Expect(p, TOKEN_IDENTIFIER, "expects identifier in field designator");
        
        des->field = field->lexeme;
    }

    return des;
}

ast_initializer_t *ParseInitializer(parser_t *p) {
    ast_initializer_t *init = PushStruct(globalArena, ast_initializer_t);
    
    if (MatchPunctuation(p, PUNCTUATION_OPEN_CURLY)) {
        init->kind = INITIALIZER_LIST;
        init->list = DArrayCreate(ast_initializer_list_t *);
        
        do {
            ast_initializer_list_t *item = PushStruct(globalArena, ast_initializer_list_t);
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
    ast_init_declarator_t *node = PushStruct(globalArena, ast_init_declarator_t);

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
        printf("expect known type, given " SLICE_STR "\n", SLICE_ARGS(Peek(p)->lexeme));
        return NULL;
    }

    new->decl.specifiers = *declSpecifiers;

    new->decl.initDeclList = DArrayCreate(ast_declarator_t *);

    if (IsDeclaratorStart(p)) {
        do {
            ast_init_declarator_t *declarator = ParseInitDeclarator(p);

            if (declSpecifiers->storageClass == STORAGE_SPEC_TYPEDEF) {
                ast_declarator_t *decl = declarator->declarator;
                while (decl->kind != DECL_IDENTIFIER) {
                    switch (decl->kind) {
                        case DECL_ARRAY: decl = decl->array.inner; break;
                        case DECL_POINTER: decl = decl->pointer.inner; break;
                        case DECL_FUNCTION: decl = decl->function.inner; break;

                        default: break;
                    }
                }

                InsertTypedef(p, &decl->identifier.name);
            }

            DArrayPush(new->decl.initDeclList, declarator);
        } while (MatchPunctuation(p, PUNCTUATION_COMMA));
    }

    ExpectPunctuation(p, PUNCTUATION_SEMICOLON, "expects semicolon after declaration");

    return new;
}

// Statement Parsing
statement_kind GetNextStatementKind(parser_t *p) {
    token_t *t = Peek(p);

    if (t->type == TOKEN_PUNCTUATION && t->puncType == PUNCTUATION_OPEN_CURLY) {
        return STATEMENT_COMPOUND;
    }

    if (t->type == TOKEN_KEYWORD) {
        switch (t->keywordType) {
            case KEYWORD_CONTINUE:
            case KEYWORD_BREAK:
            case KEYWORD_RETURN: return STATEMENT_JUMP;

            case KEYWORD_WHILE:
            case KEYWORD_DO:
            case KEYWORD_FOR: return STATEMENT_ITERATION;

            case KEYWORD_IF:
            case KEYWORD_SWITCH: return STATEMENT_SELECTION;

            case KEYWORD_CASE:
            case KEYWORD_DEFAULT: return STATEMENT_LABELED;

            default: break;
        }
    }

    return STATEMENT_EXPRESSION;
}

bool IsTypeSpecifier(parser_t *p) {
    token_t *t = Peek(p);
    builtin_type_t type;

    if (t->type == TOKEN_KEYWORD) {
        if (KeywordToBuiltinType(t->keywordType, &type)) return true;
        switch (t->keywordType) {
            case KEYWORD_STRUCT:
            case KEYWORD_UNION:
            case KEYWORD_ENUM: return true;

            default: return false;
        }
    } else if (t->type == TOKEN_IDENTIFIER) {
        if (HashContains(p->scope->typedefs, &t->lexeme)) {
            return true;
        }
    }

    return false;
}

bool IsDeclarationStart(parser_t *p) {
    token_t *t = Peek(p);

    storage_class_specifier spec;
    type_qualifier qual;

    if (t->type == TOKEN_KEYWORD) {
        if (KeywordToStorageClass(t->keywordType, &spec)) return true;
        if (KeywordToTypeQualifier(t->keywordType, &qual)) return true;
    }

    if (IsTypeSpecifier(p)) return true;

    return false;
}

ast_node_t *ParseStatement(parser_t *p) {
    ast_node_t *statement = GetNewNode(AST_STATEMENT);
    statement->statement.kind = GetNextStatementKind(p);

    switch (statement->statement.kind) {
        case STATEMENT_LABELED: {
            if (MatchKeyword(p, KEYWORD_CASE)) {
                statement->statement.labeled.kind = LABELED_STATEMENT_CASE;
                statement->statement.labeled.label_case.label = ParseExpression(p);
                ExpectPunctuation(p, PUNCTUATION_COLON, "expects colon after case expression");
            } else if (MatchKeyword(p, KEYWORD_DEFAULT)) {
                statement->statement.labeled.kind = LABELED_STATEMENT_DEFAULT;
                ExpectPunctuation(p, PUNCTUATION_COLON, "expects colon after `default`");
            }

            statement->statement.labeled.inner = ParseStatement(p);
        } break;

        case STATEMENT_COMPOUND: {
            ExpectPunctuation(p, PUNCTUATION_OPEN_CURLY, "expects open curly bracket for compound statement");
            while (!MatchPunctuation(p, PUNCTUATION_CLOSE_CURLY)) {
                if (!statement->statement.compound.statements)
                    statement->statement.compound.statements = DArrayCreate(ast_node_t *);

                if (IsDeclarationStart(p)) {
                    ast_node_t *decl = ParseDeclaration(p);
                    DArrayPush(statement->statement.compound.statements, decl);
                } else {
                    ast_node_t *s = ParseStatement(p);
                    DArrayPush(statement->statement.compound.statements, s);
                }
            }
        } break;

        case STATEMENT_EXPRESSION: {
            if (!MatchPunctuation(p, PUNCTUATION_SEMICOLON)) {
                statement->statement.expression.expression = ParseExpression(p);
                if (!statement->statement.expression.expression) {
                    printf("expected expression in statement expression\n");
                    return NULL;
                }
                ExpectPunctuation(p, PUNCTUATION_SEMICOLON, "Expects semicolon after expression statement");
            }
        } break;

        case STATEMENT_SELECTION: {
            if (MatchKeyword(p, KEYWORD_IF)) {
                statement->statement.selection.kind = SELECTION_STATEMENT_IF;

                ExpectPunctuation(p, PUNCTUATION_OPEN_PAREN, "expects open parenthese for if condition expression");
                statement->statement.selection.if_statement.condition = ParseExpression(p);
                ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expects close parenthese for if condition expression");

                statement->statement.selection.if_statement.ifStatement = ParseStatement(p);

                if (MatchKeyword(p, KEYWORD_ELSE)) {
                    statement->statement.selection.if_statement.elseStatement = ParseStatement(p);
                }
            } else if (MatchKeyword(p, KEYWORD_SWITCH)) {
                statement->statement.selection.kind = SELECTION_STATEMENT_SWITCH;

                ExpectPunctuation(p, PUNCTUATION_OPEN_PAREN, "expects open parenthese for switch condition expression");
                statement->statement.selection.switch_statement.condition = ParseExpression(p);
                ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expects close parenthese for switch condition expression");

                statement->statement.selection.switch_statement.statement = ParseStatement(p);
            } else {
                printf("Expects `if` or `switch` in selection statement");
                return NULL;
            }
        } break;

        case STATEMENT_ITERATION: {
            if (MatchKeyword(p, KEYWORD_WHILE)) {
                statement->statement.iteration.kind = ITERATION_STATEMENT_WHILE;
                
                ExpectPunctuation(p, PUNCTUATION_OPEN_PAREN, "expects open parenthese around condition for while");
                statement->statement.iteration.while_statement.condition = ParseExpression(p);
                ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expects close parenthese around condition for while");

                statement->statement.iteration.while_statement.statement = ParseStatement(p);
            } else if (MatchKeyword(p, KEYWORD_DO)) {
                statement->statement.iteration.kind = ITERATION_STATEMENT_WHILE;
                statement->statement.iteration.while_statement.hasDo = true;
                statement->statement.iteration.while_statement.statement = ParseStatement(p);
                
                ExpectKeyword(p, KEYWORD_WHILE, "expects `while` in do-while statement");
                ExpectPunctuation(p, PUNCTUATION_OPEN_PAREN, "expects open parenthese around condition for while");
                statement->statement.iteration.while_statement.condition = ParseExpression(p);
                ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expects close parenthese around condition for while");
                ExpectPunctuation(p, PUNCTUATION_SEMICOLON, "expects semicolon after do-while statement");
            } else if (MatchKeyword(p, KEYWORD_FOR)) {
                statement->statement.iteration.kind = ITERATION_STATEMENT_FOR;

                ExpectPunctuation(p, PUNCTUATION_OPEN_PAREN, "expects open parenthese in `for` statement");

                for (int i = 0; i < 3; i++) {
                    if (i < 2 && !MatchPunctuation(p, PUNCTUATION_SEMICOLON)) {
                        if (i == 0 && IsDeclarationStart(p)) {
                            statement->statement.iteration.for_statement.expressions[i] = ParseDeclaration(p);
                        } else {
                            statement->statement.iteration.for_statement.expressions[i] = ParseExpression(p);
                            ExpectPunctuation(p, PUNCTUATION_SEMICOLON, "expects semicolon after each `for` expression");
                        }
                    } else if (i == 2 && !MatchPunctuation(p, PUNCTUATION_CLOSE_PAREN)) {
                        statement->statement.iteration.for_statement.expressions[i] = ParseExpression(p);
                        ExpectPunctuation(p, PUNCTUATION_CLOSE_PAREN, "expects close parenthese after `for` condition");
                    }
                }

                statement->statement.iteration.for_statement.statement = ParseStatement(p);
            }
        } break;

        case STATEMENT_JUMP: {
            if (MatchKeyword(p, KEYWORD_CONTINUE)) {
                statement->statement.jump.kind = JUMP_STATEMENT_CONTINUE;
                ExpectPunctuation(p, PUNCTUATION_SEMICOLON, "expects semicolon after `continue`");
            } else if (MatchKeyword(p, KEYWORD_BREAK)) {
                statement->statement.jump.kind = JUMP_STATEMENT_BREAK;
                ExpectPunctuation(p, PUNCTUATION_SEMICOLON, "expects semicolon after `break`");
            } else if (MatchKeyword(p, KEYWORD_RETURN)) {
                statement->statement.jump.kind = JUMP_STATEMENT_RETURN;
                
                if (!MatchPunctuation(p, PUNCTUATION_SEMICOLON)) {
                    statement->statement.jump.return_statement.expr = ParseExpression(p);
                    ExpectPunctuation(p, PUNCTUATION_SEMICOLON, "expects semicolon after return statement");
                }
            }
        } break;

        default: break;
    }

    return statement;
}

// Translation Unit Parsing
ast_node_t *ParseTranslationUnit(parser_t *p) {
    u32 beginning = p->pos;

    if (!IsDeclarationStart(p) && !IsDeclaratorStart(p)) {
        printf("expects function definition or declaration in translation unit\n");
        return NULL;
    }

    decl_specifiers_t *specs = ParseDeclarationSpecifiers(p);
    ast_declarator_t *decl = ParseDeclarator(p, false);

    token_t *t = Peek(p);
    if (t->type == TOKEN_PUNCTUATION && t->puncType == PUNCTUATION_OPEN_CURLY) {
        ast_node_t *new = GetNewNode(AST_FUNC_DEF);
        new->func_def.specs = specs;
        new->func_def.declarator = decl;
        new->func_def.statement = ParseStatement(p);

        return new;
    } else {
        GoTo(p, beginning);
        return ParseDeclaration(p);
    }
}

ast_node_t *AstFromTokens(token_t *tokens) {
    ast_node_t *program = PushStruct(globalArena, ast_node_t);
    program->type = AST_PROGRAM;
    program->program.units = DArrayCreate(ast_node_t *);

    parser_t p = {0};
    p.tokens = tokens;
    p.count = DArrayLength(tokens);
    p.pos = 0;
    p.scope = NULL;
    
    PushTypedefScope(&p);
    
    while (!Match(&p, TOKEN_EOF)) {
        ast_node_t *unit = ParseTranslationUnit(&p);
        if (!unit) {
            return NULL;
        }

        DArrayPush(program->program.units, unit);
    }

    PopTypedefScope(&p);

    return program;
}

// Printing Stuff
void PrintDeclSpecifiers(decl_specifiers_t *specifiers, int depth) {
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
            if (needSpace)
                printf(" ");
            printf("%s ", spec->struct_union.isUnion ? "union" : "struct");
            if (spec->struct_union.name.str) {
                printf(SLICE_STR " ", SLICE_ARGS(spec->struct_union.name));
            }

            printf("{\n%*s", (depth + 2) * 4, "");

            int fieldCount = DArrayLength(spec->struct_union.fields);
            for (int i = 0; i < fieldCount; i++) {
                ast_node_t node = {};
                node.type = AST_DECL;
                node.decl = spec->struct_union.fields[i];

                PrintAst(&node, depth + 1);
                printf(";\n");

                if (i < fieldCount - 1) {
                    printf("%*s", (depth + 2) * 4, "");
                }
            }
            printf("%*s}", (depth + 1) * 4, "");
        } break;

        case SPECIFIER_ENUM: {
            if (needSpace)
                printf(" ");
            printf("enum ");
            if (spec->enum_type.name.str) {
                printf(SLICE_STR " ", SLICE_ARGS(spec->enum_type.name));
            }

            printf("{\n");
            int enumCount = DArrayLength(spec->enum_type.enumerators);
            for (int i = 0; i < enumCount; i++) {
                printf("%*s" SLICE_STR, (depth + 2)*4, "", SLICE_ARGS(spec->enum_type.enumerators[i].name));

                if (spec->enum_type.enumerators[i].value) {
                    printf(" = ");
                    PrintAst(spec->enum_type.enumerators[i].value, 0);
                }

                printf("\n");
            }

            printf("%*s}", (depth + 1) * 4, "");
        } break;

        case SPECIFIER_TYPEDEF_NAME: {
            printf(SLICE_STR, SLICE_ARGS(spec->typedef_type.name));
        } break;

        default: break;
    }
}

void PrintAstDeclarator(ast_declarator_t *decl, int depth) {
    switch (decl->kind) {
        case DECL_IDENTIFIER: {
            printf(SLICE_STR, SLICE_ARGS(decl->identifier.name));
        } break;

        case DECL_POINTER: {
            printf("*");
            for (int i = 0; i < TYPE_QUALIFIER_COUNT; i++) {
                if (decl->pointer.qualifiers & (1 << i)) {
                    printf(" %s", TypeQualifierToStr((type_qualifier)(1 << i)));
                }
            }

            if (decl->pointer.inner)
                PrintAstDeclarator(decl->pointer.inner, depth);
        } break;

        case DECL_ARRAY: {
            PrintAstDeclarator(decl->array.inner, depth);
            printf("[");
            PrintAst(decl->array.size, 0);
            printf("]");
        } break;

        case DECL_FUNCTION: {
            PrintAstDeclarator(decl->function.inner, depth);
            printf("(");
            if (decl->function.parameters) {
                int paramCount = DArrayLength(decl->function.parameters);
                for (int i = 0; i < paramCount; i++) {
                    ast_parameter_t *param = decl->function.parameters[i];
                    PrintDeclSpecifiers(&param->specifiers, depth);
                    printf(" ");
                    PrintAstDeclarator(param->declarator, depth);
                    
                    if (i < paramCount - 1) {
                        printf(", ");
                    }
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
                                printf("." SLICE_STR, SLICE_ARGS(init->designation[j]->field));
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
                if (i < listLen - 1) {
                    printf(", ");
                }
            }
        } break;
    }
}

void PrintAstStatement(ast_statement_t *statement, int depth) {
    printf("Statement(");
    switch (statement->kind) {
        case STATEMENT_COMPOUND: {
            printf("{\n%*s", (depth + 1) * 4, "");
            
            if (statement->compound.declarations) {
                int declCount = DArrayLength(statement->compound.declarations);
                for (int i = 0; i < declCount; i++) {
                    PrintAst(statement->compound.declarations[i], depth + 1);
                    if (i < declCount - 1) {
                        printf(",\n%*s", (depth + 1) * 4, "");
                    }
                }
            }
            
            if (statement->compound.statements) {
                if (statement->compound.declarations) {
                    printf("\n%*s", (depth + 1) * 4, "");
                }

                int statementCount = DArrayLength(statement->compound.statements);
                for (int i = 0; i < statementCount; i++) {
                    PrintAst(statement->compound.statements[i], depth + 1);
                    if (i < statementCount - 1) {
                        printf(",\n%*s", (depth + 1) * 4, "");
                    }
                }
            }

            printf("\n%*s}", depth * 4, "");
        } break;

        case STATEMENT_LABELED: {
            if (statement->labeled.kind == LABELED_STATEMENT_CASE) {
                printf("case ");
                PrintAst(statement->labeled.label_case.label, depth);
                printf(": ");
                PrintAst(statement->labeled.inner, depth);
            } else if (statement->labeled.kind == LABELED_STATEMENT_DEFAULT) {
                printf("default: ");
                PrintAst(statement->labeled.inner, depth);
            }
        } break;

        case STATEMENT_EXPRESSION: {
            PrintAst(statement->expression.expression, depth);
        } break;

        case STATEMENT_SELECTION: {
            if (statement->selection.kind == SELECTION_STATEMENT_IF) {
                printf("if (");
                PrintAst(statement->selection.if_statement.condition, depth + 1);
                printf(") ");
                PrintAst(statement->selection.if_statement.ifStatement, depth + 1);
                printf("}");

                if (statement->selection.if_statement.elseStatement) {
                    printf(" else { ");
                    PrintAst(statement->selection.if_statement.elseStatement, depth + 1);
                    printf("\n%*s}", (depth + 1) * 4, "");
                }
            } else if (statement->selection.kind == SELECTION_STATEMENT_SWITCH) {
                printf("switch (");
                PrintAst(statement->selection.switch_statement.condition, depth + 1);
                printf(") ");
                PrintAst(statement->selection.switch_statement.statement, depth);
                // printf("\n%*s", (depth + 1) * 4, "");
            }
        } break;

        case STATEMENT_ITERATION: {
            switch (statement->iteration.kind) {
                case ITERATION_STATEMENT_WHILE: {
                    if (statement->iteration.while_statement.hasDo) {
                        printf("do ");
                        PrintAst(statement->iteration.while_statement.statement, depth);
                        printf(" while (");
                        PrintAst(statement->iteration.while_statement.condition, depth + 1);
                        printf(")");
                    } else {
                        printf("while (");
                        PrintAst(statement->iteration.while_statement.condition, depth + 1);
                        printf(")\n");
                        PrintAst(statement->iteration.while_statement.statement, depth);
                    }
                } break;;

                case ITERATION_STATEMENT_FOR: {
                    printf("for (");
                    for (int i = 0; i < 3; i++) {
                        if (statement->iteration.for_statement.expressions[i]) {
                            PrintAst(statement->iteration.for_statement.expressions[i], depth);
                            if (i < 2) {
                                printf("; ");
                            }
                        }
                    }
                    printf(") ");
                    PrintAst(statement->iteration.for_statement.statement, depth);
                }
            }
        } break;

        case STATEMENT_JUMP: {
            switch (statement->jump.kind) {
                case JUMP_STATEMENT_CONTINUE: printf("continue"); break;
                case JUMP_STATEMENT_BREAK: printf("break"); break;
                case JUMP_STATEMENT_RETURN: {
                    printf("return");
                    if (statement->jump.return_statement.expr) {
                        printf(" ");
                        PrintAst(statement->jump.return_statement.expr, depth);
                    }
                } break;

                default: break;
            }
        } break;

        default: break;
    }

    printf(")");
}

// ---- Semantic annotation printers ----

static void PrintType(type_t *type) {
    if (!type) { printf("?"); return; }

    if (type->qualifiers & TYPE_QUALIFIER_CONST)    printf("const ");
    if (type->qualifiers & TYPE_QUALIFIER_VOLATILE) printf("volatile ");
    if (type->qualifiers & TYPE_QUALIFIER_RESTRICT) printf("restrict ");

    switch (type->kind) {
        case TYPE_BUILTIN: {
            if (type->builtin.sign == SIGN_SIGNED)        printf("signed ");
            else if (type->builtin.sign == SIGN_UNSIGNED) printf("unsigned ");
            if      (type->builtin.width == WIDTH_SHORT)    printf("short ");
            else if (type->builtin.width == WIDTH_LONG)     printf("long ");
            else if (type->builtin.width == WIDTH_LONGLONG) printf("long long ");
            switch (type->builtin.base) {
                case BUILTIN_VOID:   printf("void");   break;
                case BUILTIN_CHAR:   printf("char");   break;
                case BUILTIN_INT:    printf("int");    break;
                case BUILTIN_FLOAT:  printf("float");  break;
                case BUILTIN_DOUBLE: printf("double"); break;
                default:             printf("int");    break;
            }
        } break;

        case TYPE_POINTER:
            PrintType(type->ptr.base);
            printf("*");
            break;

        case TYPE_ARRAY:
            PrintType(type->array.base);
            if (type->array.hasSize) printf("[%u]", type->array.size);
            else                     printf("[]");
            break;

        case TYPE_FUNCTION:
            PrintType(type->function.returnType);
            printf("(");
            if (type->function.paramTypes) {
                int n = (int)DArrayLength(type->function.paramTypes);
                for (int i = 0; i < n; i++) {
                    PrintType(type->function.paramTypes[i]);
                    if (i < n - 1) printf(", ");
                }
            }
            printf(")");
            break;

        case TYPE_STRUCT:
            printf("struct");
            if (type->struct_union.name.str)
                printf(" " SLICE_STR, SLICE_ARGS(type->struct_union.name));
            break;

        case TYPE_UNION:
            printf("union");
            if (type->struct_union.name.str)
                printf(" " SLICE_STR, SLICE_ARGS(type->struct_union.name));
            break;

        case TYPE_ENUM:
            printf("enum");
            if (type->enum_type.name.str)
                printf(" " SLICE_STR, SLICE_ARGS(type->enum_type.name));
            break;

        default: printf("?"); break;
    }
}

// Prints  :<type>  when resolvedType is non-NULL — appended right after a node.
static void PrintTypeAnnotation(ast_node_t *node) {
    if (!node->resolvedType) return;
    printf(":");
    PrintType(node->resolvedType);
}

// Prints  [kind]  info for an identifier's symbol — inserted inside Ident().
static void PrintSymbolAnnotation(ast_node_t *node) {
    symbol_t *sym = node->symbol;
    if (!sym) return;
    printf("[");
    switch (sym->kind) {
        case SYMBOL_VAR:          printf("var");                               break;
        case SYMBOL_FUNC:         printf("func");                              break;
        case SYMBOL_TYPEDEF:      printf("typedef");                           break;
        case SYMBOL_ENUM_CONSTS:  printf("enum=%lld", (long long)sym->enumConstantValue); break;
        case SYMBOL_STRUCT_UNION: printf("struct_tag");                        break;
        case SYMBOL_ENUM:         printf("enum_tag");                          break;
        default:                  printf("?");                                 break;
    }
    printf("]");
}

// ---- AST printer ----

void PrintAst(ast_node_t *parent, int depth) {
    switch (parent->type) {
        case AST_PROGRAM: {
            printf("[\n");

            int unitCount = DArrayLength(parent->program.units);
            for (int i = 0; i < unitCount; i++) {
                printf("%*s", (depth + 1) * 4, "");
                PrintAst(parent->program.units[i], depth + 1);

                if (i < unitCount - 1) {
                    printf(", ");
                }
                printf("\n");
            }
            printf("]\n");
        } break;

        case AST_DECL: {
            printf("Declaration(");
            PrintDeclSpecifiers(&parent->decl.specifiers, depth);
            int declCount = DArrayLength(parent->decl.initDeclList);

            if (declCount > 1)
                printf("\n");
            else
                printf(" ");

            for (int i = 0; i < declCount; i++) {
                ast_init_declarator_t *decl = parent->decl.initDeclList[i];
                if (declCount > 1)
                    printf("%*s", (depth + 2) * 4, "");
                PrintAstDeclarator(decl->declarator, depth);

                if (decl->initializer) {
                    printf(" = { ");
                    PrintAstInitializer(decl->initializer);
                    printf(" }");
                }

                if (i < declCount - 1) {
                    printf(",\n");
                }
            }
            printf(")");
        } break;

        case AST_FUNC_DEF: {
            printf("Function(");
            if (parent->func_def.specs) {
                PrintDeclSpecifiers(parent->func_def.specs, depth);
                printf(" ");
            }
            PrintAstDeclarator(parent->func_def.declarator, depth);
            printf(" ");
            PrintAst(parent->func_def.statement, depth);
            printf(")");
        } break;

        case AST_STATEMENT: {
            PrintAstStatement(&parent->statement, depth);
        } break;

        case AST_FUNC_CALL: {
            printf("FunCall(");
            PrintAst(parent->func_call.fun, depth + 1);
            printf(", params = [");

            i32 paramCount = DArrayLength(parent->func_call.params);
            for (int i = 0; i < paramCount; i++) {
                PrintAst(parent->func_call.params[i], depth + 1);
                if (i < paramCount - 1) {
                    printf(", ");
                }
            }
            printf("])");
            PrintTypeAnnotation(parent);
        } break;

        case AST_TERNARY_EXPR: {
            printf("TernExpr(");
            PrintAst(parent->ternary_expr.condition, depth + 1);
            printf(" ? ");
            PrintAst(parent->ternary_expr.then_expr, depth + 1);
            printf(" : ");
            PrintAst(parent->ternary_expr.else_expr, depth + 1);
            printf(")");
            PrintTypeAnnotation(parent);
        } break;

        case AST_BINARY_EXPR: {
            printf("BinOp(");
            PrintAst(parent->binary_op.left, depth + 1);
            printf(" %s ", BinaryToStr(parent->binary_op.op));
            PrintAst(parent->binary_op.right, depth + 1);
            printf(")");
            PrintTypeAnnotation(parent);
        } break;

        case AST_UNARY_EXPR: {
            printf("UnOp(%s", UnaryToStr(parent->unary_op.op));
            PrintAst(parent->unary_op.expr, depth + 1);
            printf(")");
            PrintTypeAnnotation(parent);
        } break;

        case AST_ASSIGN_EXPR: {
            printf("Assign(");
            PrintAst(parent->assign_op.left, depth);
            printf(" %s ", AssignToStr(parent->assign_op.op));
            PrintAst(parent->assign_op.right, depth);
            printf(")");
            PrintTypeAnnotation(parent);
        } break;

        case AST_CAST_EXPR: {
            printf("Cast(");
            decl_specifiers_t specs = {
                .typeQualifier = parent->cast_expr.qualifiers,
                .typeSpecifier = parent->cast_expr.type,
            };
            PrintDeclSpecifiers(&specs, depth + 1);
            if (parent->cast_expr.declarator) {
                PrintAstDeclarator(parent->cast_expr.declarator, depth + 1);
            }
            printf(", ");
            PrintAst(parent->cast_expr.expr, depth + 1);
            printf(")");
            PrintTypeAnnotation(parent);
        } break;

        case AST_INDEX: {
            printf("Index(");
            PrintAst(parent->index.array, depth + 1);
            printf("[");
            PrintAst(parent->index.index, depth + 1);
            printf("])");
            PrintTypeAnnotation(parent);
        } break;

        case AST_MEMBER: {
            printf("Member(");
            PrintAst(parent->member.parent, depth + 1);
            printf("%s" SLICE_STR ")", parent->member.isPointer ? "->" : ".", SLICE_ARGS(parent->member.member));
            PrintTypeAnnotation(parent);
        } break;

        case AST_LITERAL_INT: {
            printf("%d", parent->int_literal.literal);
            PrintTypeAnnotation(parent);
        } break;

        case AST_LITERAL_STRING: {
            printf("\"" SLICE_STR "\"", SLICE_ARGS(parent->string_literal.str));
            PrintTypeAnnotation(parent);
        } break;

        case AST_IDENTIFIER: {
            printf("Ident(" SLICE_STR, SLICE_ARGS(parent->identifier.name));
            PrintSymbolAnnotation(parent);
            printf(")");
            PrintTypeAnnotation(parent);
        } break;

        default: break;
    }
}