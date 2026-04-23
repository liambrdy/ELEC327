#include "vm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---- Init ----

void VmInit(vm_t *vm, u8 *code, u32 code_size, u32 entry_point) {
    memset(vm, 0, sizeof(vm_t));
    vm->code      = code;
    vm->code_size = code_size;
    vm->ip        = entry_point;
}

// ---- ROM loader ----

u8 *VmLoadRom(vm_t *vm, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("vm: failed to open '%s'\n", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long flen = ftell(fp);
    rewind(fp);

    if (flen <= 0) {
        printf("vm: empty or unreadable file '%s'\n", path);
        fclose(fp);
        return NULL;
    }

    u8 *buf = (u8 *)malloc((u32)flen);
    if (!buf) {
        printf("vm: out of memory\n");
        fclose(fp);
        return NULL;
    }

    if (fread(buf, 1, (u32)flen, fp) != (size_t)flen) {
        printf("vm: read error on '%s'\n", path);
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    u32 len = (u32)flen;

    // Validate minimum size and magic.
    if (len < sizeof(rom_header_t)) {
        printf("vm: ROM too small (%u bytes)\n", len);
        return NULL;
    }

    rom_header_t *hdr = (rom_header_t *)buf;
    if (hdr->magic[0] != 0x52 || hdr->magic[1] != 0x4F ||
        hdr->magic[2] != 0x4D || hdr->magic[3] != 0x21) {
        printf("vm: bad magic (not a ROM! file)\n");
        return NULL;
    }

    if (hdr->version_major != ROM_VERSION_MAJOR) {
        printf("vm: unsupported ROM version %u.%u\n",
               hdr->version_major, hdr->version_minor);
        return NULL;
    }

    // Walk the section table looking for a CODE section.
    u32 sec_off = hdr->section_table_off;
    u32 sec_count = hdr->section_count;
    u8 *code_bytes = NULL;
    u32 code_size  = 0;

    for (u32 i = 0; i < sec_count; i++) {
        if (sec_off + sizeof(rom_section_entry_t) > len) {
            printf("vm: section table truncated\n");
            return NULL;
        }

        rom_section_entry_t *sec = (rom_section_entry_t *)(buf + sec_off);

        if (sec->type == SECTION_TYPE_CODE) {
            if (sec->file_offset + sec->file_size > len) {
                printf("vm: code section out of bounds\n");
                return NULL;
            }
            code_bytes = buf + sec->file_offset;
            code_size  = sec->file_size;
        } else if (sec->type == SECTION_TYPE_DATA) {
            if (sec->file_offset + sec->file_size > len) {
                printf("vm: data section out of bounds\n");
                return NULL;
            }
            if ((u64)sec->mem_address + sec->file_size > VM_MEMORY_SIZE) {
                printf("vm: data section exceeds memory\n");
                return NULL;
            }
            memcpy(vm->memory + sec->mem_address, buf + sec->file_offset, sec->file_size);
        }

        sec_off += sizeof(rom_section_entry_t);
    }

    if (!code_bytes) {
        printf("vm: no CODE section found\n");
        return NULL;
    }

    VmInit(vm, code_bytes, code_size, hdr->entry_point);
    return buf; // caller owns this; vm->code points into it
}

// ---- Result strings ----

const char *VmResultStr(vm_result_t result) {
    switch (result) {
        case VM_OK:              return "ok";
        case VM_ERR_STACK_OVERFLOW:  return "stack overflow";
        case VM_ERR_STACK_UNDERFLOW: return "stack underflow";
        case VM_ERR_CALL_OVERFLOW:   return "call stack overflow";
        case VM_ERR_CALL_UNDERFLOW:  return "call stack underflow";
        case VM_ERR_BAD_OPCODE:      return "bad opcode";
        case VM_ERR_DIV_ZERO:        return "division by zero";
        case VM_ERR_BAD_MEMORY:      return "bad memory access";
        case VM_ERR_NO_HALT:         return "ran off end of code";
        default:                     return "unknown error";
    }
}

// ---- Fetch helpers ----

static inline u8 vm_read_u8(vm_t *vm) {
    return vm->code[vm->ip++];
}

static inline u16 vm_read_u16(vm_t *vm) {
    u16 v = (u16)vm->code[vm->ip] | ((u16)vm->code[vm->ip+1] << 8);
    vm->ip += 2;
    return v;
}

static inline u32 vm_read_u32(vm_t *vm) {
    u32 v = (u32)vm->code[vm->ip]           |
            ((u32)vm->code[vm->ip+1] <<  8) |
            ((u32)vm->code[vm->ip+2] << 16) |
            ((u32)vm->code[vm->ip+3] << 24);
    vm->ip += 4;
    return v;
}

// PUSH / POP use early-return on error; only valid inside VmRun.
#define VM_PUSH(v) do { \
    if (vm->sp >= VM_STACK_SIZE) return VM_ERR_STACK_OVERFLOW; \
    vm->stack[vm->sp++] = (u32)(v); \
} while(0)

#define VM_POP(dest) do { \
    if (vm->sp == 0) return VM_ERR_STACK_UNDERFLOW; \
    (dest) = vm->stack[--vm->sp]; \
} while(0)

