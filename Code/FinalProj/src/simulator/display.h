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
void display_draw_bitmap(int x, int y, const int *pixels, int w, int h);
/* fg = RGB565 foreground; bg = RGB565 background, or -1 for transparent */
void display_draw_text(int x, int y, const char *str, int fg, int bg);
void display_present(void);

#endif
