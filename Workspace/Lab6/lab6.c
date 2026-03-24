
/*
 * Copyright (c) 2026, Caleb Kemere
 * All rights reserved, see LICENSE.md
 *
 */

#include <ti/devices/msp/msp.h>
#include "delay.h"
#include "buttons.h"
#include "timing.h"
#include "buzzer.h"
#include "leds.h"
#include "state.h"

int main(void)
{
    InitializeButtonGPIO();
    InitializeBuzzer();
    InitializeLEDInterface();
    InitializeTimerG0();

    EnableBuzzer();

    SetTimerG0Delay(20);
    EnableTimerG0();

    state_t state = {0};
    state.state = STATE_SONG;

    state_t prevState = {0};
    prevState.state = STATE_SONG;
    prevState.currentNote = -1;

    while (1) {
        if (timer_wakeup) {
            uint32_t input = GPIOA->DIN31_0 & (SW1 + SW2 + SW3 + SW4);

            state = GetNextState(state, input);
            OutputFromState(&state, &prevState);
            prevState = state;

            timer_wakeup = false;

            __WFI();
        }
    }

}

