#include "rom.h"
#include "semantics.h"

#include "darray.h"
#include "str.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---- Scope management ----

static rom_scope_t *PushScope(rom_scope_t *parent) {
    rom_scope_t *s = (rom_scope_t *)malloc(sizeof(rom_scope_t));
    s->variables = CreateHashTable(sizeof(rom_variable_t));
    s->parent = parent;
    return s;
}

static rom_scope_t *PopScope(rom_scope_t *scope) {
    rom_scope_t *s = scope->parent;
    free(scope);
    return s;
}

static bool DeclareVariable(rom_scope_t *scope, rom_variable_t *var) {
    if (HashContains(scope->variables, &var->name))
        return false;
    HashInsert(scope->variables, &var->name, var);
    return true;
}

static rom_variable_t *LookupVariable(rom_scope_t *scope, slice_t *name) {
    while (scope) {
        void *var = NULL;
        if (HashContainsRet(scope->variables, name, &var))
            return (rom_variable_t *)var;
        scope = scope->parent;
    }
    return NULL;
}

// ---- Declarator helpers ----

static slice_t GetNameFromDeclarator(ast_declarator_t *decl) {
    ast_declarator_t *cur = decl;
    while (cur) {
        switch (cur->kind) {
            case DECL_IDENTIFIER: return cur->identifier.name;
            case DECL_POINTER:    cur = cur->pointer.inner;   break;
            case DECL_ARRAY:      cur = cur->array.inner;     break;
            case DECL_FUNCTION:   cur = cur->function.inner;  break;
            default:              cur = NULL;                  break;
        }
    }
    return (slice_t){0};
}

// Walk the declarator chain to find the outermost DECL_FUNCTION node.
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

u32 GetTypeSize(type_t *type) {
    return TypeSize(type);
}

static field_t *FindField(type_t *structType, slice_t *name) {
    if (!structType || !structType->struct_union.fields) return NULL;
    u32 n = DArrayLength(structType->struct_union.fields);
    for (u32 i = 0; i < n; i++) {
        if (CompareSlices(&structType->struct_union.fields[i].name, name))
            return &structType->struct_union.fields[i];
    }
    return NULL;
}

static bool IsStructType(type_t *t) {
    return t && (t->kind == TYPE_STRUCT || t->kind == TYPE_UNION);
}

// ---- Symbol table ----

static void DeclareSymbol(rom_context_t *ctx, rom_symbol_entry_t *sym) {
    DArrayPush(ctx->symbols, *sym);
    HashInsert(ctx->symbolTable, &sym->name, sym);
}

// Update the address of an already-declared function symbol (for forward refs).
static void UpdateFunctionAddress(rom_context_t *ctx, slice_t *name, u32 addr) {
    // Update hash table entry in place (HashInsert overwrites on duplicate key).
    rom_symbol_entry_t updated = {
        .name = *name,
        .address = addr,
        .kind = SYMBOL_ENTRY_FUNCTION,
    };
    HashInsert(ctx->symbolTable, name, &updated);

    // Also update the copy in the symbols DArray.
    u32 n = DArrayLength(ctx->symbols);
    for (u32 i = 0; i < n; i++) {
        if (CompareSlices(&ctx->symbols[i].name, name)) {
            ctx->symbols[i].address = addr;
            break;
        }
    }
}

// ---- Emit helpers ----

static void WriteByte(rom_context_t *ctx, u8 c) {
    DArrayPush(ctx->code, c);
    ctx->currentAddress++;
}

static void WriteBytes(rom_context_t *ctx, u32 count, u8 *data) {
    for (u32 i = 0; i < count; i++)
        WriteByte(ctx, data[i]);
}

static void WriteU16(rom_context_t *ctx, u16 num) {
    WriteBytes(ctx, 2, (u8 *)&num);
}

static void WriteU32(rom_context_t *ctx, u32 num) {
    WriteBytes(ctx, 4, (u8 *)&num);
}

// Emits [opcode][0x00000000] and returns the code-buffer byte index of the
// 4-byte placeholder, which PatchJmp can later overwrite.
static u32 EmitJmpPlaceholder(rom_context_t *ctx, opcode op) {
    WriteByte(ctx, (u8)op);
    u32 patchOffset = DArrayLength(ctx->code);
    WriteU32(ctx, 0);
    return patchOffset;
}

// Overwrites the 4-byte placeholder at patchOffset with targetAddr (little-endian).
static void PatchJmp(rom_context_t *ctx, u32 patchOffset, u32 targetAddr) {
    ctx->code[patchOffset + 0] = (u8)(targetAddr >>  0);
    ctx->code[patchOffset + 1] = (u8)(targetAddr >>  8);
    ctx->code[patchOffset + 2] = (u8)(targetAddr >> 16);
    ctx->code[patchOffset + 3] = (u8)(targetAddr >> 24);
}

// ---- Loop context helpers (for break/continue backpatching) ----

static void PushLoopCtx(rom_context_t *ctx) {
    loop_ctx_t lc = {
        .breakPatches    = DArrayCreate(u32),
        .continuePatches = DArrayCreate(u32),
    };
    DArrayPush(ctx->loopStack, lc);
}

// Patch all break-placeholder jumps to the current address, then pop.
static void PopLoopCtx(rom_context_t *ctx, u32 continueTarget) {
    u32 n = DArrayLength(ctx->loopStack);
    if (n == 0) return;
    loop_ctx_t lc;
    DArrayPop(ctx->loopStack, &lc);

    // Patch continues to the supplied target.
    u32 nc = DArrayLength(lc.continuePatches);
    for (u32 i = 0; i < nc; i++)
        PatchJmp(ctx, lc.continuePatches[i], continueTarget);

    // Patch breaks to the current address (just past the loop).
    u32 nb = DArrayLength(lc.breakPatches);
    for (u32 i = 0; i < nb; i++)
        PatchJmp(ctx, lc.breakPatches[i], ctx->currentAddress);
}

static void EmitBreak(rom_context_t *ctx) {
    u32 n = DArrayLength(ctx->loopStack);
    if (n == 0) { printf("error: break outside loop\n"); return; }
    loop_ctx_t *lc = &ctx->loopStack[n - 1];
    u32 patch = EmitJmpPlaceholder(ctx, OPCODE_JMP);
    DArrayPush(lc->breakPatches, patch);
}

static void EmitContinue(rom_context_t *ctx) {
    u32 n = DArrayLength(ctx->loopStack);
    if (n == 0) { printf("error: continue outside loop\n"); return; }
    loop_ctx_t *lc = &ctx->loopStack[n - 1];
    u32 patch = EmitJmpPlaceholder(ctx, OPCODE_JMP);
    DArrayPush(lc->continuePatches, patch);
}

// ---- Forward declarations (mutual recursion) ----

static void RomGenFromAst(rom_context_t *ctx, ast_node_t *ast);
static void RomGenDecl(rom_context_t *ctx, ast_node_t *node);
static void RomGenStatement(rom_context_t *ctx, ast_node_t *node);

