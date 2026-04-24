#include "game_api.h"

void do_fill() {
    display_fill(COLOR_BLACK);
    return;
}

int main() {
    do_fill();
    while (1) {
        buttons_read();
    }
    return 0;
}
