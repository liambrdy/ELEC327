#include "game_api.h"

/* ---- Layout ---- */
#define DISP_W       320
#define DISP_H       240
#define HUD_H         18

#define BRICK_COLS    10
#define BRICK_ROWS     6
#define BRICK_W       28
#define BRICK_H       12
#define BRICK_GAP      2
#define BRICK_X0      11
#define BRICK_Y0      22

/* Brick area bottom: BRICK_Y0 + BRICK_ROWS*(BRICK_H+BRICK_GAP) - BRICK_GAP = 102 */

#define PADDLE_W      64
#define PADDLE_H       8
#define PADDLE_Y     224
#define PADDLE_SPEED   5
#define PADDLE_MIN_X   4
#define PADDLE_MAX_X 252   /* DISP_W - PADDLE_W - PADDLE_MIN_X */

#define BALL_SIZE      8

#define STATE_TITLE    0
#define STATE_PLAY     1
#define STATE_DEAD     2
#define STATE_GAMEOVER 3
#define STATE_WIN      4

/* ---- Global state ---- */
int bricks[60];
int row_colors[6];

int paddle_x;
int old_paddle_x;

int ball_x;
int ball_y;
int old_ball_x;
int old_ball_y;
int ball_dx;
int ball_dy;

int score;
int prev_score;
int lives;
int bricks_left;
int game_state;
int last_time;
int prev_btns;
int hud_dirty;

/* ---- Math helpers ---- */

int abs_val(int x) {
    if (x < 0) return 0 - x;
    return x;
}

int imin(int a, int b) {
    if (a < b) return a;
    return b;
}

int imax(int a, int b) {
    if (a > b) return a;
    return b;
}

/* ---- Initialisation ---- */

void init_colors() {
    row_colors[0] = COLOR_RED;
    row_colors[1] = COLOR_ORANGE;
    row_colors[2] = COLOR_YELLOW;
    row_colors[3] = COLOR_GREEN;
    row_colors[4] = COLOR_CYAN;
    row_colors[5] = COLOR_BLUE;
}

void reset_bricks() {
    int r;
    int c;
    bricks_left = 0;
    for (r = 0; r < BRICK_ROWS; r++) {
        for (c = 0; c < BRICK_COLS; c++) {
            bricks[r * BRICK_COLS + c] = row_colors[r];
            bricks_left++;
        }
    }
}

void reset_ball() {
    ball_x = paddle_x + PADDLE_W / 2 - BALL_SIZE / 2;
    ball_y = PADDLE_Y - BALL_SIZE - 4;
    /* Randomise horizontal direction */
    if ((random() & 1) != 0) {
        ball_dx = 3;
    } else {
        ball_dx = 0 - 3;
    }
    ball_dy = 0 - 3;
    old_ball_x = ball_x;
    old_ball_y = ball_y;
}

void start_game() {
    paddle_x = 128;
    old_paddle_x = 128;
    score = 0;
    prev_score = 0;
    lives = 3;
    hud_dirty = 1;
    reset_bricks();
    reset_ball();
    game_state = STATE_PLAY;
}

/* ---- Drawing: bricks ---- */

void draw_brick(int r, int c) {
    int bx = BRICK_X0 + c * (BRICK_W + BRICK_GAP);
    int by = BRICK_Y0 + r * (BRICK_H + BRICK_GAP);
    int color = bricks[r * BRICK_COLS + c];
    if (color != 0) {
        display_fill_rect(bx, by, BRICK_W, BRICK_H, color);
        /* top and left edge highlight */
        display_fill_rect(bx, by, BRICK_W, 1, COLOR_WHITE);
        display_fill_rect(bx, by, 1, BRICK_H, COLOR_WHITE);
    } else {
        display_fill_rect(bx, by, BRICK_W, BRICK_H, COLOR_BLACK);
    }
}

void draw_all_bricks() {
    int r;
    int c;
    for (r = 0; r < BRICK_ROWS; r++) {
        for (c = 0; c < BRICK_COLS; c++) {
            draw_brick(r, c);
        }
    }
}

/* Repaint any bricks that overlap the given screen rectangle (for ball-erase repair). */
void repair_bricks(int rx, int ry, int rw, int rh) {
    int rx1 = rx + rw - 1;
    int ry1 = ry + rh - 1;
    int r;
    int c;
    for (r = 0; r < BRICK_ROWS; r++) {
        int by  = BRICK_Y0 + r * (BRICK_H + BRICK_GAP);
        int by1 = by + BRICK_H - 1;
        if (by > ry1 || by1 < ry) continue;
        for (c = 0; c < BRICK_COLS; c++) {
            int bx  = BRICK_X0 + c * (BRICK_W + BRICK_GAP);
            int bx1 = bx + BRICK_W - 1;
            if (bx > rx1 || bx1 < rx) continue;
            draw_brick(r, c);
        }
    }
}

