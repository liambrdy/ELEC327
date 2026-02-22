#ifndef _AST_H
#define _AST_H

#include "common.h"
#include "lexer.h"

#include <stdbool.h>

typedef enum ast_binary_op {
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_MULT,
    BINARY_OP_DIV,
    BINARY_OP_MOD,
    BINARY_OP_LOGIC_OR,
    BINARY_OP_LOGIC_AND,
    BINARY_OP_OR,
    BINARY_OP_AND,
    BINARY_OP_XOR,
    BINARY_OP_EQUIV,
    BINARY_OP_LT,
    BINARY_OP_LTE,
    BINARY_OP_GT,
    BINARY_OP_GTE,
    BINARY_OP_NOT_EQUIV,
    BINARY_OP_SHL,
    BINARY_OP_SHR,
} ast_binary_op;

typedef enum ast_assignment_op {
    ASSIGN,
    ASSIGN_ADD,
    ASSIGN_SUB,
    ASSIGN_MUL,
    ASSIGN_DIV,
} ast_assignment_op;

typedef enum ast_unary_op {
    UNARY_OP_NEGATE,
    UNARY_OP_INCREMENT,
    UNARY_OP_DECREMENT,
    UNARY_OP_NOT,
    UNARY_OP_LOGIC_NOT,
} ast_unary_op;

typedef enum ast_node_type {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_RETURN,
    AST_IF,
    AST_WHILE,
    
    AST_TERNARY_EXPR,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_ASSIGN_EXPR,

    AST_INDEX,
    AST_MEMBER,

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
            ast_node_t *condition;
            ast_node_t *then_expr;
            ast_node_t *else_expr;
        } ternary_expr;

        struct {
            ast_binary_op op;
            ast_node_t *left;
            ast_node_t *right;
        } binary_op;

        struct {
            ast_assignment_op op;
            ast_node_t *left;
            ast_node_t *right;
        } assign_op;

        struct {
            ast_unary_op op;
            ast_node_t *expr;
        } unary_op;

        struct {
            ast_node_t *array;
            ast_node_t *index;
        } index;

        struct {
            ast_node_t *parent;
            u8 *member;
        } member;

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
void PrintAst(ast_node_t *parent);

static bool PunctuationToAssignment(token_punctuation_type type, ast_assignment_op *op) {
    switch (type) {
        case PUNCTUATION_EQUALS: *op = ASSIGN; break;
        case PUNCTUATION_PLUS_EQUALS: *op = ASSIGN_ADD; break;
        case PUNCTUATION_MINUS_EQUALS: *op = ASSIGN_SUB; break;
        case PUNCTUATION_MULT_EQUALS: *op = ASSIGN_MUL; break;
        case PUNCTUATION_DIV_EQUALS: *op = ASSIGN_DIV; break;

        default: return false;
    }
}

static bool PunctuationToLogicOr(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_LOGIC_OR: *op = BINARY_OP_LOGIC_OR; return true;
        default: return false;
    }
}

static bool PunctuationToLogicAnd(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_LOGIC_AND: *op = BINARY_OP_LOGIC_AND; return true;
        default: return false;
    }
}

static bool PunctuationToOr(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_OR: *op = BINARY_OP_OR; return true;
        default: return false;
    }
}

static bool PunctuationToXOr(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_CARROT: *op = BINARY_OP_XOR; return true;
        default: return false;
    }
}

static bool PunctuationToAnd(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_AND: *op = BINARY_OP_AND; return true;
        default: return false;
    }
}

static bool PunctuationToEquality(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_EQUIV: *op = BINARY_OP_EQUIV; return true;
        case PUNCTUATION_NOT_EQUIV: *op = BINARY_OP_NOT_EQUIV; return true;
        default: return false;
    }
}

static bool PunctuationToRelation(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_LT: *op = BINARY_OP_LT; return true;
        case PUNCTUATION_GT: *op = BINARY_OP_GT; return true;
        case PUNCTUATION_LT_EQ: *op = BINARY_OP_LTE; return true;
        case PUNCTUATION_GT_EQ: *op = BINARY_OP_GTE; return true;
        default: return false;
    }
}

static bool PunctuationToShift(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_SHL: *op = BINARY_OP_SHL; return true;
        case PUNCTUATION_SHR: *op = BINARY_OP_SHR; return true;
        default: return false;
    }
}

static bool PunctuationToAdd(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_PLUS: *op = BINARY_OP_ADD; return true;
        case PUNCTUATION_MINUS: *op = BINARY_OP_SUB; return true;
        default: return false;
    }
}

static bool PunctuationToMult(token_punctuation_type type, ast_binary_op *op) {
    switch (type) {
        case PUNCTUATION_MULT: *op = BINARY_OP_MULT; return true;
        case PUNCTUATION_DIV: *op = BINARY_OP_DIV; return true;
        case PUNCTUATION_MOD: *op = BINARY_OP_MOD; return true;
        default: return false;
    }
}

static bool PunctuationToUnary(token_punctuation_type type, ast_unary_op *op) {
    switch (type) {
        case PUNCTUATION_INCREMENT: *op = UNARY_OP_INCREMENT; return true;
        case PUNCTUATION_DECREMENT: *op = UNARY_OP_DECREMENT; return true;
        case PUNCTUATION_MINUS: *op = UNARY_OP_NEGATE; return true;
        case PUNCTUATION_LOGIC_NOT: *op = UNARY_OP_LOGIC_NOT; return true;
        case PUNCTUATION_NOT: *op = UNARY_OP_NOT; return true;
        default: return false;
    }
}

static u8 *BinaryToStr(ast_binary_op op) {
    switch (op) {
        case BINARY_OP_ADD: return "+";
        case BINARY_OP_SUB: return "-";
        case BINARY_OP_MULT: return "*";
        case BINARY_OP_DIV: return "/";
        case BINARY_OP_MOD: return "%";
        case BINARY_OP_LOGIC_OR: return "||";
        case BINARY_OP_LOGIC_AND: return "&&";
        case BINARY_OP_OR: return "|";
        case BINARY_OP_AND: return "&";
        case BINARY_OP_XOR: return "^";
        case BINARY_OP_EQUIV: return "==";
        case BINARY_OP_LT: return "<";
        case BINARY_OP_LTE: return "<=";
        case BINARY_OP_GT: return ">";
        case BINARY_OP_GTE: return ">=";
        case BINARY_OP_NOT_EQUIV: return "!=";
        case BINARY_OP_SHL: return "<<";
        case BINARY_OP_SHR: return ">>";

        default: return "";
    }
}

static u8 *UnaryToStr(ast_unary_op op) {
    switch (op) {
        case UNARY_OP_NEGATE: return "-";
        case UNARY_OP_INCREMENT: return "++";
        case UNARY_OP_DECREMENT: return "--";
        case UNARY_OP_NOT: return "~";
        case UNARY_OP_LOGIC_NOT: return "!";

        default: return "";
    }
}

#endif