#ifndef _SEMANTICS_H
#define _SEMANTICS_H

#include "ast.h"

typedef enum type_kind {
    TYPE_BUILTIN,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_FUNCTION,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
} type_kind;

typedef struct field_t {
    slice_t name;
    type_t *type;
} field_t;

typedef struct type_t {
    type_kind kind;
    i32 qualifiers;

    union {
        builtin_type_t builtin;

        struct {
            struct type_t *base;
        } ptr;

        struct {
            struct type_t *base;
            u32 size;
            bool hasSize;
        } array;

        struct {
            struct type_t *returnType;
            struct type_t **paramTypes;
        } function;

        struct {
            slice_t name;
            field_t *fields;
        } struct_union;

        struct {
            slice_t name;
        } enum_type;
    };
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
    type_t *type;

    storage_class_specifier storageSpecs;
    
    i64 enumConstantValue;
    bool emptyDecl;
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