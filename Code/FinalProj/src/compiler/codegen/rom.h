#ifndef _ROM_H
#define _ROM_H

#include "ast.h"
#include "stringbuf.h"

#define ROM_VERSION_MAJOR 1
#define ROM_VERSION_MINOR 0

#define ROM_GLOBAL_OFFSET 0x1000

typedef struct rom_header_t {
    u8 magic[4]; // 'ROM!'
    u16 version_major;
    u16 version_minor;
    u32 flags;
    u32 entry_point;
    u32 section_count;
    u32 section_table_off;
    u32 rom_size;
    u32 checksum;
    u8 reserved[32];
} rom_header_t;

typedef enum rom_section_entry_type {
    SECTION_TYPE_CODE = 1,
    SECTION_TYPE_CONST,
    SECTION_TYPE_SYMBOL,
    SECTION_TYPE_DEBUG,
} rom_section_entry_type;

typedef struct rom_section_entry_t {
    u32 type;
    u32 flags;
    u32 file_offset;
    u32 file_size;
    u32 mem_address;
    u32 mem_size;
    u8 reserved[8];
} rom_section_entry_t;

typedef struct rom_code_section_fields {
    u32 instruction_count;
    u32 code_flags;
    u8 reserved[8];
} rom_code_section_fields;

typedef enum opcode {
    OPCODE_ADD,
    OPCODE_SUB,
    OPCODE_MUL,
    OPCODE_DIV,
} opcode;

typedef enum rom_symbol_entry_kind {
    SYMBOL_ENTRY_FUNCTION = 1,
    SYMBOL_ENTRY_GLOBAL,
    SYMBOL_ENTRY_LABEL,
} rom_symbol_entry_kind;

typedef struct rom_symbol_entry_t {
    u16 name_len;
    u8 *name;
    u32 address;
    u8 kind;
} rom_symbol_entry_t;

typedef struct rom_variable_t {
    u32 size;
    u32 location;
    slice_t name;
} rom_variable_t;

typedef struct rom_scope_t {
    hash_table_t *variables;
    struct rom_scope_t *parent;
} rom_scope_t;

typedef struct rom_context_t {
    rom_scope_t *scope;

    rom_symbol_entry_t *symbols;
    hash_table_t *symbolTable;

    u32 currentAddress;
    u32 currentStack;
    u32 currentGlobal;

    u8 *code;

    ast_node_t *ast;
} rom_context_t;

string_buf CodegenRom(ast_node_t *ast);

#endif