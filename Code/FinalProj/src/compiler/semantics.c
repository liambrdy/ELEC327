#include "semantics.h"

#include <stdio.h>
#include <stdlib.h>

#include "arena.h"
#include "astEval.h"

static scope_t *PushScope(scope_t *parent) {
    scope_t *s = (scope_t *)malloc(sizeof(scope_t));
    s->table = CreateHashTable(sizeof(symbol_t));
    s->parent = parent;

    return s;
}

static scope_t *PopScope(scope_t *scope) {
    scope_t *s = scope->parent;
    free(scope);
    return s;
}

static bool DeclareSymbol(scope_t *scope, symbol_t *sym) {
    if (HashContains(scope->table, &sym->name)) {
        return false;
    }

    HashInsert(scope->table, &sym->name, sym);
    return true;
}

slice_t GetNameFromDeclarator(ast_declarator_t *decl) {
    ast_declarator_t *current = decl;

    while (current != NULL) {
        switch (current->kind) {
            case DECL_IDENTIFIER: return current->identifier.name;

            case DECL_POINTER: {
                current = current->pointer.inner;
            } break;

            case DECL_ARRAY: {
                current = current->array.inner;
            } break;

            case DECL_FUNCTION: {
                current = current->function.inner;
            } break;

            default: break;
        }
    }

    return (slice_t) {0};
}


static type_t *GetType(sema_context_t *ctx, decl_specifiers_t *specs, ast_declarator_t *decl) {
    type_t *ret = PushStruct(globalArena, type_t);

    switch (decl->kind) {
        case DECL_IDENTIFIER: {
            ret->qualifiers = specs->typeQualifier;
            switch (specs->typeSpecifier->kind) {
                case SPECIFIER_BUILTIN: {
                    ret->kind = TYPE_BUILTIN;
                    ret->builtin = specs->typeSpecifier->builtin.type;
                } break;

                case SPECIFIER_STRUCT_UNION: {
                    ret->kind = specs->typeSpecifier->struct_union.isUnion ? TYPE_UNION : TYPE_STRUCT;
                    ret->struct_union.name = specs->typeSpecifier->struct_union.name;
                    
                    if (specs->typeSpecifier->struct_union.fields) {
                        ret->struct_union.fields = DArrayCreate(field_t);
                        int memberCount = DArrayLength(specs->typeSpecifier->struct_union.fields);
                        for (int i = 0; i < memberCount; i++) {
                            ast_decl_t *d = &specs->typeSpecifier->struct_union.fields[i];
                            if (d->initDeclList) {
                                int declCount = DArrayLength(d->initDeclList);
                                for (int j = 0; j < declCount; j++) {
                                    field_t f = {0};
                                    f.type = GetType(ctx, &d->specifiers, d->initDeclList[j]->declarator);
                                    f.name = GetNameFromDeclarator(d->initDeclList[j]->declarator);
                                    DArrayPush(ret->struct_union.fields, f);

                                    if (d->initDeclList[j]->initializer) {
                                        printf("cannot have initializer in struct/union definition\n");
                                        return NULL;
                                    }
                                }
                            }
                        }
                    }
                } break;

                case SPECIFIER_ENUM: {
                    ret->kind = TYPE_ENUM;
                    ret->enum_type.name = specs->typeSpecifier->enum_type.name;
                } break;

                case SPECIFIER_TYPEDEF_NAME: {
                    symbol_t *typedefSym;
                    if (!HashContainsRet(ctx->scope->table, &specs->typeSpecifier->typedef_type.name, (void **)&typedefSym)) {
                        printf("typedef type does not exist: " SLICE_STR "\n", SLICE_ARGS(decl->identifier.name));
                    }

                    if (typedefSym->kind != SYMBOL_TYPEDEF) {
                        printf("typedef symbol is not declared with typedef: %d\n", typedefSym->kind);
                    }

                    ret = typedefSym->type;
                } break;

                default: break;
            }
        } break;

        case DECL_POINTER: {
            ret->kind = TYPE_POINTER;
            ret->qualifiers = decl->pointer.qualifiers;
            ret->ptr.base = GetType(ctx, specs, decl->pointer.inner);
        } break;

        case DECL_ARRAY: {
            ret->kind = DECL_ARRAY;
            ret->array.base = GetType(ctx, specs, decl->array.inner);
            ret->array.size = EvaluateConstantExpression(decl->array.size);
        } break;

        case DECL_FUNCTION: {
            ret->kind = DECL_FUNCTION;
            ret->function.returnType = GetType(ctx, specs, decl->function.inner);
            if (decl->function.parameters) {
                ret->function.paramTypes = DArrayCreate(type_t *);
                int paramLen = DArrayLength(decl->function.parameters);
                for (int i = 0; i < paramLen; i++) {
                    ast_parameter_t *param = decl->function.parameters[i];
                    type_t *p = GetType(ctx, &param->specifiers, param->declarator);
                    DArrayPush(ret->function.paramTypes, p);
                }
            }
        } break;
    }

    return ret;
}

