
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

    // SetTimerG0Delay(20); // 20 ticks at 32 kHz is 0.6 ms
    // EnableTimerG0();

    volatile uint8_t id[3] = {0};
    TFTReadCommand(0x04, id, 3);

    TFTWriteCommand(0x11, NULL, 1);
    delay_cycles(32000 * 120);

    TFTWriteCommand(0x29, NULL, 1);

    while (true) {
        asm("nop");
    }
}

