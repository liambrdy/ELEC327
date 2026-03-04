#ifndef _SEMANTICS_H
#define _SEMANTICS_H

#include "ast.h"

typedef struct type_specifier_t type_specifier_t;
typedef struct ast_node_t ast_node_t;

typedef struct type_t {
    type_specifier_t *spec;
    u32 qualifiers;
} type_t;

typedef enum symbol_kind {
    SYMBOL_FUNC,
    SYMBOL_VAR,
    SYMBOL_STRUCT_UNION,
    SYMBOL_ENUM,
    SYMBOL_ENUM_CONSTS,
    SYMBOL_TYPEDEF,
} symbol_kind;

typedef struct symbol_t {
    symbol_kind kind;
    slice_t name;
    type_t type;
} symbol_t;

typedef struct scope_t {
    hash_table_t *table;
    struct scope_t *parent;
} scope_t;

typedef struct sema_context_t {
    scope_t *scope;
    type_t *currentRetType;
} sema_context_t;

void AnnotateAst(ast_node_t *node);

#endif