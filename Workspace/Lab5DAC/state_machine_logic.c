#include "state_machine_logic.h"
#include <ti/devices/msp/msp.h>

#include <stdbool.h>
#include <math.h>

#include "hw_interface.h"

#define SAMPLE_RATE 32000.0f

#define TICKS_TO_MS(ticks) (ticks * 0.6)
#define MS_TO_TICKS(ms) (ms * 1.66)

#define FREQ_TO_DAC(hz) (SAMPLE_RATE / (4 * hz))

#define G3 195.998
#define C4 261.626f
#define D4 293.66f
#define E4 329.628f
#define G4 391.995f

#define CL FREQ_TO_DAC(C4)
#define DL FREQ_TO_DAC(D4)
#define EL FREQ_TO_DAC(E4)

#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

float tones[4] = {
    FREQ_TO_DAC(G4),
    FREQ_TO_DAC(E4),
    FREQ_TO_DAC(C4),
    FREQ_TO_DAC(G3),
};

// bpm
#define TEMPO 120
#define TICKS_IN_QUARTER MS_TO_TICKS(60000 / TEMPO)

#define STUCCATONESS 0.8

note_t song[] = {
    {EL, NOTE_QUARTER}, {DL, NOTE_QUARTER}, {CL, NOTE_QUARTER}, {DL, NOTE_QUARTER},
    {EL, NOTE_QUARTER}, {EL, NOTE_QUARTER}, {EL, NOTE_HALF},
    {DL, NOTE_QUARTER}, {DL, NOTE_QUARTER}, {DL, NOTE_HALF},
    {EL, NOTE_QUARTER}, {EL, NOTE_QUARTER}, {EL, NOTE_HALF},
    {EL, NOTE_QUARTER}, {DL, NOTE_QUARTER}, {CL, NOTE_QUARTER}, {DL, NOTE_QUARTER},
    {EL, NOTE_QUARTER}, {EL, NOTE_QUARTER}, {EL, NOTE_QUARTER}, {CL, NOTE_QUARTER},
    {DL, NOTE_QUARTER}, {DL, NOTE_QUARTER}, {EL, NOTE_QUARTER}, {DL, NOTE_QUARTER}, {CL, NOTE_WHOLE},
};

static int NoteToTicks(note_type type) {
    switch (type) {
        case NOTE_WHOLE: return TICKS_IN_QUARTER * 4;
        case NOTE_HALF: return TICKS_IN_QUARTER * 2;
        case NOTE_QUARTER: return TICKS_IN_QUARTER;

        default: return 0;
    }
}

state_t GetNextState(state_t current_state, uint32_t button_input) {
    state_t new_state = current_state;
    
    for (int i = 0; i < 4; i++) {
        bool button_down = (button_input & (1 << (23 + i))) == 0;
        if (button_down) {
            new_state.buttonDownLength[i] = current_state.buttonDownLength[i] + 1;
            if (new_state.buttonDownLength[i] > TICKS_TO_MS(5)) {
                new_state.buttonDown[i] = true;

                new_state.state = STATE_TONE;
            }
        } else {
            new_state.buttonDown[i] = false;
            new_state.buttonDownLength[i] = 0;
        }
    }

    if (new_state.state == STATE_SONG || new_state.state == STATE_PAUSE) {
        new_state.songCounter++;
        int currentNoteTicks = NoteToTicks(song[current_state.currentNote].type);
        int threshold = (new_state.state == STATE_SONG) ? (STUCCATONESS * currentNoteTicks) : currentNoteTicks;

        if (new_state.songCounter >= threshold) {
            if (new_state.state == STATE_SONG)
                new_state.state = STATE_PAUSE;
            else if (new_state.state == STATE_PAUSE) {
                new_state.state = STATE_SONG;
                new_state.songCounter = 0;
                new_state.currentNote = (new_state.currentNote + 1) % ARR_LEN(song);
            }
        }
    }

    return new_state;
}

static uint8_t GetDataForDAC(state_t current_state) {
    float val = sinf(2.0f * M_PI * current_state.dacCounter);
    return (uint8_t)(val * 120 + 128);
}

void OutputFromState(state_t *current_state) {
    switch (current_state->state) {
        case STATE_SONG: {
            EnableDAC();
            current_state->dacCounter += song[current_state->currentNote].freq / SAMPLE_RATE;
            
            if (current_state->dacCounter >= 1.0f)
                current_state->dacCounter -= 1.0f;

            uint8_t val = GetDataForDAC(*current_state);
            SetDACData(val);
        } break;

        case STATE_PAUSE: {
            DisableDAC();
        } break;
        
        case STATE_TONE: {
            int buttonDownCount = 0;
            for (int i = 0; i < 4; i++) buttonDownCount += (int)current_state->buttonDown[i];

            if (buttonDownCount == 0) {
                DisableDAC();
            }

            for (int i = 0; i < 4; i++) {
                if (current_state->buttonDown[i]) {
                    EnableDAC();

                    current_state->dacCounter += tones[i] / SAMPLE_RATE;
                    if (current_state->dacCounter >= 1.0f)
                        current_state->dacCounter -= 1.0;

                    uint8_t val = GetDataForDAC(*current_state);
                    SetDACData(val);
                    break;
                }
            }
        } break;
    }
}