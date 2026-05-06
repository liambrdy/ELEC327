#include "game_api.h"

/* ---- Layout ---- */
#define DISP_W      320
#define DISP_H      240

#define CELL        10
#define COLS        32
#define ROWS        22
#define GRID_Y0     18
#define GRID_SIZE   704   /* COLS * ROWS */

/* Pixel origin of grid */
#define GRID_X0     0

/* ---- Game states ---- */
#define STATE_TITLE  0
#define STATE_PLAY   1
#define STATE_OVER   2

/* ---- Cell values ---- */
#define CELL_EMPTY   0
#define CELL_SNAKE   1
#define CELL_FOOD    2

/* ---- Directions ---- */
#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_LEFT  2
#define DIR_RIGHT 3

/* ---- Speed ---- */
#define INIT_SPEED  200
#define MIN_SPEED    80
#define SPEED_STEP   10
#define SPEED_EVERY   5

/* ---- Global state ---- */
int grid[704];
int snake[704];

int snake_head;
int snake_len;
int dir;
int next_dir;
int food_pos;
int score;
int high_score;
int game_state;
int last_time;
int speed;
int prev_btns;

/* ---- Helpers ---- */

int pos_row(int pos) {
    int r;
    r = 0;
    while (r < ROWS) {
        if (pos < (r + 1) * COLS) return r;
        r++;
    }
    return 0;
}

int pos_col(int pos) {
    int r;
    r = pos_row(pos);
    return pos - r * COLS;
}

/* ---- Rendering ---- */

void draw_cell(int pos, int color) {
    int px;
    int py;
    px = GRID_X0 + pos_col(pos) * CELL;
    py = GRID_Y0 + pos_row(pos) * CELL;
    display_fill_rect(px, py, CELL, CELL, COLOR_BLACK);
    if (color == CELL_FOOD) {
        display_fill_circle(px + 5, py + 5, 4, COLOR_RED);
    } else if (color == CELL_SNAKE) {
        display_fill_circle(px + 5, py + 5, 4, COLOR_GREEN);
    }
}

