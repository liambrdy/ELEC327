
/*
 * Game console firmware — SD card launcher.
 *
 * Boot sequence (normal):
 *   1. Init TFT, SysTick, buttons.
 *   2. Init SD card and mount FAT32.
 *   3. Scan root directory for *.ROM files (up to MAX_GAMES).
 *   4. Show a scrollable game-selection menu.
 *   5. On A press: stream ROM from SD into flash slot, run the VM.
 *   6. When the game returns, go back to step 3.
 *
 * Debug mode (no SD card needed):
 *   Uncomment #define DEBUG_ROM below.  The firmware will skip SD init and
 *   run the ROM baked into rom_data.h directly from flash on every boot.
 *   Regenerate rom_data.h with:
 *     ./gen_rom.sh game.c -H Workspace/TFTHomemadeClaude/rom_data.h
 */

#define DEBUG_ROM /* uncomment to skip SD and run built-in rom_data.h */

#include <ti/devices/msp/msp.h>
#include "tft.h"
#include "delay.h"
#include "vm.h"
#include "game_syscall.h"
#include "graphics.h"
#include "buttons.h"

#ifdef DEBUG_ROM
#include "rom_data.h"
#include "flash_rom.h"
#else
#include "sd.h"
#include "fat32.h"
#include "flash_rom.h"
#endif

/* ---- TFT initialisation sequence (ILI9341) ---- */

static void tft_init_sequence(void) {
    TFTWriteCommand(0x01, NULL, 0);           /* software reset */
    delay_cycles(32000 * 150);

    uint8_t p[15];
    p[0]=0x03; p[1]=0x80; p[2]=0x02;
    TFTWriteCommand(0xEF, p, 3);

    p[0]=0x00; p[1]=0xC1; p[2]=0x30;
    TFTWriteCommand(0xCF, p, 3);

    p[0]=0x64; p[1]=0x03; p[2]=0x12; p[3]=0x81;
    TFTWriteCommand(0xED, p, 4);

    p[0]=0x85; p[1]=0x00; p[2]=0x78;
    TFTWriteCommand(0xE8, p, 3);

    p[0]=0x39; p[1]=0x2C; p[2]=0x00; p[3]=0x34; p[4]=0x02;
    TFTWriteCommand(0xCB, p, 5);

    p[0]=0x20;
    TFTWriteCommand(0xF7, p, 1);

    p[0]=0x00; p[1]=0x00;
    TFTWriteCommand(0xEA, p, 2);

    p[0]=0x23;
    TFTWriteCommand(0xC0, p, 1);
    p[0]=0x10;
    TFTWriteCommand(0xC1, p, 1);

    p[0]=0x3E; p[1]=0x28;
    TFTWriteCommand(0xC5, p, 2);
    p[0]=0x86;
    TFTWriteCommand(0xC7, p, 1);

    p[0] = 0x28; /* MADCTL: MV=1 landscape, BGR=1 */
    TFTWriteCommand(0x36, p, 1);

    p[0]=0x00;
    TFTWriteCommand(0x37, p, 1);
    p[0]=0x55;
    TFTWriteCommand(0x3A, p, 1);   /* pixel format: RGB565 */
    p[0]=0x00; p[1]=0x18;
    TFTWriteCommand(0xB1, p, 2);
    p[0]=0x08; p[1]=0x82; p[2]=0x27;
    TFTWriteCommand(0xB6, p, 3);
    p[0]=0x00;
    TFTWriteCommand(0xF2, p, 1);
    p[0]=0x01;
    TFTWriteCommand(0x26, p, 1);

    TFTWriteCommand(0x11, NULL, 0); /* sleep out */
    delay_cycles(32000 * 120);
    TFTWriteCommand(0x29, NULL, 0); /* display on */
    delay_cycles(32000 * 20);
}

/* ---- SysTick: 1 ms tick at 32 MHz ---- */

static void init_systick(void) {
    SysTick->LOAD = 32000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = 7u;
}

/* ---- GPIO power-up ---- */

static void InitializeGpio(void) {
    GPIOA->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W |
                            GPIO_RSTCTL_RESETSTKYCLR_CLR |
                            GPIO_RSTCTL_RESETASSERT_ASSERT);
    GPIOA->GPRCM.PWREN  = (GPIO_PWREN_KEY_UNLOCK_W |
                            GPIO_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY);

    GPIOB->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W |
                            GPIO_RSTCTL_RESETSTKYCLR_CLR |
                            GPIO_RSTCTL_RESETASSERT_ASSERT);
    GPIOB->GPRCM.PWREN  = (GPIO_PWREN_KEY_UNLOCK_W |
                            GPIO_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY);
}

/* ---- Launcher constants ---- */

