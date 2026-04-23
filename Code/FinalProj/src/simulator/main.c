#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#include "vm.h"
#include "display.h"
#include "syscall.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <rom_file>\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    display_init();

    vm_t vm;
    u8 *rom = VmLoadRom(&vm, argv[1]);
    if (!rom) {
        SDL_Quit();
        return 1;
    }

    vm.syscall_handler = sim_syscall;

    vm_result_t result = VmRun(&vm);
    if (result != VM_OK)
        printf("vm error: %s\n", VmResultStr(result));

    free(rom);
    SDL_Quit();
    return vm.exit_code;
}
