#include "semantics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "astEval.h"

// ---- Forward declarations ----

static type_t *GetBaseType(sema_context_t *ctx, decl_specifiers_t *specs);
static type_t *GetType(sema_context_t *ctx, decl_specifiers_t *specs, ast_declarator_t *decl);
static type_t *SemaExpr(sema_context_t *ctx, ast_node_t *node);
static void    SemaStatement(sema_context_t *ctx, ast_node_t *stmt);
static void    SemaDecl(sema_context_t *ctx, ast_node_t *node);
static void    SemaInitializer(sema_context_t *ctx, ast_initializer_t *init);

// ---- Scope management ----

static scope_t *PushScope(scope_t *parent) {
    scope_t *s = (scope_t *)malloc(sizeof(scope_t));
    s->table       = CreateHashTable(sizeof(symbol_t));
    s->struct_tags = CreateHashTable(sizeof(symbol_t));
    s->enum_tags   = CreateHashTable(sizeof(symbol_t));
    s->parent      = parent;
    return s;
}

static scope_t *PopScope(scope_t *scope) {
    scope_t *parent = scope->parent;
    free(scope);
    return parent;
}

// Insert into ordinary-identifier namespace; returns false on duplicate.
static bool DeclareSymbol(scope_t *scope, symbol_t *sym) {
    if (HashContains(scope->table, &sym->name))
        return false;
    HashInsert(scope->table, &sym->name, sym);
    return true;
}

// Search ordinary-identifier namespace up the scope chain.
static symbol_t *LookupSymbol(scope_t *scope, slice_t *name) {
    while (scope) {
        void *sym = NULL;
        if (HashContainsRet(scope->table, name, &sym))
            return (symbol_t *)sym;
        scope = scope->parent;
    }
    return NULL;
}

// Search struct/union tag namespace up the scope chain.
static symbol_t *LookupStructTag(scope_t *scope, slice_t *name) {
    while (scope) {
        void *sym = NULL;
        if (HashContainsRet(scope->struct_tags, name, &sym))
            return (symbol_t *)sym;
        scope = scope->parent;
    }
    return NULL;
}

// Search enum tag namespace up the scope chain.
static symbol_t *LookupEnumTag(scope_t *scope, slice_t *name) {
    while (scope) {
        void *sym = NULL;
        if (HashContainsRet(scope->enum_tags, name, &sym))
            return (symbol_t *)sym;
        scope = scope->parent;
    }
    return NULL;
}

// ---- GetNameFromDeclarator (identical to the original) ----

slice_t GetNameFromDeclarator(ast_declarator_t *decl) {
    ast_declarator_t *cur = decl;
    while (cur) {
        switch (cur->kind) {
            case DECL_IDENTIFIER: return cur->identifier.name;
            case DECL_POINTER:    cur = cur->pointer.inner;  break;
            case DECL_ARRAY:      cur = cur->array.inner;    break;
            case DECL_FUNCTION:   cur = cur->function.inner; break;
            default:              cur = NULL;                 break;
        }
    }
    return (slice_t){0};
}

// Walk a declarator chain to find the outermost DECL_FUNCTION node (for
// extracting the parameter list when processing a function definition).
static ast_declarator_t *FindFunctionDecl(ast_declarator_t *decl) {
    while (decl) {
        if (decl->kind == DECL_FUNCTION) return decl;
        switch (decl->kind) {
            case DECL_POINTER: decl = decl->pointer.inner; break;
            case DECL_ARRAY:   decl = decl->array.inner;   break;
            default:           return NULL;
        }
    }
    return NULL;
}

// ---- Type helpers ----

