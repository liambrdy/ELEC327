#include "disass.h"

#include <stdio.h>

static symval_t new_val(sim_stack_t *s) {
    symval_t v = { .id = s->next_id++ };
    return v;
}

static void sim_push(sim_stack_t *s, symval_t v) {
    if (s->sp < MAX_SIM_STACK)
        s->data[s->sp++] = v;
}

static symval_t sim_pop(sim_stack_t *s) {
    if (s->sp == 0) return (symval_t){ .id = -1 };
    return s->data[--s->sp];
}

static void print_stack(sim_stack_t *s) {
    printf("    ; [");
    for (int i = 0; i < s->sp; i++) {
        printf("v%d", s->data[i].id);
        if (i != s->sp - 1) printf(", ");
    }
    printf("]\n");
}

static void add_label(label_table_t *t, u32 addr) {
    for (u32 i = 0; i < t->count; i++) {
        if (t->labels[i].addr == addr) return; // dedupe
    }
    if (t->count < MAX_LABELS) {
        t->labels[t->count++].addr = addr;
    }
}

static int find_label(label_table_t *t, u32 addr) {
    for (u32 i = 0; i < t->count; i++) {
        if (t->labels[i].addr == addr) return (int)i;
    }
    return -1;
}

void collect_labels(u8 *code, u32 size, label_table_t *labels) {
    u32 ip = 0;

    while (ip < size) {
        opcode op = (opcode)code[ip++];

        switch (op) {
            case OPCODE_PUSH_CONST: ip += 4; break;
            case OPCODE_LOAD_LOCAL:
            case OPCODE_STORE_LOCAL: ip += 2; break;

            case OPCODE_LOAD_GLOBAL:
            case OPCODE_STORE_GLOBAL:
            case OPCODE_FIELD_OFFSET: ip += 4; break;

            case OPCODE_LOAD_INDIRECT:
            case OPCODE_STORE_INDIRECT: ip += 1; break;

            case OPCODE_JMP:
            case OPCODE_JMP_IF:
            case OPCODE_JMP_IF_NOT: {
                u32 target =
                    (u32)code[ip] |
                    ((u32)code[ip+1] << 8) |
                    ((u32)code[ip+2] << 16) |
                    ((u32)code[ip+3] << 24);
                add_label(labels, target);
                ip += 4;
            } break;

            case OPCODE_CALL: {
                u32 target =
                    (u32)code[ip] |
                    ((u32)code[ip+1] << 8) |
                    ((u32)code[ip+2] << 16) |
                    ((u32)code[ip+3] << 24);
                add_label(labels, target);
                ip += 5; // target + argc
            } break;

            case OPCODE_SYSCALL: ip += 1; break;

            default:
                // single byte ops
                break;
        }
    }
}

static u8 read_u8(u8 *code, u32 *ip) {
    return code[(*ip)++];
}

static u16 read_u16(u8 *code, u32 *ip) {
    u16 v = (u16)code[*ip] | ((u16)code[*ip + 1] << 8);
    *ip += 2;
    return v;
}

static u32 read_u32(u8 *code, u32 *ip) {
    u32 v = (u32)code[*ip] |
            ((u32)code[*ip + 1] << 8) |
            ((u32)code[*ip + 2] << 16) |
            ((u32)code[*ip + 3] << 24);
    *ip += 4;
    return v;
}

