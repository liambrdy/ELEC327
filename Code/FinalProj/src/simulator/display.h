#ifndef _DISPLAY_H
#define _DISPLAY_H

#define DISPLAY_W 320
#define DISPLAY_H 240

/* COLOR_MAGENTA (63519) is the chroma key — transparent in display_draw_bitmap */
#define DISPLAY_CHROMA_KEY 63519

void display_init(void);
void display_fill(int color);
void display_draw_pixel(int x, int y, int color);
void display_fill_rect(int x, int y, int w, int h, int color);
void display_draw_rect(int x, int y, int w, int h, int color);
void display_draw_hline(int x, int y, int len, int color);
void display_draw_vline(int x, int y, int len, int color);
void display_draw_line(int x0, int y0, int x1, int y1, int color);
void display_fill_circle(int cx, int cy, int r, int color);
/* fill_circle_bg: pixels inside circle get 'color', bounding-box corners get 'bg'. */
void display_fill_circle_bg(int cx, int cy, int r, int color, int bg);
void display_draw_circle(int cx, int cy, int r, int color);
void display_draw_bitmap(int x, int y, const int *pixels, int w, int h);
/* fg = RGB565 foreground; bg = RGB565 background, or -1 for transparent */
void display_draw_text(int x, int y, const char *str, int fg, int bg);
/* draw_char: single character; bg = -1 for transparent */
void display_draw_char(int x, int y, int ch, int fg, int bg);
void display_present(void);
void display_commit(void);  /* no-op in simulator; syncs DMA on hardware */

/*
 * Hardware scroll emulation (mirrors ILI9341 VSCRDEF / VSCRSADD).
 * In landscape mode this produces horizontal panning:
 *   display_scroll_define(left, width, right) — left+width+right must == 320
 *   display_scroll_set(pos)                   — pos in [0, width-1]
 * The effect is applied during the next display_present() call.
 */
void display_scroll_define(int left_fixed, int scroll_width, int right_fixed);
void display_scroll_set(int pos);

#endif
