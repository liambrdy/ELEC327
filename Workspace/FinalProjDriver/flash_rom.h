#ifndef FLASH_ROM_H
#define FLASH_ROM_H

#include <stdint.h>
#include <stdbool.h>

#define ROM_FLASH_ADDR    0x00018000u  /* last 32 KB of 128 KB flash */
#define ROM_FLASH_SIZE    0x00008000u
#define ROM_FLASH_SECTOR  1024u        /* erase granularity */
#define ROM_MAX_SIZE      ROM_FLASH_SIZE

bool flash_rom_erase(uint32_t rom_size);
bool flash_rom_write_chunk(uint32_t offset, const uint8_t *data, uint32_t len);

#endif /* FLASH_ROM_H */