u32 TypeSize(type_t *type) {
    if (!type) return 0;
    switch (type->kind) {
        case TYPE_BUILTIN: {
            switch (type->builtin.base) {
                case BUILTIN_VOID:   return 0;
                case BUILTIN_CHAR:   return 1;
                case BUILTIN_FLOAT:  return 4;
                case BUILTIN_DOUBLE:
                    return (type->builtin.width == WIDTH_LONG) ? 16 : 8;
                case BUILTIN_INT:
                    switch (type->builtin.width) {
                        case WIDTH_SHORT:    return 2;
                        case WIDTH_LONG:     return 4;
                        case WIDTH_LONGLONG: return 8;
                        default:            return 4;
                    }
                default: return 4;
            }
        }
        case TYPE_POINTER:  return 4; // 32-bit VM
        case TYPE_ARRAY:
            return type->array.hasSize
                 ? type->array.size * TypeSize(type->array.base)
                 : 0;
        case TYPE_FUNCTION: return 0;
        case TYPE_STRUCT: {
            u32 size = 0;
            if (type->struct_union.fields) {
                u32 n = DArrayLength(type->struct_union.fields);
                for (u32 i = 0; i < n; i++)
                    size += TypeSize(type->struct_union.fields[i].type);
            }
            return size;
        }
        case TYPE_UNION: {
            u32 max = 0;
            if (type->struct_union.fields) {
                u32 n = DArrayLength(type->struct_union.fields);
                for (u32 i = 0; i < n; i++) {
                    u32 fs = TypeSize(type->struct_union.fields[i].type);
                    if (fs > max) max = fs;
                }
            }
            return max;
        }
        case TYPE_ENUM: return 4;
        default:        return 4;
    }
}

static type_t *MakeBuiltin(builtin_base base, builtin_sign sign, builtin_width width) {
    type_t *t = PushStruct(globalArena, type_t);
    memset(t, 0, sizeof(type_t));
    t->kind         = TYPE_BUILTIN;
    t->builtin.base = base;
    t->builtin.sign = sign;
    t->builtin.width = width;
    return t;
}

static type_t *MakePointer(type_t *base) {
    type_t *t = PushStruct(globalArena, type_t);
    memset(t, 0, sizeof(type_t));
    t->kind     = TYPE_POINTER;
    t->ptr.base = base;
    return t;
}

// Return the result type for a binary arithmetic/comparison expression.
static type_t *BinaryResultType(type_t *left, type_t *right, ast_binary_op op) {
    // Relational and equality operators always produce int.
    switch (op) {
        case BINARY_OP_EQUIV:
        case BINARY_OP_NOT_EQUIV:
        case BINARY_OP_LT:
        case BINARY_OP_LTE:
        case BINARY_OP_GT:
        case BINARY_OP_GTE:
        case BINARY_OP_LOGIC_AND:
        case BINARY_OP_LOGIC_OR:
            return MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);
        default: break;
    }

    if (!left)  return right;
    if (!right) return left;

    // Pointer arithmetic: pointer ± int → pointer.
    if (left->kind  == TYPE_POINTER || left->kind  == TYPE_ARRAY) return left;
    if (right->kind == TYPE_POINTER || right->kind == TYPE_ARRAY) return right;

    // Both builtin: use whichever has the larger size (simplified promotion).
    if (left->kind == TYPE_BUILTIN && right->kind == TYPE_BUILTIN) {
        return (TypeSize(left) >= TypeSize(right)) ? left : right;
    }

    return left;
}

// ---- Type construction from AST declaration pieces ----

