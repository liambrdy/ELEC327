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
    ASSIGN_SHL,
    ASSIGN_SHR,
    ASSIGN_MOD,
    ASSIGN_AND,
    ASSIGN_OR,
    ASSIGN_XOR,
} ast_assignment_op;

typedef enum ast_unary_op {
    UNARY_OP_NEGATE,
    UNARY_OP_INCREMENT,
    UNARY_OP_DECREMENT,
    UNARY_OP_NOT,
    UNARY_OP_LOGIC_NOT,
    UNARY_OP_ADDRESS,
    UNARY_OP_DEREFRENCE,
} ast_unary_op;

typedef enum ast_node_type {
    AST_PROGRAM,
    AST_DECL,
    AST_FUNC_DEF,
    AST_STATEMENT,
    AST_FUNC_CALL,
    
    AST_TERNARY_EXPR,
    AST_BINARY_EXPR,
    AST_UNARY_EXPR,
    AST_ASSIGN_EXPR,

    AST_CAST_EXPR,

    AST_INDEX,
    AST_MEMBER,

    AST_LITERAL_INT,
    AST_LITERAL_STRING,
    AST_IDENTIFIER,
} ast_node_type;

typedef struct ast_decl_t ast_decl_t;
typedef struct ast_node_t ast_node_t;

typedef enum storage_class_specifier {
    STORAGE_SPEC_NONE,
    STORAGE_SPEC_AUTO,
    STORAGE_SPEC_EXTERN,
    STORAGE_SPEC_REGISTER,
    STORAGE_SPEC_STATIC,
    STORAGE_SPEC_TYPEDEF,
} storage_class_specifier;

typedef enum type_qualifier {
    TYPE_QUALIFIER_NONE = 0,
    TYPE_QUALIFIER_CONST = (1 << 0),
    TYPE_QUALIFIER_RESTRICT = (1 << 1),
    TYPE_QUALIFIER_VOLATILE = (1 << 2),
    TYPE_QUALIFIER_COUNT = 3,
} type_qualifier;

typedef enum function_specifier {
    FUNCTION_SPECIFIER_NONE = 0,
    FUNCTION_SPECIFIER_INLINE = (1 << 0),
    FUNCTION_SPECIFIER_COUNT = 1,
} function_specifier;

typedef enum type_specifier_kind {
    SPECIFIER_BUILTIN,
    SPECIFIER_STRUCT_UNION,
    SPECIFIER_ENUM,
    SPECIFIER_TYPEDEF_NAME,
} type_specifier_kind;

typedef enum builtin_base {
    BUILTIN_NONE,
    BUILTIN_VOID,
    BUILTIN_CHAR,
    BUILTIN_INT,
    BUILTIN_FLOAT,
    BUILTIN_DOUBLE,
    BUILTIN_BOOL,
} builtin_base;

typedef enum builtin_sign {
    SIGN_NONE,
    SIGN_SIGNED,
    SIGN_UNSIGNED,
} builtin_sign;

typedef enum builtin_width {
    WIDTH_DEFAULT,
    WIDTH_SHORT,
    WIDTH_LONG,
    WIDTH_LONGLONG,
} builtin_width;

typedef struct builtin_type_t {
    builtin_base base;
    builtin_sign sign;
    builtin_width width;
} builtin_type_t;

typedef struct ast_enum_item_t {
    u8 *name;
    ast_node_t *value;
} ast_enum_item_t;

typedef struct type_specifier_t {
    type_specifier_kind kind;

    union {
        struct { builtin_type_t type; } builtin;

        struct {
            u8 *name;
            ast_decl_t *fields;
            bool isUnion;
        } struct_union;

        struct {
            u8 *name;
            ast_enum_item_t *enumerators;
        } enum_type;

        struct {
            u8 *name;
        } typedef_type;
    };
} type_specifier_t;

typedef struct decl_specifiers_t {
    storage_class_specifier storageClass;
    u32 typeQualifier;
    u32 functionSpecifier;
    type_specifier_t *typeSpecifier;
} decl_specifiers_t;

