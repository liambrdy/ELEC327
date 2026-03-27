#include <ti/devices/msp/msp.h>
#include "state_machine_logic.h"
#include "buzzer.h"
#include "music.h"
#include "buttons.h"
#include "leds.h"
#include "colors.h"
#include "random.h"

const uint32_t button_mask[] = {SW1, SW2, SW3, SW4};

const uint32_t buzzer_notes[] = {
    C5_LOAD,
    D5_LOAD,
    E5_LOAD,
    G5_LOAD,
};

button_t UpdateButton(button_t button, uint32_t input, uint32_t mask) {
    button_t new_button = button;

    if ((input & mask) == 0) {
        switch (button.state) {
            case BUTTON_IDLE:
                new_button.state = BUTTON_BOUNCING;
                new_button.depressed_counter = 1;
                break;
            case BUTTON_BOUNCING:
                new_button.depressed_counter = button.depressed_counter + 1;
                if (new_button.depressed_counter > BUTTON_BOUNCE_LIMIT) {
                    new_button.state = BUTTON_PRESS;
                }
                break;
            case BUTTON_PRESS:
            default:
                break;
        }
    }
    else {
        switch (button.state) {
            case BUTTON_PRESS: {
                new_button.state = BUTTON_RELEASE;
            } break;
            case BUTTON_RELEASE:
            case BUTTON_BOUNCING:
            case BUTTON_IDLE: {
                new_button.state = BUTTON_IDLE;
            } break;

            default: break;
        }
        
        new_button.depressed_counter = 0;
    }

    return new_button;
}

state_t GetNextState(state_t current_state, uint32_t input)
{
    state_t new_state = current_state;

    // Update buttons
    int button_pressed = 0; // helper
    int button_released = 0;

    for (int i = 0; i < 4; i++) {
        new_state.buttons[i] = UpdateButton(current_state.buttons[i], input, button_mask[i]);
        if (new_state.buttons[i].state == BUTTON_PRESS)
            button_pressed++;
        if (new_state.buttons[i].state == BUTTON_RELEASE)
            button_released++;
    }

    switch (new_state.mode) {
        case MODE_STARTUP: {
            if (button_pressed > 0) new_state.mode = MODE_SIMON_SAYS;
            new_state.sequenceLength = 1;
            new_state.sequenceCounter = 0;
            new_state.counter = 0;
        } break;
        case MODE_SIMON_SAYS: {
            if (new_state.sequenceCounter == 0) {
                srand(new_state.gameSeed);
            }

            if (new_state.counter == 0) {
                uint16_t pick = rand();
                new_state.buzzer.sound_on = true;
                new_state.buzzer.period = buzzer_notes[pick];
                new_state.leds = &single_leds[pick];
            }

            if (new_state.counter >= SIMON_SAYS_NOTE_DURATION) {
                new_state.sequenceCounter++;
                new_state.counter = 0;
                new_state.buzzer.sound_on = false;
                new_state.leds = &leds_off;
                new_state.mode = MODE_SIMON_SAYS_PAUSE;

                if (new_state.sequenceCounter >= new_state.sequenceLength) {
                    new_state.sequenceCounter = 0;
                    new_state.mode = MODE_SIMON_RESPOND;
                }
            } else {
                new_state.counter++;
            }
        } break;
        case MODE_SIMON_SAYS_PAUSE: {
            if (new_state.counter >= SIMON_SAYS_PAUSE) {
                new_state.counter = 0;
                new_state.mode = MODE_SIMON_SAYS;
            } else {
                new_state.counter++;
            }
        } break;
        case MODE_SIMON_RESPOND: {
            if (new_state.sequenceCounter == 0) {
                srand(new_state.gameSeed);
            }

            if (new_state.counter == 0) {
                new_state.currentCorrect = rand();
                new_state.buzzer.sound_on = false;
            }

            if (new_state.counter >= TIME_PER_RESPONSE) {
                new_state.counter = 0;
                new_state.mode = MODE_GAME_OVER_LOSE;
            }

            if (button_pressed > 0) {
                for (int i = 0; i < 4; i++) {
                    if (new_state.buttons[i].state != BUTTON_PRESS) continue;
                    
                    if (new_state.currentCorrect == i) {
                        new_state.buzzer.period = buzzer_notes[i];
                        new_state.buzzer.sound_on = true;
                        new_state.leds = &single_leds[i];
                    } else {
                        new_state.counter = 0;
                        new_state.mode = MODE_GAME_OVER_LOSE;
                    }
                }
            } else if (button_released > 0) {
                new_state.leds = &leds_off;
                new_state.buzzer.sound_on = false;
                new_state.counter = 0;
                new_state.sequenceCounter++;

                if (new_state.sequenceCounter >= new_state.sequenceLength) {
                    new_state.sequenceCounter = 0;
                    new_state.sequenceLength++;
                    new_state.mode = MODE_SIMON_SAYS_PAUSE;

                    if (new_state.sequenceLength > WIN_LENGTH) {
                        new_state.counter = 0;
                        new_state.mode = MODE_GAME_OVER_WIN;
                    }
                }
            } else {
                new_state.counter++;
            }
        } break;
        case MODE_GAME_OVER_WIN: {
            int ledIndex = (new_state.counter * 4) / (FLASH_COUNTER + 1);
            current_state.leds = &single_leds[ledIndex];
            new_state.counter++;
        } break;
        case MODE_GAME_OVER_LOSE: {

        } break;
        default: break;
    }

    // Update buzzer state

    // for (int i = 0; i < 4; i++) {
    //     if (new_state.buttons[i].state == BUTTON_PRESS) {
    //         new_state.buzzer.period = G5_LOAD;
    //         new_state.buzzer.sound_on = true;
    //         new_state.leds = &leds_on;
    //         break;
    //     }
    // }


    return new_state;
}

void SetBuzzerState(buzzer_state_t buzzer) {
    // if (buzzer.sound_on) {
    //     EnableBuzzer();
    // }
    // else {
    //     DisableBuzzer();
    // }

    // SetBuzzerPeriod(buzzer.period);

    DisableBuzzer();
}
