#include "syscall.h"
#include "display.h"
#include "buttons.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>

static uint32_t rng_state = 12345;

void sim_syscall(vm_t *vm, u8 id) {
    switch ((syscall_id_t)id) {

        case SYSCALL_DISPLAY_FILL: {
            /* display_fill(int color) */
            int color = (int)vm->stack[vm->frame_base + 0];
            display_fill(color);
        } break;

        case SYSCALL_DISPLAY_DRAW_PIXEL: {
            /* display_draw_pixel(int x, int y, int color) */
            int x     = (int)vm->stack[vm->frame_base + 0];
            int y     = (int)vm->stack[vm->frame_base + 1];
            int color = (int)vm->stack[vm->frame_base + 2];
            display_draw_pixel(x, y, color);
        } break;

        case SYSCALL_DISPLAY_FILL_RECT: {
            /* display_fill_rect(int x, int y, int w, int h, int color) */
            int x     = (int)vm->stack[vm->frame_base + 0];
            int y     = (int)vm->stack[vm->frame_base + 1];
            int w     = (int)vm->stack[vm->frame_base + 2];
            int h     = (int)vm->stack[vm->frame_base + 3];
            int color = (int)vm->stack[vm->frame_base + 4];
            display_fill_rect(x, y, w, h, color);
        } break;

        case SYSCALL_BUTTONS_READ: {
            /* int buttons_read(void) — also presents the frame and caps fps */
            int mask = buttons_poll();
            vm->stack[vm->sp++] = (u32)mask;
        } break;

        case SYSCALL_MILLIS: {
            /* int millis(void) */
            u32 ms = SDL_GetTicks();
            vm->stack[vm->sp++] = ms;
        } break;

        case SYSCALL_DISPLAY_DRAW_BITMAP: {
            /* display_draw_bitmap(int x, int y, int *pixels, int w, int h) */
            int      x    = (int)vm->stack[vm->frame_base + 0];
            int      y    = (int)vm->stack[vm->frame_base + 1];
            uint32_t addr = vm->stack[vm->frame_base + 2]; /* VM byte address */
            int      w    = (int)vm->stack[vm->frame_base + 3];
            int      h    = (int)vm->stack[vm->frame_base + 4];
            const int *pixels = (const int *)(vm->memory + addr);
            display_draw_bitmap(x, y, pixels, w, h);
        } break;

        case SYSCALL_RANDOM: {
            /* int random() — LCG, returns non-negative value */
            rng_state = rng_state * 1664525u + 1013904223u;
            vm->stack[vm->sp++] = rng_state >> 1;
        } break;

        case SYSCALL_SEED_RANDOM: {
            /* void seed_random(int seed) */
            rng_state = (uint32_t)vm->stack[vm->frame_base + 0];
            if (rng_state == 0) rng_state = 1;
        } break;

        case SYSCALL_DISPLAY_DRAW_TEXT: {
            /* void display_draw_text(int x, int y, int *str, int fg, int bg) */
            int      x    = (int)vm->stack[vm->frame_base + 0];
            int      y    = (int)vm->stack[vm->frame_base + 1];
            uint32_t addr = vm->stack[vm->frame_base + 2];
            int      fg   = (int)vm->stack[vm->frame_base + 3];
            int      bg   = (int)vm->stack[vm->frame_base + 4];
            display_draw_text(x, y, (const char *)(vm->memory + addr), fg, bg);
        } break;

        case SYSCALL_DISPLAY_DRAW_INT: {
            /* void display_draw_int(int x, int y, int n, int fg, int bg) */
            int     x  = (int)vm->stack[vm->frame_base + 0];
            int     y  = (int)vm->stack[vm->frame_base + 1];
            int32_t n  = (int32_t)vm->stack[vm->frame_base + 2];
            int     fg = (int)vm->stack[vm->frame_base + 3];
            int     bg = (int)vm->stack[vm->frame_base + 4];
            char buf[12];
            int len = 0;
            if (n == 0) {
                buf[len++] = '0';
            } else {
                if (n < 0) { buf[len++] = '-'; n = -n; }
                char tmp[10]; int tlen = 0;
                uint32_t u = (uint32_t)n;
                while (u > 0) { tmp[tlen++] = (char)('0' + u % 10); u /= 10; }
                for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
            }
            buf[len] = '\0';
            display_draw_text(x, y, buf, fg, bg);
        } break;

        case SYSCALL_DISPLAY_SCROLL_DEFINE: {
            int left  = (int)vm->stack[vm->frame_base + 0];
            int width = (int)vm->stack[vm->frame_base + 1];
            int right = (int)vm->stack[vm->frame_base + 2];
            display_scroll_define(left, width, right);
        } break;

        case SYSCALL_DISPLAY_SCROLL_SET: {
            int pos = (int)vm->stack[vm->frame_base + 0];
            display_scroll_set(pos);
        } break;

        case SYSCALL_DISPLAY_DRAW_RECT: {
            int x = (int)vm->stack[vm->frame_base + 0];
            int y = (int)vm->stack[vm->frame_base + 1];
            int w = (int)vm->stack[vm->frame_base + 2];
            int h = (int)vm->stack[vm->frame_base + 3];
            int c = (int)vm->stack[vm->frame_base + 4];
            display_draw_rect(x, y, w, h, c);
        } break;

        case SYSCALL_DISPLAY_DRAW_HLINE: {
            int x = (int)vm->stack[vm->frame_base + 0];
            int y = (int)vm->stack[vm->frame_base + 1];
            int l = (int)vm->stack[vm->frame_base + 2];
            int c = (int)vm->stack[vm->frame_base + 3];
            display_draw_hline(x, y, l, c);
        } break;

        case SYSCALL_DISPLAY_DRAW_VLINE: {
            int x = (int)vm->stack[vm->frame_base + 0];
            int y = (int)vm->stack[vm->frame_base + 1];
            int l = (int)vm->stack[vm->frame_base + 2];
            int c = (int)vm->stack[vm->frame_base + 3];
            display_draw_vline(x, y, l, c);
        } break;

        case SYSCALL_DISPLAY_DRAW_LINE: {
            int x0 = (int)vm->stack[vm->frame_base + 0];
            int y0 = (int)vm->stack[vm->frame_base + 1];
            int x1 = (int)vm->stack[vm->frame_base + 2];
            int y1 = (int)vm->stack[vm->frame_base + 3];
            int c  = (int)vm->stack[vm->frame_base + 4];
            display_draw_line(x0, y0, x1, y1, c);
        } break;

        case SYSCALL_DISPLAY_FILL_CIRCLE: {
            int cx = (int)vm->stack[vm->frame_base + 0];
            int cy = (int)vm->stack[vm->frame_base + 1];
            int r  = (int)vm->stack[vm->frame_base + 2];
            int c  = (int)vm->stack[vm->frame_base + 3];
            display_fill_circle(cx, cy, r, c);
        } break;

        case SYSCALL_DISPLAY_FILL_CIRCLE_BG: {
            int cx = (int)vm->stack[vm->frame_base + 0];
            int cy = (int)vm->stack[vm->frame_base + 1];
            int r  = (int)vm->stack[vm->frame_base + 2];
            int c  = (int)vm->stack[vm->frame_base + 3];
            int bg = (int)vm->stack[vm->frame_base + 4];
            display_fill_circle_bg(cx, cy, r, c, bg);
        } break;

        case SYSCALL_DISPLAY_DRAW_CIRCLE: {
            int cx = (int)vm->stack[vm->frame_base + 0];
            int cy = (int)vm->stack[vm->frame_base + 1];
            int r  = (int)vm->stack[vm->frame_base + 2];
            int c  = (int)vm->stack[vm->frame_base + 3];
            display_draw_circle(cx, cy, r, c);
        } break;

        case SYSCALL_DISPLAY_DRAW_CHAR: {
            int x  = (int)vm->stack[vm->frame_base + 0];
            int y  = (int)vm->stack[vm->frame_base + 1];
            int ch = (int)vm->stack[vm->frame_base + 2];
            int fg = (int)vm->stack[vm->frame_base + 3];
            int bg = (int)vm->stack[vm->frame_base + 4];
            display_draw_char(x, y, ch, fg, bg);
        } break;

        case SYSCALL_DISPLAY_COMMIT: {
            display_commit();
        } break;

        default:
            printf("sim: unknown syscall id %u\n", (unsigned)id);
            break;
    }
}