// ---- LValue address helper ----
// Emits code that leaves the *address* of an lvalue on the stack.
// Used by both AST_INDEX (read) and assignment targets.
static void EmitLValueAddr(rom_context_t *ctx, ast_node_t *lval) {
    if (!lval) return;
    switch (lval->type) {
        case AST_IDENTIFIER: {
            rom_variable_t *var = LookupVariable(ctx->scope, &lval->identifier.name);
            if (!var) {
                printf("error: unknown identifier '" SLICE_STR "' in lvalue addr\n",
                       SLICE_ARGS(lval->identifier.name));
                WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 0);
                break;
            }
            // Memory-allocated vars (arrays, globals) use their constant byte address.
            // Pointer/scalar locals hold an address as their value — load it.
            bool isPointerType = var->varType && var->varType->kind == TYPE_POINTER;
            if (var->isGlobal && !isPointerType) {
                WriteByte(ctx, OPCODE_PUSH_CONST);
                WriteU32(ctx, var->location);
            } else {
                // Load pointer value from its slot (global or local).
                if (var->isGlobal) {
                    WriteByte(ctx, OPCODE_LOAD_GLOBAL); WriteU32(ctx, var->location);
                } else {
                    WriteByte(ctx, OPCODE_LOAD_LOCAL);  WriteU16(ctx, (u16)var->location);
                }
            }
        } break;

        case AST_INDEX: {
            // address = base + index * elemSize
            type_t *elemType = lval->resolvedType;
            u32 elemSize = (elemType && TypeSize(elemType) > 0) ? TypeSize(elemType) : 4;
            EmitLValueAddr(ctx, lval->index.array);          // push base address
            RomGenFromAst(ctx, lval->index.index);           // push index
            if (elemSize > 1) {
                WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, elemSize);
                WriteByte(ctx, OPCODE_MUL);
            }
            WriteByte(ctx, OPCODE_ADD);
        } break;

        case AST_MEMBER: {
            ast_node_t *parent = lval->member.parent;
            if (lval->member.isPointer) {
                // ptr->field: address = ptr_value + field_offset
                type_t *pointeeType = parent->resolvedType
                                      ? parent->resolvedType->ptr.base : NULL;
                field_t *f = FindField(pointeeType, &lval->member.member);
                u32 fieldOff = f ? f->offset : 0;
                RomGenFromAst(ctx, parent);
                if (fieldOff > 0) {
                    WriteByte(ctx, OPCODE_FIELD_OFFSET); WriteU32(ctx, fieldOff);
                }
            } else {
                // s.field: address = &s + field_offset (only for memory-allocated structs)
                type_t *structType = parent->resolvedType;
                field_t *f = FindField(structType, &lval->member.member);
                u32 fieldOff = f ? f->offset : 0;
                if (parent->type == AST_IDENTIFIER) {
                    rom_variable_t *var = LookupVariable(ctx->scope,
                                                         &parent->identifier.name);
                    if (var && var->isGlobal) {
                        WriteByte(ctx, OPCODE_PUSH_CONST);
                        WriteU32(ctx, var->location + fieldOff);
                    } else {
                        // Stack-slot struct: address not directly expressible
                        printf("error: cannot take address of stack-slot struct field\n");
                        WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 0);
                    }
                } else {
                    EmitLValueAddr(ctx, parent);
                    if (fieldOff > 0) {
                        WriteByte(ctx, OPCODE_FIELD_OFFSET); WriteU32(ctx, fieldOff);
                    }
                }
            }
        } break;

        case AST_UNARY_EXPR:
            if (lval->unary_op.op == UNARY_OP_DEREFRENCE) {
                // *ptr: address is just the pointer value
                RomGenFromAst(ctx, lval->unary_op.expr);
            } else {
                printf("error: not an lvalue\n");
                WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 0);
            }
            break;

        default:
            printf("error: unsupported lvalue for address\n");
            WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 0);
            break;
    }
}

// ---- Compile-time constant evaluation (for switch case values) ----
static bool EvalConstExpr(ast_node_t *node, u32 *out) {
    if (!node) return false;
    if (node->type == AST_LITERAL_INT) { *out = (u32)node->int_literal.literal; return true; }
    if (node->type == AST_IDENTIFIER && node->symbol &&
        node->symbol->kind == SYMBOL_ENUM_CONSTS) {
        *out = (u32)node->symbol->enumConstantValue;
        return true;
    }
    return false;
}

// ---- Load / store helpers ----

static void EmitLoad(rom_context_t *ctx, rom_variable_t *var) {
    if (var->isGlobal) {
        WriteByte(ctx, OPCODE_LOAD_GLOBAL);
        WriteU32(ctx, var->location);
    } else {
        WriteByte(ctx, OPCODE_LOAD_LOCAL);
        WriteU16(ctx, (u16)var->location);
    }
}

static void EmitStore(rom_context_t *ctx, rom_variable_t *var) {
    if (var->isGlobal) {
        WriteByte(ctx, OPCODE_STORE_GLOBAL);
        WriteU32(ctx, var->location);
    } else {
        WriteByte(ctx, OPCODE_STORE_LOCAL);
        WriteU16(ctx, (u16)var->location);
    }
}

// ---- Declaration codegen ----

