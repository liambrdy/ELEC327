#include "music.h"
#include "colors.h"
#include "state_machine_logic.h"

const animation_note_t winAnim[] = {
    {.note = {C5_LOAD, true}, .leds = &single_leds[0], .duration = NOTE_SIXTEENTH},
    {.note = {D5_LOAD, true}, .leds = &single_leds[1], .duration = NOTE_SIXTEENTH},
    {.note = {E5_LOAD, true}, .leds = &single_leds[2], .duration = NOTE_SIXTEENTH},
    {.note = {G5_LOAD, true}, .leds = &single_leds[3], .duration = NOTE_SIXTEENTH},
    {.note = {E5_LOAD, true}, .leds = &single_leds[1], .duration = NOTE_EIGHTH},
    {.note = {G5_LOAD, true}, .leds = &single_leds[3], .duration = NOTE_EIGHTH},
    {.note = {A5_LOAD, true}, .leds = &single_leds[2], .duration = NOTE_EIGHTH},
    {.note = {G5_LOAD, true}, .leds = &single_leds[0], .duration = NOTE_SIXTEENTH},
    {.note = {A5_LOAD, true}, .leds = &single_leds[3], .duration = NOTE_SIXTEENTH},
    {.note = {G5_LOAD, true}, .leds = &single_leds[1], .duration = NOTE_SIXTEENTH},
    {.note = {C5_LOAD, true}, .leds = &leds_on, .duration = NOTE_HALF},
    {.note = {A5_LOAD, true}, .leds = &leds_on, .duration = NOTE_HALF},
    {.note = {A5_LOAD, false}, .leds = &leds_off, .duration = NOTE_QUARTER},
};
const int winAnimLen = sizeof(winAnim) / sizeof(animation_note_t);

const animation_note_t loseAnim[] = {
    {.note = {G5_LOAD,  true}, .leds = &leds_on,        .duration = NOTE_EIGHTH},
    {.note = {FS5_LOAD, true}, .leds = &single_leds[3], .duration = NOTE_EIGHTH},
    {.note = {F5_LOAD,  true}, .leds = &single_leds[2], .duration = NOTE_EIGHTH},
    {.note = {E5_LOAD,  true}, .leds = &single_leds[1], .duration = NOTE_EIGHTH},
    {.note = {D5_LOAD,  true}, .leds = &single_leds[0], .duration = NOTE_EIGHTH},
    {.note = {C5_LOAD,  true}, .leds = &leds_off,       .duration = NOTE_HALF},
    {.note = {C5_LOAD,  false}, .leds = &leds_off,      .duration = NOTE_QUARTER},
};
const int loseAnimLen = sizeof(loseAnim) / sizeof(animation_note_t);

const animation_note_t startupAnim[] = {
    {.note = {E5_LOAD, true}, .leds = &single_leds[2], .duration = NOTE_EIGHTH},
    {.note = {B4_LOAD, true}, .leds = &single_leds[0], .duration = NOTE_EIGHTH},
    {.note = {C5_LOAD, true}, .leds = &single_leds[1], .duration = NOTE_EIGHTH},
    {.note = {D5_LOAD, true}, .leds = &single_leds[2], .duration = NOTE_EIGHTH},
    {.note = {C5_LOAD, true}, .leds = &single_leds[1], .duration = NOTE_EIGHTH},
    {.note = {B4_LOAD, true}, .leds = &single_leds[0], .duration = NOTE_EIGHTH},
    {.note = {C5_LOAD, true}, .leds = &single_leds[1], .duration = NOTE_EIGHTH},
    {.note = {D5_LOAD, true}, .leds = &single_leds[2], .duration = NOTE_EIGHTH},
    {.note = {E5_LOAD, true}, .leds = &single_leds[3], .duration = NOTE_QUARTER},
    {.note = {G5_LOAD, true}, .leds = &leds_on,        .duration = NOTE_HALF},
    {.note = {G5_LOAD, false}, .leds = &leds_off,      .duration = NOTE_QUARTER},
};
const int startupAnimLen = sizeof(startupAnim) / sizeof(animation_note_t);