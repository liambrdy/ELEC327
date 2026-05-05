#ifndef _GAME_SYSCALL_H
#define _GAME_SYSCALL_H

#include "vm.h"

/* IDs must match declaration order in res/langs/game_api.h */
typedef enum {
    SYSCALL_DISPLAY_FILL          = 0,
    SYSCALL_DISPLAY_DRAW_PIXEL    = 1,
    SYSCALL_DISPLAY_FILL_RECT     = 2,
    SYSCALL_BUTTONS_READ          = 3,
    SYSCALL_MILLIS                = 4,
    SYSCALL_DISPLAY_DRAW_BITMAP   = 5,
    SYSCALL_RANDOM                = 6,
    SYSCALL_SEED_RANDOM           = 7,
    SYSCALL_DISPLAY_DRAW_TEXT     = 8,
    SYSCALL_DISPLAY_DRAW_INT      = 9,
    SYSCALL_DISPLAY_SCROLL_DEFINE = 10,
    SYSCALL_DISPLAY_SCROLL_SET    = 11,
    SYSCALL_DISPLAY_DRAW_RECT     = 12,
    SYSCALL_DISPLAY_DRAW_HLINE    = 13,
    SYSCALL_DISPLAY_DRAW_VLINE    = 14,
    SYSCALL_DISPLAY_DRAW_LINE     = 15,
    SYSCALL_DISPLAY_FILL_CIRCLE   = 16,
    SYSCALL_DISPLAY_FILL_CIRCLE_BG = 17,
    SYSCALL_DISPLAY_DRAW_CIRCLE   = 18,
    SYSCALL_DISPLAY_DRAW_CHAR     = 19,
    SYSCALL_DISPLAY_COMMIT        = 20,
} syscall_id_t;

#define VM_DISPLAY_W 320
#define VM_DISPLAY_H 240

#define CHROMA_KEY 0xF81FU  /* COLOR_MAGENTA — transparent in draw_bitmap */

extern volatile uint32_t millis_tick;

void game_syscall(vm_t *vm, u8 id);

#endif /* _GAME_SYSCALL_H */