typedef enum declarator_kind {
    DECL_IDENTIFIER,
    DECL_POINTER,
    DECL_ARRAY,
    DECL_FUNCTION,
} declarator_kind;

typedef struct ast_declarator_t ast_declarator_t;

typedef struct ast_parameter_t {
    decl_specifiers_t specifiers;
    ast_declarator_t *declarator;
} ast_parameter_t;

typedef struct ast_declarator_t {
    declarator_kind kind;

    union {
        struct { u8 *name; } identifier;
        
        struct {
            struct ast_declarator_t *inner;
            u32 qualifiers;
        } pointer;

        struct {
            struct ast_declarator_t *inner;
            ast_node_t *size;
        } array;

        struct {
            struct ast_declarator_t *inner;
            ast_parameter_t **parameters;
        } function;
    };
} ast_declarator_t;

typedef enum designator_kind {
    DESIGNATOR_INDEX,
    DESIGNATOR_FIELD,
} designator_kind;

typedef struct ast_designator_t {
    designator_kind kind;

    union {
        ast_node_t *index;
        u8 *field;
    };
} ast_designator_t;

typedef struct ast_initializer_t ast_initializer_t;

typedef struct ast_initializer_list_t {
    ast_initializer_t *initializer;
    ast_designator_t **designation;
} ast_initializer_list_t;

typedef enum initializer_kind {
    INITIALIZER_EXPR,
    INITIALIZER_LIST,
} initializer_kind;

typedef struct ast_initializer_t {
    initializer_kind kind;

    union {
        ast_node_t *expr;
        ast_initializer_list_t **list;
    };
} ast_initializer_t;

typedef struct ast_init_declarator_t {
    ast_declarator_t *declarator;
    ast_initializer_t *initializer;
} ast_init_declarator_t;

typedef struct ast_decl_t {
    decl_specifiers_t specifiers;
    ast_init_declarator_t **initDeclList;
} ast_decl_t;

typedef struct ast_statement_t ast_statement_t;

typedef enum statement_kind {
    STATEMENT_LABELED,
    STATEMENT_COMPOUND,
    STATEMENT_EXPRESSION,
    STATEMENT_SELECTION,
    STATEMENT_ITERATION,
    STATEMENT_JUMP,
} statement_kind;

typedef enum labeled_statement_kind {
    LABELED_STATEMEN_IDENTIFIER,
    LABELED_STATEMENT_CASE,
    LABELED_STATEMENT_DEFAULT,
} labeled_statement_kind;

typedef struct ast_labeled_statement_t {
    labeled_statement_kind kind;
    ast_node_t *inner;

    union {
        struct {
            u8 *ident;
        } identifier;

        struct {
            ast_node_t *label;
        } label_case;
    };
} ast_labeled_statement_t;

typedef enum selection_statement_kind {
    SELECTION_STATEMENT_IF,
    SELECTION_STATEMENT_SWITCH,
} selection_statement_kind;

typedef struct ast_selection_statement_t {
    selection_statement_kind kind;

    union {
        struct {
            ast_node_t *condition;
            ast_node_t *ifStatement;
            ast_node_t *elseStatement;
        } if_statement;

        struct {
            ast_node_t *condition;
            ast_node_t *statement;
        } switch_statement;
    };
} ast_selection_statement_t;

typedef enum iteration_statement_kind {
    ITERATION_STATEMENT_WHILE,
    ITERATION_STATEMENT_FOR,
} iteration_statement_kind;

typedef struct ast_iteration_statement_t {
    iteration_statement_kind kind;

    union {
        struct {
            ast_node_t *condition;
            ast_node_t *statement;
            bool hasDo;
        } while_statement;

        struct {
            union {
                struct {
                    ast_node_t *initExpr;
                    ast_node_t *conditionExpr;
                    ast_node_t *updationExpr;
                };

                ast_node_t *expressions[3];
            };

            ast_node_t *statement;
        } for_statement;
    };
} ast_iteration_statement_t;

