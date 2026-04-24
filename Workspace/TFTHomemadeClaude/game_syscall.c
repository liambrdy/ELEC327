#include "game_syscall.h"
#include "tft.h"
#include "font5x7.h"

#include <stdint.h>
#include <stdbool.h>

static uint32_t rng_state = 12345u;

volatile uint32_t millis_tick = 0;

/* SysTick fires every 1 ms at 32 MHz (LOAD = 31999). */
void SysTick_Handler(void) {
    millis_tick++;
}

/* ---- Helper: read a little-endian u16 from vm->memory ---- */
static inline uint16_t mem_read_u16(const vm_t *vm, uint32_t addr) {
    return (uint16_t)vm->memory[addr] | ((uint16_t)vm->memory[addr + 1] << 8);
}

/* ---- Helper: draw one character of the 5x7 font to the TFT ---- */
static void draw_char_hw(int cx, int cy, uint8_t ch, uint16_t fg, int bg) {
    int has_bg = (bg >= 0 && bg <= 0xFFFF);
    uint16_t bg16 = (uint16_t)bg;
    const unsigned char *glyph = (ch >= 0x20 && ch <= 0x7E)
                                 ? font5x7[ch - 0x20]
                                 : font5x7[0];
    if (cx >= VM_DISPLAY_W) return;

    for (int row = 0; row < FONT_CHAR_H; row++) {
        int py = cy + row;
        if (py < 0 || py >= VM_DISPLAY_H) continue;
        int x0 = cx < 0 ? 0 : cx;
        int x1 = (cx + FONT_CHAR_W - 1 >= VM_DISPLAY_W)
                 ? VM_DISPLAY_W - 1 : cx + FONT_CHAR_W - 1;
        if (x0 > x1) continue;
        if (has_bg) {
            TFTBeginPixels((uint16_t)x0, (uint16_t)py,
                           (uint16_t)x1, (uint16_t)py);
            for (int col = x0 - cx; col <= x1 - cx; col++) {
                int on = (col < 5 && row < 7) && ((glyph[col] >> row) & 1);
                TFTSendPixel(on ? fg : bg16);
            }
            TFTEndPixels();
        } else {
            for (int col = x0 - cx; col <= x1 - cx; col++) {
                if (col >= 5 || row >= 7) continue;
                if (!((glyph[col] >> row) & 1)) continue;
                int px = cx + col;
                TFTFillRegion((uint16_t)px, (uint16_t)py,
                              (uint16_t)px, (uint16_t)py, fg);
            }
        }
    }
}

/* ---- Syscall dispatcher ---- */

