#ifndef graphics_h
#define graphics_h

#include <stdint.h>

/* Display is configured in landscape mode (MADCTL 0x28). */
#define SCREEN_W 320
#define SCREEN_H 240

/* 5×7 bitmap font — character cell is 6×8 pixels (glyph + 1px gap each axis).
   font5x7.h also defines these; guards prevent redefinition if both are included. */
#ifndef FONT_CHAR_W
#define FONT_CHAR_W 6
#endif
#ifndef FONT_CHAR_H
#define FONT_CHAR_H 8
#endif

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_ORANGE  0xFC00

void TFTFillScreen(uint16_t color);
void TFTFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void TFTDrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void TFTDrawHLine(int16_t x, int16_t y, int16_t len, uint16_t color);
void TFTDrawVLine(int16_t x, int16_t y, int16_t len, uint16_t color);
void TFTDrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void TFTFillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void TFTFillCircleBG(int16_t cx, int16_t cy, int16_t r, uint16_t color, uint16_t bg);
void TFTDrawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);

/* Text rendering using the built-in 5×7 font. */
/* Basic text — bg is the background color (opaque). */
void TFTDrawChar(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg);
void TFTDrawString(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg);
void TFTDrawInt(int16_t x, int16_t y, int32_t n, uint16_t fg, uint16_t bg);

/* Extended text — bg < 0 renders transparent (no background pixels). */
void TFTDrawCharEx(int16_t x, int16_t y, uint8_t ch, uint16_t fg, int bg);
void TFTDrawStringEx(int16_t x, int16_t y, const char *s, uint16_t fg, int bg);
void TFTDrawIntEx(int16_t x, int16_t y, int32_t n, uint16_t fg, int bg);

#endif