#define MAX_GAMES      16
#define VISIBLE_ROWS   10   /* game rows visible at once              */
#define ROW_H          18   /* pixels per game list row               */
#define LIST_Y0        18   /* y of first game row                    */
#define FOOTER_Y       (LIST_Y0 + VISIBLE_ROWS * ROW_H + 2)
#define TITLE_BG       0x0010u   /* very dark blue                    */
#define SEL_BG         0x000Fu   /* dark blue highlight                */
#define NORMAL_BG      COLOR_BLACK
#define TITLE_FG       COLOR_YELLOW
#define NORMAL_FG      COLOR_WHITE
#define SEL_FG         COLOR_WHITE
#define FOOTER_FG      0x7BEFu   /* mid-grey                          */

/* ---- Static allocation ---- */

static vm_t vm;

#ifndef DEBUG_ROM

static fat32_file_t  game_list[MAX_GAMES];
static int           game_count;

static bool flash_write_cb(uint32_t offset, const uint8_t *data,
                           uint32_t len, void *ctx) {
    (void)ctx;
    return flash_rom_write_chunk(offset, data, len);
}

/* ---- Launcher UI -------------------------------------------------------- */

static void draw_title(void) {
    TFTFillRect(0, 0, SCREEN_W, LIST_Y0 - 1, TITLE_BG);
    TFTDrawString(4, 5, "GAME LAUNCHER", TITLE_FG, TITLE_BG);
    TFTFillRect(0, LIST_Y0 - 1, SCREEN_W, 1, COLOR_WHITE);
}

static void draw_footer(void) {
    TFTFillRect(0, FOOTER_Y, SCREEN_W, SCREEN_H - FOOTER_Y, TITLE_BG);
    TFTFillRect(0, FOOTER_Y, SCREEN_W, 1, COLOR_WHITE);
    TFTDrawString(4, FOOTER_Y + 4, "UP/DOWN: SELECT  A: LAUNCH", FOOTER_FG, TITLE_BG);
}

static void draw_row(int row_idx, int game_idx, int selected) {
    int y = LIST_Y0 + row_idx * ROW_H;
    uint16_t bg = (game_idx == selected) ? SEL_BG : NORMAL_BG;
    uint16_t fg = (game_idx == selected) ? SEL_FG : NORMAL_FG;

    TFTFillRect(0, y, SCREEN_W, ROW_H, bg);

    /* Selection cursor bar on the left */
    if (game_idx == selected) {
        TFTFillRect(0, y + 1, 3, ROW_H - 2, COLOR_CYAN);
    }

    if (game_idx < game_count) {
        TFTDrawString(8, y + 5, game_list[game_idx].name, fg, bg);
    }
}

static void draw_all_rows(int scroll, int selected) {
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        draw_row(i, scroll + i, selected);
    }
}

static void show_message(const char *line1, const char *line2, uint16_t color) {
    TFTFillScreen(COLOR_BLACK);
    TFTDrawString(10, 100, line1, color, COLOR_BLACK);
    if (line2) TFTDrawString(10, 116, line2, FOOTER_FG, COLOR_BLACK);
}

/*
 * Block until an SD card is present and successfully initialised.
 * Handles: no card, bad card, card swapped after failure.
 */
static void wait_for_sd(void) {
    for (;;) {
        /* Wait for a card to be physically present. */
        if (!sd_card_present()) {
            show_message("Insert SD card", "to continue.", COLOR_ORANGE);
            while (!sd_card_present()) {}
            delay_cycles(32000u * 200u); /* 200 ms debounce */
        }

        show_message("Initialising SD card...", NULL, NORMAL_FG);

        if (!sd_init()) {
            show_message("SD card init failed.", "Try reseating the card.", COLOR_RED);
            /* Wait for card removal so the user can reseat it. */
            while (sd_card_present()) {}
            continue;
        }

        if (!fat32_init()) {
            show_message("FAT32 error.", "Format SD as FAT32.", COLOR_RED);
            while (sd_card_present()) {}
            continue;
        }

        return; /* SD is ready */
    }
}

#endif /* !DEBUG_ROM */

/* ---- Main --------------------------------------------------------------- */