// Build the "base" type described by the specifiers alone (no declarator
// modifiers like pointer/array/function).  Also registers struct/enum tags
// and enum constants as a side-effect.
static type_t *GetBaseType(sema_context_t *ctx, decl_specifiers_t *specs) {
    if (!specs || !specs->typeSpecifier)
        return MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);

    type_t *ret = PushStruct(globalArena, type_t);
    memset(ret, 0, sizeof(type_t));
    ret->qualifiers = specs->typeQualifier;

    switch (specs->typeSpecifier->kind) {
        case SPECIFIER_BUILTIN: {
            ret->kind   = TYPE_BUILTIN;
            ret->builtin = specs->typeSpecifier->builtin.type;
        } break;

        case SPECIFIER_STRUCT_UNION: {
            bool isUnion = specs->typeSpecifier->struct_union.isUnion;
            slice_t tagName = specs->typeSpecifier->struct_union.name;

            // Forward reference (no body) – look up existing definition.
            if (!specs->typeSpecifier->struct_union.fields) {
                if (tagName.str) {
                    symbol_t *existing = LookupStructTag(ctx->scope, &tagName);
                    if (existing) return existing->type;
                }
                // Incomplete type placeholder.
                ret->kind              = isUnion ? TYPE_UNION : TYPE_STRUCT;
                ret->struct_union.name = tagName;
                return ret;
            }

            ret->kind              = isUnion ? TYPE_UNION : TYPE_STRUCT;
            ret->struct_union.name = tagName;
            ret->struct_union.fields = DArrayCreate(field_t);

            u32 offset = 0;
            int memberCount = DArrayLength(specs->typeSpecifier->struct_union.fields);
            for (int i = 0; i < memberCount; i++) {
                ast_decl_t *d = &specs->typeSpecifier->struct_union.fields[i];
                if (!d->initDeclList) continue;
                int declCount = DArrayLength(d->initDeclList);
                for (int j = 0; j < declCount; j++) {
                    field_t f   = {0};
                    f.type   = GetType(ctx, &d->specifiers, d->initDeclList[j]->declarator);
                    f.name   = GetNameFromDeclarator(d->initDeclList[j]->declarator);
                    f.offset = isUnion ? 0 : offset;
                    if (!isUnion) offset += TypeSize(f.type);
                    DArrayPush(ret->struct_union.fields, f);
                }
            }

            // Register this struct/union tag so forward references can find it.
            if (tagName.str) {
                symbol_t tagSym = {0};
                tagSym.kind = SYMBOL_STRUCT_UNION;
                tagSym.name = tagName;
                tagSym.type = ret;
                HashInsert(ctx->scope->struct_tags, &tagName, &tagSym);
            }
        } break;

        case SPECIFIER_ENUM: {
            slice_t tagName = specs->typeSpecifier->enum_type.name;

            // Forward reference – look up existing definition.
            if (!specs->typeSpecifier->enum_type.enumerators) {
                if (tagName.str) {
                    symbol_t *existing = LookupEnumTag(ctx->scope, &tagName);
                    if (existing) return existing->type;
                }
                ret->kind            = TYPE_ENUM;
                ret->enum_type.name  = tagName;
                return ret;
            }

            ret->kind           = TYPE_ENUM;
            ret->enum_type.name = tagName;

            // Register all enumerator constants in the ordinary-identifier namespace.
            i32 currentVal = 0;
            int enumLen = DArrayLength(specs->typeSpecifier->enum_type.enumerators);
            for (int j = 0; j < enumLen; j++) {
                ast_enum_item_t *e = &specs->typeSpecifier->enum_type.enumerators[j];
                if (e->value)
                    currentVal = (i32)EvaluateConstantExpression(e->value);

                symbol_t constSym = {0};
                constSym.kind               = SYMBOL_ENUM_CONSTS;
                constSym.name               = e->name;
                constSym.type               = ret;
                constSym.storageSpecs       = STORAGE_SPEC_NONE;
                constSym.enumConstantValue  = currentVal++;
                DeclareSymbol(ctx->scope, &constSym);
            }

            // Register the enum tag.
            if (tagName.str) {
                symbol_t tagSym = {0};
                tagSym.kind = SYMBOL_ENUM;
                tagSym.name = tagName;
                tagSym.type = ret;
                HashInsert(ctx->scope->enum_tags, &tagName, &tagSym);
            }
        } break;

        case SPECIFIER_TYPEDEF_NAME: {
            symbol_t *typedefSym = LookupSymbol(ctx->scope, &specs->typeSpecifier->typedef_type.name);
            if (!typedefSym) {
                printf("error: undeclared type '" SLICE_STR "'\n",
                       SLICE_ARGS(specs->typeSpecifier->typedef_type.name));
                return MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);
            }
            if (typedefSym->kind != SYMBOL_TYPEDEF) {
                printf("error: '" SLICE_STR "' is not a type\n",
                       SLICE_ARGS(specs->typeSpecifier->typedef_type.name));
            }
            // Apply any extra qualifiers from this use-site on top of the typedef.
            if (specs->typeQualifier) {
                type_t *copy = PushStruct(globalArena, type_t);
                *copy = *typedefSym->type;
                copy->qualifiers |= specs->typeQualifier;
                return copy;
            }
            return typedefSym->type;
        } break;

        default: break;
    }

    return ret;
}

