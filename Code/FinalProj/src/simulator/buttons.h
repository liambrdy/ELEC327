#ifndef _BUTTONS_H
#define _BUTTONS_H

/* Bitmask bits — must match BTN_* defines in game_api.h */
#define BTN_UP    (1 << 0)
#define BTN_DOWN  (1 << 1)
#define BTN_LEFT  (1 << 2)
#define BTN_RIGHT (1 << 3)
#define BTN_A     (1 << 4)
#define BTN_B     (1 << 5)

/*
 * Present the current frame, pump SDL events, apply a ~60 fps cap,
 * and return the current button bitmask.
 * Calls exit(0) if the window is closed.
 */
int buttons_poll(void);

#endif
