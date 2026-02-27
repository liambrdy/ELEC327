
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

    InitializeTimerA1_PWM();
    DisableTimerA1PWM();

    SetTimerG0Delay(20); // 20 ticks at 32 kHz is 0.6 ms
    EnableTimerG0();

    state_t state = {0};
    state.state = STATE_SONG;

    while (1) {
        uint32_t input = GPIOA->DIN31_0 & (SW1 | SW2 | SW3 | SW4);
    
        state = GetNextState(state, input);
        OutputFromState(state);

        __WFI(); // Go to sleep until timer counts down again.
    }

}


