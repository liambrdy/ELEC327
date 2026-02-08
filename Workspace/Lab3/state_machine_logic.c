#include "state_machine_logic.h"
#include <ti/devices/msp/msp.h>

#include <stdbool.h>

#include "hw_interface.h"

#define BRIGHTNESS_LEVELS 15
#define FLASH_FREQUENCY 1

#define MS_TO_US(ms) (ms * 1000)

int BrightnessToDutyCycle(int brightness) {
    return 1 + 7*brightness;
}

time_state GetNextState(time_state current_state, int button_up)
{
    current_state.pwm++;

    int us = current_state.button_press_length * PWM_PERIOD_US;
    bool button_action = false;

    if (!button_up) {
        current_state.button_press_length++;
    } else if (us >= MS_TO_US(5)) {
        bool long_press = us >= MS_TO_US(1000);

        if (long_press) {
            current_state.mode = (current_state.mode + 1) % CLOCK_MODE_MAX;
        } else {
            button_action = true;
        }

        current_state.button_press_length = 0;
    }

    switch (current_state.mode) {
        case NORMAL: {
            if (current_state.pwm % PWM_FREQUENCY == 0) {
                current_state.minute++;
            }

            if (current_state.minute >= 12) {
                current_state.minute = 0;
                current_state.hour++;
            }

            if (current_state.hour >= 12) {
                current_state.hour = 0;
            }
        } break;

        case HOUR_SET: {
            if (button_action) current_state.hour = (current_state.hour + 1) % 12;
        } break;

        case MINUTE_SET: {
            if (button_action) current_state.minute = (current_state.minute + 1) % 12;
        } break;

        case BRIGHTNESS_SET: {
            if (button_action) current_state.brightness = (current_state.brightness + 1) % BRIGHTNESS_LEVELS;
        } break;

        default: break;
    }

    return current_state;
}

int GetStateOutput(time_state current_state) {
    // Get the led data for current state
    pin_config_t hourLed = hour_leds[current_state.hour];
    pin_config_t minLed = minute_leds[current_state.minute];

    uint32_t hourBit = 0x0;
    uint32_t minBit = 0x0;

    if (current_state.pwm % 100 <= BrightnessToDutyCycle(current_state.brightness)) {
        int flashFreq = PWM_FREQUENCY / FLASH_FREQUENCY;
        bool flash = current_state.pwm % flashFreq <= flashFreq / 2;
        bool hourFlash = (current_state.mode == HOUR_SET || current_state.mode == BRIGHTNESS_SET) && flash;
        bool minFlash = (current_state.mode == MINUTE_SET || current_state.mode == BRIGHTNESS_SET) && flash;

        hourBit = hourFlash ? 0x0 : (1 << hourLed.pin_number);
        minBit = minFlash ? 0x0 : (1 << minLed.pin_number);
    }

    return ~(hourBit | minBit);
}