void SemaDecl(sema_context_t *ctx, ast_node_t *decl) {
    int declCount = DArrayLength(decl->decl.initDeclList);
    for (int i = 0; i < declCount; i++) {
        symbol_t newSymbol = {};
        newSymbol.type = GetType(ctx, &decl->decl.specifiers, decl->decl.initDeclList[i]->declarator);

        if (decl->decl.specifiers.storageClass == STORAGE_SPEC_TYPEDEF) {
            switch (newSymbol.type->kind) {
                case TYPE_UNION:
                case TYPE_STRUCT:
                case TYPE_ENUM: {
                    symbol_t enumSym = {0};
                    enumSym.kind = SYMBOL_ENUM;
                    enumSym.name = decl->decl.specifiers.typeSpecifier->enum_type.name;
                    enumSym.storageSpecs = STORAGE_SPEC_NONE;
                    enumSym.type = newSymbol.type;

                    symbol_t *existingSym = NULL;
                    if (HashContainsRet(ctx->scope->table, &enumSym.name, (void **)&existingSym) && !existingSym->emptyDecl) {
                        printf("enum has already been defined: " SLICE_STR "\n", SLICE_ARGS(enumSym.name));
                        break;
                    }

                    if (decl->decl.specifiers.typeSpecifier->enum_type.enumerators) {
                        int enumLen = DArrayLength(decl->decl.specifiers.typeSpecifier->enum_type.enumerators);
                        i32 currentVal = 0;

                        for (int j = 0; j < enumLen; j++) {
                            ast_enum_item_t *e = &decl->decl.specifiers.typeSpecifier->enum_type.enumerators[j];
                            
                            symbol_t enumConstSym = {0};
                            enumConstSym.kind = SYMBOL_ENUM_CONSTS;
                            enumConstSym.type = newSymbol.type;
                            enumConstSym.storageSpecs = STORAGE_SPEC_NONE;
                            enumConstSym.name = e->name;
                            
                            if (e->value) {
                                currentVal = EvaluateConstantExpression(e->value);
                            }

                            enumConstSym.enumConstantValue = currentVal++;

                            if (!DeclareSymbol(ctx->scope, &enumConstSym)) {
                                printf("duplicate enum constant: " SLICE_STR, SLICE_ARGS(e->name));
                            }                         }
                    }
                } break;

                default: break;
            }
        }

        newSymbol.name = GetNameFromDeclarator(decl->decl.initDeclList[i]->declarator);
        newSymbol.storageSpecs = decl->decl.specifiers.storageClass;
        
        if (decl->decl.specifiers.storageClass == STORAGE_SPEC_TYPEDEF) {
            newSymbol.kind = SYMBOL_TYPEDEF;
            newSymbol.name = decl->decl.initDeclList[i]->declarator->identifier.name;
            if (!DeclareSymbol(ctx->scope, &newSymbol)) {
                printf("duplicate symbol: " SLICE_STR, SLICE_ARGS(newSymbol.name));
            }
        }
    }
}

void SemaFunc(sema_context_t *ctx, ast_node_t *def) {

}

void AnnotateAst(ast_node_t *node) {
    if (node->type != AST_PROGRAM) {
        printf("Must pass AST_PROGRAM to semantic analysis\n");
        return;
    }

    sema_context_t ctx = {0};

    ctx.scope = PushScope(NULL);

    u32 externCount = DArrayLength(node->program.units);
    for (int i = 0; i < externCount; i++) {
        ast_node_t *n = node->program.units[i];

        if (n->type == AST_DECL) {
            SemaDecl(&ctx, n);
        } else if (n->type == AST_FUNC_DEF) {
            SemaFunc(&ctx, n);
        }
    }

    ctx.scope = PopScope(ctx.scope);
}