void disassemble(vm_t *vm) {
    u8 *code = vm->code;
    u32 size = vm->code_size;
    
    label_table_t labels = {0};

    add_label(&labels, vm->ip);
    collect_labels(code, size, &labels);
    
    sim_stack_t sim = {0};

    u32 ip = 0;

    while (ip < size) {
        int lbl = find_label(&labels, ip);
        if (lbl >= 0) {
            printf("L%d:\n", lbl);
        }

        u32 addr = ip;
        opcode op = (opcode)read_u8(code, &ip);

        printf("    %04x: %02x ", addr, code[addr]);
        // printf("    %04x: ", addr);

        switch (op) {
            case OPCODE_PUSH_CONST: {
                u32 v = read_u32(code, &ip);
                printf("PUSH_CONST %u\n", v);
            } break;

            case OPCODE_POP:        printf("POP\n"); break;
            case OPCODE_DUP:        printf("DUP\n"); break;
            case OPCODE_SWAP:       printf("SWAP\n"); break;
            case OPCODE_ROT3:       printf("ROT3\n"); break;

            case OPCODE_LOAD_LOCAL: {
                u16 s = read_u16(code, &ip);
                printf("LOAD_LOCAL %u\n", s);
            } break;

            case OPCODE_STORE_LOCAL: {
                u16 s = read_u16(code, &ip);
                printf("STORE_LOCAL %u\n", s);
            } break;

            case OPCODE_LOAD_GLOBAL: {
                u32 a = read_u32(code, &ip);
                printf("LOAD_GLOBAL [0x%08x]\n", a);
            } break;

            case OPCODE_STORE_GLOBAL: {
                u32 a = read_u32(code, &ip);
                printf("STORE_GLOBAL [0x%08x]\n", a);
            } break;

            case OPCODE_LOAD_INDIRECT: {
                u8 sz = read_u8(code, &ip);
                printf("LOAD_INDIRECT %u\n", sz);
            } break;

            case OPCODE_STORE_INDIRECT: {
                u8 sz = read_u8(code, &ip);
                printf("STORE_INDIRECT %u\n", sz);
            } break;

            case OPCODE_FIELD_OFFSET: {
                u32 off = read_u32(code, &ip);
                printf("FIELD_OFFSET %u\n", off);
            } break;

            case OPCODE_ADD: printf("ADD\n"); break;
            case OPCODE_SUB: printf("SUB\n"); break;
            case OPCODE_MUL: printf("MUL\n"); break;
            case OPCODE_DIV: printf("DIV\n"); break;
            case OPCODE_MOD: printf("MOD\n"); break;

            case OPCODE_AND: printf("AND\n"); break;
            case OPCODE_OR:  printf("OR\n"); break;
            case OPCODE_XOR: printf("XOR\n"); break;
            case OPCODE_SHL: printf("SHL\n"); break;
            case OPCODE_SHR: printf("SHR\n"); break;

            case OPCODE_NEG:       printf("NEG\n"); break;
            case OPCODE_NOT:       printf("NOT\n"); break;
            case OPCODE_LOGIC_NOT: printf("LOGIC_NOT\n"); break;

            case OPCODE_EQ:  printf("EQ\n"); break;
            case OPCODE_NEQ: printf("NEQ\n"); break;
            case OPCODE_LT:  printf("LT\n"); break;
            case OPCODE_LTE: printf("LTE\n"); break;
            case OPCODE_GT:  printf("GT\n"); break;
            case OPCODE_GTE: printf("GTE\n"); break;

            case OPCODE_JMP: {
                u32 t = read_u32(code, &ip);
                printf("JMP L%d\n", find_label(&labels, t));
            } break;

            case OPCODE_JMP_IF: {
                u32 t = read_u32(code, &ip);
                printf("JMP_IF L%d\n", find_label(&labels, t));
            } break;

            case OPCODE_JMP_IF_NOT: {
                u32 t = read_u32(code, &ip);
                printf("JMP_IF_NOT L%d\n", find_label(&labels, t));
            } break;

            case OPCODE_CALL: {
                u32 t = read_u32(code, &ip);
                u8 argc = read_u8(code, &ip);
                printf("CALL L%d argc=%u\n", find_label(&labels, t), argc);
            } break;

            case OPCODE_RET: printf("RET\n"); break;
            case OPCODE_HALT: printf("HALT\n"); break;

            case OPCODE_SYSCALL: {
                u8 id = read_u8(code, &ip);
                printf("SYSCALL %u\n", id);
            } break;

            default:
                printf("UNKNOWN (%u)\n", op);
                return;
        }
    }
}