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

// ---- Forward declarations (mutual recursion) ----

static void RomGenFromAst(rom_context_t *ctx, ast_node_t *ast);
static void RomGenDecl(rom_context_t *ctx, ast_node_t *node);
static void RomGenStatement(rom_context_t *ctx, ast_node_t *node);

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

    bool isGlobal = (ctx->scope->parent == NULL);
    u32 declCount = DArrayLength(node->decl.initDeclList);

    for (u32 i = 0; i < declCount; i++) {
        ast_init_declarator_t *initDecl = node->decl.initDeclList[i];
        if (!initDecl->declarator) continue;

        slice_t name = GetNameFromDeclarator(initDecl->declarator);
        if (!name.str) continue;

        rom_variable_t var;
        if (isGlobal) {
            var = (rom_variable_t){
                .name     = name,
                .size     = 4,  // TODO: use actual type size once sema symbols wired up
                .location = ROM_GLOBAL_OFFSET + ctx->currentGlobal,
                .isGlobal = true,
            };
            ctx->currentGlobal += var.size;
        } else {
            var = (rom_variable_t){
                .name     = name,
                .size     = 4,
                .location = ctx->frameSlot++,
                .isGlobal = false,
            };
        }

        DeclareVariable(ctx->scope, &var);

        // Generate initializer code
        if (initDecl->initializer &&
            initDecl->initializer->kind == INITIALIZER_EXPR &&
            initDecl->initializer->expr) {
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
            }
            // TODO: switch statement
        } break;

        case STATEMENT_ITERATION: {
            if (node->statement.iteration.kind == ITERATION_STATEMENT_WHILE) {
                bool hasDo = node->statement.iteration.while_statement.hasDo;
                if (!hasDo) {
                    // while (cond) body
                    u32 loopTop = ctx->currentAddress;
                    RomGenFromAst(ctx, node->statement.iteration.while_statement.condition);
                    u32 toEnd = EmitJmpPlaceholder(ctx, OPCODE_JMP_IF_NOT);
                    RomGenFromAst(ctx, node->statement.iteration.while_statement.statement);
                    WriteByte(ctx, OPCODE_JMP);
                    WriteU32(ctx, loopTop);
                    PatchJmp(ctx, toEnd, ctx->currentAddress);
                } else {
                    // do { body } while (cond)
                    u32 loopTop = ctx->currentAddress;
                    RomGenFromAst(ctx, node->statement.iteration.while_statement.statement);
                    RomGenFromAst(ctx, node->statement.iteration.while_statement.condition);
                    WriteByte(ctx, OPCODE_JMP_IF);
                    WriteU32(ctx, loopTop);
                }
            } else {
                // for (init; cond; update) body
                ctx->scope = PushScope(ctx->scope);

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

                if (upd) {
                    RomGenFromAst(ctx, upd);
                    WriteByte(ctx, OPCODE_POP);
                }

                WriteByte(ctx, OPCODE_JMP);
                WriteU32(ctx, loopTop);

                if (hasCond)
                    PatchJmp(ctx, toEnd, ctx->currentAddress);

                ctx->scope = PopScope(ctx->scope);
            }
        } break;

        case STATEMENT_JUMP: {
            switch (node->statement.jump.kind) {
                case JUMP_STATEMENT_RETURN: {
                    if (node->statement.jump.return_statement.expr)
                        RomGenFromAst(ctx, node->statement.jump.return_statement.expr);
                    WriteByte(ctx, OPCODE_RET);
                } break;

                // TODO: break/continue (need per-loop backpatch lists)
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
                // TODO: short-circuit evaluation for && and ||
                case BINARY_OP_LOGIC_AND: WriteByte(ctx, OPCODE_AND);       break;
                case BINARY_OP_LOGIC_OR:  WriteByte(ctx, OPCODE_OR);        break;
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
                    WriteByte(ctx, OPCODE_LOAD_INDIRECT);
                    WriteByte(ctx, 4); // TODO: use actual pointee type size
                } break;

                case UNARY_OP_ADDRESS: {
                    // TODO: push address of variable (needs address-of opcode)
                } break;

                // TODO: proper pre/post increment/decrement
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
                } else if (lhs->type == AST_UNARY_EXPR &&
                           lhs->unary_op.op == UNARY_OP_DEREFRENCE) {
                    // *ptr = val: need address + STORE_INDIRECT
                    // TODO
                    WriteByte(ctx, OPCODE_POP);
                } else {
                    // TODO: index, member
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
                }
                // TODO: compound assignment to *ptr, array index, struct member
            }
        } break;

        case AST_CAST_EXPR: {
            // Ignore the cast target type for now; just emit the inner expression.
            RomGenFromAst(ctx, ast->cast_expr.expr);
        } break;

        case AST_INDEX: {
            // array[index] => push base address, push index, add, load indirect
            RomGenFromAst(ctx, ast->index.array);
            RomGenFromAst(ctx, ast->index.index);
            // TODO: multiply index by element type size
            WriteByte(ctx, OPCODE_ADD);
            WriteByte(ctx, OPCODE_LOAD_INDIRECT);
            WriteByte(ctx, 4); // TODO: actual element type size
        } break;

        case AST_MEMBER: {
            // struct.field / ptr->field — push base, apply field offset, load indirect
            // TODO: look up field offset from sema type
            RomGenFromAst(ctx, ast->member.parent);
            if (ast->member.isPointer) {
                // Parent is already a pointer; dereference to get struct base.
                WriteByte(ctx, OPCODE_LOAD_INDIRECT);
                WriteByte(ctx, 4);
            }
            // TODO: OPCODE_FIELD_OFFSET + actual offset
            WriteByte(ctx, OPCODE_LOAD_INDIRECT);
            WriteByte(ctx, 4);
        } break;

        case AST_LITERAL_INT: {
            WriteByte(ctx, OPCODE_PUSH_CONST);
            WriteU32(ctx, (u32)ast->int_literal.literal);
        } break;

        case AST_LITERAL_STRING: {
            // TODO: intern string into a const section, push its address
            WriteByte(ctx, OPCODE_PUSH_CONST);
            WriteU32(ctx, 0); // placeholder
        } break;

        case AST_IDENTIFIER: {
            // Check codegen scope first.
            rom_variable_t *var = LookupVariable(ctx->scope, &ast->identifier.name);
            if (var) {
                EmitLoad(ctx, var);
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
    };

    RomGenFromAst(&ctx, ast);
    WriteByte(&ctx, OPCODE_HALT);

    // Compute layout.
    u32 headerSize       = sizeof(rom_header_t);
    u32 sectionEntrySize = sizeof(rom_section_entry_t);
    u32 codeHdrSize      = sizeof(rom_code_section_fields);
    u32 codeSize         = (u32)DArrayLength(ctx.code);
    u32 codeFileOffset   = headerSize + sectionEntrySize + codeHdrSize;
    u32 totalRomSize     = codeFileOffset + codeSize;

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
    header.magic[0]          = 0x52; // 'R'
    header.magic[1]          = 0x4F; // 'O'
    header.magic[2]          = 0x4D; // 'M'
    header.magic[3]          = 0x21; // '!'
    header.version_major     = ROM_VERSION_MAJOR;
    header.version_minor     = ROM_VERSION_MINOR;
    header.flags             = 0;
    header.entry_point       = entryPoint;
    header.section_count     = 1;
    header.section_table_off = headerSize;
    header.rom_size          = totalRomSize;
    header.checksum          = 0; // TODO

    // Build code section entry.
    rom_section_entry_t codeSect = {0};
    codeSect.type        = SECTION_TYPE_CODE;
    codeSect.flags       = 0;
    codeSect.file_offset = codeFileOffset;
    codeSect.file_size   = codeSize;
    codeSect.mem_address = 0;
    codeSect.mem_size    = codeSize;

    // Build code section header.
    rom_code_section_fields codeHdr = {0};
    codeHdr.instruction_count = codeSize; // byte count approximation
    codeHdr.code_flags        = 0;

    // Serialize into a single malloc'd buffer.
    u8 *out = (u8 *)malloc(totalRomSize);
    u8 *p   = out;
    memcpy(p, &header,   headerSize);       p += headerSize;
    memcpy(p, &codeSect, sectionEntrySize); p += sectionEntrySize;
    memcpy(p, &codeHdr,  codeHdrSize);      p += codeHdrSize;
    memcpy(p, ctx.code,  codeSize);

    *outSize = totalRomSize;
    return out;
}
