#include "display.h"
#include "font5x7.h"
#include <SDL2/SDL.h>
#include <string.h>

#define SCALE 2

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;
static uint32_t      framebuf[DISPLAY_H * DISPLAY_W];

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

void display_present(void) {
    SDL_UpdateTexture(texture, NULL, framebuf, DISPLAY_W * (int)sizeof(uint32_t));
    SDL_RenderClear(renderer);
    // Scale the 240x320 texture up to fill the window.
    SDL_Rect dst = { 0, 0, DISPLAY_W * SCALE, DISPLAY_H * SCALE };
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_RenderPresent(renderer);
}
