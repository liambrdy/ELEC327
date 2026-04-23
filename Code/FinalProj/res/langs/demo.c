#include "game_api.h"

/* Simple bouncing-box demo: a 20x20 square bounces around the 320x240 screen.
   Arrow keys change its color.  Close the window or press Escape to quit. */

int main() {
    int x = 150;
    int y = 110;
    int dx = 2;
    int dy = 2;
    int color = COLOR_RED;

    display_fill(COLOR_BLACK);

    while (1) {
        int btns = buttons_read();

        if (btns & BTN_UP)    color = COLOR_RED;
        if (btns & BTN_DOWN)  color = COLOR_GREEN;
        if (btns & BTN_LEFT)  color = COLOR_BLUE;
        if (btns & BTN_RIGHT) color = COLOR_YELLOW;

        display_fill_rect(x, y, 20, 20, COLOR_BLACK);

        display_draw_text(20, 20, "Hello world!", COLOR_WHITE, COLOR_RED);

        x = x + dx;
        y = y + dy;

        if (x < 0)   { x = 0;   dx = 2; }
        if (x > 300) { x = 300; dx = 0 - 2; }
        if (y < 0)   { y = 0;   dy = 2; }
        if (y > 220) { y = 220; dy = 0 - 2; }

        display_fill_rect(x, y, 20, 20, color);
    }

    return 0;
}
