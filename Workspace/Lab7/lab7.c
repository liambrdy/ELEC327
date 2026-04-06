/*
 * This template file implements an interrupt-driven infinite loop which
 * cycles a state machine.
 */

#include <ti/devices/msp/msp.h>
#include "hw_interface.h"
#include "state_machine_logic.h"



uint32_t generate_neopixel(char color) {
    uint32_t neopixel_bits = 0; // This will store data with the first bit transmitted as the LSB
    int b = 0;
    for (int i = 7; i >= 0; i--) {
        neopixel_bits |= (0x1 << b++);
        if (color & (0x1 << i))
            neopixel_bits |= (0x1 << b++);
        else
            b++; // 0
        b++; // final 0
    }

    return neopixel_bits;
}

// Tune these NOP counts by scope — subtract overhead from set/clear/branch
#define NOP1  __asm(" NOP")
#define NOP4  do { NOP1; NOP1; NOP1; NOP1; } while(0)
#define NOP8  do { NOP4; NOP4; } while(0)
#define NOP16 do { NOP8; NOP8; } while(0)
#define NOP32 do { NOP16; NOP16; } while(0)


static inline void send_bit(uint32_t bit) __attribute__((always_inline)) {
    if (bit) {
        // T1H: ~800 ns high
        GPIOA->DOUTSET31_0 = 0x1 << 24;
        NOP16; 
        // T1L: ~450 ns low
        GPIOA->DOUTCLR31_0 = 0x1 << 24;
        NOP8;
    } else {
        // T0H: ~400 ns high
        GPIOA->DOUTSET31_0 = 0x1 << 24;
        NOP8; 
        // T0L: ~850 ns low
        GPIOA->DOUTCLR31_0 = 0x1 << 24;
        NOP16;
    }
}

static inline void send_byte(uint8_t b) __attribute__((always_inline)) {
    #pragma clang loop unroll(full)
    for (int i = 7; i >= 0; i--) {
        send_bit((b >> i) & 1);
    }
}


int main(void)
{
    InitializeLED();

    InitializeTimerG0();

    state_t state; // initialize state machine
    state.hour = 0;
    state.minute = 0;

    register uint32_t neopixel_red = generate_neopixel(0xAA);
    register uint32_t neopixel_green = generate_neopixel(0x77);
    register uint32_t neopixel_blue = generate_neopixel(0x77);

    SetTimerG0Delay(32000); // 1 s interrupts
    EnableTimerG0();

    delay_cycles(10000);
    
    while (1) {
        send_byte(0x20); // R
        send_byte(0x05); // G
        send_byte(0x05); // B

        // Switch shared neoxpixel/button pin to input
        IOMUX->SECCFG.PINCM[IOMUX_PINCM25] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000000));        

        // Could do something with button here!

        __WFI(); // Go to sleep until timer counts down again.
        // Switch shared neoxpixel/button pin to output
        IOMUX->SECCFG.PINCM[IOMUX_PINCM25] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));


        send_byte(0); // R
        send_byte(0); // G
        send_byte(0); // B        
        IOMUX->SECCFG.PINCM[IOMUX_PINCM25] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000000));        
        __WFI(); // Go to sleep until timer counts down again.
        IOMUX->SECCFG.PINCM[IOMUX_PINCM25] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    }

}


/*
 * Copyright (c) 2026, Caleb Kemere
 * Derived from example code which is
 *
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
