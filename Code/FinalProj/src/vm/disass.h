#ifndef _DISASS_H
#define _DISASS_H

#include "common.h"

#include "vm.h"

#define MAX_LABELS 1024

typedef struct {
    u32 addr;
} label_t;

typedef struct {
    label_t labels[MAX_LABELS];
    u32 count;
} label_table_t;

typedef struct {
    int id;
} symval_t;

#define MAX_SIM_STACK 1024

typedef struct {
    symval_t data[MAX_SIM_STACK];
    int sp;
    int next_id;
} sim_stack_t;

void disassemble(vm_t *vm);

#endif