#ifndef state_machine_logic_include
#define state_machine_logic_include

#include <stdint.h>
#include <stdbool.h>
#include "leds.h" // For LED state type

// PERIOD * MAX_COUNTER = 32000
#define PERIOD 20 // This means updates every 0.625 ms
#define MAX_COUNTER 1600 // This needs to be a multiple of 4 for modulo PWM to work!
#define FLASH_COUNTER 400 // 2x per second flashing

#define BUTTON_BOUNCE_LIMIT 3

#define SIXTEENTH_NOTE 200

#define SIMON_SAYS_PAUSE MAX_COUNTER / 8
#define SIMON_SAYS_NOTE_DURATION SIXTEENTH_NOTE * 4

#define TIME_PER_RESPONSE SIMON_SAYS_NOTE_DURATION * 8
#define WIN_LENGTH 5

#define TICKS_TO_MS(ticks) (ticks * 0.6)
#define MS_TO_TICKS(ms) (ms * 1.66)

// bpm
#define TEMPO 120
#define TICKS_IN_QUARTER MS_TO_TICKS(60000.0f / TEMPO)

#define STUCCATONESS 0.5

/* For our state machine, we need to think about the MODE, the SOUND, and the BUTTONS */

/* Let's start by defining a enum and a struct that will be useful for tracking the state of  
 *  the buttons. */
typedef enum {
    BUTTON_IDLE = 0,
    BUTTON_BOUNCING,
    BUTTON_PRESS,
    BUTTON_RELEASE,
} button_state_t;

typedef struct {
   button_state_t state; 
   uint32_t       depressed_counter;
} button_t;

/* Next, let's define the state of the sound*/
// Basic info for the buzzer's state
typedef struct {
    uint16_t period;
    bool     sound_on;
} buzzer_state_t;

typedef enum note_type {
    NOTE_WHOLE,
    NOTE_HALF,
    NOTE_QUARTER,
    NOTE_EIGHTH,
    NOTE_SIXTEENTH,
} note_type;

// More complex info for playing songs.
// We'll define a song as a linked list of notes and durations.
// Rests would be defined with sound_on = false.
typedef struct music_note {
    buzzer_state_t     note;
    note_type          duration;
} music_note_t;

typedef struct animation_note {
    buzzer_state_t note;
    const leds_message_t *leds;
    uint16_t       duration;
} animation_note_t;

typedef enum {
    PLAYING_NOTE,
    INTERNOTE
} internote_t;

typedef struct {
    const animation_note_t *song;         // this is an array
    int          song_length;
    int          index;
    internote_t  note_state;  // allows us to keep track of inter-note breaks
    uint32_t     music_counter; // this will actually be used with the tick counter to achieve durations
} song_state_t;

/* And last, let's define the possible modes */
typedef enum {
    MODE_STARTUP = 0,
    MODE_PAUSE_BEFORE,
    MODE_SIMON_SAYS,
    MODE_SIMON_SAYS_PAUSE,
    MODE_SIMON_RESPOND,
    MODE_GAME_OVER_WIN,
    MODE_GAME_OVER_LOSE,
} mode_t;

/* Finally, we can define our state machine state*/
typedef struct {
    button_t buttons[4];
    buzzer_state_t buzzer;
    const leds_message_t *leds;
    mode_t mode;
    song_state_t song_state; 

    uint16_t gameSeed;
    uint16_t sequenceCounter;
    uint16_t sequenceLength;

    uint16_t counter;
    bool readyToRestart;
    bool seededThisRound;

    uint8_t currentCorrect;
} state_t;

state_t GetNextState(state_t current_state, uint32_t input);
void SetBuzzerState(buzzer_state_t);

extern const animation_note_t winAnim[];
extern const int winAnimLen;

extern const animation_note_t loseAnim[];
extern const int loseAnimLen;

extern const animation_note_t startupAnim[];
extern const int startupAnimLen;

#endif /* state_machine_logic_include */
