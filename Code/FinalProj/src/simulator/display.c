#include "display.h"
#include "font5x7.h"
#include <SDL2/SDL.h>
#include <string.h>

#define SCALE 2

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;
static uint32_t      framebuf[DISPLAY_H * DISPLAY_W];

/* Scroll state — mirrors ILI9341 VSCRDEF / VSCRSADD.
   In landscape mode the scroll area pans horizontally. */
static int scroll_tfa = 0;           /* left fixed pixels  */
static int scroll_vsa = DISPLAY_W;   /* scrolling pixels   */
static int scroll_pos = 0;           /* current scroll offset (0 = no shift) */

/* Second framebuffer used only when scroll is active. */
static uint32_t scroll_buf[DISPLAY_H * DISPLAY_W];

static uint32_t rgb565_to_u32(int color) {
    uint16_t c = (uint16_t)(color & 0xFFFF);
    uint8_t r = (uint8_t)(((c >> 11) & 0x1F) << 3);
    uint8_t g = (uint8_t)(((c >>  5) & 0x3F) << 2);
    uint8_t b = (uint8_t)(((c >>  0) & 0x1F) << 3);
    // Extend lowest bits so full white (0xFFFF) maps to 0xFFFFFF, not 0xF8FCF8.
    r |= r >> 5;
    g |= g >> 6;
    b |= b >> 5;
    return (uint32_t)((r << 16) | (g << 8) | b);
}

void display_init(void) {
    window = SDL_CreateWindow(
        "Game Simulator  [Z=A  X=B  arrows=d-pad]",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        DISPLAY_W * SCALE, DISPLAY_H * SCALE,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    SDL_RenderSetLogicalSize(renderer, DISPLAY_W * SCALE, DISPLAY_H * SCALE);
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB888,
        SDL_TEXTUREACCESS_STREAMING,
        DISPLAY_W, DISPLAY_H
    );
    memset(framebuf, 0, sizeof(framebuf));
}

void display_fill(int color) {
    uint32_t c = rgb565_to_u32(color);
    int total = DISPLAY_W * DISPLAY_H;
    for (int i = 0; i < total; i++) framebuf[i] = c;
}

void display_draw_pixel(int x, int y, int color) {
    if (x < 0 || x >= DISPLAY_W || y < 0 || y >= DISPLAY_H) return;
    framebuf[y * DISPLAY_W + x] = rgb565_to_u32(color);
}

void display_fill_rect(int x, int y, int w, int h, int color) {
    uint32_t c = rgb565_to_u32(color);
    int x1 = x < 0 ? 0 : x;
    int y1 = y < 0 ? 0 : y;
    int x2 = x + w > DISPLAY_W ? DISPLAY_W : x + w;
    int y2 = y + h > DISPLAY_H ? DISPLAY_H : y + h;
    for (int row = y1; row < y2; row++)
        for (int col = x1; col < x2; col++)
            framebuf[row * DISPLAY_W + col] = c;
}

void display_draw_rect(int x, int y, int w, int h, int color) {
    display_fill_rect(x,         y,         w, 1, color);
    display_fill_rect(x,         y + h - 1, w, 1, color);
    display_fill_rect(x,         y,         1, h, color);
    display_fill_rect(x + w - 1, y,         1, h, color);
}

void display_draw_hline(int x, int y, int len, int color) {
    display_fill_rect(x, y, len, 1, color);
}

void display_draw_vline(int x, int y, int len, int color) {
    display_fill_rect(x, y, 1, len, color);
}

void display_draw_line(int x0, int y0, int x1, int y1, int color) {
    uint32_t c = rgb565_to_u32(color);
    int dx  =  (x1 > x0 ? x1 - x0 : x0 - x1);
    int dy  = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int sx  = x0 < x1 ? 1 : -1;
    int sy  = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        if (x0 >= 0 && x0 < DISPLAY_W && y0 >= 0 && y0 < DISPLAY_H)
            framebuf[y0 * DISPLAY_W + x0] = c;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void display_fill_circle(int cx, int cy, int r, int color) {
    uint32_t c = rgb565_to_u32(color);
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= DISPLAY_H) continue;
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy > r2) continue;
            int px = cx + dx;
            if (px < 0 || px >= DISPLAY_W) continue;
            framebuf[py * DISPLAY_W + px] = c;
        }
    }
}

void display_fill_circle_bg(int cx, int cy, int r, int color, int bg) {
    uint32_t c32  = rgb565_to_u32(color);
    uint32_t bg32 = rgb565_to_u32(bg);
    int r2 = r * r;
    int x0 = cx - r < 0          ? 0           : cx - r;
    int y0 = cy - r < 0          ? 0           : cy - r;
    int x1 = cx + r >= DISPLAY_W ? DISPLAY_W-1 : cx + r;
    int y1 = cy + r >= DISPLAY_H ? DISPLAY_H-1 : cy + r;
    for (int py = y0; py <= y1; py++) {
        int dy = py - cy;
        for (int px = x0; px <= x1; px++) {
            int dx = px - cx;
            framebuf[py * DISPLAY_W + px] = (dx*dx + dy*dy <= r2) ? c32 : bg32;
        }
    }
}

