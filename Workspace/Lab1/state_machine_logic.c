#include "state_machine_logic.h"
#include <ti/devices/msp/msp.h>

#include "initialize_leds.h"

#define MAX_MINUTES 60

time_state GetNextState(time_state current_state)
{
    current_state.seconds++;

    if (current_state.seconds >= 60) {
        current_state.seconds = 0;
        current_state.minutes++;
    }

    if (current_state.minutes >= MAX_MINUTES) {
        current_state.minutes = 0;
    }

    return current_state;
}

int GetStateOutputGPIOA(time_state current_state) {
    int minLedIndex = current_state.minutes / 5;
    int secLedIndex = current_state.seconds / 5;

    led minLed = minLeds[minLedIndex];
    led secLed = secLeds[secLedIndex];

    int result = ~((1 << minLed.gpio) | (1 << secLed.gpio));

    return result;
}

int GetStateOutputGPIOB(time_state current_state) {
    return 0;
};