// Build a full type by wrapping the base type through the declarator chain.
static type_t *GetType(sema_context_t *ctx, decl_specifiers_t *specs, ast_declarator_t *decl) {
    // No declarator, or a bare identifier: just return the base type.
    if (!decl || decl->kind == DECL_IDENTIFIER)
        return GetBaseType(ctx, specs);

    switch (decl->kind) {
        case DECL_POINTER: {
            type_t *ret = PushStruct(globalArena, type_t);
            memset(ret, 0, sizeof(type_t));
            ret->kind        = TYPE_POINTER;
            ret->qualifiers  = decl->pointer.qualifiers;
            ret->ptr.base    = GetType(ctx, specs, decl->pointer.inner);
            return ret;
        }

        case DECL_ARRAY: {
            type_t *ret = PushStruct(globalArena, type_t);
            memset(ret, 0, sizeof(type_t));
            ret->kind        = TYPE_ARRAY;
            ret->array.base  = GetType(ctx, specs, decl->array.inner);
            if (decl->array.size) {
                ret->array.size    = EvaluateConstantExpression(decl->array.size);
                ret->array.hasSize = true;
            }
            return ret;
        }

        case DECL_FUNCTION: {
            type_t *ret = PushStruct(globalArena, type_t);
            memset(ret, 0, sizeof(type_t));
            ret->kind                = TYPE_FUNCTION;
            ret->function.returnType = GetType(ctx, specs, decl->function.inner);
            if (decl->function.parameters) {
                ret->function.paramTypes = DArrayCreate(type_t *);
                int paramLen = DArrayLength(decl->function.parameters);
                for (int i = 0; i < paramLen; i++) {
                    ast_parameter_t *param = decl->function.parameters[i];
                    type_t *pt = GetType(ctx, &param->specifiers, param->declarator);
                    DArrayPush(ret->function.paramTypes, pt);
                }
            }
            return ret;
        }

        default:
            return GetBaseType(ctx, specs);
    }
}

// ---- Initializer annotation ----

static void SemaInitializer(sema_context_t *ctx, ast_initializer_t *init) {
    if (!init) return;
    if (init->kind == INITIALIZER_EXPR) {
        if (init->expr) SemaExpr(ctx, init->expr);
    } else {
        int n = DArrayLength(init->list);
        for (int i = 0; i < n; i++)
            SemaInitializer(ctx, init->list[i]->initializer);
    }
}

// ---- Declaration annotation ----

static void SemaDecl(sema_context_t *ctx, ast_node_t *node) {
    ast_decl_t *decl = &node->decl;
    bool isTypedef = (decl->specifiers.storageClass == STORAGE_SPEC_TYPEDEF);

    // Process base type once (registers struct/enum tags and enum constants).
    type_t *baseType = GetBaseType(ctx, &decl->specifiers);
    (void)baseType;

    if (!decl->initDeclList || DArrayLength(decl->initDeclList) == 0)
        return;

    int count = DArrayLength(decl->initDeclList);
    for (int i = 0; i < count; i++) {
        ast_init_declarator_t *initDecl = decl->initDeclList[i];
        if (!initDecl->declarator) continue;

        type_t   *varType = GetType(ctx, &decl->specifiers, initDecl->declarator);
        slice_t   name    = GetNameFromDeclarator(initDecl->declarator);
        if (!name.str) continue;

        symbol_t sym = {0};
        sym.name         = name;
        sym.type         = varType;
        sym.storageSpecs = decl->specifiers.storageClass;

        if (isTypedef) {
            sym.kind = SYMBOL_TYPEDEF;
        } else if (varType && varType->kind == TYPE_FUNCTION) {
            sym.kind = SYMBOL_FUNC;
        } else {
            sym.kind = SYMBOL_VAR;
        }

        if (!DeclareSymbol(ctx->scope, &sym)) {
            printf("error: duplicate symbol '" SLICE_STR "'\n", SLICE_ARGS(name));
        }

        if (initDecl->initializer)
            SemaInitializer(ctx, initDecl->initializer);
    }
}

