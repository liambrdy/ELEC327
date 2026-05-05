#include "vm.h"

void vm_init(vm_t *vm, const u8 *code, u32 code_size, u32 entry_point) {
    memset(vm, 0, sizeof(vm_t));
    vm->code      = code;
    vm->code_size = code_size;
    vm->ip        = entry_point;
}

bool vm_load_rom(vm_t *vm, const u8 *rom_data, u32 rom_size) {
    if (!rom_data || rom_size < sizeof(rom_header_t))
        return false;

    const rom_header_t *hdr = (const rom_header_t *)rom_data;

    if (hdr->magic[0] != 0x52 || hdr->magic[1] != 0x4F ||
        hdr->magic[2] != 0x4D || hdr->magic[3] != 0x21)
        return false;

    if (hdr->version_major != ROM_VERSION_MAJOR)
        return false;

    memset(vm, 0, sizeof(vm_t));

    const u8 *code_bytes = 0;
    u32       code_size  = 0;
    u32       sec_off    = hdr->section_table_off;

    for (u32 i = 0; i < hdr->section_count; i++) {
        if (sec_off + sizeof(rom_section_entry_t) > rom_size)
            return false;

        const rom_section_entry_t *sec =
            (const rom_section_entry_t *)(rom_data + sec_off);

        if (sec->type == SECTION_TYPE_CODE) {
            if (sec->file_offset + sec->file_size > rom_size)
                return false;
            code_bytes = rom_data + sec->file_offset;
            code_size  = sec->file_size;
        } else if (sec->type == SECTION_TYPE_DATA) {
            if (sec->file_offset + sec->file_size > rom_size)
                return false;
            if (sec->mem_address + sec->file_size > VM_MEMORY_SIZE)
                return false;
            memcpy(vm->memory + sec->mem_address,
                   rom_data + sec->file_offset,
                   sec->file_size);
        }

        sec_off += sizeof(rom_section_entry_t);
    }

    if (!code_bytes)
        return false;

    vm->code      = code_bytes;
    vm->code_size = code_size;
    vm->ip        = hdr->entry_point;
    return true;
}

static inline u8 fetch_u8(vm_t *vm) {
    return vm->code[vm->ip++];
}

static inline u16 fetch_u16(vm_t *vm) {
    u16 v = (u16)vm->code[vm->ip] | ((u16)vm->code[vm->ip + 1] << 8);
    vm->ip += 2;
    return v;
}

static inline u32 fetch_u32(vm_t *vm) {
    u32 v = (u32)vm->code[vm->ip]
          | ((u32)vm->code[vm->ip + 1] <<  8)
          | ((u32)vm->code[vm->ip + 2] << 16)
          | ((u32)vm->code[vm->ip + 3] << 24);
    vm->ip += 4;
    return v;
}

#define VM_PUSH(v) do { \
    if (vm->sp >= VM_STACK_SIZE) return VM_ERR_STACK_OVERFLOW; \
    vm->stack[vm->sp++] = (u32)(v); \
} while (0)

#define VM_POP(dst) do { \
    if (vm->sp == 0) return VM_ERR_STACK_UNDERFLOW; \
    (dst) = vm->stack[--vm->sp]; \
} while (0)

