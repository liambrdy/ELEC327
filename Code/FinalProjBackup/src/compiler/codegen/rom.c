#include "rom.h"

#include "darray.h"
#include "str.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    if (HashContains(scope->variables, &var->name)) {
        return false;
    }

    HashInsert(scope->variables, &var->name, var);
    return true;
}


static slice_t GetNameFromDeclarator(ast_declarator_t *decl) {
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

u32 GetTypeSize(ast_declarator_t *decl, decl_specifiers_t *specifiers) {
    return 4;
}

void DeclareSymbol(rom_context_t *ctx, rom_symbol_entry_t *sym) {
    DArrayPush(ctx->symbols, *sym);
    HashInsert(ctx->symbolTable, &sym->name, sym);
}

void WriteByte(rom_context_t *ctx, u8 c) {
    DArrayPush(ctx->code, c);
    ctx->currentAddress++;
}

void WriteBytes(rom_context_t *ctx, u32 count, u8 *data) {
    for (u32 i = 0; i < count; i++) {
        WriteByte(ctx, data[i]);
    }
}

void WriteU32(rom_context_t *ctx, u32 num) {
    WriteBytes(ctx, 4, &num);
}

void RomGenFromAst(rom_context_t *ctx, ast_node_t *ast) {
    switch (ast->type) {
        case AST_PROGRAM: {
            i32 unitCount = DArrayLength(ast->program.units);
            for (i32 i = 0; i < unitCount; i++) {
                ast_node_t *node = ast->program.units[i];
                if (node->type != AST_DECL) {
                    RomGenFromAst(ctx, node);
                } else {
                    u32 declCount = DArrayLength(node->decl.initDeclList);
                    for (u32 i = 0; i < declCount; i++) {
                        ast_init_declarator_t *decl = node->decl.initDeclList[i];
                        u32 size = GetTypeSize(decl->declarator, &node->decl.specifiers);
                        rom_variable_t globalVar = {
                            .name = GetNameFromDeclarator(decl->declarator),
                            .size = size,
                            .location = ROM_GLOBAL_OFFSET + ctx-> currentGlobal,
                        };
                        DeclareVariable(ctx->scope, &globalVar);

                        ctx->currentGlobal += size;
                    }
                }
            }
        } break;

        case AST_DECL: {
            
        } break;

        case AST_FUNC_DEF: {
            slice_t name = GetNameFromDeclarator(ast->func_def.declarator);
            rom_symbol_entry_t fun = {
                .name = name,
                .address = ctx->currentAddress,
                .kind = SYMBOL_ENTRY_FUNCTION,
            };

            DeclareSymbol(ctx, &fun);
            
            ctx->scope = PushScope(ctx->scope);
            u32 argCount = DArrayLength(ast->func_def.declarator->function.parameters);
            for (u32 i = 0; i < argCount; i++) {
                ast_parameter_t *param = ast->func_def.declarator->function.parameters[i];
                slice_t paramName = GetNameFromDeclarator(param->declarator);
                u32 size = GetTypeSize(param->declarator, &param->specifiers);

                rom_variable_t var = {
                    .name = paramName,
                    .size = size,
                    .location = ctx->currentStack++,
                };

                DeclareVariable(ctx->scope, &var);
            }

            RomGenFromAst(ctx, ast->func_def.statement);
            ctx->scope = PopScope(ctx->scope);
            ctx->currentStack -= argCount;
        } break;

        case AST_STATEMENT: {
            
        } break;

        case AST_FUNC_CALL: {
            ast_node_t *fun = ast->func_call.fun;
            if (fun->type != AST_IDENTIFIER) {
                printf("Function call must be with identifier!");
                return;
            }

            slice_t name = fun->identifier.name;
            rom_symbol_entry_t *sym = NULL;
            if (!HashContainsRet(ctx->symbolTable, &name, (void **)&sym)) {
                printf("Function " SLICE_STR " does not exist!\n", SLICE_ARGS(name));
                return;
            }

            if (sym->kind != SYMBOL_ENTRY_FUNCTION) {
                printf("Symbol " SLICE_STR " is not function\n", SLICE_ARGS(name));
                return;
            }

            u32 argCount = DArrayLength(ast->func_call.params);
            for (u32 i = 0; i < argCount; i++) {
                ast_node_t *arg = ast->func_call.params[i];
                RomGenFromAst(ctx, arg);
                // TODO: Use generated value for arg call:
            }

            WriteByte(ctx, OPCODE_CALL);
            WriteU32(ctx, sym->address);
        } break;

        case AST_TERNARY_EXPR: {

        } break;

        case AST_BINARY_EXPR: {
            
        } break;

        case AST_UNARY_EXPR: {

        } break;

        case AST_ASSIGN_EXPR: {

        } break;

        case AST_CAST_EXPR: {

        } break;

        case AST_INDEX: {

        } break;

        case AST_MEMBER: {

        } break;

        case AST_LITERAL_INT: {

        } break;

        case AST_LITERAL_STRING: {

        } break;

        case AST_IDENTIFIER: {

        } break;

    }
}

// Header
// Section Table
// Code Header
// Code
// Symbol Section

string_buf CodegenRom(ast_node_t *ast) {
    if (ast->type != AST_PROGRAM) {
        printf("AST type is not program!\n");
        return NULL;
    }

    u32 headerSize = sizeof(rom_header_t);
    u32 sectionTableSize = sizeof(rom_section_entry_t) * 2;
    u32 codeHeader = sizeof(rom_code_section_fields);

    rom_context_t ctx = {
        .ast = ast,
        .code = DArrayCreate(u8),
        .currentStack = 0,
        .currentAddress = 0,
        .scope = PushScope(NULL),
        .symbols = DArrayCreate(rom_symbol_entry_t),
        .symbolTable = CreateHashTable(sizeof(rom_symbol_entry_t)),
    };
    
    RomGenFromAst(&ctx, ast);

    rom_header_t header = {0};
    header.magic[0] = 0x52;
    header.magic[1] = 0x4F;
    header.magic[2] = 0x4D;
    header.magic[3] = 0x21;

    header.version_major = ROM_VERSION_MAJOR;
    header.version_minor = ROM_VERSION_MINOR;
    header.flags = 0;
    header.entry_point = 0;
    header.section_count = 1;
    header.section_table_off = sizeof(rom_header_t);
    header.rom_size = 0;
    header.checksum = 0;

    rom_section_entry_t entry = {0};
    entry.type = SECTION_TYPE_CODE;
    entry.flags = 0;
    entry.file_offset = 0;
    entry.file_size = 0;
    entry.mem_address = 0;
    entry.mem_size = 0;

    string_buf sb = CreateStringBuf();

    return sb;
}