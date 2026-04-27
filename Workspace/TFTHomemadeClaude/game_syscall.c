#include "game_syscall.h"
#include "tft.h"
#include "graphics.h"
#include "buttons.h"

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

/* ---- Syscall dispatcher ---- */

void game_syscall(vm_t *vm, u8 id) {
    switch ((syscall_id_t)id) {

        /* display_fill(int color) ----------------------------------------- */
        case SYSCALL_DISPLAY_FILL: {
            uint16_t color = (uint16_t)vm->stack[vm->frame_base + 0];
            TFTFillScreen(color);
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
            TFTFillRect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, c);
        } break;

        /* int buttons_read() ----------------------------------------------- */
        case SYSCALL_BUTTONS_READ: {
            vm->stack[vm->sp++] = hw_buttons_read();
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
                TFTDrawCharEx(x + (int)i * FONT_CHAR_W, y, c, fg, bg);
            }
        } break;

        /* void display_draw_int(int x, int y, int n, int fg, int bg) -------- */
        case SYSCALL_DISPLAY_DRAW_INT: {
            int      x  = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int      y  = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int32_t  n  = (int32_t)vm->stack[vm->frame_base + 2];
            uint16_t fg = (uint16_t)vm->stack[vm->frame_base + 3];
            int      bg = (int)(int32_t)vm->stack[vm->frame_base + 4];
            TFTDrawIntEx((int16_t)x, (int16_t)y, n, fg, bg);
        } break;

        /* void display_scroll_define(int left, int width, int right) -------- */
        case SYSCALL_DISPLAY_SCROLL_DEFINE: {
            uint16_t left  = (uint16_t)vm->stack[vm->frame_base + 0];
            uint16_t width = (uint16_t)vm->stack[vm->frame_base + 1];
            uint16_t right = (uint16_t)vm->stack[vm->frame_base + 2];
            TFTScrollDefine(left, width, right);
        } break;

        /* void display_scroll_set(int pos) ---------------------------------- */
        case SYSCALL_DISPLAY_SCROLL_SET: {
            uint16_t pos = (uint16_t)vm->stack[vm->frame_base + 0];
            TFTScrollSet(pos);
        } break;

        /* void display_draw_rect(int x, int y, int w, int h, int color) ---- */
        case SYSCALL_DISPLAY_DRAW_RECT: {
            int x = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int y = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int w = (int)(int32_t)vm->stack[vm->frame_base + 2];
            int h = (int)(int32_t)vm->stack[vm->frame_base + 3];
            uint16_t c = (uint16_t)vm->stack[vm->frame_base + 4];
            TFTDrawRect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, c);
        } break;

        /* void display_draw_hline(int x, int y, int len, int color) --------- */
        case SYSCALL_DISPLAY_DRAW_HLINE: {
            int x = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int y = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int l = (int)(int32_t)vm->stack[vm->frame_base + 2];
            uint16_t c = (uint16_t)vm->stack[vm->frame_base + 3];
            TFTDrawHLine((int16_t)x, (int16_t)y, (int16_t)l, c);
        } break;

        /* void display_draw_vline(int x, int y, int len, int color) --------- */
        case SYSCALL_DISPLAY_DRAW_VLINE: {
            int x = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int y = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int l = (int)(int32_t)vm->stack[vm->frame_base + 2];
            uint16_t c = (uint16_t)vm->stack[vm->frame_base + 3];
            TFTDrawVLine((int16_t)x, (int16_t)y, (int16_t)l, c);
        } break;

        /* void display_draw_line(int x0, int y0, int x1, int y1, int color) - */
        case SYSCALL_DISPLAY_DRAW_LINE: {
            int x0 = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int y0 = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int x1 = (int)(int32_t)vm->stack[vm->frame_base + 2];
            int y1 = (int)(int32_t)vm->stack[vm->frame_base + 3];
            uint16_t c = (uint16_t)vm->stack[vm->frame_base + 4];
            TFTDrawLine((int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1, c);
        } break;

        /* void display_fill_circle(int cx, int cy, int r, int color) -------- */
        case SYSCALL_DISPLAY_FILL_CIRCLE: {
            int cx = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int cy = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int r  = (int)(int32_t)vm->stack[vm->frame_base + 2];
            uint16_t c = (uint16_t)vm->stack[vm->frame_base + 3];
            TFTFillCircle((int16_t)cx, (int16_t)cy, (int16_t)r, c);
        } break;

        /* void display_fill_circle_bg(int cx, int cy, int r, int color, int bg) */
        case SYSCALL_DISPLAY_FILL_CIRCLE_BG: {
            int cx = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int cy = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int r  = (int)(int32_t)vm->stack[vm->frame_base + 2];
            uint16_t c  = (uint16_t)vm->stack[vm->frame_base + 3];
            uint16_t bg = (uint16_t)vm->stack[vm->frame_base + 4];
            TFTFillCircleBG((int16_t)cx, (int16_t)cy, (int16_t)r, c, bg);
        } break;

        /* void display_draw_circle(int cx, int cy, int r, int color) -------- */
        case SYSCALL_DISPLAY_DRAW_CIRCLE: {
            int cx = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int cy = (int)(int32_t)vm->stack[vm->frame_base + 1];
            int r  = (int)(int32_t)vm->stack[vm->frame_base + 2];
            uint16_t c = (uint16_t)vm->stack[vm->frame_base + 3];
            TFTDrawCircle((int16_t)cx, (int16_t)cy, (int16_t)r, c);
        } break;

        /* void display_draw_char(int x, int y, int ch, int fg, int bg) ------ */
        case SYSCALL_DISPLAY_DRAW_CHAR: {
            int      x  = (int)(int32_t)vm->stack[vm->frame_base + 0];
            int      y  = (int)(int32_t)vm->stack[vm->frame_base + 1];
            uint8_t  ch = (uint8_t)vm->stack[vm->frame_base + 2];
            uint16_t fg = (uint16_t)vm->stack[vm->frame_base + 3];
            int      bg = (int)(int32_t)vm->stack[vm->frame_base + 4];
            TFTDrawCharEx((int16_t)x, (int16_t)y, ch, fg, bg);
        } break;

        default:
            break;
    }
}