static void RomGenDecl(rom_context_t *ctx, ast_node_t *node) {
    if (!node || node->type != AST_DECL) return;
    if (node->decl.specifiers.storageClass == STORAGE_SPEC_TYPEDEF) return;

    bool isGlobal = (ctx->scope->parent == NULL);
    u32 declCount = DArrayLength(node->decl.initDeclList);

    for (u32 i = 0; i < declCount; i++) {
        ast_init_declarator_t *initDecl = node->decl.initDeclList[i];
        if (!initDecl->declarator) continue;

        slice_t name = GetNameFromDeclarator(initDecl->declarator);
        if (!name.str) continue;

        // Function prototype (no body): emit a syscall stub [SYSCALL id][RET].
        if (FindFunctionDecl(initDecl->declarator)) {
            if (HashContains(ctx->symbolTable, &name)) continue; // already declared
            u8 id = (u8)(ctx->nextSyscallId++);
            rom_symbol_entry_t sym = {
                .name    = name,
                .address = ctx->currentAddress,
                .kind    = SYMBOL_ENTRY_FUNCTION,
            };
            DeclareSymbol(ctx, &sym);
            WriteByte(ctx, OPCODE_SYSCALL); WriteByte(ctx, id);
            WriteByte(ctx, OPCODE_RET);
            continue;
        }

        type_t *varType  = initDecl->resolvedType;
        bool    isStruct = IsStructType(varType);
        bool    isArray  = (varType && varType->kind == TYPE_ARRAY);
        u32     typeSize = varType ? TypeSize(varType) : 4;
        if (typeSize == 0) typeSize = 4;
        // Struct locals: ceil(typeSize/4) consecutive stack slots.
        // Array locals: allocated in memory (like globals) so indexing works.
        u32     slotCount = isStruct ? (typeSize + 3) / 4 : 1;

        rom_variable_t var;
        // Arrays (local or global) and globals always live in vm->memory so
        // that pointer arithmetic and &var work at compile time.
        bool allocInMemory = isGlobal || isStruct || isArray;
        if (allocInMemory) {
            var = (rom_variable_t){
                .name     = name,
                .size     = typeSize,
                .location = ROM_GLOBAL_OFFSET + ctx->currentGlobal,
                .isGlobal = true,
                .varType  = varType,
            };
            ctx->currentGlobal += typeSize;
        } else {
            var = (rom_variable_t){
                .name     = name,
                .size     = typeSize,
                .location = ctx->frameSlot,
                .isGlobal = false,
                .varType  = varType,
            };
            ctx->frameSlot += slotCount;
        }

        DeclareVariable(ctx->scope, &var);

        // Generate initializer code.
        if (var.isGlobal) {
            // Variable lives in vm->memory (zero-initialized by VmInit).
            if (initDecl->initializer &&
                initDecl->initializer->kind == INITIALIZER_EXPR &&
                initDecl->initializer->expr) {
                RomGenFromAst(ctx, initDecl->initializer->expr);
                EmitStore(ctx, &var);
            } else if (initDecl->initializer &&
                       initDecl->initializer->kind == INITIALIZER_LIST && isStruct) {
                // Designated struct initializer: memory is already zero; write named fields.
                u32 n = DArrayLength(initDecl->initializer->list);
                for (u32 j = 0; j < n; j++) {
                    ast_initializer_list_t *item = initDecl->initializer->list[j];
                    if (!item->initializer ||
                        item->initializer->kind != INITIALIZER_EXPR ||
                        !item->initializer->expr) continue;

                    u32 fieldByteAddr = var.location;
                    if (item->designation) {
                        u32 nd = DArrayLength(item->designation);
                        for (u32 d = 0; d < nd; d++) {
                            ast_designator_t *desig = item->designation[d];
                            if (desig->kind == DESIGNATOR_FIELD) {
                                field_t *f = FindField(varType, &desig->field);
                                if (f) fieldByteAddr = var.location + f->offset;
                            }
                        }
                    }

                    RomGenFromAst(ctx, item->initializer->expr);
                    WriteByte(ctx, OPCODE_STORE_GLOBAL);
                    WriteU32(ctx, fieldByteAddr);
                }
            } else if (initDecl->initializer &&
                       initDecl->initializer->kind == INITIALIZER_LIST && isArray) {
                // Array initializer: memory is already zero; write provided elements.
                type_t *elemType = varType ? varType->array.base : NULL;
                u32 elemSize = (elemType && TypeSize(elemType) > 0) ? TypeSize(elemType) : 4;

                u32 pos = 0;
                u32 n = DArrayLength(initDecl->initializer->list);
                for (u32 j = 0; j < n; j++) {
                    ast_initializer_list_t *item = initDecl->initializer->list[j];
                    if (!item->initializer ||
                        item->initializer->kind != INITIALIZER_EXPR ||
                        !item->initializer->expr) { pos++; continue; }

                    u32 idx = pos;
                    if (item->designation) {
                        u32 nd = DArrayLength(item->designation);
                        for (u32 d = 0; d < nd; d++) {
                            ast_designator_t *desig = item->designation[d];
                            if (desig->kind == DESIGNATOR_INDEX) {
                                u32 cv = 0;
                                EvalConstExpr(desig->index, &cv);
                                idx = cv;
                            }
                        }
                    }

                    u32 byteAddr = var.location + idx * elemSize;
                    RomGenFromAst(ctx, item->initializer->expr);
                    if (elemSize == 4) {
                        WriteByte(ctx, OPCODE_STORE_GLOBAL);
                        WriteU32(ctx, byteAddr);
                    } else {
                        WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, byteAddr);
                        WriteByte(ctx, OPCODE_SWAP);
                        WriteByte(ctx, OPCODE_STORE_INDIRECT); WriteByte(ctx, (u8)elemSize);
                    }
                    pos = idx + 1;
                }
            }
        } else if (!initDecl->initializer) {
            // No initializer: push zeros to reserve all slots on the stack.
            for (u32 j = 0; j < slotCount; j++) {
                WriteByte(ctx, OPCODE_PUSH_CONST);
                WriteU32(ctx, 0);
            }
        } else if (initDecl->initializer->kind == INITIALIZER_EXPR &&
                   initDecl->initializer->expr) {
            // Scalar initializer.
            RomGenFromAst(ctx, initDecl->initializer->expr);
            EmitStore(ctx, &var);
        }
    }
}

// ---- Statement codegen ----

