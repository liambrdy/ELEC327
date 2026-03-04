#include "semantics.h"

#include <stdio.h>
#include <stdlib.h>

#include "arena.h"

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

void SemaDecl(sema_context_t *ctx, ast_node_t *decl) {
    symbol_t *newSymbol = PushStruct(globalArena, symbol_t);
    
    if (!DeclareSymbol(ctx->scope, newSymbol)) {
        printf("duplicate symbol: " SLICE_STR, SLICE_ARGS(newSymbol->name));
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