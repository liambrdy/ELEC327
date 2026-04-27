#include "graphics.h"
#include "tft.h"
#include "font5x7.h"
#include <stdint.h>

void TFTFillScreen(uint16_t color) {
    TFTFillRegion(0, 0, SCREEN_W - 1, SCREEN_H - 1, color);
}

void TFTFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= SCREEN_W || y >= SCREEN_H || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    TFTFillRegion(x, y, x + w - 1, y + h - 1, color);
}

void TFTDrawHLine(int16_t x, int16_t y, int16_t len, uint16_t color) {
    TFTFillRect(x, y, len, 1, color);
}

void TFTDrawVLine(int16_t x, int16_t y, int16_t len, uint16_t color) {
    TFTFillRect(x, y, 1, len, color);
}

void TFTDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    TFTDrawHLine(x, y, w, color);
    TFTDrawHLine(x, y + h - 1, w, color);
    TFTDrawVLine(x, y, h, color);
    TFTDrawVLine(x + w - 1, y, h, color);
}

void TFTDrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx =  (x1 > x0 ? x1 - x0 : x0 - x1);
    int16_t dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;
    while (1) {
        TFTFillRect(x0, y0, 1, 1, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Filled circle: streams the entire bounding box in one CS transaction.
// Pixels inside the circle get 'color', corners get 'bg' (usually COLOR_BLACK).
// One CS assertion for the whole shape — no per-row toggling, no stripe artifacts.
void TFTFillCircleBG(int16_t cx, int16_t cy, int16_t r, uint16_t color, uint16_t bg) {
    int16_t x0 = cx - r, y0 = cy - r;
    int16_t x1 = cx + r, y1 = cy + r;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
    if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;

    int32_t r2 = (int32_t)r * r;
    TFTBeginPixels(x0, y0, x1, y1);
    for (int16_t py = y0; py <= y1; py++) {
        int32_t dy2 = (int32_t)(py - cy) * (py - cy);
        for (int16_t px = x0; px <= x1; px++) {
            int32_t dx = px - cx;
            TFTSendPixel(dx * dx + dy2 <= r2 ? color : bg);
        }
    }
    TFTEndPixels();
}

void TFTFillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    TFTFillCircleBG(cx, cy, r, color, COLOR_BLACK);
}

/* ---- Text drawing ------------------------------------------------------ */

/* Row-by-row char render; bg < 0 = transparent (only fg pixels are written). */
void TFTDrawCharEx(int16_t x, int16_t y, uint8_t ch, uint16_t fg, int bg) {
    if (ch < 0x20 || ch > 0x7E) ch = (uint8_t)'?';
    const unsigned char *g = font5x7[ch - 0x20];
    int has_bg = (bg >= 0);
    uint16_t bg16 = (uint16_t)bg;

    for (int row = 0; row < FONT_CHAR_H; row++) {
        int py = y + row;
        if (py < 0 || py >= SCREEN_H) continue;
        int x0 = x < 0 ? 0 : x;
        int x1 = (x + FONT_CHAR_W - 1 >= SCREEN_W) ? SCREEN_W - 1 : x + FONT_CHAR_W - 1;
        if (x0 > x1) continue;

        if (has_bg) {
            TFTBeginPixels((uint16_t)x0, (uint16_t)py, (uint16_t)x1, (uint16_t)py);
            for (int col = x0 - x; col <= x1 - x; col++) {
                int on = (col < 5 && row < 7) && ((g[col] >> row) & 1);
                TFTSendPixel(on ? fg : bg16);
            }
            TFTEndPixels();
        } else {
            for (int col = x0 - x; col <= x1 - x; col++) {
                if (col >= 5 || row >= 7) continue;
                if (!((g[col] >> row) & 1)) continue;
                int px = x + col;
                TFTFillRegion((uint16_t)px, (uint16_t)py, (uint16_t)px, (uint16_t)py, fg);
            }
        }
    }
}

void TFTDrawChar(int16_t x, int16_t y, char ch, uint16_t fg, uint16_t bg) {
    TFTDrawCharEx(x, y, (uint8_t)ch, fg, (int)bg);
}

void TFTDrawStringEx(int16_t x, int16_t y, const char *s, uint16_t fg, int bg) {
    while (*s) {
        if (x >= SCREEN_W) break;
        TFTDrawCharEx(x, y, (uint8_t)*s++, fg, bg);
        x += FONT_CHAR_W;
    }
}

void TFTDrawString(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg) {
    TFTDrawStringEx(x, y, s, fg, (int)bg);
}

void TFTDrawIntEx(int16_t x, int16_t y, int32_t n, uint16_t fg, int bg) {
    char buf[12];
    int  len = 0;
    if (n == 0) {
        buf[len++] = '0';
    } else {
        if (n < 0) { buf[len++] = '-'; n = -n; }
        char     tmp[10];
        int      tlen = 0;
        uint32_t u = (uint32_t)n;
        while (u > 0) { tmp[tlen++] = (char)('0' + u % 10); u /= 10; }
        for (int i = tlen - 1; i >= 0; i--) buf[len++] = tmp[i];
    }
    buf[len] = '\0';
    TFTDrawStringEx(x, y, buf, fg, bg);
}

void TFTDrawInt(int16_t x, int16_t y, int32_t n, uint16_t fg, uint16_t bg) {
    TFTDrawIntEx(x, y, n, fg, (int)bg);
}

// Circle outline using Bresenham midpoint algorithm
void TFTDrawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    int16_t x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        TFTFillRect(cx + x, cy + y, 1, 1, color);
        TFTFillRect(cx - x, cy + y, 1, 1, color);
        TFTFillRect(cx + x, cy - y, 1, 1, color);
        TFTFillRect(cx - x, cy - y, 1, 1, color);
        TFTFillRect(cx + y, cy + x, 1, 1, color);
        TFTFillRect(cx - y, cy + x, 1, 1, color);
        TFTFillRect(cx + y, cy - x, 1, 1, color);
        TFTFillRect(cx - y, cy - x, 1, 1, color);
        x++;
        if (d < 0) d += 4 * x + 6;
        else { y--; d += 4 * (x - y) + 10; }
    }
}