// ---- Expression annotation ----

static type_t *SemaExpr(sema_context_t *ctx, ast_node_t *node) {
    if (!node) return NULL;

    type_t *result = NULL;

    switch (node->type) {
        case AST_LITERAL_INT: {
            result = MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);
        } break;

        case AST_LITERAL_STRING: {
            result = MakePointer(MakeBuiltin(BUILTIN_CHAR, SIGN_NONE, WIDTH_DEFAULT));
        } break;

        case AST_IDENTIFIER: {
            symbol_t *sym = LookupSymbol(ctx->scope, &node->identifier.name);
            if (sym) {
                node->symbol = sym;
                result = sym->type;
            } else {
                // Undeclared (e.g. extern printf) – fall back to int.
                result = MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);
            }
        } break;

        case AST_BINARY_EXPR: {
            type_t *lt = SemaExpr(ctx, node->binary_op.left);
            type_t *rt = SemaExpr(ctx, node->binary_op.right);
            result = BinaryResultType(lt, rt, node->binary_op.op);
        } break;

        case AST_UNARY_EXPR: {
            type_t *et = SemaExpr(ctx, node->unary_op.expr);
            switch (node->unary_op.op) {
                case UNARY_OP_ADDRESS:
                    result = MakePointer(et);
                    break;
                case UNARY_OP_DEREFRENCE:
                    if (et && et->kind == TYPE_POINTER) result = et->ptr.base;
                    else if (et && et->kind == TYPE_ARRAY) result = et->array.base;
                    else result = et;
                    break;
                default:
                    result = et; // negate, ~, !, ++, -- preserve the operand type
                    break;
            }
        } break;

        case AST_ASSIGN_EXPR: {
            type_t *lt = SemaExpr(ctx, node->assign_op.left);
            SemaExpr(ctx, node->assign_op.right);
            result = lt; // assignment expression has the type of the left operand
        } break;

        case AST_TERNARY_EXPR: {
            SemaExpr(ctx, node->ternary_expr.condition);
            type_t *tt = SemaExpr(ctx, node->ternary_expr.then_expr);
            SemaExpr(ctx, node->ternary_expr.else_expr);
            result = tt;
        } break;

        case AST_CAST_EXPR: {
            SemaExpr(ctx, node->cast_expr.expr);
            decl_specifiers_t castSpecs = {
                .typeQualifier = node->cast_expr.qualifiers,
                .typeSpecifier = node->cast_expr.type,
            };
            result = GetType(ctx, &castSpecs, node->cast_expr.declarator);
        } break;

        case AST_FUNC_CALL: {
            type_t *funType = SemaExpr(ctx, node->func_call.fun);

            // Unwrap function pointer: (*fp)() has funType == pointer-to-function.
            if (funType && funType->kind == TYPE_POINTER &&
                funType->ptr.base && funType->ptr.base->kind == TYPE_FUNCTION)
                funType = funType->ptr.base;

            if (node->func_call.params) {
                int n = DArrayLength(node->func_call.params);
                for (int i = 0; i < n; i++)
                    SemaExpr(ctx, node->func_call.params[i]);
            }

            result = (funType && funType->kind == TYPE_FUNCTION)
                   ? funType->function.returnType
                   : MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);
        } break;

        case AST_INDEX: {
            type_t *arrType = SemaExpr(ctx, node->index.array);
            SemaExpr(ctx, node->index.index);
            if (arrType) {
                if      (arrType->kind == TYPE_POINTER) result = arrType->ptr.base;
                else if (arrType->kind == TYPE_ARRAY)   result = arrType->array.base;
                else                                    result = arrType;
            }
        } break;

        case AST_MEMBER: {
            type_t *parentType = SemaExpr(ctx, node->member.parent);

            // For `->`  the parent is a pointer; dereference it.
            if (node->member.isPointer && parentType && parentType->kind == TYPE_POINTER)
                parentType = parentType->ptr.base;

            if (parentType &&
                (parentType->kind == TYPE_STRUCT || parentType->kind == TYPE_UNION) &&
                parentType->struct_union.fields) {
                u32 n = DArrayLength(parentType->struct_union.fields);
                for (u32 i = 0; i < n; i++) {
                    field_t *f = &parentType->struct_union.fields[i];
                    if (CompareSlices(&f->name, &node->member.member)) {
                        result = f->type;
                        break;
                    }
                }
            }

            if (!result)
                result = MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);
        } break;

        default: {
            result = MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);
        } break;
    }

    node->resolvedType = result;
    return result;
}

