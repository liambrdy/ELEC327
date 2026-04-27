#ifndef SD_H
#define SD_H

#include <stdint.h>
#include <stdbool.h>

/*
 * SD card SPI driver — shares SPI1 with the TFT.
 * The SD card needs its own CS pin; change the defines below to match
 * your PCB wiring.
 *
 * Steps to find the right values:
 *   1. Pick a free GPIO pin (e.g. PA3).
 *   2. Look up its PINCM index in the MSPM0G3507 datasheet (Table 6-xx).
 *   3. Fill in SD_CS_PINCM, SD_CS_PF, SD_CS_PORT, SD_CS_PIN.
 *
 * The examples below use PA3 (PINCM4).  Change them to your actual pin.
 */
#define SD_CS_PINCM   IOMUX_PINCM4
#define SD_CS_PF      IOMUX_PINCM4_PF_GPIOA_DIO29
#define SD_CS_PORT    GPIOA
#define SD_CS_PIN     (1UL << 3)

/*
 * Card-detect (CD) pin — the mechanical switch inside the SD socket.
 * Change these to match your PCB wiring.
 *
 * Most SD sockets/modules are active-low: the pin is pulled LOW when a card
 * is inserted.  Set SD_CD_ACTIVE_LOW to 1 for that case (most common).
 * If your socket pulls the pin HIGH on insertion, set it to 0.
 *
 * The example below uses PA4 (PINCM5).  Change to your actual pin.
 */
#define SD_CD_PINCM       IOMUX_PINCM5
#define SD_CD_PF          IOMUX_PINCM5_PF_GPIOA_DIO30
#define SD_CD_PORT        GPIOA
#define SD_CD_PIN         (1UL << 4)
#define SD_CD_ACTIVE_LOW  1   /* 1 = card present when pin LOW (most common) */

/* Initialise the SD card over SPI.  Must be called after InitializeTFT().
   Returns true on success, false if the card failed to initialise. */
bool sd_init(void);

/* Return true if a card is physically present in the socket.
   Reads the CD switch; does not talk to the card over SPI. */
bool sd_card_present(void);

/* Read one 512-byte block (sector) from the SD card.
   block : zero-based block address (byte address / 512).
   buf   : caller-supplied buffer, must be at least 512 bytes.
   Returns true on success. */
bool sd_read_block(uint32_t block, uint8_t *buf);

#endif /* SD_H */
