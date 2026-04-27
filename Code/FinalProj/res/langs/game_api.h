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

/*
 * Hardware horizontal scroll (landscape mode).
 *
 * display_scroll_define(left_fixed, scroll_width, right_fixed)
 *   Divides the 320-pixel wide display into three regions:
 *     left_fixed   : pixels from the left edge that never scroll
 *     scroll_width : pixels that pan horizontally
 *     right_fixed  : pixels from the right edge that never scroll
 *   Constraint: left_fixed + scroll_width + right_fixed == 320.
 *   The top/bottom HUD rows (Y axis) are never affected — they stay fixed.
 *   Call once at game start.  Use display_scroll_define(0,320,0) for
 *   full-screen scroll.
 *
 * display_scroll_set(pos)
 *   Sets the scroll position: the frame-buffer column that appears at the
 *   left edge of the scroll area.  Range 0..scroll_width-1.
 *   Incrementing pos by 1 moves content one pixel to the LEFT on screen.
 *   The frame buffer wraps — column (pos+screen_w) % scroll_width is the
 *   right edge, which is where you draw incoming content for a side-scroller.
 */
void display_scroll_define(int left_fixed, int scroll_width, int right_fixed); /* id 10 */
void display_scroll_set(int pos);                                               /* id 11 */

/* Additional drawing primitives */
void display_draw_rect(int x, int y, int w, int h, int color);                  /* id 12 */
void display_draw_hline(int x, int y, int len, int color);                      /* id 13 */
void display_draw_vline(int x, int y, int len, int color);                      /* id 14 */
void display_draw_line(int x0, int y0, int x1, int y1, int color);              /* id 15 */
void display_fill_circle(int cx, int cy, int r, int color);                     /* id 16 */
/* display_fill_circle_bg: fill circle with 'color', corners filled with 'bg'. */
void display_fill_circle_bg(int cx, int cy, int r, int color, int bg);          /* id 17 */
void display_draw_circle(int cx, int cy, int r, int color);                     /* id 18 */
/* display_draw_char: draw a single character; bg = -1 for transparent. */
void display_draw_char(int x, int y, int ch, int fg, int bg);                   /* id 19 */

/*
 * display_commit — wait for any in-flight DMA pixel transfer to complete.
 * Call once per frame after all drawing is done to guarantee the display is
 * fully updated before reading buttons or measuring time.  Not required for
 * correctness (each draw call already serialises automatically), but useful
 * for explicit frame-rate control.
 */
void display_commit();                                                           /* id 20 */

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