typedef enum jump_statement_kind {
    JUMP_STATEMENT_GOTO,
    JUMP_STATEMENT_CONTINUE,
    JUMP_STATEMENT_BREAK,
    JUMP_STATEMENT_RETURN,
} jump_statement_kind;

typedef struct ast_jump_statement_t {
    jump_statement_kind kind;

    union {
        struct { u8 *identifier; } goto_statement;
        struct { ast_node_t *expr; } return_statement;
    };
} ast_jump_statement_t;

typedef struct ast_statement_t {
    statement_kind kind;

    union {
        ast_labeled_statement_t labeled;

        struct {
            ast_node_t **declarations;
            ast_node_t **statements;
        } compound;

        struct {
            ast_node_t *expression;
        } expression;

        ast_selection_statement_t selection;

        ast_iteration_statement_t iteration;

        ast_jump_statement_t jump;
    };
} ast_statement_t;

typedef struct ast_node_t {
    ast_node_type type;

    union {
        struct {
            ast_node_t **units;
        } program;

        struct {
            decl_specifiers_t *specs;
            ast_declarator_t *declarator;
            ast_node_t *statement;
        } func_def;

        ast_decl_t decl;
        ast_statement_t statement;

        struct {
            ast_node_t *fun;
            ast_node_t **params;
        } func_call;

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
            ast_node_t *expr;
            type_specifier_t *type;
            u32 qualifiers;
            ast_declarator_t *declarator;
        } cast_expr;

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
            bool isPointer;
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
void PrintAst(ast_node_t *parent, int depth);

static bool PunctuationToAssignment(token_punctuation_type type, ast_assignment_op *op) {
    switch (type) {
        case PUNCTUATION_EQUALS: *op = ASSIGN; return true;
        case PUNCTUATION_PLUS_EQUALS: *op = ASSIGN_ADD; return true;
        case PUNCTUATION_MINUS_EQUALS: *op = ASSIGN_SUB; return true;
        case PUNCTUATION_MULT_EQUALS: *op = ASSIGN_MUL; return true;
        case PUNCTUATION_DIV_EQUALS: *op = ASSIGN_DIV; return true;
        case PUNCTUATION_SHL_EQUALS: *op = ASSIGN_SHL; return true;
        case PUNCTUATION_SHR_EQUALS: *op = ASSIGN_SHR; return true;
        case PUNCTUATION_MOD_EQUALS: *op = ASSIGN_MOD; return true;
        case PUNCTUATION_AND_EQUALS: *op = ASSIGN_AND; return true;
        case PUNCTUATION_OR_EQUALS: *op = ASSIGN_OR; return true;
        case PUNCTUATION_XOR_EQUALS: *op = ASSIGN_XOR; return true;

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
        case PUNCTUATION_STAR: *op = BINARY_OP_MULT; return true;
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
        case PUNCTUATION_STAR: *op = UNARY_OP_DEREFRENCE; return true;
        case PUNCTUATION_AND: *op = UNARY_OP_ADDRESS; return true;
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
        case UNARY_OP_DEREFRENCE: return "*";
        case UNARY_OP_ADDRESS: return "&";

        default: return "";
    }
}

static bool KeywordToStorageClass(token_keyword_type type, storage_class_specifier *spec) {
    switch (type) {
        case KEYWORD_AUTO: *spec = STORAGE_SPEC_AUTO; return true;
        case KEYWORD_EXTERN: *spec = STORAGE_SPEC_EXTERN; return true;
        case KEYWORD_REGISTER: *spec = STORAGE_SPEC_REGISTER; return true;
        case KEYWORD_STATIC: *spec = STORAGE_SPEC_STATIC; return true;
        case KEYWORD_TYPEDEF: *spec = STORAGE_SPEC_TYPEDEF; return true;

        default: return false;
    }
}

static bool KeywordToTypeQualifier(token_keyword_type type, type_qualifier *qual) {
    switch (type) {
        case KEYWORD_CONST: *qual = TYPE_QUALIFIER_CONST; return true;
        case KEYWORD_RESTRICT: *qual = TYPE_QUALIFIER_RESTRICT; return true;
        case KEYWORD_VOLATILE: *qual = TYPE_QUALIFIER_VOLATILE; return true;

        default: return false;
    }
}

static bool KeywordToFunctionSpecifier(token_keyword_type type, function_specifier *spec) {
    switch (type) {
        case KEYWORD_INLINE: *spec = FUNCTION_SPECIFIER_INLINE; return true;

        default: return false;
    }
}

static bool KeywordToBuiltinType(token_keyword_type type, builtin_type_t *builtin) {    
    switch (type) {
        case KEYWORD_VOID: builtin->base = BUILTIN_VOID; return true;
        case KEYWORD_CHAR: builtin->base = BUILTIN_CHAR; return true;
        case KEYWORD_SHORT: builtin->width = WIDTH_SHORT; return true;
        case KEYWORD_INT: builtin->base = BUILTIN_INT; return true;
        case KEYWORD_LONG: builtin->width = WIDTH_LONG; return true;
        case KEYWORD_FLOAT: builtin->base = BUILTIN_FLOAT; return true;
        case KEYWORD_DOUBLE: builtin->base = BUILTIN_DOUBLE; return true;

        case KEYWORD_SIGNED: builtin->sign = SIGN_SIGNED; return true;
        case KEYWORD_UNSIGNED: builtin->sign = SIGN_UNSIGNED; return true;

        default: return false;
    }
}

static u8 *StorageClassToStr(storage_class_specifier spec) {
    switch (spec) {
        case STORAGE_SPEC_AUTO: return "auto";
        case STORAGE_SPEC_EXTERN: return "extern";
        case STORAGE_SPEC_REGISTER: return "register";
        case STORAGE_SPEC_STATIC: return "static";
        case STORAGE_SPEC_TYPEDEF: return "typedef";

        default: return "";
    }
}

static u8 *TypeQualifierToStr(type_qualifier qual) {
    switch (qual) {
        case TYPE_QUALIFIER_CONST: return "const";
        case TYPE_QUALIFIER_RESTRICT: return "restrict";
        case TYPE_QUALIFIER_VOLATILE: return "volatile";

        default: return "";
    }
}

static u8 *FunctionSpecifierToStr(function_specifier spec) {
    switch (spec) {
        case FUNCTION_SPECIFIER_INLINE: return "inline";

        default: return "";
    }
}

static u8 *SignToStr(builtin_sign sign) {
    switch (sign) {
        case SIGN_SIGNED: return "signed";
        case SIGN_UNSIGNED: return "unsigned";

        default: return "";
    }
}

static u8 *WidthToStr(builtin_width width) {
    switch (width) {
        case WIDTH_SHORT: return "short";
        case WIDTH_LONG: return "long";
        case WIDTH_LONGLONG: return "long long";

        default: return "";
    }
}

static u8 *BaseToStr(builtin_base base) {
    switch (base) {
        case BUILTIN_VOID: return "void";
        case BUILTIN_CHAR: return "char";
        case BUILTIN_INT: return "int";
        case BUILTIN_FLOAT: return "float";
        case BUILTIN_DOUBLE: return "double";
        case BUILTIN_BOOL: return "bool";

        default: return "";
    }
}

static u8 *AssignToStr(ast_assignment_op op) {
    switch (op) {
        case ASSIGN: return "=";
        case ASSIGN_ADD: return "+=";
        case ASSIGN_SUB: return "-=";
        case ASSIGN_MUL: return "*=";
        case ASSIGN_DIV: return "/=";
        case ASSIGN_SHL: return "<<=";
        case ASSIGN_SHR: return ">>=";
        case ASSIGN_MOD: return "%=";
        case ASSIGN_AND: return "&=";
        case ASSIGN_OR: return "|=";
        case ASSIGN_XOR: return "^=";

        default: return "";
    }
}

#endif