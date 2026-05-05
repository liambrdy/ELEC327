#include "flash_rom.h"
#include <ti/devices/msp/msp.h>
#include <ti/driverlib/dl_flashctl.h>
#include <string.h>

bool flash_rom_erase(uint32_t rom_size) {
    uint32_t sectors = (rom_size + ROM_FLASH_SECTOR - 1) / ROM_FLASH_SECTOR;
    for (uint32_t i = 0; i < sectors; i++) {
        uint32_t addr = ROM_FLASH_ADDR + i * ROM_FLASH_SECTOR;
        DL_FlashCTL_unprotectSector(FLASHCTL, addr, DL_FLASHCTL_REGION_SELECT_MAIN);
        DL_FLASHCTL_COMMAND_STATUS s =
            DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, addr,
                                           DL_FLASHCTL_COMMAND_SIZE_SECTOR);
        if (s == DL_FLASHCTL_COMMAND_STATUS_FAILED) return false;
    }
    return true;
}

bool flash_rom_write_chunk(uint32_t offset, const uint8_t *data, uint32_t len) {
    uint32_t addr = ROM_FLASH_ADDR + offset;
    for (uint32_t i = 0; i < len; i += 8) {
        uint32_t buf[2];  /* uint32_t for 4-byte alignment required by DriverLib */
        memset(buf, 0xFF, 8);
        uint32_t n = (len - i < 8u) ? (len - i) : 8u;
        memcpy(buf, data + i, n);
        DL_FlashCTL_unprotectSector(FLASHCTL, addr + i, DL_FLASHCTL_REGION_SELECT_MAIN);
        DL_FLASHCTL_COMMAND_STATUS s =
            DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(
                FLASHCTL, addr + i, buf);
        if (s == DL_FLASHCTL_COMMAND_STATUS_FAILED) return false;
    }
    return true;
}
