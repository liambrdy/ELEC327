#include "buttons.h"
#include "display.h"
#include <SDL2/SDL.h>
#include <stdlib.h>

#define TARGET_FRAME_MS 16  /* ~60 fps */

static uint32_t last_present_ms = 0;

int buttons_poll(void) {
    /* Frame-rate cap: wait until 16 ms since last present. */
    uint32_t now = SDL_GetTicks();
    uint32_t elapsed = now - last_present_ms;
    if (elapsed < TARGET_FRAME_MS)
        SDL_Delay(TARGET_FRAME_MS - elapsed);
    last_present_ms = SDL_GetTicks();

    /* Present the frame built up since the last poll. */
    display_present();

    /* Pump all pending events; quit on window close. */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) exit(0);
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) exit(0);
    }

    /* Sample current keyboard state. */
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    int mask = 0;
    if (keys[SDL_SCANCODE_UP])     mask |= BTN_UP;
    if (keys[SDL_SCANCODE_DOWN])   mask |= BTN_DOWN;
    if (keys[SDL_SCANCODE_LEFT])   mask |= BTN_LEFT;
    if (keys[SDL_SCANCODE_RIGHT])  mask |= BTN_RIGHT;
    if (keys[SDL_SCANCODE_Z])      mask |= BTN_A;
    if (keys[SDL_SCANCODE_X])      mask |= BTN_B;
    return mask;
}