/* ---- Drawing: HUD, paddle, ball ---- */

void draw_hud() {
    display_fill_rect(0, 0, DISP_W, HUD_H, COLOR_BLACK);
    display_draw_text(4, 5, "SCORE", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(40, 5, score, COLOR_YELLOW, COLOR_BLACK);
    display_draw_text(200, 5, "LIVES", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(236, 5, lives, COLOR_CYAN, COLOR_BLACK);
}

void draw_paddle() {
    display_fill_rect(paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H, COLOR_CYAN);
    display_fill_rect(paddle_x, PADDLE_Y, PADDLE_W, 1, COLOR_WHITE);
}

void erase_paddle() {
    display_fill_rect(old_paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H, COLOR_BLACK);
}

void draw_ball() {
    display_fill_rect(ball_x, ball_y, BALL_SIZE, BALL_SIZE, COLOR_WHITE);
}

void erase_ball() {
    display_fill_rect(old_ball_x, old_ball_y, BALL_SIZE, BALL_SIZE, COLOR_BLACK);
    /* Restore any brick pixels that were covered by the ball's old position */
    repair_bricks(old_ball_x, old_ball_y, BALL_SIZE, BALL_SIZE);
}

void draw_play_screen() {
    display_fill(COLOR_BLACK);
    draw_hud();
    draw_all_bricks();
    draw_paddle();
    draw_ball();
    hud_dirty = 0;
    prev_score = score;
}

/* ---- Overlays ---- */

void show_title() {
    display_fill(COLOR_BLACK);

    /* Title at the top where text is known to render */
    display_draw_text(136, 6, "BREAKOUT", COLOR_YELLOW, COLOR_BLACK);

    /* Coloured brick preview strip */
    display_fill_rect(0, 20, DISP_W, 12, COLOR_RED);
    display_fill_rect(0, 34, DISP_W, 12, COLOR_ORANGE);
    display_fill_rect(0, 48, DISP_W, 12, COLOR_YELLOW);
    display_fill_rect(0, 62, DISP_W, 12, COLOR_GREEN);
    display_fill_rect(0, 76, DISP_W, 12, COLOR_CYAN);
    display_fill_rect(0, 90, DISP_W, 12, COLOR_BLUE);

    /* Instructions below the strips */
    display_draw_text(76, 110, "ARROWS  =  MOVE", COLOR_WHITE, COLOR_BLACK);
    display_draw_text(112, 124, "A  =  START", COLOR_WHITE, COLOR_BLACK);
}

void show_gameover() {
    display_fill_rect(50, 96, 220, 64, COLOR_BLACK);
    display_draw_text(100, 104, "GAME OVER", COLOR_RED, COLOR_BLACK);
    display_draw_text(88, 122, "SCORE:", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(136, 122, score, COLOR_YELLOW, COLOR_BLACK);
    display_draw_text(88, 140, "A = PLAY AGAIN", COLOR_WHITE, COLOR_BLACK);
}

void show_win() {
    display_fill_rect(50, 96, 220, 64, COLOR_BLACK);
    display_draw_text(108, 104, "YOU WIN!", COLOR_GREEN, COLOR_BLACK);
    display_draw_text(88, 122, "SCORE:", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(136, 122, score, COLOR_YELLOW, COLOR_BLACK);
    display_draw_text(88, 140, "A = PLAY AGAIN", COLOR_WHITE, COLOR_BLACK);
}

void show_dead_overlay() {
    display_fill_rect(0, 108, DISP_W, 28, COLOR_BLACK);
    display_draw_text(96, 112, "BALL LOST!", COLOR_RED, COLOR_BLACK);
    display_draw_text(80, 126, "A = CONTINUE", COLOR_WHITE, COLOR_BLACK);
}

void clear_dead_overlay() {
    display_fill_rect(0, 108, DISP_W, 28, COLOR_BLACK);
    repair_bricks(0, 108, DISP_W, 28);
    draw_paddle();
    draw_ball();
}

/* ---- Physics ---- */

void update_paddle(int btns) {
    old_paddle_x = paddle_x;
    if (btns & BTN_LEFT) {
        paddle_x = paddle_x - PADDLE_SPEED;
    }
    if (btns & BTN_RIGHT) {
        paddle_x = paddle_x + PADDLE_SPEED;
    }
    if (paddle_x < PADDLE_MIN_X) paddle_x = PADDLE_MIN_X;
    if (paddle_x > PADDLE_MAX_X) paddle_x = PADDLE_MAX_X;
}

void update_ball() {
    /* Move */
    ball_x = ball_x + ball_dx;
    ball_y = ball_y + ball_dy;

    /* Wall bounces */
    if (ball_x < 0) {
        ball_x = 0;
        ball_dx = abs_val(ball_dx);
    }
    if (ball_x + BALL_SIZE > DISP_W) {
        ball_x = DISP_W - BALL_SIZE;
        ball_dx = 0 - abs_val(ball_dx);
    }
    if (ball_y < HUD_H + 2) {
        ball_y = HUD_H + 2;
        ball_dy = abs_val(ball_dy);
    }

    /* Fell off bottom — lose a life */
    if (ball_y + BALL_SIZE > DISP_H) {
        lives--;
        hud_dirty = 1;
        if (lives <= 0) {
            game_state = STATE_GAMEOVER;
        } else {
            game_state = STATE_DEAD;
            reset_ball();
        }
        return;
    }

    /* Paddle bounce */
    if (ball_dy > 0 &&
        ball_y + BALL_SIZE >= PADDLE_Y &&
        ball_y < PADDLE_Y + PADDLE_H &&
        ball_x + BALL_SIZE > paddle_x &&
        ball_x < paddle_x + PADDLE_W) {

        ball_y = PADDLE_Y - BALL_SIZE;
        ball_dy = 0 - abs_val(ball_dy);

        /* Angle: offset from paddle center maps to horizontal velocity */
        int center = paddle_x + PADDLE_W / 2;
        int offset = (ball_x + BALL_SIZE / 2) - center;
        ball_dx = offset / 8;
        if (ball_dx == 0) ball_dx = 1;
    }

    /* Brick collision — check each alive brick; stop after first hit */
    int r;
    int c;
    int hit;
    int ball_r;
    int ball_b;
    hit = 0;
    for (r = 0; r < BRICK_ROWS; r++) {
        if (hit) break;
        for (c = 0; c < BRICK_COLS; c++) {
            if (bricks[r * BRICK_COLS + c] == 0) continue;
            int bx  = BRICK_X0 + c * (BRICK_W + BRICK_GAP);
            int by  = BRICK_Y0 + r * (BRICK_H + BRICK_GAP);
            int bx1 = bx + BRICK_W - 1;
            int by1 = by + BRICK_H - 1;
            ball_r = ball_x + BALL_SIZE - 1;
            ball_b = ball_y + BALL_SIZE - 1;

            if (ball_x <= bx1 && ball_r >= bx && ball_y <= by1 && ball_b >= by) {
                int ov_x = imin(ball_r, bx1) - imax(ball_x, bx) + 1;
                int ov_y = imin(ball_b, by1) - imax(ball_y, by) + 1;

                bricks[r * BRICK_COLS + c] = 0;
                bricks_left--;
                score = score + (BRICK_ROWS - r) * 10;
                hud_dirty = 1;

                if (ov_x <= ov_y) {
                    ball_dx = 0 - ball_dx;
                } else {
                    ball_dy = 0 - ball_dy;
                }

                draw_brick(r, c);
                hit = 1;

                if (bricks_left <= 0) {
                    game_state = STATE_WIN;
                }
                break;
            }
        }
    }
}

/* ---- Main ---- */

int main() {
    seed_random(millis());
    init_colors();

    game_state = STATE_TITLE;
    prev_btns  = 0;
    last_time  = 0;
    score      = 0;

    show_title();

    while (1) {
        int btns = buttons_read();
        int a_pressed = (btns & BTN_A) && !(prev_btns & BTN_A);
        prev_btns = btns;

        if (game_state == STATE_TITLE) {
            if (a_pressed) {
                start_game();
                draw_play_screen();
            }

        } else if (game_state == STATE_PLAY) {
            int t = millis();
            if (t - last_time < 33) continue;
            last_time = t;

            old_ball_x = ball_x;
            old_ball_y = ball_y;

            update_paddle(btns);
            update_ball();

            if (game_state != STATE_PLAY) {
                /* State changed during physics — show overlay */
                erase_ball();
                if (hud_dirty) {
                    draw_hud();
                    hud_dirty = 0;
                }
                if (game_state == STATE_DEAD) {
                    show_dead_overlay();
                } else if (game_state == STATE_GAMEOVER) {
                    show_gameover();
                } else if (game_state == STATE_WIN) {
                    show_win();
                }
            } else {
                /* Normal frame */
                erase_ball();
                if (old_paddle_x != paddle_x) {
                    erase_paddle();
                    draw_paddle();
                }
                draw_ball();
                if (hud_dirty) {
                    draw_hud();
                    hud_dirty = 0;
                }
            }

        } else if (game_state == STATE_DEAD) {
            if (a_pressed) {
                clear_dead_overlay();
                game_state = STATE_PLAY;
            }

        } else if (game_state == STATE_GAMEOVER) {
            if (a_pressed) {
                start_game();
                draw_play_screen();
            }

        } else if (game_state == STATE_WIN) {
            if (a_pressed) {
                start_game();
                draw_play_screen();
            }
        }
    }

    return 0;
}