vm_result_t vm_run(vm_t *vm) {
    while (!vm->halted) {
        if (vm->ip >= vm->code_size)
            return VM_ERR_NO_HALT;

        opcode_t op = (opcode_t)fetch_u8(vm);

        switch (op) {

            case OPCODE_PUSH_CONST: {
                u32 val = fetch_u32(vm);
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
                u32 tmp             = vm->stack[vm->sp - 1];
                vm->stack[vm->sp-1] = vm->stack[vm->sp - 2];
                vm->stack[vm->sp-2] = tmp;
            } break;

            case OPCODE_ROT3: {
                /* (a, b, c) → (c, a, b) */
                if (vm->sp < 3) return VM_ERR_STACK_UNDERFLOW;
                u32 c = vm->stack[vm->sp - 1];
                u32 b = vm->stack[vm->sp - 2];
                u32 a = vm->stack[vm->sp - 3];
                vm->stack[vm->sp - 3] = c;
                vm->stack[vm->sp - 2] = a;
                vm->stack[vm->sp - 1] = b;
            } break;

            case OPCODE_LOAD_LOCAL: {
                u16 slot = fetch_u16(vm);
                u32 idx  = vm->frame_base + slot;
                if (idx >= VM_STACK_SIZE) return VM_ERR_BAD_MEMORY;
                VM_PUSH(vm->stack[idx]);
            } break;

            case OPCODE_STORE_LOCAL: {
                u16 slot = fetch_u16(vm);
                u32 val;
                VM_POP(val);
                u32 idx = vm->frame_base + slot;
                if (idx >= VM_STACK_SIZE) return VM_ERR_BAD_MEMORY;
                vm->stack[idx] = val;
                if (vm->sp <= idx) vm->sp = idx + 1;
            } break;

            case OPCODE_LOAD_GLOBAL: {
                u32 addr = fetch_u32(vm);
                if (addr + 4 > VM_MEMORY_SIZE) return VM_ERR_BAD_MEMORY;
                u32 val = (u32)vm->memory[addr]
                        | ((u32)vm->memory[addr+1] <<  8)
                        | ((u32)vm->memory[addr+2] << 16)
                        | ((u32)vm->memory[addr+3] << 24);
                VM_PUSH(val);
            } break;

            case OPCODE_STORE_GLOBAL: {
                u32 addr = fetch_u32(vm);
                u32 val;
                VM_POP(val);
                if (addr + 4 > VM_MEMORY_SIZE) return VM_ERR_BAD_MEMORY;
                vm->memory[addr]   = (u8)(val);
                vm->memory[addr+1] = (u8)(val >>  8);
                vm->memory[addr+2] = (u8)(val >> 16);
                vm->memory[addr+3] = (u8)(val >> 24);
            } break;

            case OPCODE_LOAD_INDIRECT: {
                u8 size = fetch_u8(vm);
                u32 addr;
                VM_POP(addr);
                if (addr + size > VM_MEMORY_SIZE) return VM_ERR_BAD_MEMORY;
                u32 val = 0;
                for (u8 i = 0; i < size; i++)
                    val |= (u32)vm->memory[addr + i] << (i * 8);
                VM_PUSH(val);
            } break;

            case OPCODE_STORE_INDIRECT: {
                u8 size = fetch_u8(vm);
                u32 val;  VM_POP(val);
                u32 addr; VM_POP(addr);
                if (addr + size > VM_MEMORY_SIZE) return VM_ERR_BAD_MEMORY;
                for (u8 i = 0; i < size; i++)
                    vm->memory[addr + i] = (u8)(val >> (i * 8));
            } break;

            case OPCODE_FIELD_OFFSET: {
                u32 off = fetch_u32(vm);
                u32 addr;
                VM_POP(addr);
                VM_PUSH(addr + off);
            } break;

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

            case OPCODE_AND: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a & b); } break;
            case OPCODE_OR:  { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a | b); } break;
            case OPCODE_XOR: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a ^ b); } break;
            case OPCODE_SHL: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a << (b & 31)); } break;
            case OPCODE_SHR: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a >> (b & 31)); } break;

            case OPCODE_NEG:       { u32 a; VM_POP(a); VM_PUSH((u32)(-(i32)a));    } break;
            case OPCODE_NOT:       { u32 a; VM_POP(a); VM_PUSH(~a);                } break;
            case OPCODE_LOGIC_NOT: { u32 a; VM_POP(a); VM_PUSH(a == 0 ? 1u : 0u); } break;

            case OPCODE_EQ:  { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a == b            ? 1u:0u); } break;
            case OPCODE_NEQ: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH(a != b            ? 1u:0u); } break;
            case OPCODE_LT:  { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH((i32)a <  (i32)b  ? 1u:0u); } break;
            case OPCODE_LTE: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH((i32)a <= (i32)b  ? 1u:0u); } break;
            case OPCODE_GT:  { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH((i32)a >  (i32)b  ? 1u:0u); } break;
            case OPCODE_GTE: { u32 b; VM_POP(b); u32 a; VM_POP(a); VM_PUSH((i32)a >= (i32)b  ? 1u:0u); } break;

            case OPCODE_JMP: {
                vm->ip = fetch_u32(vm);
            } break;

            case OPCODE_JMP_IF: {
                u32 target = fetch_u32(vm);
                u32 cond; VM_POP(cond);
                if (cond != 0) vm->ip = target;
            } break;

            case OPCODE_JMP_IF_NOT: {
                u32 target = fetch_u32(vm);
                u32 cond; VM_POP(cond);
                if (cond == 0) vm->ip = target;
            } break;

            case OPCODE_CALL: {
                u32 target = fetch_u32(vm);
                u8  argc   = fetch_u8(vm);
                if (vm->call_depth >= VM_MAX_FRAMES)
                    return VM_ERR_CALL_OVERFLOW;
                vm->call_frames[vm->call_depth++] = (vm_call_frame_t){
                    .return_ip  = vm->ip,
                    .frame_base = vm->frame_base,
                };
                vm->frame_base = vm->sp - argc;
                vm->ip         = target;
            } break;

            case OPCODE_RET: {
                bool has_retval = (vm->sp > vm->frame_base);
                u32 retval = has_retval ? vm->stack[vm->sp - 1] : 0;
                vm->sp = vm->frame_base;
                if (vm->call_depth == 0) {
                    vm->exit_code = (i32)retval;
                    vm->halted    = true;
                } else {
                    vm_call_frame_t f = vm->call_frames[--vm->call_depth];
                    vm->ip         = f.return_ip;
                    vm->frame_base = f.frame_base;
                    if (has_retval) VM_PUSH(retval);
                }
            } break;

            case OPCODE_HALT: {
                vm->halted = true;
            } break;

            case OPCODE_SYSCALL: {
                u8 id = fetch_u8(vm);
                if (vm->syscall_handler)
                    vm->syscall_handler(vm, id);
            } break;

            default:
                return VM_ERR_BAD_OPCODE;
        }
    }

    return VM_OK;
}

#undef VM_PUSH
#undef VM_POP
