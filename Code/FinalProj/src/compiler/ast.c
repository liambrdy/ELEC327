#include "ast.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include "darray.h"
#include "parser.h"

ast_node_t *GetNewNode(ast_node_type type) {
    ast_node_t *node = (ast_node_t *)malloc(sizeof(ast_node_t));
    node->type = type;

    return node;
}

static ast_node_t *ParseExpression(parser_t *p);

ast_node_t *ParseEqualityExpression(parser_t *p) {
    ast_node_t *relationalExpr = ParseRelationalExpression(p);

    if (MatchPunctuation(p, PUNCTUATION_EQUIV)) {
        ast_node_t *secondExpr = ParseRelationalExpression(p);

        ast_node_t *newNode = GetNewNode(AST_BINARY_EXPR);
        newNode->binary_op.op = BINARY_OP_EQUIV;
        newNode->binary_op.left = relationalExpr;
        newNode->binary_op.right = secondExpr;

        return newNode;
    }

    if (MatchPunctuation(p, PUNCTUATION_NOT_EQUIV)) {
        ast_node_t *secondExpr = ParseRelationalExpression(p);

        ast_node_t *newNode = GetNewNode(AST_BINARY_EXPR);
        newNode->binary_op.op = BINARY_OP_NOT_EQUIV;
        newNode->binary_op.left = relationalExpr;
        newNode->binary_op.right = secondExpr;

        return newNode;
    }

    return relationalExpr;
}

ast_node_t *ParseAndExpression(parser_t *p) {
    ast_node_t *equalityExpr = ParseEqualityExpression(p);

    if (MatchPunctuation(p, PUNCTUATION_AND)) {
        ast_node_t *secondExpr = ParseEqualityExpression(p);

        ast_node_t *newNode = GetNewNode(AST_BINARY_EXPR);
        newNode->binary_op.op = BINARY_OP_AND;
        newNode->binary_op.left = equalityExpr;
        newNode->binary_op.right = secondExpr;

        return newNode;
    }

    return equalityExpr;
}

ast_node_t *ParseExclusiveOrExpression(parser_t *p) {
    ast_node_t *xorExpr = ParseAndExpression(p);

    if (MatchPunctuation(p, PUNCTUATION_CARROT)) {
        ast_node_t *secondExpr = ParseAndExpression(p);

        ast_node_t *newNode = GetNewNode(AST_BINARY_EXPR);
        newNode->binary_op.op = BINARY_OP_XOR;
        newNode->binary_op.left = xorExpr;
        newNode->binary_op.right = secondExpr;

        return newNode;
    }

    return xorExpr;
}

ast_node_t *ParseInclusiveOrExpression(parser_t *p) {
    ast_node_t *xorExpr = ParseExclusiveOrExpression(p);

    if (MatchPunctuation(p, PUNCTUATION_OR)) {
        ast_node_t *secondExpr = ParseExclusiveOrExpression(p);

        ast_node_t *newNode = GetNewNode(AST_BINARY_EXPR);
        newNode->binary_op.op = BINARY_OP_OR;
        newNode->binary_op.left = xorExpr;
        newNode->binary_op.right = secondExpr;

        return newNode;
    }

    return xorExpr;
}

ast_node_t *ParseLogicAndExpression(parser_t *p) {
    ast_node_t *orExpr = ParseInclusiveOrExpression(p);

    if (MatchPunctuation(p, PUNCTUATION_LOGIC_AND)) {
        ast_node_t *secondExpr = ParseInclusiveOrExpression(p);

        ast_node_t *newNode = GetNewNode(AST_BINARY_EXPR);
        newNode->binary_op.op = BINARY_OP_LOGIC_AND;
        newNode->binary_op.left = orExpr;
        newNode->binary_op.right = secondExpr;

        return newNode;
    }

    return orExpr;
}

ast_node_t *ParseLogicOrExpression(parser_t *p) {
    ast_node_t *andExpr = ParseLogicAndExpression(p);

    if (MatchPunctuation(p, PUNCTUATION_LOGIC_OR)) {
        ast_node_t *secondExpr = ParseLogicAndExpression(p);

        ast_node_t *newNode = GetNewNode(AST_BINARY_EXPR);
        newNode->binary_op.op = BINARY_OP_LOGIC_OR;
        newNode->binary_op.left = andExpr;
        newNode->binary_op.right = secondExpr;

        return newNode;
    }

    return andExpr;
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
        case PUNCTUATION_TIMES_EQUALS: *op = ASSIGN_MUL; break;
        case PUNCTUATION_DIV_EQUALS: *op = ASSIGN_DIV; break;

        default: return false;
    }

    Advance(p);

    return true;
}

ast_node_t *ParseAssignmentExpression(parser_t *p) {
    ast_node_t *condExpr = ParseConditionalExpression(p);

    ast_assignment_op op;
    if (HasAssignmentOperator(p, &op)) {
        ast_node_t *assnExpr = ParseAssignmentExpression(p);

        ast_node_t *newExpr = GetNewNode(AST_ASSIGN_EXPR);
        newExpr->assign_op.op = op;
        newExpr->assign_op.left = condExpr;
        newExpr->assign_op.right = assnExpr;

        return newExpr;
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

    ast_node_t *node = GetNewNode(AST_VAR_DECL);
    node->var_decl.name = name->lexeme;
    node->var_decl.value = init;

    return node;
}

ast_node_t *ParseFunction(parser_t *p) {

}

ast_node_t *ParseDeclaration(parser_t *p) {
    if (MatchKeyword(p, KEYWORD_LET)) {
        return ParseVariableDeclaration(p);
    }

    if (MatchKeyword(p, KEYWORD_FUNC)) {
        return ParseFunction(p);
    }

    printf("Expected function or global variable declaration\n");
    return NULL;
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