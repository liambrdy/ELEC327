#include "state.h"
#include <ti/devices/msp/msp.h>

#include <stdbool.h>
#include <math.h>

#include "buzzer.h"
#include "leds.h"

#define TICKS_TO_MS(ticks) (ticks * 0.6)
#define MS_TO_TICKS(ms) (ms * 1.66)

#define FREQ_TO_DAC(hz) (4 * hz)

#define G3 195.998
#define C4 261.626f
#define D4 293.66f
#define E4 329.628f
#define G4 391.995f

#define CL FREQ_TO_DAC(C4)
#define DL FREQ_TO_DAC(D4)
#define EL FREQ_TO_DAC(E4)

#define ARR_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define ON 0x0F
#define BRIGHT 0x10

#define START_FRAME 0x0,0x0
#define END_FRAME 0xFFFF,0xFFFF
#define LED_FRAME(r, g, b, brightness) ((0b111 << 13) | (brightness << 8) | (b)),(((g) << 8) | (r))
#define LED_OFF LED_FRAME(0x0, 0x0, 0x0, BRIGHT)

float tones[4] = {
    FREQ_TO_DAC(G4),
    FREQ_TO_DAC(E4),
    FREQ_TO_DAC(C4),
    FREQ_TO_DAC(G3),
};

uint16_t offTxPacket[] = {START_FRAME, LED_OFF, LED_OFF, LED_OFF, LED_OFF, END_FRAME};
#define LED_DATA_LEN (ARR_LEN(offTxPacket))

uint16_t toneLEDs[] = {
    START_FRAME, LED_FRAME(0x0, ON, 0x0, BRIGHT), LED_OFF, LED_OFF, LED_OFF, END_FRAME,
    START_FRAME, LED_OFF, LED_FRAME(ON, 0x0, 0x0, BRIGHT), LED_OFF, LED_OFF, END_FRAME,
    START_FRAME, LED_OFF, LED_OFF, LED_FRAME(ON / 2, ON / 2, 0x0, BRIGHT), LED_OFF, END_FRAME,
    START_FRAME, LED_OFF, LED_OFF, LED_OFF, LED_FRAME(0x0, 0x0, ON, BRIGHT), END_FRAME,
};

#define E_COL 0x0F, 0x00, 0x00   // red
#define D_COL 0x00, 0x0F, 0x00   // green  
#define C_COL 0x00, 0x00, 0x0F   // blue

note_t song[] = {
    {EL, NOTE_QUARTER, E_COL}, {DL, NOTE_QUARTER, D_COL}, {CL, NOTE_QUARTER, C_COL}, {DL, NOTE_QUARTER, D_COL},
    {EL, NOTE_QUARTER, E_COL}, {EL, NOTE_QUARTER, E_COL}, {EL, NOTE_HALF, E_COL},
    {DL, NOTE_QUARTER, D_COL}, {DL, NOTE_QUARTER, D_COL}, {DL, NOTE_HALF, D_COL},
    {EL, NOTE_QUARTER, E_COL}, {EL, NOTE_QUARTER, E_COL}, {EL, NOTE_HALF, E_COL},
    {EL, NOTE_QUARTER, E_COL}, {DL, NOTE_QUARTER, D_COL}, {CL, NOTE_QUARTER, C_COL}, {DL, NOTE_QUARTER, D_COL},
    {EL, NOTE_QUARTER, E_COL}, {EL, NOTE_QUARTER, E_COL}, {EL, NOTE_QUARTER, E_COL}, {CL, NOTE_QUARTER, C_COL},
    {DL, NOTE_QUARTER, D_COL}, {DL, NOTE_QUARTER, D_COL}, {EL, NOTE_QUARTER, E_COL}, {DL, NOTE_QUARTER, D_COL}, {CL, NOTE_WHOLE, C_COL},
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

void OutputFromState(state_t *current_state, state_t *prev_state) {
    bool stateChanged = prev_state->state != current_state->state;
    bool noteChanged = prev_state->currentNote != current_state->currentNote;

    switch (current_state->state) {
        case STATE_SONG: {
            if (stateChanged || noteChanged) {
                SetBuzzerFrequency(song[current_state->currentNote].freq);
                current_state->lerpCounter = 0;
            }

            #define LERP_DURATION (int)(2 * TICKS_IN_QUARTER * STUCCATONESS)
            current_state->lerpCounter = (current_state->lerpCounter + 1) % LERP_DURATION;

            note_t *note = &song[current_state->currentNote];
            int t = (current_state->lerpCounter * 256) / LERP_DURATION;
            int brightLED = (t * 4) / 257;

            uint8_t r = note->r;
            uint8_t g = note->g;
            uint8_t b = note->b;

            uint8_t invR = ON - r;
            uint8_t invG = ON - g;
            uint8_t invB = ON - b;

            uint8_t origMax = r > g ? (r > b ? r : b) : (g > b ? g : b);
            uint8_t invMax  = invR > invG ? (invR > invB ? invR : invB) : (invG > invB ? invG : invB);

            if (invMax > 0) {
                invR = (invR * origMax) / invMax;
                invG = (invG * origMax) / invMax;
                invB = (invB * origMax) / invMax;
            }

            static uint16_t songPacket[12];
            songPacket[0] = 0x0; songPacket[1] = 0x0;
            for (int i = 0; i < 4; i++) {
                uint16_t scale = (i == brightLED) ? 256 : 30;
                uint8_t pr = (i == brightLED) ? r : invR;
                uint8_t pg = (i == brightLED) ? g : invG;
                uint8_t pb = (i == brightLED) ? b : invB;
                uint8_t sr = (scale * pr) / 256;
                uint8_t sg = (scale * pg) / 256;
                uint8_t sb = (scale * pb) / 256;
                songPacket[2 + i*2]     = (0b111 << 13) | (BRIGHT << 8) | sb;
                songPacket[2 + i*2 + 1] = (sg << 8) | sr;
            }
            songPacket[10] = 0xFFFF; songPacket[11] = 0xFFFF;
            SendSPIMessage(songPacket, 12);
        } break;

        case STATE_PAUSE: {
            if (stateChanged) { 
                SetBuzzerFrequency(0.0f);
            }

            SendSPIMessage(offTxPacket, LED_DATA_LEN);
        } break;
        
        case STATE_TONE: {
            int buttonDownCount = 0;
            for (int i = 0; i < 4; i++) buttonDownCount += (int)current_state->buttonDown[i];

            if (buttonDownCount == 0) {
                SetBuzzerFrequency(0.0f);
                
                SendSPIMessage(offTxPacket, LED_DATA_LEN);
            }

            for (int i = 0; i < 4; i++) {
                if (current_state->buttonDown[i]) {
                    if (prev_state->buttonDown[i] != current_state->buttonDown[i]) {
                        SetBuzzerFrequency(tones[i]);
                    }

                    SendSPIMessage(toneLEDs + (i * LED_DATA_LEN), LED_DATA_LEN);
                    break;
                }
            }
        } break;
    }
}