void display_draw_circle(int cx, int cy, int r, int color) {
    uint32_t c = rgb565_to_u32(color);
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        int pts[8][2] = {
            {cx+x, cy+y}, {cx-x, cy+y}, {cx+x, cy-y}, {cx-x, cy-y},
            {cx+y, cy+x}, {cx-y, cy+x}, {cx+y, cy-x}, {cx-y, cy-x}
        };
        for (int i = 0; i < 8; i++) {
            int px = pts[i][0], py = pts[i][1];
            if (px >= 0 && px < DISPLAY_W && py >= 0 && py < DISPLAY_H)
                framebuf[py * DISPLAY_W + px] = c;
        }
        x++;
        if (d < 0) d += 4*x + 6;
        else { y--; d += 4*(x - y) + 10; }
    }
}

void display_draw_char(int x, int y, int ch, int fg, int bg) {
    uint32_t fg32   = rgb565_to_u32(fg);
    int      has_bg = (bg >= 0 && bg <= 0xFFFF);
    uint32_t bg32   = has_bg ? rgb565_to_u32(bg) : 0;
    unsigned char c = (unsigned char)(ch & 0xFF);
    const unsigned char *glyph = (c >= 0x20 && c <= 0x7E) ? font5x7[c-0x20] : font5x7[0];
    for (int row = 0; row < FONT_CHAR_H; row++) {
        int py = y + row;
        if (py < 0 || py >= DISPLAY_H) continue;
        for (int col = 0; col < FONT_CHAR_W; col++) {
            int px = x + col;
            if (px < 0 || px >= DISPLAY_W) continue;
            int on = (col < 5 && row < 7) && ((glyph[col] >> row) & 1);
            if (on)         framebuf[py * DISPLAY_W + px] = fg32;
            else if (has_bg) framebuf[py * DISPLAY_W + px] = bg32;
        }
    }
}

void display_commit(void) {
    /* No-op in simulator: framebuf is always up-to-date.
       Hardware equivalent: TFTWaitIdle(). */
}

void display_draw_bitmap(int x, int y, const int *pixels, int w, int h) {
    for (int row = 0; row < h; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= DISPLAY_H) continue;
        for (int col = 0; col < w; col++) {
            int dx = x + col;
            if (dx < 0 || dx >= DISPLAY_W) continue;
            int px = pixels[row * w + col];
            if (px == DISPLAY_CHROMA_KEY) continue; /* transparent */
            framebuf[dy * DISPLAY_W + dx] = rgb565_to_u32(px);
        }
    }
}

void display_draw_text(int x, int y, const char *str, int fg, int bg) {
    uint32_t fg32 = rgb565_to_u32(fg);
    int has_bg = (bg >= 0 && bg <= 0xFFFF);
    uint32_t bg32 = has_bg ? rgb565_to_u32(bg) : 0;

    for (int i = 0; str[i] != '\0'; i++) {
        unsigned char c = (unsigned char)str[i];
        int cx = x + i * FONT_CHAR_W;
        if (cx >= DISPLAY_W) break;

        const unsigned char *glyph = (c >= 0x20 && c <= 0x7E)
                                     ? font5x7[c - 0x20]
                                     : font5x7[0]; /* unknown → space */

        for (int row = 0; row < FONT_CHAR_H; row++) {
            int py = y + row;
            if (py < 0 || py >= DISPLAY_H) continue;
            for (int col = 0; col < FONT_CHAR_W; col++) {
                int px = cx + col;
                if (px < 0 || px >= DISPLAY_W) continue;
                int on = (col < 5 && row < 7) && ((glyph[col] >> row) & 1);
                if (on)
                    framebuf[py * DISPLAY_W + px] = fg32;
                else if (has_bg)
                    framebuf[py * DISPLAY_W + px] = bg32;
            }
        }
    }
}

void display_scroll_define(int left_fixed, int scroll_width, int right_fixed) {
    (void)right_fixed; /* BFA is implied: DISPLAY_W - left_fixed - scroll_width */
    scroll_tfa = left_fixed;
    scroll_vsa = scroll_width;
    scroll_pos = 0;    /* reset position when scroll area changes */
}

void display_scroll_set(int pos) {
    scroll_pos = pos;
}

void display_present(void) {
    uint32_t *pixels = framebuf;

    if (scroll_pos != 0) {
        /*
         * Build a scrolled view of the framebuffer into scroll_buf.
         *
         * For each screen column x:
         *   - x in [0, TFA)            : fixed  → src = framebuf col x
         *   - x in [TFA, TFA+VSA)      : scrolls → src = framebuf col
         *                                  TFA + (scroll_pos + (x-TFA)) % VSA
         *   - x in [TFA+VSA, DISPLAY_W): fixed  → src = framebuf col x
         *
         * All rows apply the same column mapping, so build a lookup table
         * for the column remapping first, then apply it to every row.
         */
        int col_map[DISPLAY_W];
        int right_start = scroll_tfa + scroll_vsa;
        for (int x = 0; x < DISPLAY_W; x++) {
            if (x < scroll_tfa || x >= right_start) {
                col_map[x] = x;
            } else {
                col_map[x] = scroll_tfa + (scroll_pos + (x - scroll_tfa)) % scroll_vsa;
            }
        }

        for (int y = 0; y < DISPLAY_H; y++) {
            const uint32_t *src = &framebuf[y * DISPLAY_W];
            uint32_t       *dst = &scroll_buf[y * DISPLAY_W];
            for (int x = 0; x < DISPLAY_W; x++)
                dst[x] = src[col_map[x]];
        }
        pixels = scroll_buf;
    }

    SDL_UpdateTexture(texture, NULL, pixels, DISPLAY_W * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_Rect dst = { 0, 0, DISPLAY_W * SCALE, DISPLAY_H * SCALE };
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_RenderPresent(renderer);
}