void draw_hud() {
    display_fill_rect(0, 0, DISP_W, GRID_Y0, COLOR_BLACK);
    display_draw_text(4, 4, "SCORE", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(50, 4, score, COLOR_YELLOW, COLOR_BLACK);
    display_draw_text(170, 4, "BEST", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(210, 4, high_score, COLOR_CYAN, COLOR_BLACK);
}

void draw_grid_border() {
    display_fill_rect(0, GRID_Y0 - 1, DISP_W, 1, COLOR_WHITE);
    display_fill_rect(0, GRID_Y0 + ROWS * CELL, DISP_W, 1, COLOR_WHITE);
}

void draw_full_grid() {
    int i;
    display_fill_rect(0, GRID_Y0, DISP_W, ROWS * CELL, COLOR_BLACK);
    for (i = 0; i < GRID_SIZE; i++) {
        if (grid[i] == CELL_SNAKE) {
            draw_cell(i, CELL_SNAKE);
        } else if (grid[i] == CELL_FOOD) {
            draw_cell(i, CELL_FOOD);
        }
    }
}

/* ---- Food spawning ---- */

void spawn_food() {
    int pos;
    int tries;
    tries = 0;
    pos = random() % GRID_SIZE;
    while (grid[pos] != CELL_EMPTY && tries < GRID_SIZE) {
        pos = (pos + 1) % GRID_SIZE;
        tries++;
    }
    if (grid[pos] == CELL_EMPTY) {
        food_pos = pos;
        grid[food_pos] = CELL_FOOD;
        draw_cell(food_pos, CELL_FOOD);
    }
}

/* ---- Game init ---- */

void start_game() {
    int i;
    int start_pos;
    for (i = 0; i < GRID_SIZE; i++) {
        grid[i] = CELL_EMPTY;
        snake[i] = 0;
    }

    start_pos = ROWS / 2 * COLS + COLS / 2;
    snake[0] = start_pos;
    snake_head = 0;
    snake_len = 1;
    grid[start_pos] = CELL_SNAKE;

    dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    score = 0;
    speed = INIT_SPEED;
    last_time = millis();

    display_fill(COLOR_BLACK);
    draw_hud();
    draw_grid_border();
    draw_full_grid();
    spawn_food();

    game_state = STATE_PLAY;
}

/* ---- Title and overlay screens ---- */

void show_title() {
    display_fill(COLOR_BLACK);
    display_draw_text(124, 60, "SNAKE", COLOR_GREEN, COLOR_BLACK);
    display_draw_text(64, 90, "ARROWS  =  MOVE", COLOR_WHITE, COLOR_BLACK);
    display_draw_text(100, 108, "Z  =  START", COLOR_WHITE, COLOR_BLACK);
    if (high_score > 0) {
        display_draw_text(88, 140, "BEST SCORE:", COLOR_CYAN, COLOR_BLACK);
        display_draw_int(184, 140, high_score, COLOR_YELLOW, COLOR_BLACK);
    }
}

void show_gameover() {
    display_fill_rect(60, 88, 200, 72, COLOR_BLACK);
    display_fill_rect(60, 88, 200, 1, COLOR_RED);
    display_fill_rect(60, 159, 200, 1, COLOR_RED);
    display_fill_rect(60, 88, 1, 72, COLOR_RED);
    display_fill_rect(259, 88, 1, 72, COLOR_RED);
    display_draw_text(100, 100, "GAME OVER", COLOR_RED, COLOR_BLACK);
    display_draw_text(88, 118, "SCORE:", COLOR_WHITE, COLOR_BLACK);
    display_draw_int(140, 118, score, COLOR_YELLOW, COLOR_BLACK);
    display_draw_text(80, 136, "Z = TRY AGAIN", COLOR_WHITE, COLOR_BLACK);
}

/* ---- Main ---- */

int main() {
    seed_random(millis());
    high_score = 0;
    game_state = STATE_TITLE;
    prev_btns = 0;
    show_title();

    while (1) {
        int btns = buttons_read();
        int a_pressed = (btns & BTN_A) && !(prev_btns & BTN_A);
        int b_pressed = (btns & BTN_B);
        prev_btns = btns;

        if (b_pressed) return 0;

        if (game_state == STATE_TITLE) {
            if (a_pressed) {
                start_game();
            }

        } else if (game_state == STATE_PLAY) {
            int t = millis();

            /* Direction input — queue next direction, prevent reversal */
            if (btns & BTN_UP) {
                if (dir != DIR_DOWN) next_dir = DIR_UP;
            }
            if (btns & BTN_DOWN) {
                if (dir != DIR_UP) next_dir = DIR_DOWN;
            }
            if (btns & BTN_LEFT) {
                if (dir != DIR_RIGHT) next_dir = DIR_LEFT;
            }
            if (btns & BTN_RIGHT) {
                if (dir != DIR_LEFT) next_dir = DIR_RIGHT;
            }

            if (t - last_time < speed) continue;
            last_time = t;

            dir = next_dir;

            /* Compute new head position */
            {
                int head_pos = 0;
                int head_row = 0;
                int head_col = 0;
                int new_row = 0;
                int new_col = 0;
                int new_pos = 0;
                int ate = 0;
                int tail_idx = 0;
                int tail_pos = 0;

                head_pos = snake[snake_head];
                head_row = pos_row(head_pos);
                head_col = pos_col(head_pos);

                new_row = head_row;
                new_col = head_col;

                if (dir == DIR_UP) new_row = head_row - 1;
                if (dir == DIR_DOWN) new_row = head_row + 1;
                if (dir == DIR_LEFT) new_col = head_col - 1;
                if (dir == DIR_RIGHT) new_col = head_col + 1;

                /* Wall collision */
                if (new_row < 0 || new_row >= ROWS || new_col < 0 || new_col >= COLS) {
                    if (score > high_score) high_score = score;
                    game_state = STATE_OVER;
                    show_gameover();
                    continue;
                }

                new_pos = new_row * COLS + new_col;

                ate = 0;
                if (grid[new_pos] == CELL_FOOD) {
                    ate = 1;
                } else if (grid[new_pos] == CELL_SNAKE) {
                    /* Self collision — check it's not the tail (which will vacate) */
                    tail_idx = (snake_head - snake_len + 1 + GRID_SIZE) % GRID_SIZE;
                    tail_pos = snake[tail_idx];
                    if (new_pos != tail_pos) {
                        if (score > high_score) high_score = score;
                        game_state = STATE_OVER;
                        show_gameover();
                        continue;
                    }
                }

                /* Advance head */
                snake_head = (snake_head + 1) % GRID_SIZE;
                snake[snake_head] = new_pos;
                grid[new_pos] = CELL_SNAKE;
                draw_cell(new_pos, CELL_SNAKE);

                if (ate) {
                    snake_len++;
                    score = score + 10;

                    /* Speed up every SPEED_EVERY points */
                    if (score % (SPEED_EVERY * 10) == 0) {
                        if (speed > MIN_SPEED) {
                            speed = speed - SPEED_STEP;
                        }
                    }

                    draw_hud();
                    spawn_food();
                } else {
                    /* Remove tail */
                    tail_idx = (snake_head - snake_len + GRID_SIZE) % GRID_SIZE;
                    tail_pos = snake[tail_idx];
                    grid[tail_pos] = CELL_EMPTY;
                    draw_cell(tail_pos, CELL_EMPTY);
                }
                display_commit();
            }

        } else if (game_state == STATE_OVER) {
            if (a_pressed) {
                game_state = STATE_TITLE;
                show_title();
            }
        }
    }

    return 0;
}
