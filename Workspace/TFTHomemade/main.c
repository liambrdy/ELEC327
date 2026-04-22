
/*
 * Copyright (c) 2026, Caleb Kemere
 * All rights reserved, see LICENSE.md
 *
 */

#include <ti/devices/msp/msp.h>
#include "tft.h"
#include "timing.h"
#include "delay.h"

#include <stddef.h>

int main(void)
{
    InitializeTFT();

    // Software reset
    TFTWriteCommand(0x01, NULL, 0);
    delay_cycles(32000 * 150);

    // Adafruit's required undocumented power-on sequence
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

    // Power control
    p[0]=0x23;
    TFTWriteCommand(0xC0, p, 1);
    p[0]=0x10;
    TFTWriteCommand(0xC1, p, 1);

    // VCOM
    p[0]=0x3E; p[1]=0x28;
    TFTWriteCommand(0xC5, p, 2);
    p[0]=0x86;
    TFTWriteCommand(0xC7, p, 1);

    // Memory access, pixel format, frame rate
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

    // Sleep out and display on
    TFTWriteCommand(0x11, NULL, 0);
    delay_cycles(32000 * 120);
    TFTWriteCommand(0x29, NULL, 0);
    delay_cycles(32000 * 20);

    // Fill red
    p[0]=0x00; p[1]=0x00; p[2]=0x00; p[3]=0xEF;
    TFTWriteCommand(0x2A, p, 4);
    p[0]=0x00; p[1]=0x00; p[2]=0x01; p[3]=0x3F;
    TFTWriteCommand(0x2B, p, 4);
    TFTWriteCommand(0x2C, NULL, 0);
    uint8_t pixel[2] = {0xF8, 0x00};
    for (int i = 0; i < 240*320; i++) {
        TFTWriteData(pixel, 2);
    }

    while (true) {
        asm("nop");
    }
}

