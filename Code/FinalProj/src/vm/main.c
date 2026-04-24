#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "disass.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <rom_file> [-d]\n", argv[0]);
        return 1;
    }

    bool disass = false;
    for (int i = 2; i < argc; i++) {
        char *param = argv[i];
        if (param[0] == '-' && param[1] == 'd') {
            disass = true;
        }
    }

    vm_t vm;
    u8 *rom = VmLoadRom(&vm, argv[1]);
    if (!rom) return 1;

    if (disass) {
        disassemble(&vm);
    } else {
        vm_result_t result = VmRun(&vm);
        if (result != VM_OK) {
            printf("vm error: %s\n", VmResultStr(result));
            free(rom);
            return 1;
        }
    }

    free(rom);
    return vm.exit_code;
}