// ---- Statement annotation ----

static void SemaStatement(sema_context_t *ctx, ast_node_t *stmt) {
    if (!stmt) return;

    // Occasionally a node that isn't a statement appears (e.g. a bare decl in
    // a for-init).  Route it appropriately.
    if (stmt->type == AST_DECL) {
        SemaDecl(ctx, stmt);
        return;
    }
    if (stmt->type != AST_STATEMENT) return;

    switch (stmt->statement.kind) {
        case STATEMENT_COMPOUND: {
            // Each compound statement is its own scope.
            ctx->scope = PushScope(ctx->scope);

            if (stmt->statement.compound.declarations) {
                int n = DArrayLength(stmt->statement.compound.declarations);
                for (int i = 0; i < n; i++)
                    SemaDecl(ctx, stmt->statement.compound.declarations[i]);
            }

            if (stmt->statement.compound.statements) {
                int n = DArrayLength(stmt->statement.compound.statements);
                for (int i = 0; i < n; i++)
                    SemaStatement(ctx, stmt->statement.compound.statements[i]);
            }

            ctx->scope = PopScope(ctx->scope);
        } break;

        case STATEMENT_EXPRESSION: {
            SemaExpr(ctx, stmt->statement.expression.expression);
        } break;

        case STATEMENT_SELECTION: {
            if (stmt->statement.selection.kind == SELECTION_STATEMENT_IF) {
                SemaExpr(ctx, stmt->statement.selection.if_statement.condition);
                SemaStatement(ctx, stmt->statement.selection.if_statement.ifStatement);
                SemaStatement(ctx, stmt->statement.selection.if_statement.elseStatement);
            } else {
                SemaExpr(ctx, stmt->statement.selection.switch_statement.condition);
                SemaStatement(ctx, stmt->statement.selection.switch_statement.statement);
            }
        } break;

        case STATEMENT_ITERATION: {
            if (stmt->statement.iteration.kind == ITERATION_STATEMENT_WHILE) {
                SemaExpr(ctx, stmt->statement.iteration.while_statement.condition);
                SemaStatement(ctx, stmt->statement.iteration.while_statement.statement);
            } else {
                // for-loop init may declare a variable, so it needs its own scope.
                ctx->scope = PushScope(ctx->scope);

                ast_node_t *init = stmt->statement.iteration.for_statement.initExpr;
                ast_node_t *cond = stmt->statement.iteration.for_statement.conditionExpr;
                ast_node_t *upd  = stmt->statement.iteration.for_statement.updationExpr;

                if (init) {
                    if (init->type == AST_DECL) SemaDecl(ctx, init);
                    else                        SemaExpr(ctx, init);
                }
                if (cond) SemaExpr(ctx, cond);
                if (upd)  SemaExpr(ctx, upd);

                SemaStatement(ctx, stmt->statement.iteration.for_statement.statement);

                ctx->scope = PopScope(ctx->scope);
            }
        } break;

        case STATEMENT_JUMP: {
            if (stmt->statement.jump.kind == JUMP_STATEMENT_RETURN)
                SemaExpr(ctx, stmt->statement.jump.return_statement.expr);
        } break;

        case STATEMENT_LABELED: {
            if (stmt->statement.labeled.kind == LABELED_STATEMENT_CASE)
                SemaExpr(ctx, stmt->statement.labeled.label_case.label);
            SemaStatement(ctx, stmt->statement.labeled.inner);
        } break;

        default: break;
    }
}