void game_syscall(vm_t *vm, u8 id) {
    switch ((syscall_id_t)id) {

        /* display_fill(int color) ----------------------------------------- */
        case SYSCALL_DISPLAY_FILL: {
            uint16_t color = (uint16_t)vm->stack[vm->frame_base + 0];
            TFTFillRegion(0, 0, VM_DISPLAY_W - 1, VM_DISPLAY_H - 1, color);
        } break;

        /* display_draw_pixel(int x, int y, int color) ---------------------- */
        case SYSCALL_DISPLAY_DRAW_PIXEL: {
            int x     = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int y     = (int)(int32_t)vm->stack[vm->frame_base + 1];
            uint16_t c = (uint16_t)vm->stack[vm->frame_base + 2];
            if (x >= 0 && x < VM_DISPLAY_W && y >= 0 && y < VM_DISPLAY_H)
                TFTFillRegion((uint16_t)x, (uint16_t)y,
                              (uint16_t)x, (uint16_t)y, c);
        } break;

        /* display_fill_rect(int x, int y, int w, int h, int color) --------- */
        case SYSCALL_DISPLAY_FILL_RECT: {
            int x = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int y = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int w = (int)(int32_t)vm->stack[vm->frame_base + 2];
            int h = (int)(int32_t)vm->stack[vm->frame_base + 3];
            uint16_t c = (uint16_t)vm->stack[vm->frame_base + 4];

            /* clamp to screen */
            int x0 = x < 0 ? 0 : x;
            int y0 = y < 0 ? 0 : y;
            int x1 = x + w - 1;
            int y1 = y + h - 1;
            if (x1 >= VM_DISPLAY_W) x1 = VM_DISPLAY_W - 1;
            if (y1 >= VM_DISPLAY_H) y1 = VM_DISPLAY_H - 1;
            if (x0 > x1 || y0 > y1) break;

            TFTFillRegion((uint16_t)x0, (uint16_t)y0,
                          (uint16_t)x1, (uint16_t)y1, c);
        } break;

        /* int buttons_read() ----------------------------------------------- */
        case SYSCALL_BUTTONS_READ: {
            /*
             * TODO: wire up GPIO buttons.
             * Bit layout (matches game_api.h):
             *   bit 0 = BTN_UP    bit 1 = BTN_DOWN
             *   bit 2 = BTN_LEFT  bit 3 = BTN_RIGHT
             *   bit 4 = BTN_A     bit 5 = BTN_B
             */
            vm->stack[vm->sp++] = 0;
        } break;

        /* int millis() ----------------------------------------------------- */
        case SYSCALL_MILLIS: {
            vm->stack[vm->sp++] = millis_tick;
        } break;

        /* display_draw_bitmap(int x, int y, int *pixels, int w, int h) ----- */
        case SYSCALL_DISPLAY_DRAW_BITMAP: {
            int      x    = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int      y    = (int)(int32_t)vm->stack[vm->frame_base + 1];
            uint32_t addr = vm->stack[vm->frame_base + 2]; /* VM byte address */
            int      bw   = (int)(int32_t)vm->stack[vm->frame_base + 3];
            int      bh   = (int)(int32_t)vm->stack[vm->frame_base + 4];

            for (int row = 0; row < bh; row++) {
                int sy = y + row;
                if (sy < 0 || sy >= VM_DISPLAY_H) continue;

                int col = 0;
                while (col < bw) {
                    /* skip transparent and off-screen pixels */
                    while (col < bw) {
                        int sx = x + col;
                        if (sx >= VM_DISPLAY_W) { col = bw; break; }
                        if (sx >= 0) {
                            uint32_t pa = addr + ((uint32_t)row * bw + col) * 4;
                            if (pa + 2 <= VM_MEMORY_SIZE &&
                                mem_read_u16(vm, pa) != CHROMA_KEY)
                                break; /* found first opaque pixel */
                        }
                        col++;
                    }
                    if (col >= bw) break;

                    /* find end of this opaque run */
                    int run_start = col;
                    while (col < bw && (x + col) < VM_DISPLAY_W) {
                        uint32_t pa = addr + ((uint32_t)row * bw + col) * 4;
                        if (pa + 2 > VM_MEMORY_SIZE) break;
                        if (mem_read_u16(vm, pa) == CHROMA_KEY) break;
                        col++;
                    }
                    int run_end = col - 1; /* inclusive */

                    /* compute visible screen x range for this run */
                    int vis_x0 = x + run_start;
                    int vis_x1 = x + run_end;
                    if (vis_x0 < 0) vis_x0 = 0;
                    if (vis_x1 >= VM_DISPLAY_W) vis_x1 = VM_DISPLAY_W - 1;
                    if (vis_x0 > vis_x1) continue;

                    /* bitmap columns that map to [vis_x0 .. vis_x1] */
                    int c_start = vis_x0 - x;
                    int c_end   = vis_x1 - x;

                    TFTBeginPixels((uint16_t)vis_x0, (uint16_t)sy,
                                   (uint16_t)vis_x1, (uint16_t)sy);
                    for (int c = c_start; c <= c_end; c++) {
                        uint32_t pa = addr + ((uint32_t)row * bw + c) * 4;
                        TFTSendPixel(mem_read_u16(vm, pa));
                    }
                    TFTEndPixels();
                }
            }
        } break;

        /* int random() ---------------------------------------------------- */
        case SYSCALL_RANDOM: {
            rng_state = rng_state * 1664525u + 1013904223u;
            vm->stack[vm->sp++] = rng_state >> 1;
        } break;

        /* void seed_random(int seed) -------------------------------------- */
        case SYSCALL_SEED_RANDOM: {
            rng_state = (uint32_t)vm->stack[vm->frame_base + 0];
            if (rng_state == 0) rng_state = 1;
        } break;

        /* void display_draw_text(int x, int y, int *str, int fg, int bg) - */
        case SYSCALL_DISPLAY_DRAW_TEXT: {
            int      x    = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int      y    = (int)(int32_t)vm->stack[vm->frame_base + 1];
            uint32_t addr = vm->stack[vm->frame_base + 2];
            uint16_t fg   = (uint16_t)vm->stack[vm->frame_base + 3];
            int      bg   = (int)(int32_t)vm->stack[vm->frame_base + 4];
            for (uint32_t i = 0; ; i++) {
                if (addr + i >= VM_MEMORY_SIZE) break;
                uint8_t c = vm->memory[addr + i];
                if (c == 0) break;
                draw_char_hw(x + (int)i * FONT_CHAR_W, y, c, fg, bg);
            }
        } break;

        /* void display_draw_int(int x, int y, int n, int fg, int bg) -------- */
        case SYSCALL_DISPLAY_DRAW_INT: {
            int     x  = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int     y  = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int32_t n  = (int32_t)vm->stack[vm->frame_base + 2];
            uint16_t fg = (uint16_t)vm->stack[vm->frame_base + 3];
            int     bg = (int)(int32_t)vm->stack[vm->frame_base + 4];
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
            for (int i = 0; buf[i] != '\0'; i++) {
                int cx = x + i * FONT_CHAR_W;
                if (cx >= VM_DISPLAY_W) break;
                draw_char_hw(cx, y, (uint8_t)buf[i], fg, bg);
            }
        } break;

        default:
            break;
    }
}
