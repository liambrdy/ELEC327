#ifndef _GAME_API_H
#define _GAME_API_H

/*
 * Host function declarations for the game simulator and hardware target.
 * Syscall IDs are assigned by declaration order — do not insert above existing entries.
 *
 * Display is 240 x 320 pixels, portrait orientation.
 * Colors use RGB565: 5 red bits [15:11], 6 green bits [10:5], 5 blue bits [4:0].
 */

void display_fill(int color);                                        /* id 0 */
void display_draw_pixel(int x, int y, int color);                    /* id 1 */
void display_fill_rect(int x, int y, int w, int h, int color);       /* id 2 */
int  buttons_read();                                                 /* id 3 */
int  millis();                                                       /* id 4 */
void display_draw_bitmap(int x, int y, int *pixels, int w, int h);   /* id 5 */
int  random();                                                        /* id 6 */
void seed_random(int seed);                                           /* id 7 */
/* display_draw_text: draw a null-terminated string using the 5x7 bitmap font.
   fg = foreground color; bg = background color (-1 = transparent). */
void display_draw_text(int x, int y, int *str, int fg, int bg);       /* id 8 */
/* display_draw_int: draw an integer as decimal text using the same font. */
void display_draw_int(int x, int y, int n, int fg, int bg);            /* id 9 */

/* Button bitmask bits returned by buttons_read() */
#define BTN_UP    1
#define BTN_DOWN  2
#define BTN_LEFT  4
#define BTN_RIGHT 8
#define BTN_A     16
#define BTN_B     32

/* Common RGB565 colors (decimal values — compiler does not support hex literals) */
#define COLOR_BLACK   0
#define COLOR_WHITE   65535
#define COLOR_RED     63488
#define COLOR_GREEN   2016
#define COLOR_BLUE    31
#define COLOR_YELLOW  65504
#define COLOR_CYAN    2047
#define COLOR_MAGENTA 63519
#define COLOR_ORANGE  64800

/* Pixels of this color are treated as transparent by display_draw_bitmap */
#define COLOR_TRANSPARENT COLOR_MAGENTA

#endif