static void RomGenStatement(rom_context_t *ctx, ast_node_t *node) {
    if (!node || node->type != AST_STATEMENT) return;

    switch (node->statement.kind) {
        case STATEMENT_COMPOUND: {
            ctx->scope = PushScope(ctx->scope);

            if (node->statement.compound.declarations) {
                u32 n = DArrayLength(node->statement.compound.declarations);
                for (u32 i = 0; i < n; i++)
                    RomGenDecl(ctx, node->statement.compound.declarations[i]);
            }

            if (node->statement.compound.statements) {
                u32 n = DArrayLength(node->statement.compound.statements);
                for (u32 i = 0; i < n; i++)
                    RomGenFromAst(ctx, node->statement.compound.statements[i]);
            }

            ctx->scope = PopScope(ctx->scope);
        } break;

        case STATEMENT_EXPRESSION: {
            if (node->statement.expression.expression) {
                RomGenFromAst(ctx, node->statement.expression.expression);
                WriteByte(ctx, OPCODE_POP); // discard expression result
            }
        } break;

        case STATEMENT_SELECTION: {
            if (node->statement.selection.kind == SELECTION_STATEMENT_IF) {
                RomGenFromAst(ctx, node->statement.selection.if_statement.condition);
                u32 toElse = EmitJmpPlaceholder(ctx, OPCODE_JMP_IF_NOT);

                RomGenFromAst(ctx, node->statement.selection.if_statement.ifStatement);

                if (node->statement.selection.if_statement.elseStatement) {
                    u32 toEnd = EmitJmpPlaceholder(ctx, OPCODE_JMP);
                    PatchJmp(ctx, toElse, ctx->currentAddress);
                    RomGenFromAst(ctx, node->statement.selection.if_statement.elseStatement);
                    PatchJmp(ctx, toEnd, ctx->currentAddress);
                } else {
                    PatchJmp(ctx, toElse, ctx->currentAddress);
                }
            } else if (node->statement.selection.kind == SELECTION_STATEMENT_SWITCH) {
                // Linear-scan dispatch with per-case trampolines.
                // Trampoline POPs the switch value (consumed by dispatch), then JMPs to body.
                // Fall-through paths skip the trampoline, so no double-POP.
                ast_node_t *cond = node->statement.selection.switch_statement.condition;
                ast_node_t *body = node->statement.selection.switch_statement.statement;

                // Collect cases from the body (top-level labeled statements only).
                ast_node_t **stmts = NULL;
                u32 nStmts = 0;
                if (body->type == AST_STATEMENT &&
                    body->statement.kind == STATEMENT_COMPOUND) {
                    stmts  = body->statement.compound.statements;
                    nStmts = stmts ? (u32)DArrayLength(stmts) : 0;
                }

                // Count non-default cases for dispatch table sizing.
                u32 nCases = 0;
                for (u32 ci = 0; ci < nStmts; ci++) {
                    ast_node_t *s = stmts[ci];
                    if (s->type == AST_STATEMENT &&
                        s->statement.kind == STATEMENT_LABELED &&
                        s->statement.labeled.kind == LABELED_STATEMENT_CASE)
                        nCases++;
                }

                // Allocate per-case tracking arrays on the stack.
                u32 *caseVals     = (u32 *)malloc(nCases * sizeof(u32));
                u32 *dispatchPatch = (u32 *)malloc(nCases * sizeof(u32));
                u32 *trampolinePatch = (u32 *)malloc(nCases * sizeof(u32));

                // 1. Evaluate switch expression.
                RomGenFromAst(ctx, cond);

                // 2. Dispatch table: DUP; PUSH_CONST val; EQ; JMP_IF trampoline_N
                u32 caseIdx = 0;
                for (u32 ci = 0; ci < nStmts; ci++) {
                    ast_node_t *s = stmts[ci];
                    if (s->type != AST_STATEMENT ||
                        s->statement.kind != STATEMENT_LABELED ||
                        s->statement.labeled.kind != LABELED_STATEMENT_CASE) continue;
                    u32 cval = 0;
                    EvalConstExpr(s->statement.labeled.label_case.label, &cval);
                    caseVals[caseIdx] = cval;
                    WriteByte(ctx, OPCODE_DUP);
                    WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, cval);
                    WriteByte(ctx, OPCODE_EQ);
                    dispatchPatch[caseIdx] = EmitJmpPlaceholder(ctx, OPCODE_JMP_IF);
                    caseIdx++;
                }
                u32 noMatchPatch = EmitJmpPlaceholder(ctx, OPCODE_JMP);

                // 3. Trampolines: POP then JMP to case body.
                for (u32 i = 0; i < nCases; i++) {
                    PatchJmp(ctx, dispatchPatch[i], ctx->currentAddress);
                    WriteByte(ctx, OPCODE_POP); // discard switch expr
                    trampolinePatch[i] = EmitJmpPlaceholder(ctx, OPCODE_JMP);
                }

                // 4. No-match: POP expr; jump to default body (or end).
                PatchJmp(ctx, noMatchPatch, ctx->currentAddress);
                WriteByte(ctx, OPCODE_POP);
                u32 defaultBodyPatch = EmitJmpPlaceholder(ctx, OPCODE_JMP);
                bool defaultPatched = false;

                // 5. Emit body; patch case trampolines when we see case labels.
                PushLoopCtx(ctx);
                caseIdx = 0;
                for (u32 ci = 0; ci < nStmts; ci++) {
                    ast_node_t *s = stmts[ci];
                    if (s->type == AST_STATEMENT &&
                        s->statement.kind == STATEMENT_LABELED) {
                        if (s->statement.labeled.kind == LABELED_STATEMENT_CASE) {
                            u32 cval = 0;
                            EvalConstExpr(s->statement.labeled.label_case.label, &cval);
                            // Find matching trampoline.
                            for (u32 i = 0; i < nCases; i++) {
                                if (caseVals[i] == cval) {
                                    PatchJmp(ctx, trampolinePatch[i], ctx->currentAddress);
                                    break;
                                }
                            }
                            caseIdx++;
                            if (s->statement.labeled.inner)
                                RomGenFromAst(ctx, s->statement.labeled.inner);
                        } else if (s->statement.labeled.kind == LABELED_STATEMENT_DEFAULT) {
                            PatchJmp(ctx, defaultBodyPatch, ctx->currentAddress);
                            defaultPatched = true;
                            if (s->statement.labeled.inner)
                                RomGenFromAst(ctx, s->statement.labeled.inner);
                        } else {
                            RomGenFromAst(ctx, s);
                        }
                    } else {
                        RomGenFromAst(ctx, s);
                    }
                }

                // 6. End of switch: patch default and breaks.
                if (!defaultPatched)
                    PatchJmp(ctx, defaultBodyPatch, ctx->currentAddress);
                PopLoopCtx(ctx, ctx->currentAddress); // no continue in switch

                free(caseVals);
                free(dispatchPatch);
                free(trampolinePatch);
            }
        } break;

        case STATEMENT_ITERATION: {
            if (node->statement.iteration.kind == ITERATION_STATEMENT_WHILE) {
                bool hasDo = node->statement.iteration.while_statement.hasDo;
                if (!hasDo) {
                    // while (cond) body
                    // continue → jump back to loopTop (re-evaluate condition)
                    PushLoopCtx(ctx);
                    u32 loopTop = ctx->currentAddress;
                    RomGenFromAst(ctx, node->statement.iteration.while_statement.condition);
                    u32 toEnd = EmitJmpPlaceholder(ctx, OPCODE_JMP_IF_NOT);
                    RomGenFromAst(ctx, node->statement.iteration.while_statement.statement);
                    WriteByte(ctx, OPCODE_JMP);
                    WriteU32(ctx, loopTop);
                    PatchJmp(ctx, toEnd, ctx->currentAddress);
                    PopLoopCtx(ctx, loopTop); // patches breaks to here, continues to loopTop
                } else {
                    // do { body } while (cond)
                    // continue → jump to condition check
                    PushLoopCtx(ctx);
                    u32 loopTop = ctx->currentAddress;
                    RomGenFromAst(ctx, node->statement.iteration.while_statement.statement);
                    u32 continueTarget = ctx->currentAddress; // before condition
                    RomGenFromAst(ctx, node->statement.iteration.while_statement.condition);
                    WriteByte(ctx, OPCODE_JMP_IF);
                    WriteU32(ctx, loopTop);
                    PopLoopCtx(ctx, continueTarget);
                }
            } else {
                // for (init; cond; update) body
                // continue → jump to update expression
                ctx->scope = PushScope(ctx->scope);
                PushLoopCtx(ctx);

                ast_node_t *init = node->statement.iteration.for_statement.initExpr;
                ast_node_t *cond = node->statement.iteration.for_statement.conditionExpr;
                ast_node_t *upd  = node->statement.iteration.for_statement.updationExpr;

                if (init) {
                    if (init->type == AST_DECL) RomGenDecl(ctx, init);
                    else {
                        RomGenFromAst(ctx, init);
                        WriteByte(ctx, OPCODE_POP);
                    }
                }

                u32 loopTop = ctx->currentAddress;
                u32 toEnd   = 0;
                bool hasCond = (cond != NULL);

                if (hasCond) {
                    RomGenFromAst(ctx, cond);
                    toEnd = EmitJmpPlaceholder(ctx, OPCODE_JMP_IF_NOT);
                }

                RomGenFromAst(ctx, node->statement.iteration.for_statement.statement);

                u32 continueTarget = ctx->currentAddress; // update runs here
                if (upd) {
                    RomGenFromAst(ctx, upd);
                    WriteByte(ctx, OPCODE_POP);
                }

                WriteByte(ctx, OPCODE_JMP);
                WriteU32(ctx, loopTop);

                if (hasCond)
                    PatchJmp(ctx, toEnd, ctx->currentAddress);

                PopLoopCtx(ctx, continueTarget);
                ctx->scope = PopScope(ctx->scope);
            }
        } break;

        case STATEMENT_JUMP: {
            switch (node->statement.jump.kind) {
                case JUMP_STATEMENT_RETURN: {
                    if (node->statement.jump.return_statement.expr) {
                        RomGenFromAst(ctx, node->statement.jump.return_statement.expr);
                    } else {
                        /* void return — push 0 so caller's POP always finds a value */
                        WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 0);
                    }
                    WriteByte(ctx, OPCODE_RET);
                } break;

                case JUMP_STATEMENT_BREAK:    EmitBreak(ctx);    break;
                case JUMP_STATEMENT_CONTINUE: EmitContinue(ctx); break;

                default: break;
            }
        } break;

        case STATEMENT_LABELED: {
            RomGenFromAst(ctx, node->statement.labeled.inner);
        } break;

        default: break;
    }
}

// ---- Main expression / statement codegen ----

