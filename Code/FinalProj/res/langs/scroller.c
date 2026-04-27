#include "game_api.h"

/*
 * Hardware scroll demo — infinite parallax star field.
 *
 * After init(), the main loop does ZERO display calls.  Every frame is a
 * single write to the scroll register; the ILI9341 handles the rest.
 *
 * Because the scroll area is exactly 320 pixels wide (= display width),
 * the frame buffer tiles seamlessly — stars loop back without a visible
 * seam.
 *
 * Controls:
 *   UP / DOWN  — increase / decrease scroll speed (1–8 px/frame)
 *   LEFT       — reverse direction
 *   RIGHT      — forward direction
 *   A          — exit
 */

#define DISP_W    320
#define DISP_H    240

/* Star layers: slow stars are drawn larger / brighter to fake depth. */
#define STARS_FAR   120   /* 1×1 dim white             */
#define STARS_MID    50   /* 1×1 bright white           */
#define STARS_NEAR   20   /* 2×2 bright cyan/yellow     */

/* Scenery */
#define NUM_PLANETS   4   /* coloured 5×5 blobs         */

/* ---- globals ---- */
int scroll_pos;
int speed;
int dir;           /* +1 or -1 */
int prev_btns;
int last_time;

/* ---- helpers ---- */

int iabs(int x) {
    if (x < 0) return 0 - x;
    return x;
}

/* ---- init ---- */

void draw_stars() {
    int i;

    /* Far stars — dim, 1×1 */
    for (i = 0; i < STARS_FAR; i++) {
        int x = random() % DISP_W;
        int y = random() % DISP_H;
        display_draw_pixel(x, y, 16904);  /* mid-grey RGB565 */
    }

    /* Mid stars — bright, 1×1 */
    for (i = 0; i < STARS_MID; i++) {
        int x = random() % DISP_W;
        int y = random() % DISP_H;
        display_draw_pixel(x, y, COLOR_WHITE);
    }

    /* Near stars — 2×2, coloured */
    for (i = 0; i < STARS_NEAR; i++) {
        int x = random() % (DISP_W - 2);
        int y = random() % (DISP_H - 2);
        int col = (i % 3 == 0) ? COLOR_CYAN :
                  (i % 3 == 1) ? COLOR_YELLOW : COLOR_WHITE;
        display_fill_rect(x, y, 2, 2, col);
    }
}

void draw_planets() {
    int i;
    int planet_x[4];
    int planet_y[4];
    int planet_col[4];
    int planet_r[4];

    planet_col[0] = COLOR_RED;
    planet_col[1] = COLOR_ORANGE;
    planet_col[2] = COLOR_CYAN;
    planet_col[3] = COLOR_MAGENTA;

    for (i = 0; i < NUM_PLANETS; i++) {
        planet_x[i] = 20 + (i * 75) + random() % 30;
        planet_y[i] = 30 + random() % (DISP_H - 60);
        planet_r[i] = 6 + random() % 8;

        /* Draw filled square planet (no circle syscall yet) */
        int r = planet_r[i];
        int px = planet_x[i];
        int py = planet_y[i];
        display_fill_rect(px - r, py - r, r * 2, r * 2, planet_col[i]);

        /* Highlight dot (top-left quadrant) */
        display_fill_rect(px - r / 2, py - r / 2, r / 2, r / 2, COLOR_WHITE);
    }
}

void draw_terrain() {
    /* Simple ground silhouette at the bottom of the screen. */
    int x;
    int h;
    for (x = 0; x < DISP_W; x += 4) {
        /* Vary height using a simple wave pattern */
        h = 8 + (x % 80) / 5;
        display_fill_rect(x, DISP_H - h, 4, h, 2016);   /* COLOR_GREEN */
    }
    /* Darker sub-layer */
    display_fill_rect(0, DISP_H - 4, DISP_W, 4, 1024);  /* dark green */
}

/* ---- main ---- */

int main() {
    seed_random(millis());

    scroll_pos = 0;
    speed      = 2;
    dir        = 1;
    prev_btns  = 0;

    /* Full-screen horizontal scroll: TFA=0, VSA=320, BFA=0. */
    display_scroll_define(0, DISP_W, 0);

    /* Draw the entire scene into the frame buffer once. */
    display_fill(COLOR_BLACK);
    draw_stars();
    draw_planets();
    draw_terrain();

    last_time = millis();

    while (1) {
        /* ~60 fps cap */
        int t = millis();
        if (t - last_time < 16) continue;
        last_time = t;

        int btns    = buttons_read();
        int up_p    = (btns & BTN_UP)    && !(prev_btns & BTN_UP);
        int down_p  = (btns & BTN_DOWN)  && !(prev_btns & BTN_DOWN);
        int left_p  = (btns & BTN_LEFT)  && !(prev_btns & BTN_LEFT);
        int right_p = (btns & BTN_RIGHT) && !(prev_btns & BTN_RIGHT);
        prev_btns = btns;

        if (up_p   && speed < 8) speed = speed + 1;
        if (down_p && speed > 0) speed = speed - 1;
        if (left_p)              dir   = 0 - 1;
        if (right_p)             dir   = 1;

        if (btns & BTN_A) break;

        /* Advance scroll — this is the entire render cost per frame. */
        scroll_pos = scroll_pos + speed * dir;
        if (scroll_pos < 0)      scroll_pos = scroll_pos + DISP_W;
        if (scroll_pos >= DISP_W) scroll_pos = scroll_pos - DISP_W;

        display_scroll_set(scroll_pos);
    }

    /* Reset scroll before returning to launcher. */
    display_scroll_define(0, DISP_W, 0);
    display_scroll_set(0);
    return 0;
}