int main(void) {
    InitializeGpio();
    InitializeTFT();
    tft_init_sequence();
    init_systick();
    buttons_init();

#ifdef DEBUG_ROM
    /* Debug mode: bake rom_data.h into the flash slot and run from there.
       This exercises the full erase→program→execute path without an SD card. */
    TFTFillScreen(COLOR_BLACK);
    TFTDrawString(10, 100, "DEBUG ROM MODE", COLOR_YELLOW, COLOR_BLACK);
    TFTDrawString(10, 112, rom_data_name, FOOTER_FG, COLOR_BLACK);
    TFTDrawString(10, 124, "Writing to flash...", NORMAL_FG, COLOR_BLACK);

    if (!flash_rom_erase(rom_data_size) ||
        !flash_rom_write_chunk(0, rom_data, rom_data_size)) {
        TFTFillScreen(COLOR_BLACK);
        TFTDrawString(10, 108, "Flash write failed.", COLOR_RED, COLOR_BLACK);
        for (;;) {}
    }

    TFTDrawString(10, 136, "OK", COLOR_GREEN, COLOR_BLACK);
    delay_cycles(32000u * 800u);

    for (;;) {
        if (!vm_load_rom(&vm, (const uint8_t *)ROM_FLASH_ADDR, rom_data_size)) {
            TFTFillScreen(COLOR_BLACK);
            TFTDrawString(10, 108, "Bad ROM in flash.", COLOR_RED, COLOR_BLACK);
            for (;;) {}
        }
        vm.syscall_handler = game_syscall;
        vm_run(&vm);
        TFTScrollDefine(0, 320, 0);
        TFTScrollSet(0);
    }
#else  /* !DEBUG_ROM — normal SD card launcher */

    wait_for_sd();

    /* Main launcher loop: re-scan the card each time we return from a game,
       in case the user swapped cards. */
    for (;;) {
        /* Scan root directory for .ROM files. */
        game_count = fat32_find_files("ROM", game_list, MAX_GAMES);

        /* Draw the launcher screen. */
        TFTFillScreen(COLOR_BLACK);
        draw_title();
        draw_footer();

        int selected = 0;
        int scroll   = 0;

        if (game_count == 0) {
            TFTDrawString(8, LIST_Y0 + 10, "No .ROM files found on SD card.", COLOR_RED, COLOR_BLACK);
            /* Wait for user to swap card and press A. */
            uint32_t prev = hw_buttons_read();
            while (1) {
                uint32_t btns = hw_buttons_read();
                if ((btns & BTN_MASK_A) && !(prev & BTN_MASK_A)) break;
                prev = btns;
            }
            continue;
        }

        draw_all_rows(scroll, selected);

        /* Menu navigation loop. */
        uint32_t prev_btns = hw_buttons_read();

        while (1) {
            /* Card removed while browsing — re-init before rescanning. */
            if (!sd_card_present()) {
                wait_for_sd();
                break;
            }

            uint32_t btns    = hw_buttons_read();
            uint32_t pressed = btns & ~prev_btns;
            prev_btns = btns;

            int moved = 0;

            if (pressed & BTN_MASK_UP) {
                if (selected > 0) {
                    selected--;
                    if (selected < scroll) scroll = selected;
                    moved = 1;
                }
            }

            if (pressed & BTN_MASK_DOWN) {
                if (selected < game_count - 1) {
                    selected++;
                    if (selected >= scroll + VISIBLE_ROWS)
                        scroll = selected - VISIBLE_ROWS + 1;
                    moved = 1;
                }
            }

            if (moved) {
                draw_all_rows(scroll, selected);
            }

            if (pressed & BTN_MASK_A) {
                show_message("Loading...", game_list[selected].name, NORMAL_FG);

                if (game_list[selected].size > ROM_MAX_SIZE) {
                    show_message("ROM too large.", "Max 32 KB.", COLOR_RED);
                    delay_cycles(32000u * 2000u);
                    break;
                }

                /* Erase only as many 1 KB sectors as the ROM needs. */
                if (!flash_rom_erase(game_list[selected].size)) {
                    show_message("Flash error.", "Erase failed.", COLOR_RED);
                    delay_cycles(32000u * 2000u);
                    break;
                }

                /* Stream the ROM from SD directly into the flash slot. */
                uint32_t bytes = fat32_read_file_stream(
                    &game_list[selected], flash_write_cb, NULL);

                if (bytes != game_list[selected].size) {
                    if (!sd_card_present()) {
                        wait_for_sd();
                    } else {
                        show_message("Read error.", "Check SD card.", COLOR_RED);
                        delay_cycles(32000u * 2000u);
                    }
                    break;
                }

                /* Point the VM at the ROM now resident in flash. */
                if (!vm_load_rom(&vm, (const uint8_t *)ROM_FLASH_ADDR, bytes)) {
                    show_message("Bad ROM file.", game_list[selected].name, COLOR_RED);
                    delay_cycles(32000u * 2000u);
                    break;
                }

                vm.syscall_handler = game_syscall;
                vm_run(&vm);

                TFTScrollDefine(0, 320, 0);
                TFTScrollSet(0);
                break;
            }
        }
    }

#endif /* DEBUG_ROM */
}