// ---- Function definition annotation ----

static void SemaFunc(sema_context_t *ctx, ast_node_t *def) {
    // Register the function in the enclosing scope so recursive calls and
    // forward references from later functions can find it.
    slice_t name     = GetNameFromDeclarator(def->func_def.declarator);
    type_t *funcType = def->func_def.specs
                     ? GetType(ctx, def->func_def.specs, def->func_def.declarator)
                     : MakeBuiltin(BUILTIN_INT, SIGN_NONE, WIDTH_DEFAULT);

    symbol_t funcSym = {0};
    funcSym.kind         = SYMBOL_FUNC;
    funcSym.name         = name;
    funcSym.type         = funcType;
    funcSym.storageSpecs = def->func_def.specs
                         ? def->func_def.specs->storageClass
                         : STORAGE_SPEC_NONE;

    if (!DeclareSymbol(ctx->scope, &funcSym)) {
        printf("error: duplicate function '" SLICE_STR "'\n", SLICE_ARGS(name));
    }

    // Update the return-type context for return-statement checking.
    type_t *savedRet = ctx->currentRetType;
    if (funcType && funcType->kind == TYPE_FUNCTION)
        ctx->currentRetType = funcType->function.returnType;

    // Push a scope that holds the parameters.  The body's compound statement
    // will push another scope inside this one, so both params and locals are
    // visible inside the body via scope-chain lookup.
    ctx->scope = PushScope(ctx->scope);

    ast_declarator_t *funcDecl = FindFunctionDecl(def->func_def.declarator);
    if (funcDecl && funcDecl->function.parameters) {
        int n = DArrayLength(funcDecl->function.parameters);
        for (int i = 0; i < n; i++) {
            ast_parameter_t *param = funcDecl->function.parameters[i];
            if (!param->declarator) continue;

            slice_t paramName = GetNameFromDeclarator(param->declarator);
            if (!paramName.str) continue;

            type_t *paramType = GetType(ctx, &param->specifiers, param->declarator);

            symbol_t paramSym = {0};
            paramSym.kind         = SYMBOL_VAR;
            paramSym.name         = paramName;
            paramSym.type         = paramType;
            paramSym.storageSpecs = STORAGE_SPEC_NONE;
            DeclareSymbol(ctx->scope, &paramSym);
        }
    }

    SemaStatement(ctx, def->func_def.statement);

    ctx->scope       = PopScope(ctx->scope);
    ctx->currentRetType = savedRet;
}

// ---- Entry point ----

void AnnotateAst(ast_node_t *node) {
    if (node->type != AST_PROGRAM) {
        printf("AnnotateAst: expected AST_PROGRAM\n");
        return;
    }

    sema_context_t ctx = {0};
    ctx.scope = PushScope(NULL);

    u32 n = DArrayLength(node->program.units);
    for (u32 i = 0; i < n; i++) {
        ast_node_t *unit = node->program.units[i];
        if      (unit->type == AST_DECL)     SemaDecl(&ctx, unit);
        else if (unit->type == AST_FUNC_DEF)  SemaFunc(&ctx, unit);
    }

    ctx.scope = PopScope(ctx.scope);
}
