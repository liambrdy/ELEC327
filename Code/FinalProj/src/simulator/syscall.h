#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "vm.h"

/*
 * Syscall IDs — must match declaration order in res/langs/game_api.h.
 */
typedef enum {
    SYSCALL_DISPLAY_FILL        = 0,
    SYSCALL_DISPLAY_DRAW_PIXEL  = 1,
    SYSCALL_DISPLAY_FILL_RECT   = 2,
    SYSCALL_BUTTONS_READ        = 3,
    SYSCALL_MILLIS              = 4,
    SYSCALL_DISPLAY_DRAW_BITMAP = 5,
    SYSCALL_RANDOM              = 6,
    SYSCALL_SEED_RANDOM         = 7,
    SYSCALL_DISPLAY_DRAW_TEXT   = 8,
    SYSCALL_DISPLAY_DRAW_INT    = 9,
} syscall_id_t;

void sim_syscall(vm_t *vm, u8 id);

#endif
