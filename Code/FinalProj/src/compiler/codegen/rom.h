#ifndef _ROM_H
#define _ROM_H

#include "ast.h"
#include "semantics.h"
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
    SECTION_TYPE_DATA = 5, // raw bytes loaded directly into vm->memory at mem_address
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

// Instruction encoding key:
//   [op]          -- 1-byte opcode, no operands
//   [op][u8  x]   -- opcode + 1-byte operand
//   [op][u16 x]   -- opcode + 2-byte little-endian operand
//   [op][u32 x]   -- opcode + 4-byte little-endian operand
//
// Stack notation: (top ... bottom) before → after
typedef enum opcode {
    // ---- Stack ----
    // [op][u32 val]   push a 4-byte constant
    OPCODE_PUSH_CONST,
    // [op]            discard top of stack
    OPCODE_POP,
    // [op]            duplicate top of stack
    OPCODE_DUP,
    // [op]            swap top two stack values
    OPCODE_SWAP,
    // [op]            rotate top three: (a,b,c) -> (c,a,b)  [c ends up at bottom of top-3]
    OPCODE_ROT3,

    // ---- Local variable access (index relative to the current frame base) ----
    // [op][u16 slot]  push the value in local slot <slot>
    OPCODE_LOAD_LOCAL,
    // [op][u16 slot]  (val) → ()  store val into local slot <slot>
    OPCODE_STORE_LOCAL,

    // ---- Global variable access (absolute address in the VM's memory map) ----
    // [op][u32 addr]  push the 4-byte value at address <addr>
    OPCODE_LOAD_GLOBAL,
    // [op][u32 addr]  (val) → ()  store val at address <addr>
    OPCODE_STORE_GLOBAL,

    // ---- Pointer / indirect access ----
    // [op][u8 size]   (addr) → (val)  load <size> bytes from addr, zero-extend to 4
    OPCODE_LOAD_INDIRECT,
    // [op][u8 size]   (addr, val) → ()  store low <size> bytes of val at addr
    OPCODE_STORE_INDIRECT,
    // [op][u32 off]   (addr) → (addr+off)  add a compile-time byte offset to an address;
    //                 used to reach a struct field after pushing the struct's base address
    OPCODE_FIELD_OFFSET,

    // ---- Binary arithmetic (a, b) → (result), b is top of stack ----
    OPCODE_ADD,
    OPCODE_SUB,
    OPCODE_MUL,
    OPCODE_DIV,
    OPCODE_MOD,

    // ---- Bitwise binary (a, b) → (result) ----
    OPCODE_AND,
    OPCODE_OR,
    OPCODE_XOR,
    OPCODE_SHL,
    OPCODE_SHR,

    // ---- Unary (val) → (result) ----
    OPCODE_NEG,       // arithmetic negation  (-val)
    OPCODE_NOT,       // bitwise NOT          (~val)
    OPCODE_LOGIC_NOT, // logical NOT          (val == 0 ? 1 : 0)

    // ---- Comparison (a, b) → (0 or 1), b is top of stack ----
    OPCODE_EQ,
    OPCODE_NEQ,
    OPCODE_LT,
    OPCODE_LTE,
    OPCODE_GT,
    OPCODE_GTE,

    // ---- Control flow ----
    // [op][u32 addr]  unconditional jump to <addr>
    OPCODE_JMP,
    // [op][u32 addr]  (cond) → ()  jump to <addr> if cond != 0
    OPCODE_JMP_IF,
    // [op][u32 addr]  (cond) → ()  jump to <addr> if cond == 0
    OPCODE_JMP_IF_NOT,

    // ---- Functions ----
    // [op][u32 addr][u8 arg_count]  save frame, set frame_base = sp - arg_count, jump to <addr>
    OPCODE_CALL,
    // [op]            restore caller frame, jump to saved return address;
    //                 return value (if any) must already be on the stack
    OPCODE_RET,

    // ---- Miscellaneous ----
    // [op]            halt execution
    OPCODE_HALT,
    // [op][u8 id]     call host/simulator function by id; args at stack[frame_base+];
    //                 push return value before returning if non-void
    OPCODE_SYSCALL,

    OPCODE_COUNT,
} opcode;

typedef enum rom_symbol_entry_kind {
    SYMBOL_ENTRY_FUNCTION = 1,
    SYMBOL_ENTRY_GLOBAL,
    SYMBOL_ENTRY_LABEL,
} rom_symbol_entry_kind;

typedef struct rom_symbol_entry_t {
    slice_t name;
    u32 address;
    u8 kind;
} rom_symbol_entry_t;

typedef struct rom_variable_t {
    u32 size;
    u32 location; // slot index for locals, absolute address for globals
    bool isGlobal;
    slice_t name;
    type_t *varType; // NULL for scalars; struct type for struct variables
} rom_variable_t;

typedef struct rom_scope_t {
    hash_table_t *variables;
    struct rom_scope_t *parent;
} rom_scope_t;

// Per-loop bookkeeping for break/continue backpatching.
typedef struct loop_ctx_t {
    u32 *breakPatches;    // DArray<u32>: code-buffer offsets of JMP placeholders
    u32 *continuePatches; // DArray<u32>: code-buffer offsets of JMP placeholders
} loop_ctx_t;

typedef struct rom_context_t {
    rom_scope_t *scope;

    rom_symbol_entry_t *symbols;
    hash_table_t *symbolTable;

    u32 currentAddress;
    u32 frameSlot;    // next free local slot in the current function frame
    u32 currentGlobal;

    u8 *code;

    loop_ctx_t *loopStack; // DArray<loop_ctx_t>

    // String literal interning — collected during codegen, patched afterwards.
    u8 *stringData;     // DArray<u8>: null-terminated strings concatenated
    u32 *stringRefs;    // DArray<u32>: code-buffer offsets of PUSH_CONST operands to patch
    u32 *stringOffsets; // DArray<u32>: offset within stringData for each ref

    u32 nextSyscallId;  // next ID to assign to a host function prototype

    ast_node_t *ast;
} rom_context_t;

u32 GetTypeSize(type_t *type);
// Generates a ROM binary from the given AST. Returns a malloc'd buffer;
// sets *outSize to the number of bytes written.
u8 *CodegenRom(ast_node_t *ast, u32 *outSize);

#endif