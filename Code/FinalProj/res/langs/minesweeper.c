#include "game_api.h"

#define DISP_W       320
#define DISP_H       240

#define GRID_PAD_X 10
#define GRID_PAD_Y 10

#define GRID_W 10
#define GRID_H 10

#define GRID_TILE_W 20
#define GRID_TILE_H 20

#define GRID_COLOR_1 6082378
#define GRID_COLOR_2 1541941

#define BOMB_COUNT 25

#define MIN_BOMB_DIST 1

typedef enum game_state {
    GAME_TITLE,
    GAME_PLAY,
    GAME_WIN,
    GAME_LOSE,
} game_state;

game_state state;

// GRID_W * GRID_H
int bombs[100];
int sel_x, sel_y;

void show_title() {
    display_fill(COLOR_BLACK);

    display_draw_text(136, 6, "MINESWEEPER", COLOR_YELLOW, COLOR_BLACK);

    display_draw_text(76, 110, "ARROWS  =  MOVE", COLOR_WHITE, COLOR_BLACK);
    display_draw_text(112, 124, "A  =  REVEAL", COLOR_WHITE, COLOR_BLACK);
    display_draw_text(112, 138, "B  =  MARK", COLOR_WHITE, COLOR_BLACK);
}

void show_grid() {
    for (int i = 0; i < GRID_W; i++) {
        int x = i * GRID_TILE_W + GRID_PAD_X;
        for (int j = 0; j < GRID_H; j++) {
            int y = j * GRID_TILE_H + GRID_PAD_Y;
            display_fill_rect(x, y, GRID_TILE_W, GRID_TILE_H, (i + j) % 2 == 0 ? GRID_COLOR_1 : GRID_COLOR_2);
        }
    }
}

int is_valid(int x, int y) {
    for (int dy = -MIN_BOMB_DIST; dy <= MIN_BOMB_DIST; dy++) {
        for (int dx = -MIN_BOMB_DIST; dx <= MIN_BOMB_DIST; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H) {
                if (bombs[ny * GRID_W + nx] == 1)
                    return 0;
            }
        }
    }

    return 1;
}

void generate_bombs() {
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            bombs[y * GRID_W + x] = 0;
        }
    }

    int placed = 0;
    int attempts = 0;

    while (placed < BOMB_COUNT && attempts < 10000) {
        int x = random() % GRID_W;
        int y = random() % GRID_H;

        if (bombs[y * GRID_W + x] == 0 && is_valid(x, y)) {
            bombs[y * GRID_W + x] = 1;
            placed++;
        }

        attempts++;
    }
}

int process_input(int btns) {
    if (btns & BTN_LEFT) {
        if (sel_x > 0) sel_x--;
    }

    if (btns & BTN_RIGHT) {
        if (sel_x < GRID_W - 1) sel_x++;
    }

    if (btns & BTN_UP) {
        if (sel_y > 0) sel_y--;
    }

    if (btns & BTN_DOWN) {
        if (sel_y < GRID_H - 1) sel_y++;
    }
}

int main() {
    seed_random(millis());

    state = GAME_TITLE;

    show_title();

    while (1) {
        int btns = buttons_read();
        switch (state) {
            case GAME_TITLE: {
                if (btns != 0) {
                    state = GAME_PLAY;

                    display_fill(COLOR_BLACK);

                    show_grid();
                    generate_bombs();
                }
            } break;

            case GAME_PLAY: {
                process_input(btns);
            } break;

            case GAME_LOSE: {
            } break;

            case GAME_WIN: {
            } break;
        }
    }

    return 0;
}