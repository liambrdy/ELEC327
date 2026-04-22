
/*
 * Copyright (c) 2026, Caleb Kemere
 * All rights reserved, see LICENSE.md
 *
 */

#include <ti/devices/msp/msp.h>
#include "tft.h"
#include "graphics.h"
#include "timing.h"
#include "delay.h"

#include <stddef.h>

static void tft_init_sequence(void) {
    TFTWriteCommand(0x01, NULL, 0);
    delay_cycles(32000 * 150);

    uint8_t p[15];
    p[0]=0x03; p[1]=0x80; p[2]=0x02;
    TFTWriteCommand(0xEF, p, 3);

    p[0]=0x00; p[1]=0xC1; p[2]=0x30;
    TFTWriteCommand(0xCF, p, 3);

    p[0]=0x64; p[1]=0x03; p[2]=0x12; p[3]=0x81;
    TFTWriteCommand(0xED, p, 4);

    p[0]=0x85; p[1]=0x00; p[2]=0x78;
    TFTWriteCommand(0xE8, p, 3);

    p[0]=0x39; p[1]=0x2C; p[2]=0x00; p[3]=0x34; p[4]=0x02;
    TFTWriteCommand(0xCB, p, 5);

    p[0]=0x20;
    TFTWriteCommand(0xF7, p, 1);

    p[0]=0x00; p[1]=0x00;
    TFTWriteCommand(0xEA, p, 2);

    p[0]=0x23;
    TFTWriteCommand(0xC0, p, 1);
    p[0]=0x10;
    TFTWriteCommand(0xC1, p, 1);

    p[0]=0x3E; p[1]=0x28;
    TFTWriteCommand(0xC5, p, 2);
    p[0]=0x86;
    TFTWriteCommand(0xC7, p, 1);

    p[0]=0x48;
    TFTWriteCommand(0x36, p, 1);
    p[0]=0x00;
    TFTWriteCommand(0x37, p, 1);
    p[0]=0x55;
    TFTWriteCommand(0x3A, p, 1);
    p[0]=0x00; p[1]=0x18;
    TFTWriteCommand(0xB1, p, 2);
    p[0]=0x08; p[1]=0x82; p[2]=0x27;
    TFTWriteCommand(0xB6, p, 3);
    p[0]=0x00;
    TFTWriteCommand(0xF2, p, 1);
    p[0]=0x01;
    TFTWriteCommand(0x26, p, 1);

    TFTWriteCommand(0x11, NULL, 0);
    delay_cycles(32000 * 120);
    TFTWriteCommand(0x29, NULL, 0);
    delay_cycles(32000 * 20);
}

int main(void)
{
    InitializeTFT();
    tft_init_sequence();

    TFTFillScreen(COLOR_BLACK);

    // --- Bouncing ball demo ---
    static const uint16_t palette[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE,
        COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_ORANGE
    };
    const int ncolors = sizeof(palette) / sizeof(palette[0]);

    int16_t bx = 60, by = 80;
    const int16_t r = 20;
    int16_t vx = 2, vy = 3;
    int ci = 0;

    int32_t r2 = (int32_t)r * r;

    while (true) {
        // Compute new position
        int16_t nbx = bx + vx;
        int16_t nby = by + vy;

        if (nbx - r < 0)            { nbx = r;              vx = -vx; ci = (ci + 1) % ncolors; }
        if (nbx + r >= SCREEN_W)    { nbx = SCREEN_W-1 - r; vx = -vx; ci = (ci + 1) % ncolors; }
        if (nby - r < 0)            { nby = r;              vy = -vy; ci = (ci + 1) % ncolors; }
        if (nby + r >= SCREEN_H)    { nby = SCREEN_H-1 - r; vy = -vy; ci = (ci + 1) % ncolors; }

        // Union bounding box covering both old and new positions.
        // One SPI transaction erases old and draws new simultaneously,
        // halving the tearing window vs separate erase + draw passes.
        int16_t x0 = (bx - r < nbx - r) ? bx - r : nbx - r;
        int16_t y0 = (by - r < nby - r) ? by - r : nby - r;
        int16_t x1 = (bx + r > nbx + r) ? bx + r : nbx + r;
        int16_t y1 = (by + r > nby + r) ? by + r : nby + r;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= SCREEN_W) x1 = SCREEN_W - 1;
        if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;

        TFTBeginPixels(x0, y0, x1, y1);
        for (int16_t py = y0; py <= y1; py++) {
            int32_t dy2 = (int32_t)(py - nby) * (py - nby);
            for (int16_t px = x0; px <= x1; px++) {
                int32_t dx = px - nbx;
                TFTSendPixel(dx * dx + dy2 <= r2 ? palette[ci] : COLOR_BLACK);
            }
        }
        TFTEndPixels();

        bx = nbx;
        by = nby;
    }
}
