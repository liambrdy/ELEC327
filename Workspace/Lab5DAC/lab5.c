
/*
 * Copyright (c) 2026, Caleb Kemere
 * All rights reserved, see LICENSE.md
 *
 */

#include <ti/devices/msp/msp.h>
#include "hw_interface.h"
#include "state_machine_logic.h"

int main(void)
{
    InitializeGPIO();

    InitializeTimerG0();

    InitSineTable();
    
    InitializeDAC();
    EnableDAC();
    // DisableDAC();

    SetTimerG0Delay(20); // 20 ticks at 32 kHz is 0.6 ms
    EnableTimerG0();

    state_t state = {0};
    state.state = STATE_SONG;

    state_t prevState = {0};
    prevState.state = STATE_SONG;
    prevState.currentNote = -1;

    while (1) {
        // volatile uint32_t ris = (DAC0->CPU_INT.RIS & DAC12_CPU_INT_RIS_FIFOEMPTYIFG_MASK) >> DAC12_CPU_INT_RIS_FIFOEMPTYIFG_OFS;
        if (fromTimer) {
            uint32_t input = GPIOA->DIN31_0 & (SW1 | SW2 | SW3 | SW4);

            state = GetNextState(state, input);
            OutputFromState(&state, &prevState);
            prevState = state;

            fromTimer = false;
            __WFI(); // Go to sleep until timer counts down again.
        }
    }
}