static void RomGenFromAst(rom_context_t *ctx, ast_node_t *ast) {
    if (!ast) return;

    switch (ast->type) {
        case AST_PROGRAM: {
            u32 unitCount = DArrayLength(ast->program.units);

            // Pass 1: pre-declare all functions so forward calls resolve.
            for (u32 i = 0; i < unitCount; i++) {
                ast_node_t *node = ast->program.units[i];
                if (node->type == AST_FUNC_DEF) {
                    slice_t name = GetNameFromDeclarator(node->func_def.declarator);
                    rom_symbol_entry_t fun = {
                        .name    = name,
                        .address = 0,
                        .kind    = SYMBOL_ENTRY_FUNCTION,
                    };
                    DeclareSymbol(ctx, &fun);
                }
            }

            // Pass 2: generate code.
            for (u32 i = 0; i < unitCount; i++) {
                ast_node_t *node = ast->program.units[i];
                if (node->type == AST_DECL)
                    RomGenDecl(ctx, node);
                else
                    RomGenFromAst(ctx, node);
            }
        } break;

        case AST_DECL: {
            RomGenDecl(ctx, ast);
        } break;

        case AST_FUNC_DEF: {
            slice_t name = GetNameFromDeclarator(ast->func_def.declarator);

            // Update the pre-declared symbol to the real address.
            UpdateFunctionAddress(ctx, &name, ctx->currentAddress);

            // Save caller's frame-slot counter; this function starts a new frame.
            u32 savedFrameSlot = ctx->frameSlot;
            ctx->frameSlot = 0;

            ctx->scope = PushScope(ctx->scope);

            // Assign slots to parameters.
            ast_declarator_t *funcDecl = FindFunctionDecl(ast->func_def.declarator);
            if (funcDecl && funcDecl->function.parameters) {
                u32 argCount = DArrayLength(funcDecl->function.parameters);
                for (u32 i = 0; i < argCount; i++) {
                    ast_parameter_t *param = funcDecl->function.parameters[i];
                    if (!param->declarator) continue;
                    slice_t paramName = GetNameFromDeclarator(param->declarator);
                    rom_variable_t var = {
                        .name     = paramName,
                        .size     = 4,
                        .location = ctx->frameSlot++,
                        .isGlobal = false,
                    };
                    DeclareVariable(ctx->scope, &var);
                }
            }

            RomGenFromAst(ctx, ast->func_def.statement);

            /* Implicit RET so void functions that fall off the end return cleanly.
               Always pushes 0 to keep caller's POP balanced.
               Dead code for functions that always hit an explicit return. */
            WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 0);
            WriteByte(ctx, OPCODE_RET);

            ctx->scope = PopScope(ctx->scope);
            ctx->frameSlot = savedFrameSlot;
        } break;

        case AST_STATEMENT: {
            RomGenStatement(ctx, ast);
        } break;

        case AST_FUNC_CALL: {
            ast_node_t *fun = ast->func_call.fun;
            if (fun->type != AST_IDENTIFIER) {
                printf("error: function call target must be an identifier\n");
                return;
            }

            // Push arguments left-to-right.
            u32 argCount = DArrayLength(ast->func_call.params);
            for (u32 i = 0; i < argCount; i++)
                RomGenFromAst(ctx, ast->func_call.params[i]);

            slice_t name = fun->identifier.name;
            rom_symbol_entry_t *sym = NULL;
            if (!HashContainsRet(ctx->symbolTable, &name, (void **)&sym)) {
                printf("error: function '" SLICE_STR "' not declared\n", SLICE_ARGS(name));
                return;
            }
            if (sym->kind != SYMBOL_ENTRY_FUNCTION) {
                printf("error: '" SLICE_STR "' is not a function\n", SLICE_ARGS(name));
                return;
            }

            WriteByte(ctx, OPCODE_CALL);
            WriteU32(ctx, sym->address);
            WriteByte(ctx, (u8)argCount);
        } break;

        case AST_TERNARY_EXPR: {
            RomGenFromAst(ctx, ast->ternary_expr.condition);
            u32 toElse = EmitJmpPlaceholder(ctx, OPCODE_JMP_IF_NOT);
            RomGenFromAst(ctx, ast->ternary_expr.then_expr);
            u32 toEnd = EmitJmpPlaceholder(ctx, OPCODE_JMP);
            PatchJmp(ctx, toElse, ctx->currentAddress);
            RomGenFromAst(ctx, ast->ternary_expr.else_expr);
            PatchJmp(ctx, toEnd, ctx->currentAddress);
        } break;

        case AST_BINARY_EXPR: {
            RomGenFromAst(ctx, ast->binary_op.left);
            RomGenFromAst(ctx, ast->binary_op.right);

            switch (ast->binary_op.op) {
                case BINARY_OP_ADD:       WriteByte(ctx, OPCODE_ADD);       break;
                case BINARY_OP_SUB:       WriteByte(ctx, OPCODE_SUB);       break;
                case BINARY_OP_MULT:      WriteByte(ctx, OPCODE_MUL);       break;
                case BINARY_OP_DIV:       WriteByte(ctx, OPCODE_DIV);       break;
                case BINARY_OP_MOD:       WriteByte(ctx, OPCODE_MOD);       break;
                case BINARY_OP_AND:       WriteByte(ctx, OPCODE_AND);       break;
                case BINARY_OP_OR:        WriteByte(ctx, OPCODE_OR);        break;
                case BINARY_OP_XOR:       WriteByte(ctx, OPCODE_XOR);       break;
                case BINARY_OP_SHL:       WriteByte(ctx, OPCODE_SHL);       break;
                case BINARY_OP_SHR:       WriteByte(ctx, OPCODE_SHR);       break;
                case BINARY_OP_EQUIV:     WriteByte(ctx, OPCODE_EQ);        break;
                case BINARY_OP_NOT_EQUIV: WriteByte(ctx, OPCODE_NEQ);       break;
                case BINARY_OP_LT:        WriteByte(ctx, OPCODE_LT);        break;
                case BINARY_OP_LTE:       WriteByte(ctx, OPCODE_LTE);       break;
                case BINARY_OP_GT:        WriteByte(ctx, OPCODE_GT);        break;
                case BINARY_OP_GTE:       WriteByte(ctx, OPCODE_GTE);       break;
                // Short-circuit &&: if left is false, result is 0; skip right.
                case BINARY_OP_LOGIC_AND: {
                    // Stack after left eval: [left]
                    // JMP_IF_NOT pops left; if false jump to push 0.
                    u32 toFalse = EmitJmpPlaceholder(ctx, OPCODE_JMP_IF_NOT);
                    RomGenFromAst(ctx, ast->binary_op.right);
                    WriteByte(ctx, OPCODE_LOGIC_NOT);
                    WriteByte(ctx, OPCODE_LOGIC_NOT); // normalise to 0/1
                    u32 toEnd = EmitJmpPlaceholder(ctx, OPCODE_JMP);
                    PatchJmp(ctx, toFalse, ctx->currentAddress);
                    WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 0);
                    PatchJmp(ctx, toEnd, ctx->currentAddress);
                } break;
                // Short-circuit ||: if left is true, result is 1; skip right.
                case BINARY_OP_LOGIC_OR: {
                    u32 toTrue = EmitJmpPlaceholder(ctx, OPCODE_JMP_IF);
                    RomGenFromAst(ctx, ast->binary_op.right);
                    WriteByte(ctx, OPCODE_LOGIC_NOT);
                    WriteByte(ctx, OPCODE_LOGIC_NOT);
                    u32 toEnd = EmitJmpPlaceholder(ctx, OPCODE_JMP);
                    PatchJmp(ctx, toTrue, ctx->currentAddress);
                    WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 1);
                    PatchJmp(ctx, toEnd, ctx->currentAddress);
                } break;
                default: break;
            }
        } break;

        case AST_UNARY_EXPR: {
            switch (ast->unary_op.op) {
                case UNARY_OP_NEGATE: {
                    RomGenFromAst(ctx, ast->unary_op.expr);
                    WriteByte(ctx, OPCODE_NEG);
                } break;

                case UNARY_OP_NOT: {
                    RomGenFromAst(ctx, ast->unary_op.expr);
                    WriteByte(ctx, OPCODE_NOT);
                } break;

                case UNARY_OP_LOGIC_NOT: {
                    RomGenFromAst(ctx, ast->unary_op.expr);
                    WriteByte(ctx, OPCODE_LOGIC_NOT);
                } break;

                case UNARY_OP_DEREFRENCE: {
                    RomGenFromAst(ctx, ast->unary_op.expr);
                    u8 size = 4;
                    type_t *pt = ast->unary_op.expr->resolvedType;
                    if (pt && pt->kind == TYPE_POINTER && pt->ptr.base) {
                        u32 s = TypeSize(pt->ptr.base);
                        if (s > 0 && s <= 4) size = (u8)s;
                    }
                    WriteByte(ctx, OPCODE_LOAD_INDIRECT);
                    WriteByte(ctx, size);
                } break;

                case UNARY_OP_ADDRESS: {
                    EmitLValueAddr(ctx, ast->unary_op.expr);
                } break;

                case UNARY_OP_INCREMENT:
                case UNARY_OP_DECREMENT: {
                    ast_node_t *operand = ast->unary_op.expr;
                    bool isInc = (ast->unary_op.op == UNARY_OP_INCREMENT);
                    if (operand->type == AST_IDENTIFIER) {
                        rom_variable_t *var = LookupVariable(ctx->scope, &operand->identifier.name);
                        if (var) {
                            EmitLoad(ctx, var);
                            WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 1);
                            WriteByte(ctx, isInc ? OPCODE_ADD : OPCODE_SUB);
                            WriteByte(ctx, OPCODE_DUP);
                            EmitStore(ctx, var);
                        }
                    } else {
                        // arr[i]++, ptr->field++, etc.
                        // [addr] → DUP → [addr,addr] → LOAD → [addr,old]
                        // → +1/-1 → [addr,new] → DUP → [addr,new,new]
                        // → ROT3 → [new,addr,new] → STORE_INDIRECT → [new]
                        type_t *elemType = operand->resolvedType;
                        u32 eSize = (elemType && TypeSize(elemType) > 0) ? TypeSize(elemType) : 4;
                        u8 sz = (u8)(eSize <= 4 ? eSize : 4);
                        EmitLValueAddr(ctx, operand);
                        WriteByte(ctx, OPCODE_DUP);
                        WriteByte(ctx, OPCODE_LOAD_INDIRECT); WriteByte(ctx, sz);
                        WriteByte(ctx, OPCODE_PUSH_CONST); WriteU32(ctx, 1);
                        WriteByte(ctx, isInc ? OPCODE_ADD : OPCODE_SUB);
                        WriteByte(ctx, OPCODE_DUP);
                        WriteByte(ctx, OPCODE_ROT3);
                        WriteByte(ctx, OPCODE_STORE_INDIRECT); WriteByte(ctx, sz);
                    }
                } break;

                default: {
                    RomGenFromAst(ctx, ast->unary_op.expr);
                } break;
            }
        } break;

        case AST_ASSIGN_EXPR: {
            ast_node_t *lhs = ast->assign_op.left;
            ast_node_t *rhs = ast->assign_op.right;

            if (ast->assign_op.op == ASSIGN) {
                // Evaluate RHS; DUP so assignment result stays on stack (C semantics).
                RomGenFromAst(ctx, rhs);
                WriteByte(ctx, OPCODE_DUP);

                if (lhs->type == AST_IDENTIFIER) {
                    rom_variable_t *var = LookupVariable(ctx->scope, &lhs->identifier.name);
                    if (!var) {
                        printf("error: unknown variable '" SLICE_STR "'\n",
                               SLICE_ARGS(lhs->identifier.name));
                        WriteByte(ctx, OPCODE_POP); // keep stack balanced
                        return;
                    }
                    EmitStore(ctx, var);
                } else if (lhs->type == AST_MEMBER && !lhs->member.isPointer &&
                           lhs->member.parent->type == AST_IDENTIFIER) {
                    // Stack-slot struct field: use STORE_LOCAL/STORE_GLOBAL directly.
                    ast_node_t *structNode = lhs->member.parent;
                    type_t *structType = structNode->resolvedType;
                    rom_variable_t *var = LookupVariable(ctx->scope, &structNode->identifier.name);
                    field_t *f = FindField(structType, &lhs->member.member);
                    u32 fieldOff = f ? f->offset : 0;
                    if (var) {
                        if (var->isGlobal) {
                            WriteByte(ctx, OPCODE_STORE_GLOBAL);
                            WriteU32(ctx, var->location + fieldOff);
                        } else {
                            WriteByte(ctx, OPCODE_STORE_LOCAL);
                            WriteU16(ctx, (u16)(var->location + fieldOff / 4));
                        }
                    } else {
                        WriteByte(ctx, OPCODE_POP);
                    }
                } else if (lhs->type == AST_UNARY_EXPR ||
                           lhs->type == AST_INDEX ||
                           (lhs->type == AST_MEMBER && lhs->member.isPointer)) {
                    // General indirect write: DUP already done; stack is [val, val].
                    // Push addr, SWAP so stack is [val, addr, val], then STORE_INDIRECT.
                    type_t *elemType = lhs->resolvedType;
                    u32 eSize = (elemType && TypeSize(elemType) > 0) ? TypeSize(elemType) : 4;
                    u8 storeSize = (u8)(eSize <= 4 ? eSize : 4);
                    EmitLValueAddr(ctx, lhs);      // [val, val, addr]
                    WriteByte(ctx, OPCODE_SWAP);   // [val, addr, val]
                    WriteByte(ctx, OPCODE_STORE_INDIRECT);
                    WriteByte(ctx, storeSize);
                } else {
                    WriteByte(ctx, OPCODE_POP);
                }
            } else {
                // Compound assignment (op=).
                // Load current value of LHS.
                if (lhs->type == AST_IDENTIFIER) {
                    rom_variable_t *var = LookupVariable(ctx->scope, &lhs->identifier.name);
                    if (!var) { printf("error: unknown variable\n"); return; }
                    EmitLoad(ctx, var);
                    // Evaluate RHS and apply operation.
                    RomGenFromAst(ctx, rhs);
                    switch (ast->assign_op.op) {
                        case ASSIGN_ADD: WriteByte(ctx, OPCODE_ADD); break;
                        case ASSIGN_SUB: WriteByte(ctx, OPCODE_SUB); break;
                        case ASSIGN_MUL: WriteByte(ctx, OPCODE_MUL); break;
                        case ASSIGN_DIV: WriteByte(ctx, OPCODE_DIV); break;
                        case ASSIGN_MOD: WriteByte(ctx, OPCODE_MOD); break;
                        case ASSIGN_AND: WriteByte(ctx, OPCODE_AND); break;
                        case ASSIGN_OR:  WriteByte(ctx, OPCODE_OR);  break;
                        case ASSIGN_XOR: WriteByte(ctx, OPCODE_XOR); break;
                        case ASSIGN_SHL: WriteByte(ctx, OPCODE_SHL); break;
                        case ASSIGN_SHR: WriteByte(ctx, OPCODE_SHR); break;
                        default: break;
                    }
                    // DUP result (leave copy on stack), then store.
                    WriteByte(ctx, OPCODE_DUP);
                    EmitStore(ctx, var);
                } else if (lhs->type == AST_MEMBER && !lhs->member.isPointer &&
                           lhs->member.parent->type == AST_IDENTIFIER) {
                    // s.field op= rhs
                    ast_node_t *structNode = lhs->member.parent;
                    type_t *structType = structNode->resolvedType;
                    rom_variable_t *var = LookupVariable(ctx->scope, &structNode->identifier.name);
                    field_t *f = FindField(structType, &lhs->member.member);
                    u32 fieldOff = f ? f->offset : 0;
                    if (var) {
                        // Load current field value.
                        if (var->isGlobal) {
                            WriteByte(ctx, OPCODE_LOAD_GLOBAL);
                            WriteU32(ctx, var->location + fieldOff);
                        } else {
                            WriteByte(ctx, OPCODE_LOAD_LOCAL);
                            WriteU16(ctx, (u16)(var->location + fieldOff / 4));
                        }
                        RomGenFromAst(ctx, rhs);
                        switch (ast->assign_op.op) {
                            case ASSIGN_ADD: WriteByte(ctx, OPCODE_ADD); break;
                            case ASSIGN_SUB: WriteByte(ctx, OPCODE_SUB); break;
                            case ASSIGN_MUL: WriteByte(ctx, OPCODE_MUL); break;
                            case ASSIGN_DIV: WriteByte(ctx, OPCODE_DIV); break;
                            case ASSIGN_MOD: WriteByte(ctx, OPCODE_MOD); break;
                            case ASSIGN_AND: WriteByte(ctx, OPCODE_AND); break;
                            case ASSIGN_OR:  WriteByte(ctx, OPCODE_OR);  break;
                            case ASSIGN_XOR: WriteByte(ctx, OPCODE_XOR); break;
                            case ASSIGN_SHL: WriteByte(ctx, OPCODE_SHL); break;
                            case ASSIGN_SHR: WriteByte(ctx, OPCODE_SHR); break;
                            default: break;
                        }
                        WriteByte(ctx, OPCODE_DUP);
                        if (var->isGlobal) {
                            WriteByte(ctx, OPCODE_STORE_GLOBAL);
                            WriteU32(ctx, var->location + fieldOff);
                        } else {
                            WriteByte(ctx, OPCODE_STORE_LOCAL);
                            WriteU16(ctx, (u16)(var->location + fieldOff / 4));
                        }
                    }
                } else if (lhs->type == AST_UNARY_EXPR ||
                           lhs->type == AST_INDEX ||
                           (lhs->type == AST_MEMBER && lhs->member.isPointer)) {
                    // General indirect compound assignment.
                    // Stack trace (bottom→top):
                    //   EmitLValueAddr  [addr]
                    //   DUP             [addr, addr]
                    //   LOAD_INDIRECT   [addr, cur]
                    //   eval rhs + op   [addr, new_val]
                    //   DUP             [addr, new_val, new_val]
                    //   ROT3            [new_val, addr, new_val]  (top sinks to bottom-of-3)
                    //   STORE_INDIRECT  [new_val]                 (expression result)
                    type_t *elemType = lhs->resolvedType;
                    u32 eSize = (elemType && TypeSize(elemType) > 0) ? TypeSize(elemType) : 4;
                    u8 sz = (u8)(eSize <= 4 ? eSize : 4);
                    EmitLValueAddr(ctx, lhs);
                    WriteByte(ctx, OPCODE_DUP);
                    WriteByte(ctx, OPCODE_LOAD_INDIRECT); WriteByte(ctx, sz);
                    RomGenFromAst(ctx, rhs);
                    switch (ast->assign_op.op) {
                        case ASSIGN_ADD: WriteByte(ctx, OPCODE_ADD); break;
                        case ASSIGN_SUB: WriteByte(ctx, OPCODE_SUB); break;
                        case ASSIGN_MUL: WriteByte(ctx, OPCODE_MUL); break;
                        case ASSIGN_DIV: WriteByte(ctx, OPCODE_DIV); break;
                        case ASSIGN_MOD: WriteByte(ctx, OPCODE_MOD); break;
                        case ASSIGN_AND: WriteByte(ctx, OPCODE_AND); break;
                        case ASSIGN_OR:  WriteByte(ctx, OPCODE_OR);  break;
                        case ASSIGN_XOR: WriteByte(ctx, OPCODE_XOR); break;
                        case ASSIGN_SHL: WriteByte(ctx, OPCODE_SHL); break;
                        case ASSIGN_SHR: WriteByte(ctx, OPCODE_SHR); break;
                        default: break;
                    }
                    WriteByte(ctx, OPCODE_DUP);
                    WriteByte(ctx, OPCODE_ROT3);
                    WriteByte(ctx, OPCODE_STORE_INDIRECT); WriteByte(ctx, sz);
                }
            }
        } break;

        case AST_CAST_EXPR: {
            // Ignore the cast target type for now; just emit the inner expression.
            RomGenFromAst(ctx, ast->cast_expr.expr);
        } break;

        case AST_INDEX: {
            // Compute address then load: uses EmitLValueAddr for correct base + scale.
            type_t *elemType = ast->resolvedType;
            u32 elemSize = (elemType && TypeSize(elemType) > 0) ? TypeSize(elemType) : 4;
            EmitLValueAddr(ctx, ast);
            WriteByte(ctx, OPCODE_LOAD_INDIRECT);
            WriteByte(ctx, (u8)(elemSize <= 4 ? elemSize : 4));
        } break;

        case AST_MEMBER: {
            ast_node_t *parent = ast->member.parent;
            type_t *parentType = parent->resolvedType;

            if (!ast->member.isPointer && parentType && IsStructType(parentType) &&
                parent->type == AST_IDENTIFIER) {
                // s.field — resolve to a slot or global address at compile time.
                rom_variable_t *var = LookupVariable(ctx->scope, &parent->identifier.name);
                field_t *f = FindField(parentType, &ast->member.member);
                u32 fieldOff = f ? f->offset : 0;
                if (var) {
                    if (var->isGlobal) {
                        WriteByte(ctx, OPCODE_LOAD_GLOBAL);
                        WriteU32(ctx, var->location + fieldOff);
                    } else {
                        WriteByte(ctx, OPCODE_LOAD_LOCAL);
                        WriteU16(ctx, (u16)(var->location + fieldOff / 4));
                    }
                    break;
                }
            }

            // ptr->field or fallback: push base address, add offset, load indirectly.
            RomGenFromAst(ctx, parent);
            if (ast->member.isPointer) {
                type_t *pointeeType = parentType ? parentType->ptr.base : NULL;
                field_t *f = FindField(pointeeType, &ast->member.member);
                u32 fieldOff = f ? f->offset : 0;
                if (fieldOff > 0) {
                    WriteByte(ctx, OPCODE_FIELD_OFFSET);
                    WriteU32(ctx, fieldOff);
                }
                u32 fieldSize = (f && f->type) ? TypeSize(f->type) : 4;
                WriteByte(ctx, OPCODE_LOAD_INDIRECT);
                WriteByte(ctx, (u8)fieldSize);
            } else {
                WriteByte(ctx, OPCODE_LOAD_INDIRECT);
                WriteByte(ctx, 4);
            }
        } break;

        case AST_LITERAL_INT: {
            WriteByte(ctx, OPCODE_PUSH_CONST);
            WriteU32(ctx, (u32)ast->int_literal.literal);
        } break;

        case AST_LITERAL_STRING: {
            // Record start offset within stringData, append the bytes + NUL.
            u32 strOff = (u32)DArrayLength(ctx->stringData);
            slice_t s = ast->string_literal.str;
            for (u32 j = 0; j < s.len; j++) {
                u8 ch = s.str[j];
                DArrayPush(ctx->stringData, ch);
            }
            u8 nul = 0;
            DArrayPush(ctx->stringData, nul);

            // Emit PUSH_CONST 0 (placeholder); record for later patching.
            WriteByte(ctx, OPCODE_PUSH_CONST);
            u32 refOff = (u32)DArrayLength(ctx->code); // offset of the 4-byte operand
            WriteU32(ctx, 0);
            DArrayPush(ctx->stringRefs, refOff);
            DArrayPush(ctx->stringOffsets, strOff);
        } break;

        case AST_IDENTIFIER: {
            // Check codegen scope first.
            rom_variable_t *var = LookupVariable(ctx->scope, &ast->identifier.name);
            if (var) {
                // Array-to-pointer decay: an array name evaluates to its base address.
                if (var->varType && var->varType->kind == TYPE_ARRAY) {
                    WriteByte(ctx, OPCODE_PUSH_CONST);
                    WriteU32(ctx, var->location);
                } else {
                    EmitLoad(ctx, var);
                }
                return;
            }
            // Fall back to sema symbol for enum constants.
            if (ast->symbol && ast->symbol->kind == SYMBOL_ENUM_CONSTS) {
                WriteByte(ctx, OPCODE_PUSH_CONST);
                WriteU32(ctx, (u32)ast->symbol->enumConstantValue);
                return;
            }
            printf("error: unknown identifier '" SLICE_STR "'\n",
                   SLICE_ARGS(ast->identifier.name));
        } break;
    }
}

