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

static int NoteToTicks(note_type type) {
    switch (type) {
        case NOTE_WHOLE: return TICKS_IN_QUARTER * 4;
        case NOTE_HALF: return TICKS_IN_QUARTER * 2;
        case NOTE_QUARTER: return TICKS_IN_QUARTER;
        case NOTE_EIGHTH: return TICKS_IN_QUARTER / 2;
        case NOTE_SIXTEENTH: return TICKS_IN_QUARTER / 4;

        default: return 0;
    }
}

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

void PlayAnimation(state_t *state) {
    state->song_state.music_counter++;
    int currentNoteTicks = NoteToTicks(state->song_state.song[state->song_state.index].duration);
    int threshold = (state->song_state.note_state == PLAYING_NOTE) ? (STUCCATONESS * currentNoteTicks) : currentNoteTicks;

    if (state->song_state.music_counter >= threshold) {
        if (state->song_state.note_state == PLAYING_NOTE) {
            state->song_state.note_state = INTERNOTE;
            state->buzzer.sound_on = false;
            state->leds = &leds_off;
        }
        else if (state->song_state.note_state == INTERNOTE) {
            state->song_state.note_state = PLAYING_NOTE;
            state->song_state.music_counter = 0;
            state->buzzer = state->song_state.song[state->song_state.index].note;
            state->leds = state->song_state.song[state->song_state.index].leds;
            state->song_state.index = (state->song_state.index + 1) % state->song_state.song_length;
        }
    }
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
            if (new_state.song_state.song != startupAnim) {
                new_state.song_state.song = startupAnim;
                new_state.song_state.song_length = startupAnimLen;
            }

            PlayAnimation(&new_state);

            if (button_pressed > 0) {
                new_state.mode = MODE_PAUSE_BEFORE;
                new_state.gameSeed = GenerateSeed();
                new_state.sequenceLength = 1;
                new_state.sequenceCounter = 0;
                new_state.counter = 0;
            }
        } break;
        case MODE_SIMON_SAYS: {
            if (!new_state.seededThisRound) {
                srand(new_state.gameSeed);
                new_state.seededThisRound = true;
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
                    new_state.seededThisRound = false;
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
            if (!new_state.seededThisRound) {
                srand(new_state.gameSeed);
                new_state.seededThisRound = true;
            }

            if (new_state.counter == 0) {
                new_state.currentCorrect = rand();
                new_state.buzzer.sound_on = false;
            }

            if (new_state.counter >= TIME_PER_RESPONSE) {
                new_state.counter = 0;
                new_state.mode = MODE_GAME_OVER_LOSE;
                new_state.seededThisRound = false;
                new_state.readyToRestart = false;
            } else if (button_pressed > 0) {
                for (int i = 0; i < 4; i++) {
                    if (new_state.buttons[i].state != BUTTON_PRESS) continue;

                    new_state.buzzer.period = buzzer_notes[i];
                    new_state.buzzer.sound_on = true;
                    new_state.leds = &single_leds[i];
                }
            } else if (button_released > 0) {
                for (int i = 0; i < 4; i++) {
                    if (new_state.buttons[i].state != BUTTON_RELEASE) continue;

                    new_state.leds = &leds_off;
                    new_state.buzzer.sound_on = false;
                    new_state.counter = 0;

                    if (new_state.currentCorrect != i) {
                        new_state.mode = MODE_GAME_OVER_LOSE;
                        new_state.seededThisRound = false;
                        new_state.readyToRestart = false;
                        break;
                    }

                    new_state.sequenceCounter++;

                    if (new_state.sequenceCounter >= new_state.sequenceLength) {
                        new_state.sequenceCounter = 0;
                        new_state.sequenceLength++;
                        new_state.mode = MODE_SIMON_SAYS_PAUSE;
                        new_state.seededThisRound = false;

                        if (new_state.sequenceLength > WIN_LENGTH) {
                            new_state.counter = 0;
                            new_state.mode = MODE_GAME_OVER_WIN;
                            new_state.seededThisRound = false;
                            new_state.readyToRestart = false;
                        }
                    }
                }
            } else {
                new_state.counter++;
            }
        } break;
        case MODE_GAME_OVER_WIN: {
            if (new_state.song_state.song != winAnim) {
                new_state.song_state.song = winAnim;
                new_state.song_state.song_length = winAnimLen;
            }

            PlayAnimation(&new_state);

            if (current_state.mode == MODE_GAME_OVER_WIN) {
                if (button_pressed > 0) new_state.readyToRestart = true;
                if (button_released > 0 && new_state.readyToRestart) {
                    new_state.mode = MODE_PAUSE_BEFORE;
                    new_state.gameSeed = GenerateSeed();
                    new_state.sequenceLength = 1;
                    new_state.sequenceCounter = 0;
                    new_state.counter = 0;
                }
            }
        } break;
        case MODE_GAME_OVER_LOSE: {
            if (new_state.song_state.song != loseAnim) {
                new_state.song_state.song = loseAnim;
                new_state.song_state.song_length = loseAnimLen;
            }

            PlayAnimation(&new_state);

            if (current_state.mode == MODE_GAME_OVER_LOSE) {
                if (button_pressed > 0) new_state.readyToRestart = true;
                if (button_released > 0 && new_state.readyToRestart) {
                    new_state.mode = MODE_PAUSE_BEFORE;
                    new_state.gameSeed = GenerateSeed();
                    new_state.sequenceLength = 1;
                    new_state.sequenceCounter = 0;
                    new_state.counter = 0;
                }
            }
        } break;
        case MODE_PAUSE_BEFORE: {
            new_state.buzzer.sound_on = false;
            new_state.leds = &leds_off;
            
            if (new_state.counter >= SIXTEENTH_NOTE * 4) {
                new_state.counter = 0;
                new_state.mode = MODE_SIMON_SAYS;
            } else {
                new_state.counter++;
            }
        }
        default: break;
    }

    return new_state;
}

void SetBuzzerState(buzzer_state_t buzzer) {
    if (buzzer.sound_on) {
        EnableBuzzer();
    }
    else {
        DisableBuzzer();
    }

    SetBuzzerPeriod(buzzer.period);
}
