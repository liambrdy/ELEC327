#include "game_api.h"

/*
 * Stress-test demo: 20 bouncing balls + scrolling rainbow stripe band.
 *
 * Rendering: draw new position FIRST, then erase only the trailing edge
 * (pixels the ball left behind). The ball is never invisible — no flicker.
 *
 * Layout:
 *   y=0..15    HUD (FPS counter)
 *   y=16       white separator
 *   y=17..174  Ball arena
 *   y=175      white separator
 *   y=176..239 Scrolling rainbow stripes (8 x 8px, full width)
 */

#define DISP_W       320
#define DISP_H       240
#define HUD_H         16
#define BALL_Y0       17
#define BALL_AREA_H  158
#define STRIPE_Y0    176
#define STRIPE_COUNT   8
#define STRIPE_H       8
#define NUM_BALLS     20
#define BALL_SIZE     10
#define NUM_COLORS     8
#define COLOR_CYCLE   40

int bx[20];
int by[20];
int bdx[20];
int bdy[20];
int bcol[20];

int pal[8];

int frame;
int fps;
int fps_frames;
int fps_last;
int last_time;
int stripe_offset;

void init_pal() {
    pal[0] = COLOR_RED;
    pal[1] = COLOR_ORANGE;
    pal[2] = COLOR_YELLOW;
    pal[3] = COLOR_GREEN;
    pal[4] = COLOR_CYAN;
    pal[5] = COLOR_BLUE;
    pal[6] = COLOR_MAGENTA;
    pal[7] = COLOR_WHITE;
}

void draw_stripes() {
    int i;
    for (i = 0; i < STRIPE_COUNT; i++) {
        int col = (i + stripe_offset) % NUM_COLORS;
        display_fill_rect(0, STRIPE_Y0 + i * STRIPE_H, DISP_W, STRIPE_H, pal[col]);
    }
}

void draw_hud() {
    display_fill_rect(0, 0, DISP_W, HUD_H, COLOR_BLACK);
    display_draw_text(4, 3, "STRESS TEST", COLOR_YELLOW, COLOR_BLACK);
    display_draw_text(160, 3, "BALLS:", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(208, 3, NUM_BALLS, COLOR_CYAN, COLOR_BLACK);
    display_draw_text(240, 3, "FPS:", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(280, 3, fps, COLOR_GREEN, COLOR_BLACK);
}

int main() {
    int i;
    seed_random(millis());
    init_pal();

    for (i = 0; i < NUM_BALLS; i++) {
        bx[i]   = BALL_SIZE + random() % (DISP_W - BALL_SIZE * 3);
        by[i]   = BALL_Y0 + BALL_SIZE + random() % (BALL_AREA_H - BALL_SIZE * 3);
        bdx[i]  = 2 + random() % 4;
        if ((random() & 1) != 0) bdx[i] = 0 - bdx[i];
        bdy[i]  = 2 + random() % 4;
        if ((random() & 1) != 0) bdy[i] = 0 - bdy[i];
        bcol[i] = i % NUM_COLORS;
    }

    display_fill(COLOR_BLACK);
    display_fill_rect(0, HUD_H,     DISP_W, 1, COLOR_WHITE);
    display_fill_rect(0, STRIPE_Y0, DISP_W, 1, COLOR_WHITE);

    stripe_offset = 0;
    draw_stripes();

    for (i = 0; i < NUM_BALLS; i++) {
        display_fill_rect(bx[i], by[i], BALL_SIZE, BALL_SIZE, pal[bcol[i]]);
    }

    fps        = 0;
    fps_frames = 0;
    fps_last   = millis();
    frame      = 0;
    last_time  = millis();
    draw_hud();

    while (1) {
        int t    = millis();
        int btns = buttons_read();

        if (t - last_time < 16) continue;
        last_time = t;

        frame++;
        fps_frames++;

        if (t - fps_last >= 1000) {
            fps        = fps_frames;
            fps_frames = 0;
            fps_last   = t;
            draw_hud();
        }

        /*
         * Per-ball: move → draw new → erase trailing edge only.
         * The ball is drawn at its new position before anything is erased,
         * so it is never invisible. Only the pixels the ball moved out of
         * get blacked out, in at most 2 small rects.
         *
         * All loop-body locals use = 0 initializers so they go through
         * STORE_LOCAL (sp-stable after first iteration, no stack growth).
         */
        for (i = 0; i < NUM_BALLS; i++) {
            int old_x  = bx[i];
            int old_y  = by[i];
            int dx     = 0;
            int dy     = 0;
            int ax     = 0;
            int ay     = 0;
            int strip_x = 0;
            int strip_w = 0;

            /* Move */
            bx[i] = bx[i] + bdx[i];
            by[i] = by[i] + bdy[i];

            /* Bounce */
            if (bx[i] < 0) {
                bx[i] = 0; bdx[i] = 0 - bdx[i];
            }
            if (bx[i] + BALL_SIZE > DISP_W) {
                bx[i] = DISP_W - BALL_SIZE; bdx[i] = 0 - bdx[i];
            }
            if (by[i] < BALL_Y0) {
                by[i] = BALL_Y0; bdy[i] = 0 - bdy[i];
            }
            if (by[i] + BALL_SIZE > STRIPE_Y0 - 2) {
                by[i] = STRIPE_Y0 - 2 - BALL_SIZE; bdy[i] = 0 - bdy[i];
            }

            /* Delta and absolute delta */
            dx = bx[i] - old_x;
            dy = by[i] - old_y;
            ax = dx; if (ax < 0) ax = 0 - ax;
            ay = dy; if (ay < 0) ay = 0 - ay;

            /* Color cycle */
            if ((frame + i * 7) % COLOR_CYCLE == 0) {
                bcol[i] = (bcol[i] + 1) % NUM_COLORS;
            }

            /* 1. Draw at new position — ball always visible from this point */
            display_fill_rect(bx[i], by[i], BALL_SIZE, BALL_SIZE, pal[bcol[i]]);

            /* 2. Erase X-trailing strip (full ball height) */
            if (dx > 0) {
                display_fill_rect(old_x, old_y, dx, BALL_SIZE, COLOR_BLACK);
            }
            if (dx < 0) {
                display_fill_rect(bx[i] + BALL_SIZE, old_y, ax, BALL_SIZE, COLOR_BLACK);
            }

            /* 3. Erase Y-trailing strip (only the X-overlap zone, avoids double-erase) */
            if (dy != 0) {
                if (dx >= 0) { strip_x = bx[i];   }
                else         { strip_x = old_x;    }
                strip_w = BALL_SIZE - ax;
                if (strip_w > 0) {
                    if (dy > 0) {
                        display_fill_rect(strip_x, old_y, strip_w, dy, COLOR_BLACK);
                    }
                    if (dy < 0) {
                        display_fill_rect(strip_x, by[i] + BALL_SIZE, strip_w, ay, COLOR_BLACK);
                    }
                }
            }
        }

        /* Advance rainbow by one slot per frame */
        stripe_offset = (stripe_offset + 1) % NUM_COLORS;
        draw_stripes();

        if (btns & BTN_A) break;
    }

    return 0;
}
