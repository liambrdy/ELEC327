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
            ExpectPunctuation(p, PUNCTAUTION_OPEN_PAREN, "expect open parenthese around expression");
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

            case PUNCTAUTION_OPEN_PAREN: {
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
    node->decl.name = name->lexeme;
    node->decl.value = init;

    return node;
}

ast_node_t *ParseFunction(parser_t *p) {
    return NULL;
}

ast_node_t *ParseDeclarationSpecifiers(parser_t *p) {

}

ast_node_t *ParseInitDeclaratorList(parser_t *p) {

}

ast_node_t *ParseDeclaration(parser_t *p) {
    ast_node_t *declSpecifiers = ParseDeclarationSpecifiers(p);

    if (!MatchPunctuation(p, PUNCTUATION_SEMICOLON)) {
        ast_node_t *init = ParseInitDeclaratorList(p);
    }

    
}

ast_node_t *AstFromTokens(token_t *tokens) {
    ast_node_t *program = (ast_node_t *)malloc(sizeof(ast_node_t));
    program->type = AST_PROGRAM;
    program->program.decls = DArrayCreate(ast_node_t *);

    parser_t p = {0};
    p.tokens = tokens;
    p.count = DArrayLength(tokens);
    p.pos = 0;
    
    while (!Match(&p, TOKEN_EOF)) {
        ast_node_t *decl = ParseDeclaration(&p);
        if (!decl) {
            return NULL;
        }

        DArrayPush(program->program.decls, decl);
    }

    return program;
}

void PrintAst(ast_node_t *parent) {
    switch (parent->type) {
        case AST_PROGRAM: {
            printf("[\n");
            for (int i = 0; i < DArrayLength(parent->program.decls); i++) {
                printf("\t");
                PrintAst(parent->program.decls[i]);

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
            printf("VarDecl(%s = ", parent->decl.name);
            PrintAst(parent->decl.value);
            printf(")");
        } break;

        case AST_FUNC_CALL: {
            printf("FunCall(");
            PrintAst(parent->func_call.fun);
            printf("\tparams = [");

            i32 paramCount = DArrayLength(parent->func_call.params);
            for (int i = 0; i < paramCount; i++) {
                PrintAst(parent->func_call.params[i]);
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
            PrintAst(parent->ternary_expr.condition);
            printf(" ? ");
            PrintAst(parent->ternary_expr.then_expr);
            printf(" : ");
            PrintAst(parent->ternary_expr.else_expr);
            printf(")");
        } break;

        case AST_BINARY_EXPR: {
            printf("BinOp(");
            PrintAst(parent->binary_op.left);
            printf(" %s ", BinaryToStr(parent->binary_op.op));
            PrintAst(parent->binary_op.right);
            printf(")");
        } break;

        case AST_UNARY_EXPR: {
            printf("UnOp(%s", UnaryToStr(parent->unary_op.op));
            PrintAst(parent->unary_op.expr);
            printf(")");
        } break;

        case AST_ASSIGN_EXPR: {

        } break;

        case AST_INDEX: {
            printf("Index(");
            PrintAst(parent->index.array);
            printf("[");
            PrintAst(parent->index.index);
            printf("])");
        } break;

        case AST_MEMBER: {
            printf("Member(");
            PrintAst(parent->member.parent);
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