// ---- ROM assembly ----
// Layout: [rom_header_t][rom_section_entry_t][rom_code_section_fields][code bytes]

u8 *CodegenRom(ast_node_t *ast, u32 *outSize) {
    if (ast->type != AST_PROGRAM) {
        printf("error: CodegenRom expects AST_PROGRAM\n");
        return NULL;
    }

    rom_context_t ctx = {
        .ast            = ast,
        .code           = DArrayCreate(u8),
        .frameSlot      = 0,
        .currentAddress = 0,
        .currentGlobal  = 0,
        .scope          = PushScope(NULL),
        .symbols        = DArrayCreate(rom_symbol_entry_t),
        .symbolTable    = CreateHashTable(sizeof(rom_symbol_entry_t)),
        .loopStack      = DArrayCreate(loop_ctx_t),
        .stringData     = DArrayCreate(u8),
        .stringRefs     = DArrayCreate(u32),
        .stringOffsets  = DArrayCreate(u32),
    };

    RomGenFromAst(&ctx, ast);
    WriteByte(&ctx, OPCODE_HALT);

    // Patch string literal addresses now that we know the total global size.
    u32 stringBase = ROM_GLOBAL_OFFSET + ctx.currentGlobal;
    u32 nStrRefs = (u32)DArrayLength(ctx.stringRefs);
    for (u32 i = 0; i < nStrRefs; i++) {
        u32 codeOff = ctx.stringRefs[i];
        u32 strAddr = stringBase + ctx.stringOffsets[i];
        ctx.code[codeOff + 0] = (u8)(strAddr >>  0);
        ctx.code[codeOff + 1] = (u8)(strAddr >>  8);
        ctx.code[codeOff + 2] = (u8)(strAddr >> 16);
        ctx.code[codeOff + 3] = (u8)(strAddr >> 24);
    }

    // Compute layout.
    u32 headerSize       = sizeof(rom_header_t);
    u32 sectionEntrySize = sizeof(rom_section_entry_t);
    u32 codeHdrSize      = sizeof(rom_code_section_fields);
    u32 codeSize         = (u32)DArrayLength(ctx.code);
    u32 stringSize       = (u32)DArrayLength(ctx.stringData);
    bool hasStrings      = (stringSize > 0);
    u32 sectionCount     = hasStrings ? 2 : 1;

    u32 codeFileOffset   = headerSize + sectionCount * sectionEntrySize + codeHdrSize;
    u32 stringFileOffset = codeFileOffset + codeSize;
    u32 totalRomSize     = stringFileOffset + (hasStrings ? stringSize : 0);

    // Find 'main' entry point.
    u32 entryPoint = 0;
    u32 symCount = DArrayLength(ctx.symbols);
    for (u32 i = 0; i < symCount; i++) {
        if (ctx.symbols[i].kind == SYMBOL_ENTRY_FUNCTION &&
            CompareSliceToStr(&ctx.symbols[i].name, (u8 *)"main", 4)) {
            entryPoint = ctx.symbols[i].address;
            break;
        }
    }

    // Build ROM header.
    rom_header_t header = {0};
    header.magic[0]          = 0x52;
    header.magic[1]          = 0x4F;
    header.magic[2]          = 0x4D;
    header.magic[3]          = 0x21;
    header.version_major     = ROM_VERSION_MAJOR;
    header.version_minor     = ROM_VERSION_MINOR;
    header.entry_point       = entryPoint;
    header.section_count     = sectionCount;
    header.section_table_off = headerSize;
    header.rom_size          = totalRomSize;

    // Build code section entry.
    rom_section_entry_t codeSect = {0};
    codeSect.type        = SECTION_TYPE_CODE;
    codeSect.file_offset = codeFileOffset;
    codeSect.file_size   = codeSize;
    codeSect.mem_size    = codeSize;

    // Build optional data (string) section entry.
    rom_section_entry_t dataSect = {0};
    if (hasStrings) {
        dataSect.type        = SECTION_TYPE_DATA;
        dataSect.file_offset = stringFileOffset;
        dataSect.file_size   = stringSize;
        dataSect.mem_address = stringBase;
        dataSect.mem_size    = stringSize;
    }

    // Build code section fields header.
    rom_code_section_fields codeHdr = {0};
    codeHdr.instruction_count = codeSize;

    // Serialize into a single malloc'd buffer.
    u8 *out = (u8 *)malloc(totalRomSize);
    u8 *p   = out;
    memcpy(p, &header,   headerSize);       p += headerSize;
    memcpy(p, &codeSect, sectionEntrySize); p += sectionEntrySize;
    if (hasStrings) { memcpy(p, &dataSect, sectionEntrySize); p += sectionEntrySize; }
    memcpy(p, &codeHdr,  codeHdrSize);      p += codeHdrSize;
    memcpy(p, ctx.code,  codeSize);         p += codeSize;
    if (hasStrings) memcpy(p, ctx.stringData, stringSize);

    *outSize = totalRomSize;
    return out;
}
