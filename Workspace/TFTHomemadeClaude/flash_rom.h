#ifndef FLASH_ROM_H
#define FLASH_ROM_H

#include <stdint.h>
#include <stdbool.h>

/*
 * On-demand ROM slot in main flash (last 32 KB of 128 KB).
 * The linker script reserves this region — firmware lives below 0x18000.
 *
 * Requires <ti/driverlib/dl_flashctl.h> to be reachable in the include path.
 */
#define ROM_FLASH_ADDR    0x00018000u
#define ROM_FLASH_SIZE    0x00008000u   /* 32 KB */
#define ROM_FLASH_SECTOR  1024u         /* erase granularity: 1 KB sectors */

/* Maximum ROM file size that can be loaded. */
#define ROM_MAX_SIZE      ROM_FLASH_SIZE

/*
 * Erase enough 1 KB sectors to hold rom_size bytes.
 * Must be called before flash_rom_write_chunk for a new ROM.
 * Returns false if a sector erase fails.
 */
bool flash_rom_erase(uint32_t rom_size);

/*
 * Write one chunk of ROM data to the flash slot at byte offset.
 * offset + len must not exceed ROM_FLASH_SIZE.
 * len is rounded up to 8 bytes internally (extra bytes written as 0xFF).
 * Returns false if a program operation fails.
 */
bool flash_rom_write_chunk(uint32_t offset, const uint8_t *data, uint32_t len);

#endif /* FLASH_ROM_H */
