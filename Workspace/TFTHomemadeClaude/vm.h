#ifndef _VM_H
#define _VM_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  i32;

/* ---- ROM format ---- */

#define ROM_VERSION_MAJOR 1
#define ROM_GLOBAL_OFFSET 0x1000   /* globals live at this byte address in vm->memory */

#define SECTION_TYPE_CODE 1
#define SECTION_TYPE_DATA 5

typedef struct {
    u8  magic[4];
    u16 version_major;
    u16 version_minor;
    u32 flags;
    u32 entry_point;
    u32 section_count;
    u32 section_table_off;
    u32 rom_size;
    u32 checksum;
    u8  reserved[32];
} rom_header_t;

typedef struct {
    u32 type;
    u32 flags;
    u32 file_offset;
    u32 file_size;
    u32 mem_address;
    u32 mem_size;
    u8  reserved[8];
} rom_section_entry_t;

/* ---- Opcode table (must match compiler's rom.h exactly) ---- */

typedef enum {
    OPCODE_PUSH_CONST   = 0,
    OPCODE_POP          = 1,
    OPCODE_DUP          = 2,
    OPCODE_SWAP         = 3,
    OPCODE_ROT3         = 4,
    OPCODE_LOAD_LOCAL   = 5,
    OPCODE_STORE_LOCAL  = 6,
    OPCODE_LOAD_GLOBAL  = 7,
    OPCODE_STORE_GLOBAL = 8,
    OPCODE_LOAD_INDIRECT  = 9,
    OPCODE_STORE_INDIRECT = 10,
    OPCODE_FIELD_OFFSET   = 11,
    OPCODE_ADD  = 12,
    OPCODE_SUB  = 13,
    OPCODE_MUL  = 14,
    OPCODE_DIV  = 15,
    OPCODE_MOD  = 16,
    OPCODE_AND  = 17,
    OPCODE_OR   = 18,
    OPCODE_XOR  = 19,
    OPCODE_SHL  = 20,
    OPCODE_SHR  = 21,
    OPCODE_NEG       = 22,
    OPCODE_NOT       = 23,
    OPCODE_LOGIC_NOT = 24,
    OPCODE_EQ  = 25,
    OPCODE_NEQ = 26,
    OPCODE_LT  = 27,
    OPCODE_LTE = 28,
    OPCODE_GT  = 29,
    OPCODE_GTE = 30,
    OPCODE_JMP        = 31,
    OPCODE_JMP_IF     = 32,
    OPCODE_JMP_IF_NOT = 33,
    OPCODE_CALL = 34,
    OPCODE_RET  = 35,
    OPCODE_HALT    = 36,
    OPCODE_SYSCALL = 37,
    OPCODE_COUNT   = 38,
} opcode_t;

/* ---- VM sizing (tuned for 32 KB SRAM) ---- */

#define VM_STACK_SIZE   512    /* u32 slots  = 2 KB  */
#define VM_MAX_FRAMES    64    /* call depth = 512 B */
#define VM_MEMORY_SIZE  16384  /* flat RAM   = 16 KB */

/* ---- VM structs ---- */

typedef struct {
    u32 return_ip;
    u32 frame_base;
} vm_call_frame_t;

typedef enum {
    VM_OK = 0,
    VM_ERR_STACK_OVERFLOW,
    VM_ERR_STACK_UNDERFLOW,
    VM_ERR_CALL_OVERFLOW,
    VM_ERR_CALL_UNDERFLOW,
    VM_ERR_BAD_OPCODE,
    VM_ERR_DIV_ZERO,
    VM_ERR_BAD_MEMORY,
    VM_ERR_NO_HALT,
    VM_ERR_BAD_ROM,
} vm_result_t;

typedef struct vm_t {
    u32  ip;
    bool halted;

    u32  stack[VM_STACK_SIZE];
    u32  sp;
    u32  frame_base;

    vm_call_frame_t call_frames[VM_MAX_FRAMES];
    u32  call_depth;

    const u8 *code;   /* points into ROM in flash — not owned */
    u32  code_size;

    u8   memory[VM_MEMORY_SIZE];

    i32  exit_code;

    /* Called by OPCODE_SYSCALL.  Args at stack[frame_base + i].
       Push a return value onto stack[sp++] before returning if non-void. */
    void (*syscall_handler)(struct vm_t *vm, u8 id);
} vm_t;

/* ---- API ---- */

/* Point vm at a code buffer and zero memory. */
void        vm_init(vm_t *vm, const u8 *code, u32 code_size, u32 entry_point);

/* Parse a ROM binary stored in flash (or any const byte array).
   Validates the header and loads any DATA sections into vm->memory.
   Returns true on success, false if the ROM is invalid. */
bool        vm_load_rom(vm_t *vm, const u8 *rom_data, u32 rom_size);

/* Run until HALT or an error. */
vm_result_t vm_run(vm_t *vm);

#endif /* _VM_H */
