#include "astEval.h"

#include <stdio.h>

u32 EvaluateConstantExpression(ast_node_t *expr) {
    switch (expr->type) {
        case AST_PROGRAM:
        case AST_DECL:
        case AST_FUNC_DEF:
        case AST_STATEMENT:
        case AST_FUNC_CALL:
        case AST_INDEX:
        case AST_MEMBER:
        case AST_LITERAL_STRING:
        case AST_IDENTIFIER:
        case AST_TERNARY_EXPR:
        case AST_BINARY_EXPR:
        case AST_ASSIGN_EXPR:
        case AST_UNARY_EXPR:
        case AST_CAST_EXPR: {
            printf("node is not a constant expression: ");
            PrintAst(expr, 0);
            return 0;
        } break;

        case AST_LITERAL_INT: return expr->int_literal.literal;

        default: break;
    }
}