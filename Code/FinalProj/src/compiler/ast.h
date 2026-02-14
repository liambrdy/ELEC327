#ifndef _AST_H
#define _AST_H

#include "common.h"
#include "lexer.h"

typedef enum ast_binary_op {
    BINARY_OP_ADDITION,
    BINARY_OP_SUBTRACTION,
} ast_binary_op;

typedef enum ast_unary_op {
    UNARY_OP_NEGATE,
    UNARY_OP_INCREMENT,
    UNARY_OP_DECREMENT,
} ast_unary_op;

typedef enum ast_node_type {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_RETURN,
    AST_IF,
    AST_WHILE,

    AST_BINARY_EXPR,
    AST_ASSIGN_EXPR,
    AST_UNARY_EXPR,
    AST_LITERAL_INT,
    AST_LITERAL_STRING,
    AST_IDENTIFIER,
} ast_node_type;

typedef struct ast_node_t ast_node_t;

typedef struct ast_node_t {
    ast_node_type type;

    union {
        struct {
            ast_node_t **decls;
        } program;

        struct {
            u8 *funName;
            ast_node_t **params;
            ast_node_t *body;
        } function;

        struct {
            ast_node_t **statements;
        } block;

        struct {
            u8 *name;
            ast_node_t *value;
        } var_decl;

        struct {
            ast_node_t *value;
        } return_stmt;

        struct {
            ast_node_t *condition;
            ast_node_t *then_branch;
            ast_node_t *else_branch;
        } if_stmt;

        struct {
            ast_node_t *condition;
            ast_node_t *block;
        } while_stmt;

        struct {
            ast_binary_op op;
            ast_node_t *left;
            ast_node_t *right;
        } binary_op;

        struct {
            ast_unary_op op;
            ast_node_t *expr;
        } unary_op;

        struct {
            int literal;
        } int_literal;

        struct {
            u8 *literal;
        } string_literal;

        struct {
            u8 *name;
        } identifier;
    };
} ast_node_t;

ast_node_t *AstFromTokens(token_t *tokens);

#endif