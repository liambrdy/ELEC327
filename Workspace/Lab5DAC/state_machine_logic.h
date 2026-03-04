#ifndef state_machine_logic_include
#define state_machine_logic_include

#include <stdbool.h>
#include <stdint.h>

typedef enum buzzer_state {
    STATE_SONG,
    STATE_PAUSE,
    STATE_TONE,
} buzzer_state;

typedef enum note_type {
    NOTE_WHOLE,
    NOTE_HALF,
    NOTE_QUARTER,
} note_type;

typedef struct note_t {
    float freq;
    note_type type;
} note_t;

// State struct
typedef struct state_t {
    buzzer_state state;
    int buttonDownLength[4];
    bool buttonDown[4];

    int songCounter;
    int currentNote;

    float dacCounter;
} state_t;

state_t GetNextState(state_t current_state, uint32_t button_input);
void OutputFromState(state_t *current_state);

#endif /* state_machine_logic_include */
