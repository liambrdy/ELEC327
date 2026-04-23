
/*
 * Copyright (c) 2026, Caleb Kemere
 * All rights reserved, see LICENSE.md
 */

#include <ti/devices/msp/msp.h>
#include "tft.h"
#include "delay.h"
#include "vm.h"
#include "game_syscall.h"
#include "rom_data.h"

/* ---- TFT initialisation sequence (ILI9341) ---- */

static void tft_init_sequence(void) {
    TFTWriteCommand(0x01, NULL, 0);           /* software reset */
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

    /*
     * MADCTL = 0x28: MV=1 (swap rows/cols → landscape), BGR=1
     * Width becomes 320, height becomes 240.
     * If the image appears mirrored, try 0xE8 (MY+MX+MV+BGR).
     */
    p[0] = 0x28;
    TFTWriteCommand(0x36, p, 1);

    p[0]=0x00;
    TFTWriteCommand(0x37, p, 1);
    p[0]=0x55;
    TFTWriteCommand(0x3A, p, 1);   /* pixel format: RGB565 */
    p[0]=0x00; p[1]=0x18;
    TFTWriteCommand(0xB1, p, 2);
    p[0]=0x08; p[1]=0x82; p[2]=0x27;
    TFTWriteCommand(0xB6, p, 3);
    p[0]=0x00;
    TFTWriteCommand(0xF2, p, 1);
    p[0]=0x01;
    TFTWriteCommand(0x26, p, 1);

    TFTWriteCommand(0x11, NULL, 0); /* sleep out */
    delay_cycles(32000 * 120);
    TFTWriteCommand(0x29, NULL, 0); /* display on */
    delay_cycles(32000 * 20);
}

/* ---- SysTick: 1 ms tick at 32 MHz ---- */

static void init_systick(void) {
    SysTick->LOAD = 32000 - 1;  /* 32 MHz / 32000 = 1 kHz */
    SysTick->VAL  = 0;
    SysTick->CTRL = 7u;         /* ENABLE | TICKINT | CLKSOURCE=processor */
}

/* ---- VM instance (static so it lives in .bss, not on the tiny C stack) ---- */

static vm_t vm;

/* ---- Entry point ---- */

int main(void) {
    InitializeTFT();
    tft_init_sequence();
    init_systick();

    if (!vm_load_rom(&vm, rom_data, rom_data_size)) {
        /* Bad ROM — flash the screen red/black to signal error */
        while (1) {
            TFTFillRegion(0, 0, VM_DISPLAY_W - 1, VM_DISPLAY_H - 1, 0xF800);
            delay_cycles(32000 * 300);
            TFTFillRegion(0, 0, VM_DISPLAY_W - 1, VM_DISPLAY_H - 1, 0x0000);
            delay_cycles(32000 * 300);
        }
    }

    vm.syscall_handler = game_syscall;
    vm_run(&vm);

    /* vm_run only returns on HALT or a fatal error — blink green to indicate clean exit */
    while (1) {
        TFTFillRegion(0, 0, VM_DISPLAY_W - 1, VM_DISPLAY_H - 1, 0x07E0);
        delay_cycles(32000 * 500);
        TFTFillRegion(0, 0, VM_DISPLAY_W - 1, VM_DISPLAY_H - 1, 0x0000);
        delay_cycles(32000 * 500);
    }
}