// ---- Fetch-decode-execute loop ----

vm_result_t VmRun(vm_t *vm) {
    while (!vm->halted) {
        if (vm->ip >= vm->code_size) return VM_ERR_NO_HALT;

        opcode op = (opcode)vm_read_u8(vm);

        switch (op) {

            // ---- Stack ----
            case OPCODE_PUSH_CONST: {
                u32 val = vm_read_u32(vm);
                VM_PUSH(val);
            } break;

            case OPCODE_POP: {
                u32 _;
                VM_POP(_);
                (void)_;
            } break;

            case OPCODE_DUP: {
                if (vm->sp == 0) return VM_ERR_STACK_UNDERFLOW;
                VM_PUSH(vm->stack[vm->sp - 1]);
            } break;

            case OPCODE_SWAP: {
                if (vm->sp < 2) return VM_ERR_STACK_UNDERFLOW;
                u32 tmp = vm->stack[vm->sp - 1];
                vm->stack[vm->sp - 1] = vm->stack[vm->sp - 2];
                vm->stack[vm->sp - 2] = tmp;
            } break;

            case OPCODE_ROT3: {
                // (a, b, c) → (c, a, b)  — c (top) sinks to third position
                if (vm->sp < 3) return VM_ERR_STACK_UNDERFLOW;
                u32 c = vm->stack[vm->sp - 1];
                u32 b = vm->stack[vm->sp - 2];
                u32 a = vm->stack[vm->sp - 3];
                vm->stack[vm->sp - 3] = c;
                vm->stack[vm->sp - 2] = a;
                vm->stack[vm->sp - 1] = b;
            } break;

            // ---- Locals ----
            case OPCODE_LOAD_LOCAL: {
                u16 slot = vm_read_u16(vm);
                u32 idx  = vm->frame_base + slot;
                if (idx >= VM_STACK_SIZE) return VM_ERR_BAD_MEMORY;
                VM_PUSH(vm->stack[idx]);
            } break;

            case OPCODE_STORE_LOCAL: {
                u16 slot = vm_read_u16(vm);
                u32 val;
                VM_POP(val);
                u32 idx = vm->frame_base + slot;
                if (idx >= VM_STACK_SIZE) return VM_ERR_BAD_MEMORY;
                vm->stack[idx] = val;
                // Keep the slot allocated below sp so future LOAD_LOCALs
                // to the same slot don't get trampled by PUSH operations.
                if (vm->sp <= idx) vm->sp = idx + 1;
            } break;

            // ---- Globals ----
            case OPCODE_LOAD_GLOBAL: {
                u32 addr = vm_read_u32(vm);
                if (addr + 4 > VM_MEMORY_SIZE) return VM_ERR_BAD_MEMORY;
                u32 val = (u32)vm->memory[addr]           |
                          ((u32)vm->memory[addr+1] <<  8) |
                          ((u32)vm->memory[addr+2] << 16) |
                          ((u32)vm->memory[addr+3] << 24);
                VM_PUSH(val);
            } break;

            case OPCODE_STORE_GLOBAL: {
                u32 addr = vm_read_u32(vm);
                u32 val;
                VM_POP(val);
                if (addr + 4 > VM_MEMORY_SIZE) return VM_ERR_BAD_MEMORY;
                vm->memory[addr]   = (u8)(val);
                vm->memory[addr+1] = (u8)(val >>  8);
                vm->memory[addr+2] = (u8)(val >> 16);
                vm->memory[addr+3] = (u8)(val >> 24);
            } break;

            // ---- Indirect ----
            case OPCODE_LOAD_INDIRECT: {
                u8 size = vm_read_u8(vm);
                u32 addr;
                VM_POP(addr);
                if ((u64)addr + size > VM_MEMORY_SIZE) return VM_ERR_BAD_MEMORY;
                u32 val = 0;
                for (u8 i = 0; i < size; i++)
                    val |= (u32)vm->memory[addr + i] << (i * 8);
                VM_PUSH(val);
            } break;

            case OPCODE_STORE_INDIRECT: {
                u8 size = vm_read_u8(vm);
                u32 val;  VM_POP(val);
                u32 addr; VM_POP(addr);
                if ((u64)addr + size > VM_MEMORY_SIZE) return VM_ERR_BAD_MEMORY;
                for (u8 i = 0; i < size; i++)
                    vm->memory[addr + i] = (u8)(val >> (i * 8));
            } break;

            case OPCODE_FIELD_OFFSET: {
                u32 off = vm_read_u32(vm);
                u32 addr;
                VM_POP(addr);
                VM_PUSH(addr + off);
            } break;

            // ---- Binary arithmetic ----
            case OPCODE_ADD: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a + b); } break;
            case OPCODE_SUB: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a - b); } break;
            case OPCODE_MUL: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a * b); } break;
            case OPCODE_DIV: {
                u32 b; VM_POP(b);
                u32 a; VM_POP(a);
                if (b == 0) return VM_ERR_DIV_ZERO;
                VM_PUSH((u32)((i32)a / (i32)b));
            } break;
            case OPCODE_MOD: {
                u32 b; VM_POP(b);
                u32 a; VM_POP(a);
                if (b == 0) return VM_ERR_DIV_ZERO;
                VM_PUSH((u32)((i32)a % (i32)b));
            } break;

            // ---- Bitwise ----
            case OPCODE_AND: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a & b); } break;
            case OPCODE_OR:  { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a | b); } break;
            case OPCODE_XOR: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a ^ b); } break;
            case OPCODE_SHL: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a << (b & 31)); } break;
            case OPCODE_SHR: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a >> (b & 31)); } break;

            // ---- Unary ----
            case OPCODE_NEG:       { u32 a; VM_POP(a); VM_PUSH((u32)(-(i32)a));    } break;
            case OPCODE_NOT:       { u32 a; VM_POP(a); VM_PUSH(~a);                 } break;
            case OPCODE_LOGIC_NOT: { u32 a; VM_POP(a); VM_PUSH(a == 0 ? 1u : 0u); } break;

            // ---- Comparisons (signed) ----
            case OPCODE_EQ:  { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a == b          ? 1u : 0u); } break;
            case OPCODE_NEQ: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a != b          ? 1u : 0u); } break;
            case OPCODE_LT:  { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH((i32)a <  (i32)b ? 1u : 0u); } break;
            case OPCODE_LTE: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH((i32)a <= (i32)b ? 1u : 0u); } break;
            case OPCODE_GT:  { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH((i32)a >  (i32)b ? 1u : 0u); } break;
            case OPCODE_GTE: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH((i32)a >= (i32)b ? 1u : 0u); } break;

            // ---- Control flow ----
            case OPCODE_JMP: {
                vm->ip = vm_read_u32(vm);
            } break;

            case OPCODE_JMP_IF: {
                u32 target = vm_read_u32(vm);
                u32 cond; VM_POP(cond);
                if (cond != 0) vm->ip = target;
            } break;

            case OPCODE_JMP_IF_NOT: {
                u32 target = vm_read_u32(vm);
                u32 cond; VM_POP(cond);
                if (cond == 0) vm->ip = target;
            } break;

            // ---- Functions ----
            case OPCODE_CALL: {
                u32 target = vm_read_u32(vm);
                u8  argc   = vm_read_u8(vm);

                if (vm->call_depth >= VM_MAX_FRAMES) return VM_ERR_CALL_OVERFLOW;

                vm->call_frames[vm->call_depth++] = (vm_call_frame_t){
                    .return_ip  = vm->ip,
                    .frame_base = vm->frame_base,
                };

                // Args are the top `argc` values already on the stack.
                vm->frame_base = vm->sp - argc;
                vm->ip = target;
            } break;

            case OPCODE_RET: {
                // Capture return value (topmost value above frame_base, if any).
                bool has_retval = (vm->sp > vm->frame_base);
                u32 retval = has_retval ? vm->stack[vm->sp - 1] : 0;

                // Discard the callee's entire frame (args + locals + temporaries).
                vm->sp = vm->frame_base;

                if (vm->call_depth == 0) {
                    // Outermost return (from main) — stop execution.
                    vm->exit_code = (i32)retval;
                    vm->halted    = true;
                } else {
                    vm_call_frame_t f = vm->call_frames[--vm->call_depth];
                    vm->ip         = f.return_ip;
                    vm->frame_base = f.frame_base;
                    if (has_retval) VM_PUSH(retval);
                }
            } break;

            // ---- Halt ----
            case OPCODE_HALT: {
                vm->halted = true;
            } break;

            // ---- Host interface ----
            case OPCODE_SYSCALL: {
                u8 id = vm_read_u8(vm);
                if (vm->syscall_handler) vm->syscall_handler(vm, id);
            } break;

            default:
                return VM_ERR_BAD_OPCODE;
        }
    }

    return VM_OK;
}

#undef VM_PUSH
#undef VM_POP
