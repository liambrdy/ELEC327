#include "state_machine_logic.h"
#include <ti/devices/msp/msp.h>

#include "initialize_leds.h"

#define MAX_MINUTES 60

time_state GetNextState(time_state current_state)
{
    current_state.seconds++;

    // If seconds passes 12, reset seconds and increment the outer led.
    if (current_state.seconds >= 12) {
        current_state.seconds = 0;
        current_state.minutes++;
    }

    // If the outer leds has passed 12, reset it.
    if (current_state.minutes >= 12) {
        current_state.minutes = 0;
    }

    return current_state;
}

int GetStateOutputGPIOA(time_state current_state) {
    // Get the led data for current state
    led minLed = leds[current_state.minutes];
    led secLed = leds[current_state.seconds + 12];

    // Return bit flag with only minute and second led set to 0
    return ~((1 << minLed.gpio) | (1 << secLed.gpio));
}

int GetStateOutputGPIOB(time_state current_state) {
    return 0;
};
